#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

bool CfWord(model::DeviceAddress device) {
  return (device.kind == model::DeviceKind::kD && device.index < 8000) ||
         device.kind == model::DeviceKind::kR;
}

bool CfBit(model::DeviceAddress device) {
  return device.kind == model::DeviceKind::kX ||
         device.kind == model::DeviceKind::kY ||
         device.kind == model::DeviceKind::kM ||
         device.kind == model::DeviceKind::kS;
}

bool CfAddress(model::DeviceAddress device) {
  switch (device.kind) {
    case model::DeviceKind::kX:
    case model::DeviceKind::kY:
      return device.index < 256;
    case model::DeviceKind::kM:
      return device.index < 7680 ||
             (device.index >= 8000 && device.index < 8512);
    case model::DeviceKind::kS:
      return device.index < 4096;
    case model::DeviceKind::kD:
      return device.index < 8000;
    case model::DeviceKind::kR:
      return device.index < 32768;
    default:
      return false;
  }
}
}  // namespace

bool CompiledPLCExecutor::ReadCfValues(model::DeviceAddress source, int type,
                                       std::span<CfDatum> output) {
  const bool word = CfWord(source);
  if ((!word && !CfBit(source)) || source.bit >= 0 ||
      (!word && type != 1 && source.index % 16))
    return false;
  const auto read = [&](int offset, bool wide) -> std::optional<uint32_t> {
    auto device = source;
    device.index += word ? offset : offset * 16;
    if (word) {
      const uint32_t limit =
          device.kind == model::DeviceKind::kD ? 8000 : 32768;
      if (device.index + (wide ? 2 : 1) > limit)
        return std::nullopt;
      return static_cast<uint32_t>(GetWordValue(device, wide));
    }
    const auto end = model::OffsetBitAddress(device, wide ? 31 : 15);
    if (!CfAddress(end))
      return std::nullopt;
    return static_cast<uint32_t>(ReadValue(
        {model::OperandKind::kPackedBit, device, wide ? 8 : 4}, wide));
  };
  int offset = 0;
  for (auto& datum : output) {
    datum.type = static_cast<uint8_t>(type);
    datum.length = 0;
    if (type == 1 && !word) {
      auto device = source;
      device.index += offset++;
      if (!CfAddress(device))
        return false;
      datum.bits = ReadBit(device);
    } else if (type == 7 || type == 8) {
      const int maximum = type == 8 ? 64 : 1024;
      bool terminated = false;
      for (int byte = 0; byte <= maximum; ++byte) {
        const auto value = read(offset + byte / 2, false);
        if (!value)
          return false;
        const char character =
            static_cast<char>((*value >> ((byte % 2) * 8)) & 255);
        datum.text[byte] = character;
        if (!character) {
          datum.length = static_cast<uint16_t>(byte);
          offset += (byte + 2) / 2;
          terminated = true;
          break;
        }
        if (character == ',' || character == '\r' || character == '\n')
          return false;
      }
      if (!terminated)
        return false;
    } else {
      const bool wide = type == 3 || type == 5 || type == 6;
      const auto value = read(offset, wide);
      if (!value)
        return false;
      datum.bits = type == 1 ? *value & 1 : wide ? *value : *value & 65535;
      offset += wide ? 2 : 1;
    }
  }
  return true;
}

