#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <string_view>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
constexpr size_t kStringCapacity = 65537;
}  // namespace

bool CompiledPLCExecutor::ReadString(const model::Operand& operand,
                                     std::span<char> buffer, size_t* length) {
  if (!length)
    return false;
  if (operand.kind == model::OperandKind::kImmediateString) {
    if (operand.text.size() >= buffer.size())
      return false;
    std::copy(operand.text.begin(), operand.text.end(), buffer.begin());
    *length = operand.text.size();
    return true;
  }
  if (!model::IsRegisterDevice(operand.device))
    return false;
  const auto storage = WordMemory(operand.device);
  if (operand.device.index >= storage.size())
    return false;
  const size_t available = (storage.size() - operand.device.index) * 2;
  for (size_t index = 0; index < available && index < buffer.size(); ++index) {
    const uint16_t word = storage[operand.device.index + index / 2];
    const char character = static_cast<char>(word >> ((index % 2) * 8));
    if (!character) {
      *length = index;
      return true;
    }
    buffer[index] = character;
  }
  SetError("String has no NULL terminator within device range");
  return false;
}

bool CompiledPLCExecutor::WriteString(model::DeviceAddress device,
                                      std::span<const char> text) {
  const size_t words = (text.size() + 2) / 2;
  auto storage = WordMemory(device);
  if (device.index + words > storage.size()) {
    SetError("String destination exceeds device range");
    return false;
  }
  for (size_t index = 0; index < words; ++index) {
    const size_t low = index * 2;
    const uint16_t first =
        low < text.size() ? static_cast<unsigned char>(text[low]) : 0;
    const uint16_t second =
        low + 1 < text.size() ? static_cast<unsigned char>(text[low + 1]) : 0;
    storage[device.index + index] =
        std::bit_cast<int16_t>(static_cast<uint16_t>(first | (second << 8)));
  }
  return true;
}

