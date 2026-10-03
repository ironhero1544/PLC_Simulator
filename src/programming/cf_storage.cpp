#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace plc {
namespace {
constexpr uint32_t kEmpty = UINT32_MAX;
constexpr uint32_t kDeleted = UINT32_MAX - 1;
constexpr int kStampLengths[] = {0, 19, 17, 19, 17, 19, 17, 8};
constexpr int kFieldLengths[] = {0, 1, 6, 11, 4, 8, 14, 0, 0};
}  // namespace

bool CompiledPLCExecutor::ConfigureCfCard(int channel, uint32_t capacity_bytes,
                                          uint32_t record_capacity,
                                          uint32_t buffer_capacity_bytes) {
  if (channel < 1 || channel > 2 || capacity_bytes < 4096 ||
      record_capacity < 254 || record_capacity > 2097152 ||
      buffer_capacity_bytes == 0)
    return false;
  auto card = std::make_unique<CfCard>();
  card->capacity = capacity_bytes;
  card->buffer_capacity = buffer_capacity_bytes;
  card->ids.fill(-1);
  uint32_t table_size = 1;
  while (table_size < record_capacity * 2)
    table_size *= 2;
  card->rows.resize(table_size);
  card->cells.resize(record_capacity);
  card->free_cells.reserve(record_capacity);
  for (uint32_t cell = 0; cell < record_capacity; ++cell)
    card->free_cells.push_back(record_capacity - cell - 1);
  const uint32_t pages = std::min(record_capacity, capacity_bytes / 1024);
  card->pages.resize(pages);
  card->free_pages.reserve(pages);
  for (uint32_t page = 0; page < pages; ++page)
    card->free_pages.push_back(pages - page - 1);
  cf_cards_[channel - 1] = std::move(card);
  return SetCfCardMounted(channel, true);
}

bool CompiledPLCExecutor::SetCfCardMounted(int channel, bool mounted) {
  if (channel < 1 || channel > 2 || !cf_cards_[channel - 1])
    return false;
  auto& card = *cf_cards_[channel - 1];
  if (!mounted) {
    FlushCfBuffer(&card);
  }
  card.mounted = mounted;
  const int offset = (channel - 1) * 20;
  memory_.special_m[404 + offset] = card.ready;
  memory_.special_m[405 + offset] = mounted;
  memory_.D[8408 + offset] = 100;
  return true;
}

CompiledPLCExecutor::CfRow* CompiledPLCExecutor::FindCfRow(CfCard* card,
                                                           int file, int line,
                                                           bool create) {
  const uint32_t key = static_cast<uint32_t>((file + 1) * 32768 + line);
  size_t index = (key * 2654435761u) & (card->rows.size() - 1);
  CfRow* deleted = nullptr;
  for (size_t probe = 0; probe < card->rows.size(); ++probe) {
    auto& row = card->rows[index];
    if (row.key == key)
      return &row;
    if (row.key == kDeleted && !deleted)
      deleted = &row;
    if (row.key == kEmpty) {
      if (!create)
        return nullptr;
      auto* result = deleted ? deleted : &row;
      *result = {};
      result->key = key;
      return result;
    }
    index = (index + 1) & (card->rows.size() - 1);
  }
  if (create && deleted) {
    *deleted = {};
    deleted->key = key;
    return deleted;
  }
  return nullptr;
}

void CompiledPLCExecutor::DeleteCfRow(CfCard* card, CfRow* row) {
  uint32_t cell = row->first;
  while (cell != kEmpty) {
    const auto& value = card->cells[cell];
    if (value.page != kEmpty)
      card->free_pages.push_back(value.page);
    const uint32_t next = value.next;
    card->free_cells.push_back(cell);
    cell = next;
  }
  card->used -= row->bytes;
  if (row->buffered)
    card->buffered_bytes -= row->bytes;
  *row = {};
  row->key = kDeleted;
}

void CompiledPLCExecutor::DeleteCfFile(CfCard* card, int file) {
  for (auto& row : card->rows)
    if (row.key < kDeleted && static_cast<int>(row.key / 32768) == file + 1)
      DeleteCfRow(card, &row);
  for (auto& id : card->ids)
    if (id == file)
      id = -1;
  card->files[file] = {};
}

