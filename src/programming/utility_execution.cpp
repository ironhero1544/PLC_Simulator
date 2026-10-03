#include "plc_emulator/programming/compiled_plc_executor.h"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::ExecuteUtilityOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const bool wide = instruction.wide;
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  const auto range = [](model::DeviceAddress device, uint32_t words) {
    const uint32_t limit = model::WordDeviceLimit(device);
    return model::IsRegisterDevice(device) && device.index + words <= limit;
  };
  const auto read_at = [this](model::Operand operand, uint32_t offset,
                              bool double_word = false) {
    operand.device.index += offset;
    return ReadValue(operand, double_word);
  };
  const auto write_at = [this](model::DeviceAddress device, uint32_t offset,
                               int64_t value, bool double_word = false) {
    device.index += offset;
    return WriteWord(device, value, double_word);
  };
  const auto time_value = [&](const model::Operand& operand, int maximum_hour,
                              int32_t* seconds) {
    if (!range(operand.device, 3))
      return false;
    const int hour = read_at(operand, 0);
    const int minute = read_at(operand, 1);
    const int second = read_at(operand, 2);
    if (hour < 0 || hour > maximum_hour || minute < 0 || minute > 59 ||
        second < 0 || second > 59)
      return false;
    *seconds = hour * 3600 + minute * 60 + second;
    return true;
  };
  const auto write_time = [&](model::DeviceAddress device, int32_t seconds) {
    if (!range(device, 3))
      return fail("Time result exceeds device range");
    return write_at(device, 0, seconds / 3600) &&
           write_at(device, 1, seconds / 60 % 60) &&
           write_at(device, 2, seconds % 60);
  };
  if (instruction.opcode == Opcode::kDuty) {
    const auto destination = operands[2].device;
    const int on = ReadValue(operands[0]);
    const int off = ReadValue(operands[1]);
    if (destination.kind != model::DeviceKind::kSpecialM ||
        destination.index < 330 || destination.index > 334 || on < 0 || off < 0)
      return fail("DUTY requires M8330 to M8334 and nonnegative scan counts");
    int64_t& scans = operation_elapsed_ms_[index];
    if (!power && scans < 0)
      return true;
    scans = power ? 0 : on + off == 0 ? 0 : (scans + 1) % (on + off);
    memory_.D[8000 + destination.index] = static_cast<int16_t>(scans);
    WriteBit(destination, scans < on);
    return true;
  }
  if (instruction.opcode == Opcode::kHourMeter) {
    const uint32_t width = wide ? 2 : 1;
    const auto destination = operands[1].device;
    if (!range(destination, width + 1))
      return fail("HOUR result exceeds device range");
    int64_t hours = ReadValue(operands[1], wide);
    int64_t seconds = read_at(operands[1], width);
    const int64_t maximum = wide ? INT32_MAX : INT16_MAX;
    const int32_t alarm = ReadValue(operands[0], wide);
    if (hours < 0 || seconds < 0 || seconds > 3599 || alarm < 0)
      return fail("Invalid HOUR values");
    if (power && hours < maximum) {
      int64_t& milliseconds = operation_elapsed_ms_[index];
      milliseconds += current_elapsed_ms_;
      seconds += milliseconds / 1000;
      milliseconds %= 1000;
      hours = std::min(maximum, hours + seconds / 3600);
      seconds %= 3600;
      if (!WriteWord(destination, hours, wide) ||
          !write_at(destination, width, seconds))
        return false;
    }
    WriteBit(operands[2].device, hours >= alarm);
    return true;
  }
  if (!power)
    return true;
  if (instruction.opcode == Opcode::kRandom) {
    const uint32_t seed =
        static_cast<uint16_t>(memory_.D[8310]) |
        (static_cast<uint32_t>(static_cast<uint16_t>(memory_.D[8311])) << 16);
    const uint32_t next = seed * uint32_t{1103515245} + uint32_t{12345};
    return WriteWord({model::DeviceKind::kD, 8310}, next, true) &&
           WriteValue(operands[0], (next >> 16) & 0x7fff);
  }
  if (instruction.opcode == Opcode::kTimeCompare ||
      instruction.opcode == Opcode::kTimeZoneCompare) {
    int32_t lower = 0;
    int32_t upper = 0;
    int32_t compared = 0;
    const bool zone = instruction.opcode == Opcode::kTimeZoneCompare;
    if (zone) {
      if (!time_value(operands[0], 23, &lower) ||
          !time_value(operands[1], 23, &upper) ||
          !time_value(operands[2], 23, &compared))
        return fail("Invalid TZCP time");
      upper = std::max(lower, upper);
    } else {
      const int hour = ReadValue(operands[0]);
      const int minute = ReadValue(operands[1]);
      const int second = ReadValue(operands[2]);
      if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 ||
          second > 59 || !time_value(operands[3], 23, &compared))
        return fail("Invalid TCMP time");
      lower = upper = hour * 3600 + minute * 60 + second;
    }
    auto destination = operands[zone ? 3 : 4].device;
    WriteBit(destination, compared < lower);
    ++destination.index;
    WriteBit(destination, compared >= lower && compared <= upper);
    ++destination.index;
    WriteBit(destination, compared > upper);
    return true;
  }
  if (instruction.opcode == Opcode::kTimeAdd ||
      instruction.opcode == Opcode::kTimeSub) {
    int32_t first = 0;
    int32_t second = 0;
    if (!time_value(operands[0], 23, &first) ||
        !time_value(operands[1], 23, &second))
      return fail("Invalid time arithmetic source");
    const int32_t result = instruction.opcode == Opcode::kTimeAdd
                               ? first + second
                               : first - second;
    memory_.special_m[20] = result == 0;
    if (instruction.opcode == Opcode::kTimeAdd)
      memory_.special_m[22] = result >= 86400;
    else
      memory_.special_m[21] = result < 0;
    return write_time(operands[2].device, (result + 86400) % 86400);
  }
  if (instruction.opcode == Opcode::kHoursToSeconds) {
    int32_t seconds = 0;
    if (!time_value(operands[0], wide ? 32767 : 9, &seconds) ||
        (!wide && seconds > 32767))
      return fail("HTOS time exceeds conversion range");
    return WriteWord(operands[1].device, seconds, wide);
  }
  if (instruction.opcode == Opcode::kSecondsToHours) {
    const int32_t seconds = ReadValue(operands[0], wide);
    if (seconds < 0 || seconds > 117964799)
      return fail("STOH seconds exceeds conversion range");
    return write_time(operands[1].device, seconds);
  }
  if (instruction.opcode == Opcode::kZone) {
    const int64_t value = ReadValue(operands[2], wide);
    return WriteValue(
        operands[3],
        value == 0 ? 0 : value + ReadValue(operands[value < 0 ? 0 : 1], wide),
        wide);
  }
  if (instruction.opcode == Opcode::kScale ||
      instruction.opcode == Opcode::kScale2) {
    const uint32_t width = wide ? 2 : 1;
    const int32_t points = ReadValue(operands[1], wide);
    if (points < 2 || points > 255 ||
        !range(operands[1].device, (1 + 2 * points) * width))
      return fail("Invalid scaling table length");
    const bool split = instruction.opcode == Opcode::kScale2;
    const auto coordinate = [&](int point, bool y) {
      const uint32_t position =
          split ? 1 + point + (y ? points : 0) : 1 + 2 * point + (y ? 1 : 0);
      return read_at(operands[1], position * width, wide);
    };
    const int32_t input = ReadValue(operands[0], wide);
    if (input < coordinate(0, false) || input > coordinate(points - 1, false))
      return fail("Scaling input is outside table");
    for (int point = 0; point < points; ++point) {
      if (input != coordinate(point, false))
        continue;
      int last = point;
      while (last + 1 < points && coordinate(last + 1, false) == input)
        ++last;
      return WriteValue(operands[2], coordinate((point + last + 1) / 2, true),
                        wide);
    }
    for (int point = 1; point < points; ++point) {
      const int64_t x0 = coordinate(point - 1, false);
      const int64_t x1 = coordinate(point, false);
      if (x1 < x0)
        return fail("Scaling X coordinates must be ascending");
      if (input > x1)
        continue;
      if (x0 == x1) {
        int last = point;
        while (last + 1 < points && coordinate(last + 1, false) == x1)
          ++last;
        const int selected = (point - 1 + last) / 2;
        return WriteValue(operands[2], coordinate(selected, true), wide);
      }
      const int64_t y0 = coordinate(point - 1, true);
      const int64_t y1 = coordinate(point, true);
      const long double value =
          y0 + static_cast<long double>(input - x0) * (y1 - y0) / (x1 - x0);
      const int64_t result = static_cast<int64_t>(std::round(value));
      if (result < (wide ? INT32_MIN : INT16_MIN) ||
          result > (wide ? INT32_MAX : INT16_MAX))
        return fail("Scaling result exceeds word range");
      return WriteValue(operands[2], result, wide);
    }
    return fail("Invalid scaling table");
  }
  if (instruction.opcode == Opcode::kBinaryToDecimalAscii ||
      instruction.opcode == Opcode::kDecimalAsciiToBinary) {
    const bool encode = instruction.opcode == Opcode::kBinaryToDecimalAscii;
    const int digits = wide ? 10 : 5;
    const auto device = operands[encode ? 1 : 0].device;
    if (!range(device, wide ? 6 : encode && !memory_.special_m[91] ? 4 : 3))
      return fail("Decimal ASCII conversion exceeds device range");
    const auto read_byte = [&](int byte) {
      return (static_cast<uint16_t>(
                  WordMemory(device)[device.index + byte / 2]) >>
              ((byte % 2) * 8)) &
             0xff;
    };
    if (!encode) {
      const int sign = read_byte(0);
      if (sign != ' ' && sign != '-')
        return fail("Decimal ASCII requires space or minus sign");
      int64_t value = 0;
      for (int digit = 1; digit <= digits; ++digit) {
        int character = read_byte(digit);
        if (character == ' ' || character == 0)
          character = '0';
        if (character < '0' || character > '9')
          return fail("Invalid decimal ASCII digit");
        value = value * 10 + character - '0';
      }
      if (sign == '-')
        value = -value;
      if (value < (wide ? INT32_MIN : INT16_MIN) ||
          value > (wide ? INT32_MAX : INT16_MAX))
        return fail("Decimal ASCII integer overflow");
      return WriteValue(operands[1], value, wide);
    }
    const int64_t value = ReadValue(operands[0], wide);
    std::array<char, 12> bytes{};
    bytes.fill(' ');
    bytes[0] = value < 0 ? '-' : ' ';
    char buffer[12];
    const auto [end, status] = std::to_chars(buffer, buffer + sizeof(buffer),
                                             value < 0 ? -value : value);
    if (status != std::errc{})
      return fail("Decimal ASCII formatting failed");
    const int length = static_cast<int>(end - buffer);
    std::copy(buffer, end, bytes.begin() + 1 + digits - length);
    if (wide)
      bytes[11] = memory_.special_m[91] ? ' ' : 0;
    for (int word = 0; word < (wide ? 6 : 3); ++word)
      WordMemory(device)[device.index + word] = static_cast<int16_t>(
          static_cast<uint8_t>(bytes[2 * word]) |
          (static_cast<uint16_t>(static_cast<uint8_t>(bytes[2 * word + 1]))
           << 8));
    if (!wide && !memory_.special_m[91])
      WordMemory(device)[device.index + 3] = 0;
    return true;
  }
  if (instruction.opcode == Opcode::kCrc ||
      instruction.opcode == Opcode::kCheckCode) {
    const int count = ReadValue(operands[2]);
    const bool bytes = memory_.special_m[161];
    const bool checksum = instruction.opcode == Opcode::kCheckCode;
    const int byte_count = checksum && !bytes ? count * 2 : count;
    if (count < 1 || count > 256 ||
        !range(operands[0].device,
               checksum || bytes ? count : (count + 1) / 2) ||
        !range(operands[1].device,
               instruction.opcode == Opcode::kCheckCode || bytes ? 2 : 1))
      return fail("Check code exceeds count or device range");
    uint16_t crc = 0xffff;
    uint16_t sum = 0;
    uint8_t parity = 0;
    for (int byte = 0; byte < byte_count; ++byte) {
      const uint16_t word = static_cast<uint16_t>(WordMemory(
          operands[0]
              .device)[operands[0].device.index + (bytes ? byte : byte / 2)]);
      const uint8_t value =
          static_cast<uint8_t>(word >> (bytes ? 0 : (byte % 2) * 8));
      sum += value;
      parity ^= value;
      crc ^= value;
      for (int bit = 0; bit < 8; ++bit)
        crc = static_cast<uint16_t>((crc >> 1) ^ ((crc & 1) ? 0xa001 : 0));
    }
    const auto destination = operands[1].device;
    if (instruction.opcode == Opcode::kCheckCode)
      return WriteWord(destination, sum) && write_at(destination, 1, parity);
    if (bytes)
      return WriteWord(destination, crc & 0xff) &&
             write_at(destination, 1, crc >> 8);
    return WriteWord(destination, crc);
  }
  return fail("Unsupported utility instruction");
}
}  // namespace plc