bool CompiledPLCExecutor::ExecuteStringOperation(
    const model::OpenPLCInstruction& instruction) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  std::array<char, kStringCapacity> first{};
  std::array<char, kStringCapacity> second{};
  std::array<char, kStringCapacity> result{};
  size_t first_length = 0;
  size_t second_length = 0;
  const auto write = [&](model::DeviceAddress device, const char* text,
                         size_t length) {
    return WriteString(device, {text, length});
  };
  const auto word = [&](size_t operand, uint32_t offset) {
    return WordMemory(
        operands[operand].device)[operands[operand].device.index + offset];
  };
  if (instruction.opcode == Opcode::kFloatToScientific) {
    const double value = ReadFloat(operands[0]);
    if (!std::isfinite(value))
      return fail("Invalid source for scientific notation conversion");
    int exponent =
        value == 0
            ? 0
            : static_cast<int>(std::floor(std::log10(std::abs(value)))) - 3;
    int64_t mantissa = value == 0 ? 0
                                  : static_cast<int64_t>(std::round(
                                        value / std::pow(10.0, exponent)));
    if (std::abs(mantissa) >= 10000) {
      mantissa /= 10;
      ++exponent;
    }
    // Both words store signed binary integers, despite the EBCD mnemonic.
    auto destination = operands[1].device;
    if (!WriteWord(destination, mantissa))
      return false;
    ++destination.index;
    return WriteWord(destination, exponent);
  }
  if (instruction.opcode == Opcode::kScientificToFloat) {
    const int mantissa = word(0, 0);
    const int exponent = word(0, 1);
    if ((mantissa != 0 &&
         (std::abs(mantissa) < 1000 || std::abs(mantissa) > 9999)) ||
        exponent < -41 || exponent > 35)
      return fail("Invalid scientific notation mantissa or exponent");
    return WriteFloat(operands[1].device, mantissa * std::pow(10.0, exponent),
                      true);
  }
  if (instruction.opcode == Opcode::kIntegerToString) {
    const int total = word(0, 0);
    const int decimals = word(0, 1);
    const int maximum = instruction.wide ? 13 : 8;
    if (total < 2 || total > maximum || decimals < 0 ||
        decimals > maximum - 3 || (decimals && decimals > total - 3))
      return fail("Invalid STR display specification");
    const int64_t value = ReadValue(operands[1], instruction.wide);
    const auto converted =
        std::to_chars(first.data(), first.data() + 32, std::abs(value));
    size_t digits = converted.ptr - first.data();
    const size_t padded = std::max(digits, static_cast<size_t>(decimals + 1));
    const size_t numeric = padded + (decimals ? 1 : 0);
    if (numeric + 1 > static_cast<size_t>(total))
      return fail("STR value does not fit display specification");
    std::fill_n(result.begin(), total, ' ');
    result[0] = value < 0 ? '-' : ' ';
    const size_t start = total - numeric;
    size_t digit = 0;
    for (size_t index = 0; index < padded; ++index) {
      const size_t output =
          start + index + (decimals && index >= padded - decimals ? 1 : 0);
      result[output] = index < padded - digits ? '0' : first[digit++];
    }
    // The decimal point lies before the first fractional digit.
    if (decimals)
      result[start + padded - decimals] = '.';
    return write(operands[2].device, result.data(), total);
  }
  if (instruction.opcode == Opcode::kFloatToString) {
    const int format = word(1, 0);
    const int total = word(1, 1);
    const int decimals = word(1, 2);
    const double value = ReadFloat(operands[0]);
    if (!std::isfinite(value) || (format != 0 && format != 1) || total < 2 ||
        total > 24 || decimals < 0 || decimals > 7 ||
        (format == 1 ? total < 7 || decimals > total - 7
                     : decimals && decimals > total - 3))
      return fail("Invalid ESTR display specification");
    const auto converted = std::to_chars(
        first.data(), first.data() + 100, std::abs(value),
        format ? std::chars_format::scientific : std::chars_format::fixed,
        decimals);
    if (converted.ec != std::errc{})
      return fail("ESTR conversion failed");
    const size_t numeric = converted.ptr - first.data();
    if (numeric + 1 > static_cast<size_t>(total))
      return fail("ESTR value does not fit display specification");
    std::fill_n(result.begin(), total, ' ');
    result[0] = std::signbit(value) ? '-' : ' ';
    std::copy_n(first.begin(), numeric, result.begin() + total - numeric);
    for (int index = 0; index < total; ++index) {
      if (result[index] == 'e')
        result[index] = 'E';
    }
    return write(operands[2].device, result.data(), total);
  }
  if (!ReadString(operands[0], first, &first_length))
    return false;
  const std::string_view source(first.data(), first_length);
  switch (instruction.opcode) {
    case Opcode::kStringMove:
      return write(operands[1].device, first.data(), first_length);
    case Opcode::kStringLength:
      return WriteWord(operands[1].device, first_length);
    case Opcode::kStringConcat:
      if (!ReadString(operands[1], second, &second_length))
        return false;
      if (first_length + second_length >= result.size())
        return fail("Linked string exceeds device range");
      if (operands[0].kind == model::OperandKind::kWordDevice &&
          operands[1].kind == model::OperandKind::kWordDevice &&
          operands[0].device == operands[1].device &&
          operands[0].device == operands[2].device)
        return fail("$+ cannot use the same device for all three operands");
      std::copy_n(first.begin(), first_length, result.begin());
      std::copy_n(second.begin(), second_length, result.begin() + first_length);
      return write(operands[2].device, result.data(),
                   first_length + second_length);
    case Opcode::kStringRight:
    case Opcode::kStringLeft: {
      const int count = ReadValue(operands[2]);
      if (count < 0 || static_cast<size_t>(count) > first_length)
        return fail("String extraction count exceeds source length");
      const size_t start =
          instruction.opcode == Opcode::kStringRight ? first_length - count : 0;
      return write(operands[1].device, first.data() + start, count);
    }
    case Opcode::kStringRead:
    case Opcode::kStringWrite: {
      const int position = word(2, 0);
      const int count = word(2, 1);
      if (position < 0 || count < -1 ||
          (count > 0 && static_cast<size_t>(count) > first_length))
        return fail("Invalid MIDR/MIDW position or length");
      if (!count)
        return true;
      if (count == -1)
        return write(operands[1].device, first.data(), first_length);
      const size_t start = std::max(1, position) - 1;
      if (instruction.opcode == Opcode::kStringRead) {
        if (start + count > first_length)
          return fail("MIDR extraction exceeds source string");
        return write(operands[1].device, first.data() + start, count);
      }
      if (!ReadString(operands[1], second, &second_length))
        return false;
      if (start >= second_length)
        return fail("MIDW start exceeds destination string");
      std::copy_n(first.begin(), std::min<size_t>(count, second_length - start),
                  second.begin() + start);
      return write(operands[1].device, second.data(), second_length);
    }
    case Opcode::kStringSearch: {
      const int position = ReadValue(operands[3]);
      if (position <= 0)
        return true;
      if (!ReadString(operands[1], second, &second_length))
        return false;
      const size_t found = source.find(
          std::string_view(second.data(), second_length), position - 1);
      return WriteWord(operands[2].device,
                       found == std::string_view::npos ? 0 : found + 1);
    }
    case Opcode::kStringToInteger: {
      const int maximum = instruction.wide ? 13 : 8;
      if (first_length < 2 || first_length > static_cast<size_t>(maximum) ||
          (source.front() != ' ' && source.front() != '-'))
        return fail("VAL requires a signed fixed point string");
      int64_t value = 0;
      int decimals = 0;
      bool decimal = false;
      bool digit = false;
      for (size_t index = 1; index < first_length; ++index) {
        const char character = first[index];
        if (character == ' ' && !digit && !decimal)
          continue;
        if (character == '.' && !decimal) {
          decimal = true;
          continue;
        }
        if (character < '0' || character > '9')
          return fail("Invalid numeric character in VAL source");
        digit = true;
        value = value * 10 + character - '0';
        if (decimal)
          ++decimals;
      }
      if (source.front() == '-')
        value = -value;
      if (!digit || decimals > maximum - 3 ||
          (decimals && decimals > static_cast<int>(first_length) - 3) ||
          value < (instruction.wide ? INT32_MIN : INT16_MIN) ||
          value > (instruction.wide ? INT32_MAX : INT16_MAX))
        return fail("VAL numeric value exceeds FX range");
      auto specification = operands[1].device;
      if (!WriteWord(specification, first_length))
        return false;
      ++specification.index;
      return WriteWord(specification, decimals) &&
             WriteWord(operands[2].device, value, instruction.wide);
    }
    case Opcode::kStringToFloat: {
      size_t finish = 0;
      for (size_t index = 0; index < first_length; ++index) {
        if (first[index] != ' ')
          result[finish++] = first[index];
      }
      const size_t begin = finish && result[0] == '+' ? 1 : 0;
      double value = 0;
      const auto converted =
          std::from_chars(result.data() + begin, result.data() + finish, value);
      if (converted.ec != std::errc{} ||
          converted.ptr != result.data() + finish)
        return fail("Invalid EVAL numeric string");
      return WriteFloat(operands[1].device, value, true);
    }
    default:
      return fail("Unsupported string operation");
  }
}
}  // namespace plc
