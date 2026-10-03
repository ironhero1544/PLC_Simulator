#include "plc_emulator/programming/compiled_plc_executor.h"

#include <bit>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

std::span<int16_t> CompiledPLCExecutor::WordMemory(
    model::DeviceAddress device) {
  switch (device.kind) {
    case model::DeviceKind::kD:
      return std::span(memory_.D).first(model::WordDeviceLimit(device));
    case model::DeviceKind::kR:
      return memory_.R;
    case model::DeviceKind::kV:
      return memory_.V;
    case model::DeviceKind::kZ:
      return memory_.Z;
    case model::DeviceKind::kBufferMemory:
      return device.unit < 8 ? std::span(modules_[device.unit].buffer)
                             : std::span<int16_t>{};
    default:
      return {};
  }
}

std::span<const int16_t> CompiledPLCExecutor::WordMemory(
    model::DeviceAddress device) const {
  switch (device.kind) {
    case model::DeviceKind::kD:
      return std::span(memory_.D).first(model::WordDeviceLimit(device));
    case model::DeviceKind::kR:
      return memory_.R;
    case model::DeviceKind::kV:
      return memory_.V;
    case model::DeviceKind::kZ:
      return memory_.Z;
    case model::DeviceKind::kBufferMemory:
      return device.unit < 8 ? std::span(modules_[device.unit].buffer)
                             : std::span<const int16_t>{};
    default:
      return {};
  }
}

bool CompiledPLCExecutor::ResolveOperand(model::Operand* operand,
                                         bool wide) const {
  auto& device = operand->device;
  if (!device.index_kind)
    return true;
  const int number = device.index_register;
  if (number >= 8)
    return false;
  int32_t offset =
      device.index_kind == 'V' ? memory_.V[number] : memory_.Z[number];
  if (wide) {
    offset = device.index_kind == 'V'
                 ? 0
                 : std::bit_cast<int32_t>(
                       static_cast<uint32_t>(
                           static_cast<uint16_t>(memory_.Z[number])) |
                       (static_cast<uint32_t>(
                            static_cast<uint16_t>(memory_.V[number]))
                        << 16));
  }
  device.index_kind = 0;
  device.index_register = 0;
  if (operand->kind == model::OperandKind::kImmediateDecimal ||
      operand->kind == model::OperandKind::kImmediateHex) {
    operand->immediate =
        std::bit_cast<int32_t>(static_cast<uint32_t>(operand->immediate) +
                               static_cast<uint32_t>(offset));
    return true;
  }
  if (operand->kind == model::OperandKind::kPointer) {
    const int64_t pointer = static_cast<int64_t>(operand->immediate) + offset;
    if (pointer < 0 || pointer >= 4096)
      return false;
    operand->immediate = static_cast<int32_t>(pointer);
    return true;
  }
  const int64_t address = static_cast<int64_t>(device.index) + offset;
  uint32_t limit = 0;
  switch (device.kind) {
    case model::DeviceKind::kX:
    case model::DeviceKind::kY:
      limit = 256;
      break;
    case model::DeviceKind::kM:
      limit = 7680;
      break;
    case model::DeviceKind::kS:
      limit = 4096;
      break;
    case model::DeviceKind::kT:
      limit = 512;
      break;
    case model::DeviceKind::kC:
      limit = device.index >= 200 ? 256 : 200;
      if (device.index >= 200 && address < 200)
        return false;
      break;
    default:
      limit = model::WordDeviceLimit(device);
      break;
  }
  if (address < 0 || address >= limit)
    return false;
  device.index = static_cast<uint32_t>(address);
  if (wide && operand->kind == model::OperandKind::kWordDevice &&
      device.kind != model::DeviceKind::kV &&
      device.kind != model::DeviceKind::kZ && device.index + 1 >= limit)
    return false;
  if (operand->kind == model::OperandKind::kPackedBit &&
      device.index + operand->immediate * 4 > limit)
    return false;
  return true;
}
}  // namespace plc
