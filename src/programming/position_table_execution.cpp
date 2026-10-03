#include "plc_emulator/programming/compiled_plc_executor.h"

#include <bit>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

bool PositioningMode(model::Opcode opcode) {
  return opcode == model::Opcode::kInterruptPosition ||
         opcode == model::Opcode::kVariablePulse ||
         opcode == model::Opcode::kRelativePosition ||
         opcode == model::Opcode::kAbsolutePosition;
}
}  // namespace

bool CompiledPLCExecutor::ConfigurePositioningTable(
    model::DeviceAddress head,
    const std::array<model::DeviceAddress, 4>& directions) {
  if (head.index_kind || head.bit >= 0 ||
      (head.kind != model::DeviceKind::kD &&
       head.kind != model::DeviceKind::kR) ||
      head.index + 1600 > model::WordDeviceLimit(head))
    return false;
  for (int axis = 0; axis < 4; ++axis)
    if (axes_[axis].status.busy || directions[axis].index_kind ||
        directions[axis].kind != model::DeviceKind::kY ||
        directions[axis].index >= 256 ||
        directions[axis].index == uint32_t(axis))
      return false;
  positioning_table_head_ = head;
  table_directions_ = directions;
  positioning_table_enabled_ = true;
  for (int axis = 0; axis < 4; ++axis)
    for (int entry = 0; entry < 100; ++entry) {
      const auto& data = positioning_entries_[axis][entry];
      if (PositioningMode(data.opcode))
        SetPositioningEntry(axis, entry + 1, data.opcode, data.pulses,
                            data.frequency);
    }
  return true;
}

bool CompiledPLCExecutor::SetPositioningEntry(int axis, int entry,
                                              model::Opcode opcode,
                                              int32_t pulses,
                                              int32_t frequency) {
  if (!positioning_table_enabled_ || axis < 0 || axis >= 4 || entry < 1 ||
      entry > 100 || !PositioningMode(opcode) || axes_[axis].status.busy)
    return false;
  positioning_entries_[axis][entry - 1] = {opcode, pulses, frequency};
  auto address = positioning_table_head_;
  address.index += axis * 400 + (entry - 1) * 4;
  // GX Works2 stores the PLSV frequency in the pulse field, with the
  // ordinary frequency field zero. The mode belongs to PLC parameters.
  const int32_t first =
      opcode == model::Opcode::kVariablePulse ? frequency : pulses;
  WriteWord(address, first, true);
  address.index += 2;
  return WriteWord(
      address, opcode == model::Opcode::kVariablePulse ? 0 : frequency, true);
}

bool CompiledPLCExecutor::ExecutePositionTable(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  const auto output = instruction.operands[0].device;
  if (output.kind != model::DeviceKind::kY || output.index >= 4) {
    SetError("DTBL pulse output must be Y0..Y3");
    return false;
  }
  const int axis = static_cast<int>(output.index);
  if (!power && axes_[axis].owner != index)
    return true;
  if (!power) {
    model::OpenPLCInstruction stop;
    stop.opcode = axes_[axis].opcode;
    stop.wide = true;
    const int output_operand =
        stop.opcode == model::Opcode::kVariablePulse ? 1 : 2;
    stop.operands[output_operand] = {model::OperandKind::kBitDevice, output};
    return ExecuteMotionOperation(stop, index, false);
  }
  const int entry = ReadValue(instruction.operands[1], true);
  if (!positioning_table_enabled_ || entry < 1 || entry > 100) {
    if (!power)
      return true;
    SetError(
        "DTBL requires configured PLC positioning parameters and table 1..100");
    return false;
  }
  const auto opcode = positioning_entries_[axis][entry - 1].opcode;
  if (!PositioningMode(opcode)) {
    SetError("DTBL positioning type is not configured for this table entry");
    return false;
  }
  if (axes_[axis].owner == index && axes_[axis].opcode != opcode &&
      axes_[axis].status.busy) {
    SetError("DTBL mode cannot change while its pulse output is busy");
    return false;
  }
  model::OpenPLCInstruction operation;
  operation.opcode = opcode;
  operation.wide = true;
  auto address = positioning_table_head_;
  address.index += axis * 400 + (entry - 1) * 4;
  operation.operands[0] = {model::OperandKind::kWordDevice, address};
  if (opcode == model::Opcode::kVariablePulse) {
    operation.operand_count = 3;
    operation.operands[1] = {model::OperandKind::kBitDevice, output};
    operation.operands[2] = {model::OperandKind::kBitDevice,
                             table_directions_[axis]};
  } else {
    operation.operand_count = 4;
    address.index += 2;
    operation.operands[1] = {model::OperandKind::kWordDevice, address};
    operation.operands[2] = {model::OperandKind::kBitDevice, output};
    operation.operands[3] = {model::OperandKind::kBitDevice,
                             table_directions_[axis]};
  }
  return ExecuteMotionOperation(operation, index, power);
}