void CompiledPLCExecutor::FlushCfBuffer(CfCard* card, int file) {
  for (auto& row : card->rows)
    if (row.key < kDeleted && row.buffered &&
        (file < 0 || static_cast<int>(row.key / 32768) == file + 1)) {
      card->buffered_bytes -= row.bytes;
      row.buffered = false;
    }
}

bool CompiledPLCExecutor::LoadCfRow(const CfCard& card, const CfRow& row,
                                    std::span<CfDatum> data) const {
  if (data.size() < row.count)
    return false;
  uint32_t cell = row.first;
  for (int index = 0; index < row.count; ++index) {
    if (cell == kEmpty || cell >= card.cells.size())
      return false;
    const auto& value = card.cells[cell];
    auto& destination = data[index];
    destination.type = value.type;
    destination.length = value.length;
    destination.bits = value.bits;
    if (value.page != kEmpty)
      std::copy_n(card.pages[value.page].begin(), value.length + 1,
                  destination.text.begin());
    cell = value.next;
  }
  return true;
}

int CompiledPLCExecutor::StoreCfRow(CfCard* card, int file, int line,
                                    std::span<const CfDatum> data,
                                    bool buffered) {
  auto* existing = FindCfRow(card, file, line, false);
  uint32_t bytes = 10 + kStampLengths[card->files[file].timestamp];
  uint32_t pages = 0;
  for (const auto& value : data) {
    bytes += 1 + (value.type >= 7 ? value.length : kFieldLengths[value.type]);
    pages += value.type >= 7;
  }
  if (bytes > 16384)
    return 612;
  uint32_t old_pages = 0;
  if (existing)
    for (uint32_t cell = existing->first; cell != kEmpty;
         cell = card->cells[cell].next)
      old_pages += card->cells[cell].page != kEmpty;
  if (card->used - (existing ? existing->bytes : 0) + bytes > card->capacity ||
      data.size() >
          card->free_cells.size() + (existing ? existing->count : 0) ||
      pages > card->free_pages.size() + old_pages)
    return 902;
  auto* row = existing ? existing : FindCfRow(card, file, line, true);
  if (!row)
    return 902;
  const uint32_t key = row->key;
  if (existing)
    DeleteCfRow(card, existing);
  *row = {};
  row->key = key;
  row->bytes = bytes;
  row->buffered = buffered;
  row->count = static_cast<uint16_t>(data.size());
  row->timestamp = rtc_time_.time_since_epoch().count();
  uint32_t previous = kEmpty;
  for (const auto& value : data) {
    const uint32_t index = card->free_cells.back();
    card->free_cells.pop_back();
    auto& cell = card->cells[index];
    cell = {};
    cell.type = value.type;
    cell.bits = value.bits;
    cell.length = value.length;
    if (value.type >= 7) {
      cell.page = card->free_pages.back();
      card->free_pages.pop_back();
      std::copy_n(value.text.begin(), value.length + 1,
                  card->pages[cell.page].begin());
    }
    if (previous == kEmpty)
      row->first = index;
    else
      card->cells[previous].next = index;
    previous = index;
  }
  card->used += bytes;
  if (buffered)
    card->buffered_bytes += bytes;
  if (card->buffered_bytes >= card->buffer_capacity)
    FlushCfBuffer(card);
  return 0;
}

bool CompiledPLCExecutor::PowerCycleCfCard(int channel) {
  if (channel < 1 || channel > 2 || !cf_cards_[channel - 1])
    return false;
  auto& card = *cf_cards_[channel - 1];
  for (auto& row : card.rows)
    if (row.key < kDeleted && row.buffered)
      DeleteCfRow(&card, &row);
  for (auto& file : card.files)
    file.last_line = 0;
  for (const auto& row : card.rows)
    if (row.key < kDeleted) {
      auto& file = card.files[row.key / 32768 - 1];
      file.last_line =
          std::max(file.last_line, static_cast<int>(row.key % 32768));
    }
  for (auto& file : card.files)
    file.next_line = file.last_line == file.maximum_lines && file.policy == 1
                         ? 1
                         : file.last_line + 1;
  card.owner = SIZE_MAX;
  card.mixed.active = false;
  card.errors.fill(0);
  return SetCfCardMounted(channel, true);
}