bool CompiledPLCExecutor::WriteCfValues(model::DeviceAddress destination,
                                        std::span<const CfDatum> data) {
  const bool word = CfWord(destination);
  if ((!word &&
       (!CfBit(destination) || destination.kind == model::DeviceKind::kX)) ||
      destination.bit >= 0)
    return false;
  const auto write = [&](int offset, uint32_t value, bool wide) {
    auto device = destination;
    device.index += word ? offset : offset * 16;
    if (word) {
      const uint32_t limit =
          device.kind == model::DeviceKind::kD ? 8000 : 32768;
      return device.index + (wide ? 2 : 1) <= limit &&
             WriteWord(device, value, wide);
    }
    if (destination.index % 16)
      return false;
    const auto end = model::OffsetBitAddress(device, wide ? 31 : 15);
    return CfAddress(end) &&
           WriteValue({model::OperandKind::kPackedBit, device, wide ? 8 : 4},
                      value, wide);
  };
  // Check the complete destination footprint before writing any values.
  int offset = 0;
  bool all_bits = true;
  for (const auto& value : data) {
    all_bits = all_bits && value.type == 1;
    offset += value.type >= 7 ? (value.length + 2) / 2
              : value.type == 3 || value.type == 5 || value.type == 6 ? 2
                                                                      : 1;
  }
  auto end = destination;
  end.index += word ? offset - 1 : all_bits ? data.size() - 1 : offset * 16 - 1;
  if (data.empty() || !CfAddress(end) ||
      (word && destination.kind == model::DeviceKind::kD && end.index >= 8000))
    return false;
  offset = 0;
  for (const auto& value : data) {
    if (value.type == 1 && !word) {
      auto device = destination;
      device.index += offset++;
      WriteBit(device, value.bits != 0);
    } else if (value.type >= 7) {
      for (int byte = 0; byte <= value.length; byte += 2) {
        const uint16_t low = static_cast<uint8_t>(value.text[byte]);
        const uint16_t high = byte < value.length
                                  ? static_cast<uint8_t>(value.text[byte + 1])
                                  : 0;
        if (!write(offset++, low | (high << 8), false))
          return false;
      }
    } else {
      const bool wide = value.type == 3 || value.type == 5 || value.type == 6;
      uint32_t bits = value.bits;
      if (value.type == 1) {
        auto device = destination;
        device.index += offset;
        bits |= static_cast<uint16_t>(GetWordValue(device)) & 65534;
      }
      if (!write(offset, bits, wide))
        return false;
      offset += wide ? 2 : 1;
    }
  }
  return true;
}