bool CompiledPLCExecutor::SetAbsoluteEncoder(int first_input,
                                             int32_t position) {
  if (first_input < 0 || first_input > 253)
    return false;
  encoder_configured_[first_input] = true;
  encoder_positions_[first_input] = position;
  return true;
}

bool CompiledPLCExecutor::ExecuteAbsoluteRead(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  const auto input = instruction.operands[0].device;
  const auto output = instruction.operands[1].device;
  const auto destination = instruction.operands[2].device;
  const auto last_input = model::OffsetBitAddress(input, 2);
  const auto last_output = model::OffsetBitAddress(output, 2);
  if (model::ParseDeviceAddress(model::FormatDeviceAddress(last_input)) !=
          std::optional(last_input) ||
      model::ParseDeviceAddress(model::FormatDeviceAddress(last_output)) !=
          std::optional(last_output) ||
      destination.index + 2 > WordMemory(destination).size()) {
    SetError(
        "DABS requires three input bits, three output bits and two destination "
        "words");
    return false;
  }
  auto& state = absolute_transfers_[index];
  const auto write = [&](int bit, bool value) {
    const auto device = model::OffsetBitAddress(output, bit);
    WriteBit(device, value);
    if (device.kind == model::DeviceKind::kY)
      physical_outputs_[device.index] = value;
  };
  if (!power) {
    state = {};
    for (int bit = 0; bit < 3; ++bit)
      write(bit, false);
    memory_.special_m[29] = false;
    return true;
  }
  if (!state.active) {
    state = {};
    state.active = true;
    write(0, true);
    write(1, true);
    write(2, false);
    memory_.special_m[29] = false;
    return true;
  }
  if (state.complete) {
    memory_.special_m[29] = true;
    return true;
  }
  // A configured virtual servo responds to the same request/ready handshake
  // used by externally driven input bits; it does not bypass the transfer.
  if (input.kind == model::DeviceKind::kX && encoder_configured_[input.index]) {
    bool ready = !state.waiting_data;
    uint8_t pair = 0;
    const uint32_t data =
        std::bit_cast<uint32_t>(encoder_positions_[input.index]);
    if (state.pairs < 16)
      pair = static_cast<uint8_t>((data >> (state.pairs * 2)) & 3);
    else {
      uint8_t checksum = 0;
      for (int shift = 0; shift < 32; shift += 2)
        checksum += (data >> shift) & 3;
      pair = static_cast<uint8_t>((checksum >> ((state.pairs - 16) * 2)) & 3);
    }
    physical_inputs_[input.index] = pair & 1;
    physical_inputs_[input.index + 1] = pair & 2;
    physical_inputs_[input.index + 2] = ready;
  }
  const auto read = [this](model::DeviceAddress device) {
    return device.kind == model::DeviceKind::kX ? physical_inputs_[device.index]
                                                : ReadBit(device);
  };
  const bool ready = read(last_input);
  if (!state.waiting_data) {
    if (ready) {
      write(2, true);
      state.waiting_data = true;
    }
    return true;
  }
  if (ready)
    return true;
  const uint8_t pair = static_cast<uint8_t>(
      static_cast<int>(read(input)) |
      (static_cast<int>(read(model::OffsetBitAddress(input, 1))) << 1));
  if (state.pairs < 16) {
    state.data |= uint32_t{pair} << (state.pairs * 2);
    state.checksum += pair;
  } else {
    state.received_checksum |= pair << ((state.pairs - 16) * 2);
  }
  ++state.pairs;
  write(2, false);
  state.waiting_data = false;
  if (state.pairs < 19)
    return true;
  if (state.checksum != state.received_checksum) {
    // Communication cannot establish a valid position. Keep SON enabled and
    // retry the transfer; the ladder's timeout can detect a missing completion.
    state.pairs = 0;
    state.data = 0;
    state.checksum = state.received_checksum = 0;
    return true;
  }
  if (!WriteWord(destination, std::bit_cast<int32_t>(state.data), true))
    return false;
  state.complete = true;
  write(1, false);
  memory_.special_m[29] = true;
  return true;
}
}  // namespace plc