std::optional<std::string> CompiledPLCExecutor::ReadCfCsv(
    int channel, std::string_view name) const {
  if (channel < 1 || channel > 2 || !cf_cards_[channel - 1])
    return std::nullopt;
  std::string filename(name);
  for (char& character : filename)
    character =
        static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  if (filename.ends_with(".CSV"))
    filename.resize(filename.size() - 4);
  const auto& card = *cf_cards_[channel - 1];
  int file = -1;
  for (size_t slot = 0; slot < card.files.size(); ++slot)
    if (card.files[slot].exists && filename == card.files[slot].name.data()) {
      file = static_cast<int>(slot);
      break;
    }
  if (file < 0)
    return std::nullopt;
  std::ostringstream output;
  for (int line = 0; line <= card.files[file].last_line; ++line) {
    const uint32_t key = static_cast<uint32_t>((file + 1) * 32768 + line);
    size_t index = (key * 2654435761u) & (card.rows.size() - 1);
    const CfRow* row = nullptr;
    for (size_t probe = 0; probe < card.rows.size(); ++probe) {
      if (card.rows[index].key == key) {
        row = &card.rows[index];
        break;
      }
      if (card.rows[index].key == kEmpty)
        break;
      index = (index + 1) & (card.rows.size() - 1);
    }
    if (!row || row->buffered)
      continue;
    output << (line == 0 ? "Index,DATE TIME" : std::to_string(line) + ",");
    if (line != 0 && card.files[file].timestamp) {
      const auto time =
          std::chrono::sys_seconds{std::chrono::seconds{row->timestamp}};
      const auto date = std::chrono::year_month_day{
          std::chrono::floor<std::chrono::days>(time)};
      const std::chrono::hh_mm_ss clock{
          time - std::chrono::floor<std::chrono::days>(time)};
      const int mode = card.files[file].timestamp;
      const int year = static_cast<int>(date.year());
      const int month = static_cast<unsigned>(date.month());
      const int day = static_cast<unsigned>(date.day());
      output << std::setfill('0');
      if (mode < 7) {
        const int first = mode <= 2   ? (mode == 1 ? year : year % 100)
                          : mode <= 4 ? day
                                      : month;
        const int middle = mode <= 4 ? month : day;
        const int last = mode <= 2 ? day : mode % 2 ? year : year % 100;
        output << std::setw(mode == 1 ? 4 : 2) << first << '/' << std::setw(2)
               << middle << '/' << std::setw(mode > 2 && mode % 2 ? 4 : 2)
               << last << ' ';
      }
      output << std::setw(2) << clock.hours().count() << ':' << std::setw(2)
             << clock.minutes().count() << ':' << std::setw(2)
             << clock.seconds().count();
    }
    for (uint32_t cell = row->first; cell != kEmpty;
         cell = card.cells[cell].next) {
      const auto& value = card.cells[cell];
      output << ',' << std::setfill('0');
      if (value.type >= 7) {
        output.write(card.pages[value.page].data(), value.length);
      } else if (value.type == 1) {
        output << (value.bits != 0);
      } else if (value.type == 4 || value.type == 5) {
        output << std::hex << std::uppercase
               << std::setw(value.type == 4 ? 4 : 8) << value.bits << std::dec;
      } else if (value.type == 6) {
        output << std::scientific << std::showpos << std::uppercase
               << std::setprecision(7) << std::bit_cast<float>(value.bits)
               << std::defaultfloat << std::noshowpos;
      } else {
        const int64_t number =
            value.type == 2
                ? std::bit_cast<int16_t>(static_cast<uint16_t>(value.bits))
                : std::bit_cast<int32_t>(value.bits);
        output << (number < 0 ? '-' : '+')
               << std::setw(value.type == 2 ? 5 : 10)
               << (number < 0 ? -number : number);
      }
    }
    output << "\r\n";
  }
  return output.str();
}
}  // namespace plc