int CompiledPLCExecutor::TransferCfData(
    const model::OpenPLCInstruction& instruction, CfCard* card, bool reading) {
  const auto& operands = instruction.operands;
  const int id = ReadValue(operands[0]);
  if (id < 0 || id > 63)
    return 802;
  const int file_slot = card->ids[id];
  if (file_slot < 0)
    return 801;
  auto& file = card->files[file_slot];
  const auto parameter = operands[reading ? 1 : 2].device;
  const auto parameters = WordMemory(parameter);
  if (!CfWord(parameter) ||
      parameter.index + (reading ? 4 : 5) >
          (parameter.kind == model::DeviceKind::kD ? 8000u : parameters.size()))
    return 611;
  const int type = parameters[parameter.index];
  const int requested_line = parameters[parameter.index + 1];
  const int requested_column = parameters[parameter.index + 2];
  const int points = parameters[parameter.index + 3];
  const bool buffered = !reading && parameters[parameter.index + 4] == 1;
  if (type < 0 || type > (reading ? 7 : 8))
    return 613;
  if (points < 1 || points > 254)
    return 611;
  if (!reading && parameters[parameter.index + 4] != 0 && !buffered)
    return 612;
  const int line =
      !reading && requested_line == -1 ? file.next_line : requested_line;
  const int column = !reading && requested_column == -1 ? 1 : requested_column;
  if (line < 1 || line > file.maximum_lines)
    return requested_line == -1 ? 818 : 805;
  if (column < 1 || column > 254 || column + points - 1 > 254)
    return 815;
  const auto result = operands[3].device;
  if (!CfWord(result) ||
      result.index + (reading ? 1 : 2) > (result.kind == model::DeviceKind::kD
                                              ? 8000u
                                              : model::WordDeviceLimit(result)))
    return 611;
  auto& mixed = card->mixed;
  const bool continuing = mixed.active;
  if (continuing &&
      (mixed.reading != reading || mixed.file != file_slot ||
       mixed.result != result || (requested_line != -1 && mixed.line != line)))
    return 701;
  if (!continuing) {
    mixed.reading = reading;
    mixed.file = file_slot;
    mixed.line = line;
    mixed.column = column;
    mixed.total = points;
    mixed.progress = mixed.count = 0;
    mixed.original_count = 0;
    mixed.buffered = buffered;
    mixed.result = result;
    auto* existing = FindCfRow(card, file_slot, type == 8 ? 0 : line, false);
    if (reading && (!existing || existing->buffered))
      return 806;
    if (existing) {
      mixed.count = existing->count;
      mixed.original_count = existing->count;
      if (!LoadCfRow(*card, *existing, mixed.data))
        return 901;
    } else if (column != 1 || (!reading && requested_line != -1 &&
                               line > file.last_line + 1)) {
      return 805;
    }
  }
  if (type == 0) {
    if (continuing || file.type != 0)
      return 817;
    mixed.active = true;
    return 0;
  }
  if (file.type != 0 && type != 8 && type != file.type)
    return 817;
  const int first = mixed.column - 1 + mixed.progress;
  if (continuing && points > mixed.total - mixed.progress)
    return 615;
  const int count = reading ? std::min(points, mixed.count - first) : points;
  if (first < 0 || count < 1 || first + count > 254)
    return reading ? 806 : 815;
  if (reading) {
    for (int index = first; index < first + count; ++index)
      if (mixed.data[index].type != type)
        return 901;
    if (!WriteCfValues(operands[2].device,
                       {mixed.data.data() + first, static_cast<size_t>(count)}))
      return 611;
    WriteWord(result, mixed.count);
  } else {
    if (type == 8 && (file.last_line > 0 || mixed.count > 0))
      return 702;
    if (mixed.original_count > 0 && first + count > mixed.original_count)
      return 815;
    std::array<uint8_t, 254> previous_types{};
    std::array<uint16_t, 254> previous_lengths{};
    for (int index = first; index < std::min(first + count, mixed.count);
         ++index) {
      previous_types[index] = mixed.data[index].type;
      previous_lengths[index] = mixed.data[index].length;
    }
    if (!ReadCfValues(operands[1].device, type,
                      {mixed.data.data() + first, static_cast<size_t>(count)}))
      return 612;
    for (int index = first; index < std::min(first + count, mixed.count);
         ++index)
      if (previous_types[index] != mixed.data[index].type ||
          (type >= 7 && previous_lengths[index] < mixed.data[index].length))
        return 901;
    for (int index = first; index < std::min(first + count, mixed.count);
         ++index)
      if (type >= 7) {
        auto& value = mixed.data[index];
        std::fill(value.text.begin() + value.length,
                  value.text.begin() + previous_lengths[index], ' ');
        value.length = previous_lengths[index];
        value.text[value.length] = '\0';
      }
    mixed.count = std::max(mixed.count, first + count);
  }
  mixed.progress += count;
  if (continuing && mixed.progress < mixed.total)
    return 0;
  if (!reading) {
    const bool overwriting =
        FindCfRow(card, file_slot, type == 8 ? 0 : mixed.line, false) !=
        nullptr;
    const int error =
        StoreCfRow(card, file_slot, type == 8 ? 0 : mixed.line,
                   {mixed.data.data(), static_cast<size_t>(mixed.count)},
                   buffered && !overwriting);
    if (error)
      return error;
    if (!buffered || overwriting || mixed.line == file.maximum_lines)
      FlushCfBuffer(card, file_slot);
    int next_line = mixed.line;
    int next_column = first + count + 1;
    if (type == 8) {
      next_line = next_column = 1;
    } else if (first + count == mixed.count) {
      next_column = 1;
      next_line = mixed.line + 1;
      file.last_line = std::max(file.last_line, mixed.line);
      file.next_line = next_line;
      if (next_line > file.maximum_lines && file.policy == 1)
        file.next_line = next_line = 1;
      if (id == 0 && next_line > file.maximum_lines) {
        if (!RotateCfFifo(card))
          return 902;
        next_line = 1;
      }
    }
    WriteWord(result, next_line);
    auto next = result;
    ++next.index;
    WriteWord(next, next_column);
  }
  mixed.active = false;
  return 0;
}
}  // namespace plc
