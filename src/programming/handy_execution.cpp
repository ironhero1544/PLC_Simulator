#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <array>
#include <bit>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
constexpr std::array<uint8_t, 16> kSegments = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x27,
    0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};
}  // namespace

bool CompiledPLCExecutor::ExecuteHandyOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  auto& state = handy_states_[index];
  const bool previous = state.previous;
  state.previous = power;
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  const auto range = [](model::DeviceAddress device, uint32_t words) {
    return model::IsRegisterDevice(device) &&
           device.index + words <= model::WordDeviceLimit(device);
  };
  const auto bit_range = [](model::DeviceAddress device, uint32_t length) {
    if (length == 0)
      return true;
    device = model::OffsetBitAddress(device, length - 1);
    return model::ParseDeviceAddress(model::FormatDeviceAddress(device)) ==
           std::optional(device);
  };
  if (instruction.opcode == Opcode::kTableSort2)
    return ExecuteSortOperation(instruction, power, index);
  if (instruction.opcode == Opcode::kAnnunciatorSet) {
    const int timer = static_cast<int>(operands[0].device.index);
    const int preset = ReadValue(operands[1]);
    const auto destination = operands[2].device;
    if (timer >= 200 || preset < 1 ||
        destination.kind != model::DeviceKind::kS || destination.index < 900 ||
        destination.index > 999)
      return fail("ANS requires a 100ms timer and S900 to S999");
    ParsedInstruction numeric;
    numeric.type = ParsedInstruction::PLC_TON;
    numeric.index = timer;
    numeric.preset = preset;
    memory_.accumulator = power;
    if (!ExecuteInstruction(numeric))
      return false;
    if (timer_contacts_[timer])
      WriteBit(destination, true);
    return true;
  }
  if (instruction.opcode == Opcode::kTeachingTimer) {
    const int magnification = ReadValue(operands[1]);
    const auto destination = operands[0].device;
    if (magnification < 0 || magnification > 2 || !range(destination, 2))
      return fail("Invalid teaching timer magnification or memory range");
    if (!power) {
      state.elapsed_ms = 0;
      WordMemory(destination)[destination.index + 1] = 0;
      return true;
    }
    state.elapsed_ms += current_elapsed_ms_;
    const int64_t seconds = std::min<int64_t>(32767, state.elapsed_ms / 1000);
    const int multiplier = magnification == 0   ? 1
                           : magnification == 1 ? 10
                                                : 100;
    WordMemory(destination)[destination.index + 1] =
        static_cast<int16_t>(seconds);
    return WriteWord(destination, seconds * multiplier);
  }
  if (instruction.opcode == Opcode::kSpecialTimer) {
    const uint32_t timer = operands[0].device.index;
    const int preset = ReadValue(operands[1]);
    auto destination = operands[2].device;
    if (timer >= 200 || preset < 1 || !bit_range(destination, 4))
      return fail("STMR requires a 100ms timer and four result bits");
    const int64_t duration = static_cast<int64_t>(preset) * 100;
    if (power) {
      state.active = true;
      state.off_elapsed_ms = 0;
      state.elapsed_ms =
          previous ? std::min(duration, state.elapsed_ms + current_elapsed_ms_)
                   : 0;
      memory_.T[timer] = static_cast<int>(state.elapsed_ms / 100);
      timer_contacts_[timer] = state.elapsed_ms >= duration;
    } else {
      state.elapsed_ms = 0;
      memory_.T[timer] = 0;
      timer_contacts_[timer] = false;
      if (state.active) {
        state.off_elapsed_ms =
            previous ? 0
                     : std::min(duration,
                                state.off_elapsed_ms + current_elapsed_ms_);
        if (state.off_elapsed_ms >= duration)
          state.active = false;
      }
    }
    WriteBit(destination, state.active);
    destination = model::OffsetBitAddress(destination, 1);
    WriteBit(destination, !power && state.active);
    destination = model::OffsetBitAddress(destination, 1);
    WriteBit(destination, power);
    destination = model::OffsetBitAddress(destination, 1);
    WriteBit(destination, power ? timer_contacts_[timer] : state.active);
    timer_enabled_[timer] = power;
    timer_presets_[timer] = preset;
    return true;
  }
  if (instruction.opcode == Opcode::kRamp) {
    const int scans = ReadValue(operands[3]);
    const auto destination = operands[2].device;
    if (scans < 1 || !range(destination, 2))
      return fail("Invalid ramp scan count or destination");
    if (!power) {
      state.elapsed_ms = 0;
      WordMemory(destination)[destination.index + 1] = 0;
      memory_.special_m[29] = false;
      return true;
    }
    const int64_t first = ReadValue(operands[0]);
    const int64_t last = ReadValue(operands[1]);
    int64_t step = state.elapsed_ms;
    if (step > scans) {
      if (memory_.special_m[26])
        step = scans;
      else
        step = 0;
    }
    const int64_t value = first + (last - first) * step / scans;
    WordMemory(destination)[destination.index + 1] = static_cast<int16_t>(step);
    memory_.special_m[29] = step == scans;
    state.elapsed_ms = step + 1;
    return WriteWord(destination, value);
  }
  if (!power)
    return true;
  if (instruction.opcode == Opcode::kAnnunciatorReset) {
    for (int relay = 900; relay <= 999; ++relay) {
      if (memory_.S[relay]) {
        memory_.S[relay] = false;
        break;
      }
    }
    return true;
  }
  if (instruction.opcode == Opcode::kSevenSegment) {
    const uint16_t previous_value =
        static_cast<uint16_t>(ReadValue(operands[1]));
    return WriteValue(
        operands[1],
        (previous_value & 0xff00) |
            kSegments[static_cast<uint16_t>(ReadValue(operands[0])) & 0xf]);
  }
  if (instruction.opcode == Opcode::kAsciiConstant) {
    const auto& source = operands[0];
    const bool bytes = memory_.special_m[161];
    const auto destination = operands[1].device;
    if (source.kind != model::OperandKind::kImmediateString ||
        source.text.size() > 8 || !range(destination, bytes ? 8 : 4) ||
        std::any_of(source.text.begin(), source.text.end(),
                    [](unsigned char c) { return c < 0x20 || c > 0x7e; }))
      return fail("ASC requires up to eight printable ASCII characters");
    auto output = WordMemory(destination);
    for (int character = 0; character < 8; ++character) {
      const uint8_t value = character < static_cast<int>(source.text.size())
                                ? static_cast<uint8_t>(source.text[character])
                                : ' ';
      const uint32_t word =
          destination.index + (bytes ? character : character / 2);
      if (bytes || character % 2 == 0)
        output[word] = value;
      else
        output[word] =
            static_cast<int16_t>(static_cast<uint16_t>(output[word]) |
                                 (static_cast<uint16_t>(value) << 8));
    }
    return true;
  }
  if (instruction.opcode == Opcode::kHexToAscii ||
      instruction.opcode == Opcode::kAsciiToHex) {
    const bool encode = instruction.opcode == Opcode::kHexToAscii;
    const bool bytes = memory_.special_m[161];
    const int count = ReadValue(operands[2]);
    const uint32_t source = operands[0].device.index;
    const uint32_t destination = operands[1].device.index;
    const int characters = bytes ? count : (count + 1) / 2;
    const int words = (count + 3) / 4;
    if (count < 1 || count > 256 ||
        !range(operands[0].device, encode ? words : characters) ||
        !range(operands[1].device, encode ? characters : words))
      return fail("ASCII hexadecimal conversion exceeds count or memory range");
    const auto input = WordMemory(operands[0].device);
    auto output = WordMemory(operands[1].device);
    std::array<uint8_t, 256> digits{};
    for (int character = 0; character < count; ++character) {
      const int position = count - character - 1;
      if (encode) {
        const uint16_t value =
            static_cast<uint16_t>(input[source + position / 4]);
        digits[character] = static_cast<uint8_t>(
            "0123456789ABCDEF"[(value >> ((position % 4) * 4)) & 0xf]);
      } else {
        const uint16_t value = static_cast<uint16_t>(
            input[source + (bytes ? character : character / 2)]);
        const uint8_t ascii =
            static_cast<uint8_t>(value >> (bytes ? 0 : (character % 2) * 8));
        if (ascii >= '0' && ascii <= '9')
          digits[character] = ascii - '0';
        else if (ascii >= 'A' && ascii <= 'F')
          digits[character] = ascii - 'A' + 10;
        else
          return fail("HEX requires ASCII 0 to 9 or A to F");
      }
    }
    if (!encode)
      std::fill_n(output.data() + destination, words, 0);
    for (int character = 0; character < count; ++character) {
      const int position = count - character - 1;
      const uint32_t word =
          destination +
          (encode ? bytes ? character : character / 2 : position / 4);
      if (encode && bytes)
        output[word] = digits[character];
      else {
        const int shift = encode ? (character % 2) * 8 : (position % 4) * 4;
        const uint16_t mask =
            static_cast<uint16_t>((encode ? 0xffu : 0xfu) << shift);
        output[word] = std::bit_cast<int16_t>(static_cast<uint16_t>(
            (static_cast<uint16_t>(output[word]) & ~mask) |
            (static_cast<uint16_t>(digits[character]) << shift)));
      }
    }
    return true;
  }
  if (instruction.opcode == Opcode::kParallelRun) {
    const auto& source = operands[0];
    const auto& destination = operands[1];
    const bool to_decimal = source.device.kind == model::DeviceKind::kX &&
                            destination.device.kind == model::DeviceKind::kM;
    const bool to_octal = source.device.kind == model::DeviceKind::kM &&
                          destination.device.kind == model::DeviceKind::kY;
    if (source.kind != model::OperandKind::kPackedBit ||
        destination.kind != model::OperandKind::kPackedBit ||
        source.immediate != destination.immediate ||
        (!to_decimal && !to_octal) ||
        (source.device.kind == model::DeviceKind::kM
             ? source.device.index % 10
             : source.device.index % 8) ||
        (destination.device.kind == model::DeviceKind::kM
             ? destination.device.index % 10
             : destination.device.index % 8))
      return fail("Invalid PRUN digit specification or octal device grouping");
    const int bits = source.immediate * 4;
    const uint32_t length = (bits - 1) / 8 * 10 + (bits - 1) % 8 + 1;
    if (!bit_range(to_decimal ? destination.device : source.device, length))
      return fail("PRUN exceeds decimal relay range");
    std::array<bool, 32> values{};
    for (int bit = 0; bit < bits; ++bit) {
      auto device = source.device;
      device.index += to_octal ? bit / 8 * 10 + bit % 8 : bit;
      values[bit] = ReadBit(device);
    }
    for (int bit = 0; bit < bits; ++bit) {
      auto device = destination.device;
      device.index += to_decimal ? bit / 8 * 10 + bit % 8 : bit;
      WriteBit(device, values[bit]);
    }
    return true;
  }
  if (instruction.opcode == Opcode::kHighSpeedCounterMove) {
    const auto source = operands[0].device;
    if (!((source.kind == model::DeviceKind::kC && source.index >= 235 &&
           source.index <= 255) ||
          (source.kind == model::DeviceKind::kD &&
           (source.index == 8099 || source.index == 8398))))
      return fail("HCMOV requires a high-speed or ring counter");
    return WriteValue(operands[1], ReadValue(operands[0], true), true);
  }
  return fail("Unsupported handy instruction");
}
}  // namespace plc
