#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

uint32_t SwapBytes(uint32_t value) {
  return ((value & 0x00ff00ffu) << 8) | ((value & 0xff00ff00u) >> 8);
}

int32_t PowerOfTen(int digits) {
  int32_t value = 1;
  for (int digit = 0; digit < digits; ++digit)
    value *= 10;
  return value;
}
}  // namespace

bool CompiledPLCExecutor::ExecuteCpuOperation(
    const model::OpenPLCInstruction& instruction) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  if (opcode == Opcode::kReadClock || opcode == Opcode::kWriteClock)
    return ExecuteClockOperation(instruction);
  if (opcode == Opcode::kMean || opcode == Opcode::kWordToBytes ||
      opcode == Opcode::kBytesToWord || opcode == Opcode::kShiftWrite ||
      opcode == Opcode::kShiftRead)
    return ExecuteCpuBlockOperation(instruction);

  const int32_t first = ReadValue(operands[0], instruction.wide);
  const uint32_t bits = static_cast<uint32_t>(first);
  const auto& destination = operands[1];
  switch (opcode) {
    case Opcode::kComplement:
      return WriteValue(destination, bits ^ UINT32_MAX, instruction.wide);
    case Opcode::kNegate:
      return WriteValue(operands[0], -static_cast<int64_t>(first),
                        instruction.wide);
    case Opcode::kByteSwap:
      return WriteValue(operands[0], SwapBytes(bits), instruction.wide);
    case Opcode::kExchange: {
      const int32_t second = ReadValue(operands[1], instruction.wide);
      if (memory_.special_m[160]) {
        return WriteValue(operands[0], SwapBytes(bits), instruction.wide) &&
               (operands[0] == destination ||
                WriteValue(destination,
                           SwapBytes(static_cast<uint32_t>(second)),
                           instruction.wide));
      }
      return WriteValue(operands[0], second, instruction.wide) &&
             WriteValue(destination, first, instruction.wide);
    }
    case Opcode::kToBcd: {
      if (first < 0 || first > (instruction.wide ? 99999999 : 9999)) {
        SetError("BCD source exceeds decimal digit range");
        return false;
      }
      uint32_t decimal = static_cast<uint32_t>(first);
      uint32_t bcd = 0;
      for (unsigned shift = 0; decimal > 0; shift += 4, decimal /= 10)
        bcd |= (decimal % 10) << shift;
      return WriteValue(destination, bcd, instruction.wide);
    }
    case Opcode::kFromBcd: {
      uint32_t decimal = 0;
      uint32_t multiplier = 1;
      for (int digit = 0; digit < (instruction.wide ? 8 : 4); ++digit) {
        const uint32_t nibble = (bits >> (digit * 4)) & 0xf;
        if (nibble > 9) {
          SetError("BIN source contains a non-BCD digit");
          return false;
        }
        decimal += nibble * multiplier;
        multiplier *= 10;
      }
      return WriteValue(destination, decimal, instruction.wide);
    }
    case Opcode::kSquareRoot: {
      if (first < 0) {
        SetError("SQR source must be nonnegative");
        return false;
      }
      const int64_t root = static_cast<int64_t>(std::sqrt(first));
      memory_.special_m[20] = first == 0;
      memory_.special_m[21] = root * root != first;
      return WriteValue(destination, root, instruction.wide);
    }
    case Opcode::kToGray:
      if (first < 0) {
        SetError("GRY source must be nonnegative");
        return false;
      }
      return WriteValue(destination, bits ^ (bits >> 1), instruction.wide);
    case Opcode::kFromGray: {
      if (first < 0) {
        SetError("GBIN source must be nonnegative");
        return false;
      }
      uint32_t binary = bits & (instruction.wide ? UINT32_MAX : 0xffffu);
      for (unsigned shift = 1; shift < 32; shift *= 2)
        binary ^= binary >> shift;
      return WriteValue(destination, binary, instruction.wide);
    }
    case Opcode::kLimit:
    case Opcode::kDeadBand: {
      const int32_t upper = ReadValue(operands[1], instruction.wide);
      const int32_t input = ReadValue(operands[2], instruction.wide);
      if (first > upper) {
        SetError("LIMIT/BAND lower bound exceeds upper bound");
        return false;
      }
      const int32_t limited = std::clamp(input, first, upper);
      const int64_t value = opcode == Opcode::kLimit
                                ? limited
                                : static_cast<int64_t>(input) - limited;
      return WriteValue(operands[3], value, instruction.wide);
    }
    case Opcode::kDigitMove: {
      const int32_t previous = ReadValue(operands[3]);
      if (first < 0 || first > 9999 || previous < 0 || previous > 9999) {
        SetError("SMOV source/destination must be in 0..9999");
        return false;
      }
      const int digits = operands[2].immediate;
      const int source_scale = PowerOfTen(operands[1].immediate - digits);
      const int destination_scale = PowerOfTen(operands[4].immediate - digits);
      const int digit_mask = PowerOfTen(digits);
      const int moved = (first / source_scale) % digit_mask;
      const int replaced = (previous / destination_scale) % digit_mask;
      return WriteValue(operands[3],
                        previous + (moved - replaced) * destination_scale);
    }
    default:
      return false;
  }
}

