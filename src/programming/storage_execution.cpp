#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <array>
#include <bit>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::SetDeviceComment(const std::string& address,
                                           std::string_view comment) {
  const auto device = model::ParseDeviceAddress(address);
  if (!device || device->index_kind || device->bit >= 0 ||
      comment.size() > 16 || comment.find('\0') != std::string_view::npos)
    return false;
  auto found = std::find_if(
      device_comments_.begin(), device_comments_.end(),
      [&](const DeviceComment& item) { return item.device == *device; });
  if (found == device_comments_.end()) {
    device_comments_.push_back({*device, {}});
    found = device_comments_.end() - 1;
  }
  found->text.fill(' ');
  std::copy(comment.begin(), comment.end(), found->text.begin());
  return true;
}

bool CompiledPLCExecutor::ExecuteStorageOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const auto opcode = instruction.opcode;
  auto& state = handy_states_[index];
  const auto fail = [this](const char* message) {
    memory_.special_m[67] = true;
    memory_.D[8067] = 6706;
    SetError(message);
    return false;
  };
  if (!power) {
    if (opcode == Opcode::kSaveExtension) {
      state.active = false;
      state.elapsed_ms = 0;
      memory_.special_m[29] = false;
    }
    return true;
  }
  if (opcode == Opcode::kReadComment) {
    const auto destination = operands[1].device;
    auto storage = WordMemory(destination);
    if (destination.index + (memory_.special_m[91] ? 8 : 9) > storage.size())
      return fail("COMRD destination exceeds register range");
    const auto found =
        std::find_if(device_comments_.begin(), device_comments_.end(),
                     [&](const DeviceComment& item) {
                       return item.device == operands[0].device;
                     });
    for (int word = 0; word < 8; ++word) {
      const uint8_t low =
          found == device_comments_.end() ? ' ' : found->text[word * 2];
      const uint8_t high =
          found == device_comments_.end() ? ' ' : found->text[word * 2 + 1];
      storage[destination.index + word] = std::bit_cast<int16_t>(
          static_cast<uint16_t>(low | (uint16_t{high} << 8)));
    }
    if (!memory_.special_m[91])
      storage[destination.index + 8] = 0;
    return found != device_comments_.end() ||
           fail("Device has no registered comment");
  }
  if (opcode == Opcode::kIndexPush || opcode == Opcode::kIndexPop) {
    const auto device = operands[0].device;
    if (device.kind != model::DeviceKind::kD || device.index >= 8000)
      return fail("ZPUSH/ZPOP require normal D registers");
    const int depth = memory_.D[device.index];
    const bool push = opcode == Opcode::kIndexPush;
    if (depth < (push ? 0 : 1) ||
        device.index + 1 + static_cast<int64_t>(depth + (push ? 1 : 0)) * 16 >
            8000)
      return fail("Index register stack underflow or memory overflow");
    const int head = device.index + 1 + (push ? depth : depth - 1) * 16;
    for (int number = 0; number < 8; ++number) {
      if (push) {
        memory_.D[head + 2 * number] = memory_.Z[number];
        memory_.D[head + 2 * number + 1] = memory_.V[number];
      } else {
        memory_.Z[number] = memory_.D[head + 2 * number];
        memory_.V[number] = memory_.D[head + 2 * number + 1];
      }
    }
    memory_.D[device.index] += push ? 1 : -1;
    return true;
  }
  const auto destination =
      operands[opcode == Opcode::kLogExtension ? 2 : 0].device;
  if (destination.kind != model::DeviceKind::kR)
    return fail("Extension file operation requires R registers");
  const int head = destination.index;
  const int count =
      ReadValue(operands[opcode == Opcode::kLogExtension ? 3 : 1]);
  if (opcode == Opcode::kLoadExtension || opcode == Opcode::kRewriteExtension) {
    const int length = count == 0 ? 32768 : count;
    if (length < 0 || head + length > 32768)
      return fail("Extension register transfer exceeds memory");
    if (opcode == Opcode::kLoadExtension)
      std::copy_n(extension_file_.begin() + head, length, memory_.R + head);
    else
      std::copy_n(memory_.R + head, length, extension_file_.begin() + head);
    return true;
  }
  if (head % 2048)
    return fail("Extension register operation requires sector head");
  if (opcode == Opcode::kInitializeExtension ||
      opcode == Opcode::kInitializeExtensionFile) {
    if (count < 1 || count > 16 || head + count * 2048 > 32768)
      return fail("Extension register initialization exceeds sectors");
    std::fill_n(extension_file_.begin() + head, count * 2048, -1);
    if (opcode == Opcode::kInitializeExtension)
      std::fill_n(memory_.R + head, count * 2048, -1);
    return true;
  }
  if (opcode == Opcode::kSaveExtension) {
    if (count < 0 || count > 2048)
      return fail("SAVER chunk must be 0 to 2048");
    if (!state.active) {
      state.active = true;
      state.elapsed_ms = 0;
      state.off_elapsed_ms = head;
    } else if (state.off_elapsed_ms != head) {
      return fail("SAVER sector changed while active");
    }
    const int begin = static_cast<int>(state.elapsed_ms);
    const int end = std::min(2048, begin + (count ? count : 2048));
    for (int word = begin; word < end; ++word)
      extension_file_[head + word] &= memory_.R[head + word];
    state.elapsed_ms = end;
    memory_.special_m[29] = end == 2048;
    return WriteWord(operands[2].device, end);
  }
  if (opcode == Opcode::kLogExtension) {
    const int length = ReadValue(operands[1]);
    const auto source = operands[0].device;
    const auto storage = WordMemory(source);
    if (count < 1 || count > 16 || head + count * 2048 > 32768 || length < 1 ||
        length > 8000 || source.index + length > storage.size())
      return fail("LOGR exceeds source or sector range");
    const int capacity = 1926 * count;
    int progress = 0;
    while (progress < capacity &&
           !(static_cast<uint16_t>(memory_.R[head + capacity + progress / 16]) &
             (uint16_t{1} << (progress % 16))))
      ++progress;
    const int amount = std::min(length, capacity - progress);
    std::array<int16_t, 8000> snapshot{};
    std::copy_n(storage.begin() + source.index, amount, snapshot.begin());
    for (int word = 0; word < amount; ++word) {
      const int position = progress + word;
      memory_.R[head + position] = snapshot[word];
      extension_file_[head + position] &= snapshot[word];
      const int control = head + capacity + position / 16;
      const uint16_t mask =
          static_cast<uint16_t>(~(uint16_t{1} << (position % 16)));
      memory_.R[control] = std::bit_cast<int16_t>(static_cast<uint16_t>(
          static_cast<uint16_t>(memory_.R[control]) & mask));
      extension_file_[control] &= memory_.R[control];
    }
    return WriteWord(operands[4].device, progress + amount);
  }
  return fail("Unsupported extension register instruction");
}
}  // namespace plc
