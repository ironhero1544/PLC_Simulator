#include "plc_emulator/programming/compiled_plc_executor.h"

#include <bit>
#include <cmath>
#include <limits>
#include <numbers>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

double CompiledPLCExecutor::ReadFloat(const model::Operand& operand) const {
  if (operand.kind == model::OperandKind::kImmediateDecimal ||
      operand.kind == model::OperandKind::kImmediateHex)
    return operand.immediate;
  const int32_t bits = operand.kind == model::OperandKind::kImmediateFloat
                           ? operand.immediate
                           : ReadValue(operand, true);
  return std::bit_cast<float>(bits);
}

bool CompiledPLCExecutor::WriteFloat(model::DeviceAddress device, double value,
                                     bool flags) {
  constexpr double kMaximum = std::numeric_limits<float>::max();
  constexpr double kMinimum = std::numeric_limits<float>::min();
  if (std::isnan(value)) {
    SetError("Invalid floating point result");
    return false;
  }
  const bool zero = value == 0;
  const bool underflow = !zero && std::abs(value) < kMinimum;
  const bool overflow = std::abs(value) > kMaximum;
  if (flags) {
    memory_.special_m[20] = zero;
    memory_.special_m[21] = underflow;
    memory_.special_m[22] = overflow;
  }
  if (underflow)
    value = std::copysign(kMinimum, value);
  if (overflow)
    value = std::copysign(kMaximum, value);
  return WriteWord(device, std::bit_cast<int32_t>(static_cast<float>(value)),
                   true);
}

bool CompiledPLCExecutor::ExecuteFloatOperation(
    const model::OpenPLCInstruction& instruction) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  if (instruction.opcode == Opcode::kToFloat)
    return WriteFloat(operands[1].device,
                      ReadValue(operands[0], instruction.wide), false);
  const double first = ReadFloat(operands[0]);
  if (!std::isfinite(first))
    return fail("Invalid floating point source");
  if (instruction.opcode == Opcode::kFromFloat) {
    const double truncated = std::trunc(first);
    const double lower = instruction.wide ? INT32_MIN : INT16_MIN;
    const double upper = instruction.wide ? INT32_MAX : INT16_MAX;
    memory_.special_m[20] = truncated == 0;
    memory_.special_m[21] = truncated != first;
    memory_.special_m[22] = truncated < lower || truncated > upper;
    if (memory_.special_m[22])
      return true;  // FX leaves the destination unchanged on integer overflow.
    return WriteWord(operands[1].device, static_cast<int64_t>(truncated),
                     instruction.wide);
  }
  const bool binary = instruction.opcode >= Opcode::kFloatAdd &&
                      instruction.opcode <= Opcode::kFloatDiv;
  const bool comparison = instruction.opcode == Opcode::kFloatCompare ||
                          instruction.opcode == Opcode::kFloatZoneCompare;
  const double second = binary || comparison ? ReadFloat(operands[1]) : 0;
  if (!std::isfinite(second))
    return fail("Invalid second floating point source");
  if (comparison) {
    double compared = first;
    double lower = second;
    double upper = second;
    if (instruction.opcode == Opcode::kFloatZoneCompare) {
      lower = first;
      upper = second;
      compared = ReadFloat(operands[2]);
      if (!std::isfinite(compared))
        return fail("Invalid floating point zone comparison");
      upper = std::max(lower, upper);
    }
    auto destination = operands[instruction.operand_count - 1].device;
    WriteBit(destination, instruction.opcode == Opcode::kFloatZoneCompare
                              ? compared < lower
                              : compared > upper);
    destination = model::OffsetBitAddress(destination, 1);
    WriteBit(destination, compared >= lower && compared <= upper);
    destination = model::OffsetBitAddress(destination, 1);
    WriteBit(destination, instruction.opcode == Opcode::kFloatZoneCompare
                              ? compared > upper
                              : compared < lower);
    return true;
  }
  double value = first;
  bool flags = true;
  switch (instruction.opcode) {
    case Opcode::kFloatMove:
      flags = false;
      break;
    case Opcode::kFloatAdd:
      value = first + second;
      break;
    case Opcode::kFloatSub:
      value = first - second;
      break;
    case Opcode::kFloatMul:
      value = first * second;
      break;
    case Opcode::kFloatDiv:
      if (second == 0)
        return fail("Floating point division by zero");
      value = first / second;
      break;
    case Opcode::kFloatExp:
      value = std::exp(first);
      break;
    case Opcode::kFloatLog:
    case Opcode::kFloatLog10:
      if (first <= 0)
        return fail("Floating point logarithm requires positive source");
      value = instruction.opcode == Opcode::kFloatLog ? std::log(first)
                                                      : std::log10(first);
      break;
    case Opcode::kFloatSqrt:
      if (first < 0)
        return fail("Floating point square root requires nonnegative source");
      value = std::sqrt(first);
      break;
    case Opcode::kFloatNegate:
      value = -first;
      flags = false;
      break;
    case Opcode::kFloatSin:
      value = std::sin(first);
      break;
    case Opcode::kFloatCos:
      value = std::cos(first);
      break;
    case Opcode::kFloatTan:
      value = std::tan(first);
      break;
    case Opcode::kFloatAsin:
    case Opcode::kFloatAcos:
      if (std::abs(first) > 1)
        return fail("Inverse trigonometric source must be between -1 and 1");
      value = instruction.opcode == Opcode::kFloatAsin ? std::asin(first)
                                                       : std::acos(first);
      break;
    case Opcode::kFloatAtan:
      value = std::atan(first);
      break;
    case Opcode::kFloatRadians:
      value = first * std::numbers::pi / 180;
      break;
    case Opcode::kFloatDegrees:
      value = first * 180 / std::numbers::pi;
      break;
    default:
      return fail("Unsupported floating point operation");
  }
  if (instruction.opcode == Opcode::kFloatExp &&
      (!std::isfinite(value) || value < std::numeric_limits<float>::min() ||
       value > std::numeric_limits<float>::max()))
    return fail("Floating point exponential result exceeds FX range");
  return WriteFloat(operands[instruction.operand_count - 1].device, value,
                    flags);
}
}  // namespace plc
