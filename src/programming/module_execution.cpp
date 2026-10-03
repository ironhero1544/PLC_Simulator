#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <bit>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::SetModuleBufferWord(int unit, int address,
                                              int16_t value) {
  if (unit < 0 || unit >= 8 || address < 0 || address >= 32767)
    return false;
  modules_[unit].buffer[address] = value;
  return true;
}

std::optional<int16_t> CompiledPLCExecutor::GetModuleBufferWord(
    int unit, int address) const {
  if (unit < 0 || unit >= 8 || address < 0 || address >= 32767)
    return std::nullopt;
  return modules_[unit].buffer[address];
}

bool CompiledPLCExecutor::SetAnalogInput(int unit, int channel, int16_t value) {
  if (unit < 0 || unit >= 8 || channel < 0 || channel >= 8)
    return false;
  modules_[unit].analog_inputs[channel] = value;
  return true;
}

std::optional<int16_t> CompiledPLCExecutor::GetAnalogOutput(int unit,
                                                            int channel) const {
  if (unit < 0 || unit >= 8 || channel < 0 || channel >= 8)
    return std::nullopt;
  return modules_[unit].analog_outputs[channel];
}

bool CompiledPLCExecutor::SetVolume(int channel, uint8_t value) {
  if (channel < 0 || channel >= 8)
    return false;
  volumes_[channel] = value;
  return true;
}

bool CompiledPLCExecutor::ExecuteModuleOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const bool divided = instruction.opcode == Opcode::kReadModuleDivided ||
                       instruction.opcode == Opcode::kWriteModuleDivided;
  const bool reading = instruction.opcode == Opcode::kReadModule ||
                       instruction.opcode == Opcode::kReadModuleDivided;
  const auto fail = [this, divided](const char* message) {
    if (divided)
      memory_.special_m[329] = true;
    SetError(message);
    return false;
  };
  auto& transfer = module_transfers_[index];
  if (!power) {
    if (transfer.active && module_transfer_owners_[transfer.unit] == index)
      module_transfer_owners_[transfer.unit] = SIZE_MAX;
    if (divided && transfer.complete)
      memory_.special_m[29] = false;
    transfer = {};
    return true;
  }
  if (instruction.opcode == Opcode::kVolumeRead ||
      instruction.opcode == Opcode::kVolumeScale) {
    const int channel = ReadValue(operands[0]);
    if (channel < 0 || channel >= 8)
      return fail("Volume number must be 0 to 7");
    const int value = volumes_[channel];
    return WriteValue(operands[1], instruction.opcode == Opcode::kVolumeRead
                                       ? value
                                       : value * 10 / 255);
  }
  if (instruction.opcode == Opcode::kReadAnalog ||
      instruction.opcode == Opcode::kWriteAnalog) {
    const int unit = ReadValue(operands[0]);
    const int channel_number = ReadValue(operands[1]);
    const bool read = instruction.opcode == Opcode::kReadAnalog;
    const bool twelve_bit = channel_number == 21 || channel_number == 22;
    const int channel = twelve_bit ? channel_number - 21 : channel_number - 1;
    if (unit < 0 || unit >= 8 || channel < 0 || channel > 1 ||
        (!twelve_bit && !read && channel != 0))
      return fail("Invalid dedicated analog block or channel number");
    const int maximum = twelve_bit ? 4095 : 255;
    if (read)
      return WriteValue(
          operands[2],
          std::clamp<int>(modules_[unit].analog_inputs[channel], 0, maximum));
    modules_[unit].analog_outputs[channel] =
        static_cast<int16_t>(std::clamp(ReadValue(operands[2]), 0, maximum));
    return true;
  }
  if (divided && transfer.complete)
    return true;
  const bool wide = instruction.wide;
  const int unit = ReadValue(operands[0], wide);
  const int address = ReadValue(operands[1], wide);
  const int count = ReadValue(operands[3], wide);
  const int width = wide ? 2 : 1;
  const bool constant =
      operands[2].kind == model::OperandKind::kImmediateDecimal ||
      operands[2].kind == model::OperandKind::kImmediateHex;
  const auto device = operands[2].device;
  const uint32_t limit = model::WordDeviceLimit(device);
  if (unit < 0 || unit >= 8 || address < 0 || address >= 32767 || count < 1 ||
      count > 32767 || address + static_cast<int64_t>(count) * width > 32767 ||
      (!constant &&
       (!model::IsRegisterDevice(device) ||
        device.index + static_cast<int64_t>(count) * width > limit)))
    return fail("Module transfer exceeds unit, BFM or PLC memory range");
  int begin = 0;
  int end = count;
  if (divided) {
    if (!transfer.active) {
      if (module_transfer_owners_[unit] != SIZE_MAX &&
          module_transfer_owners_[unit] != index) {
        memory_.special_m[328] = true;
        return true;
      }
      const int chunk = ReadValue(operands[4]);
      if (chunk < 1 || chunk > 32767)
        return fail("Invalid divided module transfer chunk");
      transfer.active = true;
      transfer.unit = unit;
      transfer.address = address;
      transfer.total = count;
      transfer.chunk = chunk;
      transfer.device = device;
      module_transfer_owners_[unit] = index;
      memory_.special_m[29] = false;
      memory_.special_m[328] = false;
      memory_.special_m[329] = false;
    } else if (transfer.unit != unit || transfer.address != address ||
               transfer.total != count || transfer.device != device) {
      return fail("Module transfer operands changed while busy");
    }
    begin = transfer.progress;
    end = std::min(count, begin + transfer.chunk);
  }
  auto& buffer = modules_[unit].buffer;
  for (int item = begin; item < end; ++item) {
    auto operand = operands[2];
    operand.device.index += item * width;
    const int offset = address + item * width;
    if (reading) {
      uint32_t bits = static_cast<uint16_t>(buffer[offset]);
      if (wide)
        bits |= static_cast<uint32_t>(static_cast<uint16_t>(buffer[offset + 1]))
                << 16;
      if (!WriteWord(operand.device, bits, wide))
        return false;
    } else {
      const uint32_t bits = static_cast<uint32_t>(ReadValue(operand, wide));
      buffer[offset] = std::bit_cast<int16_t>(static_cast<uint16_t>(bits));
      if (wide)
        buffer[offset + 1] =
            std::bit_cast<int16_t>(static_cast<uint16_t>(bits >> 16));
    }
  }
  if (divided) {
    transfer.progress = end;
    transfer.complete = end == count;
    if (transfer.complete) {
      transfer.active = false;
      module_transfer_owners_[unit] = SIZE_MAX;
      memory_.special_m[29] = true;
    }
  }
  return true;
}
}  // namespace plc