bool CompiledPLCExecutor::ExecuteCpuBlockOperation(
    const model::OpenPLCInstruction& instruction) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  const int32_t count = ReadValue(operands[2], instruction.wide);
  std::string error;
  if (!model::ValidateCpuOperation(instruction, count, &error)) {
    SetError(error);
    return false;
  }
  if (opcode == Opcode::kMean) {
    int64_t sum = 0;
    auto source = operands[0];
    for (int index = 0; index < count; ++index) {
      sum += ReadValue(source, instruction.wide);
      source.device.index += instruction.wide ? 2 : 1;
    }
    return WriteValue(operands[1], sum / count, instruction.wide);
  }
  if (opcode == Opcode::kShiftWrite || opcode == Opcode::kShiftRead) {
    const auto head = operands[opcode == Opcode::kShiftWrite ? 1 : 0].device;
    auto storage = WordMemory(head);
    const int pointer = storage[head.index];
    if (pointer < 0 || (opcode == Opcode::kShiftRead && pointer >= count)) {
      SetError("Invalid FIFO pointer");
      return false;
    }
    if (opcode == Opcode::kShiftWrite) {
      memory_.special_m[22] = pointer >= count - 1;
      if (!memory_.special_m[22]) {
        const int32_t value = ReadValue(operands[0]);
        storage[head.index + pointer + 1] = static_cast<int16_t>(value);
        ++storage[head.index];
      }
      return true;
    }
    memory_.special_m[20] = pointer == 0;
    if (pointer == 0)
      return true;
    const int16_t value = storage[head.index + 1];
    std::move(storage.data() + head.index + 2,
              storage.data() + head.index + count,
              storage.data() + head.index + 1);
    --storage[head.index];
    memory_.special_m[20] = storage[head.index] == 0;
    return WriteValue(operands[1], value);
  }
  // Capturing the complete source also supports in-place/overlapping byte data.
  std::array<uint16_t, 32768> source{};
  const bool separate = opcode == Opcode::kWordToBytes;
  const int source_count = separate ? (count + 1) / 2 : count;
  const int destination_count = separate ? count : (count + 1) / 2;
  for (int index = 0; index < source_count; ++index)
    source[index] = static_cast<uint16_t>(
        WordMemory(operands[0].device)[operands[0].device.index + index]);
  auto destination = operands[1].device;
  for (int index = 0; index < destination_count; ++index, ++destination.index) {
    const uint32_t value =
        separate
            ? (source[index / 2] >> ((index % 2) * 8)) & 0xffu
            : (source[index * 2] & 0xffu) |
                  (index * 2 + 1 < count ? (source[index * 2 + 1] & 0xffu) << 8
                                         : 0);
    WriteWord(destination, value);
  }
  return true;
}

bool CompiledPLCExecutor::ExecuteClockOperation(
    const model::OpenPLCInstruction& instruction) {
  using namespace std::chrono;
  const uint32_t head = instruction.operands[0].device.index;
  if (head + 7 > model::WordDeviceLimit(instruction.operands[0].device)) {
    SetError("Clock operation exceeds register memory");
    return false;
  }
  auto storage = WordMemory(instruction.operands[0].device);
  if (instruction.opcode == model::Opcode::kWriteClock) {
    const int input_year = storage[head];
    const int month_value = storage[head + 1];
    const int day_value = storage[head + 2];
    const int hour_value = storage[head + 3];
    const int minute_value = storage[head + 4];
    const int second_value = storage[head + 5];
    if (input_year < 0 || input_year > 99 || month_value < 1 ||
        month_value > 12 || day_value < 1 || day_value > 31 || hour_value < 0 ||
        hour_value > 23 || minute_value < 0 || minute_value > 59 ||
        second_value < 0 || second_value > 59)
      return true;  // FX keeps the previous RTC on an impossible date/time.
    const year_month_day date{
        year{input_year + (input_year >= 80 ? 1900 : 2000)},
        month{static_cast<unsigned>(month_value)},
        day{static_cast<unsigned>(day_value)}};
    if (!date.ok())
      return true;
    rtc_time_ = sys_days{date} + hours{hour_value} + minutes{minute_value} +
                seconds{second_value};
    rtc_anchor_ = steady_clock::now();
    return true;
  }
  const sys_seconds clock =
      rtc_time_ + duration_cast<seconds>(steady_clock::now() - rtc_anchor_);
  const sys_days date = floor<days>(clock);
  const year_month_day calendar{date};
  const hh_mm_ss time{clock - date};
  storage[head] = static_cast<int16_t>(static_cast<int>(calendar.year()) % 100);
  storage[head + 1] =
      static_cast<int16_t>(static_cast<unsigned>(calendar.month()));
  storage[head + 2] =
      static_cast<int16_t>(static_cast<unsigned>(calendar.day()));
  storage[head + 3] = static_cast<int16_t>(time.hours().count());
  storage[head + 4] = static_cast<int16_t>(time.minutes().count());
  storage[head + 5] = static_cast<int16_t>(time.seconds().count());
  storage[head + 6] = static_cast<int16_t>(weekday{date}.c_encoding());
  return true;
}
}  // namespace plc
