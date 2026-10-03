#include "plc_emulator/programming/compiled_plc_executor.h"

#include <array>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::ExecuteDrumOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const auto opcode = instruction.opcode;
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  const auto bit_range = [](model::DeviceAddress head, int count) {
    const auto last = model::OffsetBitAddress(head, count - 1);
    return count > 0 &&
           model::ParseDeviceAddress(model::FormatDeviceAddress(last)) ==
               std::optional(last);
  };
  if (opcode == Opcode::kRotaryTable) {
    const auto source = operands[0].device;
    const auto destination = operands[3].device;
    const int divisions = ReadValue(operands[1]);
    const int low_speed = ReadValue(operands[2]);
    if (divisions < 1 || low_speed < 0 || low_speed > divisions ||
        source.index + 3 > WordMemory(source).size() ||
        !bit_range(destination, 8))
      return fail("ROTC exceeds divisions, low-speed sections or device range");
    auto& state = handy_states_[index];
    const bool phase = ReadBit(destination);
    const bool previous = state.previous;
    state.previous = phase;
    std::array<bool, 5> outputs{};
    if (power) {
      auto storage = WordMemory(source);
      int current = storage[source.index];
      const int port = storage[source.index + 1];
      const int product = storage[source.index + 2];
      if (port < 0 || port >= divisions || product < 0 ||
          product >= divisions || current < 0 || current >= divisions)
        return fail("ROTC port, product or count exceeds divisions");
      if (ReadBit(model::OffsetBitAddress(destination, 2)))
        current = 0;
      else if (phase && !previous) {
        const bool reverse = ReadBit(model::OffsetBitAddress(destination, 1));
        current = (current + (reverse ? divisions - 1 : 1)) % divisions;
      }
      storage[source.index] = static_cast<int16_t>(current);
      const int target = (product - port + divisions) % divisions;
      const int forward = (target - current + divisions) % divisions;
      const int backward = (current - target + divisions) % divisions;
      if (forward == 0)
        outputs[2] = true;
      else if (forward <= backward)
        outputs[forward <= low_speed ? 1 : 0] = true;
      else
        outputs[backward <= low_speed ? 3 : 4] = true;
    }
    for (int bit = 0; bit < 5; ++bit)
      WriteBit(model::OffsetBitAddress(destination, bit + 3), outputs[bit]);
    return true;
  }
  const auto counter = operands[1].device;
  const auto destination = operands[2].device;
  const int count = ReadValue(operands[3]);
  const int width = instruction.wide ? 2 : 1;
  const int words = count * (opcode == Opcode::kAbsoluteDrum ? width * 2 : 1);
  const auto source = operands[0];
  const bool packed = source.kind == model::OperandKind::kPackedBit;
  if (count < 1 || count > 64 || !bit_range(destination, count) ||
      counter.kind != model::DeviceKind::kC ||
      (opcode == Opcode::kIncrementalDrum ? counter.index >= 199
                                          : counter.index >= 256) ||
      (!instruction.wide && counter.index >= 200))
    return fail("Drum sequencer exceeds counter or output range");
  if (packed) {
    const uint32_t limit = source.device.kind == model::DeviceKind::kX ||
                                   source.device.kind == model::DeviceKind::kY
                               ? 256
                           : source.device.kind == model::DeviceKind::kM ? 7680
                                                                         : 4096;
    if (source.immediate != width * 4 || source.device.index % 16 ||
        source.device.index + words * 16 > limit)
      return fail("Drum table requires aligned K4/K8 bit digits");
  } else if (source.kind != model::OperandKind::kWordDevice ||
             source.device.index + words > WordMemory(source.device).size()) {
    return fail("Drum comparison table exceeds word memory");
  }
  const auto read = [&](int word) {
    auto operand = source;
    operand.device.index += packed ? word * 16 : word;
    return ReadValue(operand, instruction.wide);
  };
  if (!power)
    return true;
  if (opcode == Opcode::kAbsoluteDrum) {
    const int32_t current = ReadValue(operands[1], instruction.wide);
    std::array<bool, 64> results{};
    for (int bit = 0; bit < count; ++bit) {
      const int32_t start = read(bit * width * 2);
      const int32_t stop = read(bit * width * 2 + width);
      results[bit] = start <= stop ? current >= start && current < stop
                                   : current >= start || current < stop;
    }
    for (int bit = 0; bit < count; ++bit)
      WriteBit(model::OffsetBitAddress(destination, bit), results[bit]);
    return true;
  }
  int position = memory_.C[counter.index + 1];
  if (position < 0 || position >= count)
    return fail("INCD process counter exceeds drum steps");
  memory_.special_m[29] = false;
  if (memory_.C[counter.index] == read(position)) {
    memory_.C[counter.index] = 0;
    counter_contacts_[counter.index] = false;
    if (++position == count) {
      position = 0;
      memory_.special_m[29] = true;
    }
    memory_.C[counter.index + 1] = position;
  }
  for (int bit = 0; bit < count; ++bit)
    WriteBit(model::OffsetBitAddress(destination, bit), bit == position);
  return true;
}
}  // namespace plc
