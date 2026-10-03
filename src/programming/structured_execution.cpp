#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <bit>
#include <limits>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::ReadBit(model::DeviceAddress device) const {
  if (device.index_kind) {
    model::Operand operand{model::OperandKind::kBitDevice, device};
    return ResolveOperand(&operand, false) && ReadBit(operand.device);
  }
  const uint32_t index = device.index;
  if (device.bit >= 0 && device.kind == model::DeviceKind::kD && index < 8512)
    return (static_cast<uint16_t>(memory_.D[index]) &
            (uint16_t{1} << device.bit)) != 0;
  switch (device.kind) {
    case model::DeviceKind::kX:
      return index < 256 && memory_.X[index];
    case model::DeviceKind::kY:
      return index < 256 && memory_.Y[index];
    case model::DeviceKind::kM:
      return index < 7680 && memory_.M[index];
    case model::DeviceKind::kS:
      return index < 4096 && memory_.S[index] && !step_pending_reset_[index];
    case model::DeviceKind::kSpecialM:
      return index < 512 && memory_.special_m[index];
    case model::DeviceKind::kT:
      return index < 512 && timer_contacts_[index];
    case model::DeviceKind::kC:
      return index < 256 && counter_contacts_[index];
    case model::DeviceKind::kD:
    case model::DeviceKind::kR:
    case model::DeviceKind::kV:
    case model::DeviceKind::kZ:
    case model::DeviceKind::kBufferMemory:
      return false;
  }
  return false;
}

void CompiledPLCExecutor::WriteBit(model::DeviceAddress device, bool value) {
  const uint32_t index = device.index;
  if (device.bit >= 0 && device.kind == model::DeviceKind::kD && index < 8512) {
    const uint16_t mask = uint16_t{1} << device.bit;
    const uint16_t bits = static_cast<uint16_t>(memory_.D[index]);
    memory_.D[index] = std::bit_cast<int16_t>(
        static_cast<uint16_t>(value ? bits | mask : bits & ~mask));
    return;
  }
  switch (device.kind) {
    case model::DeviceKind::kX:
      if (index < 256)
        memory_.X[index] = value;
      break;
    case model::DeviceKind::kY:
      if (index < 256)
        memory_.Y[index] = value;
      break;
    case model::DeviceKind::kM:
      if (index < 7680)
        memory_.M[index] = value;
      break;
    case model::DeviceKind::kS:
      if (index < 4096)
        memory_.S[index] = value;
      break;
    case model::DeviceKind::kSpecialM:
      if (index < 512)
        memory_.special_m[index] = value;
      break;
    default:
      break;
  }
}

int32_t CompiledPLCExecutor::ReadValue(const model::Operand& operand,
                                       bool wide) const {
  if (operand.device.index_kind) {
    auto resolved = operand;
    return ResolveOperand(&resolved, wide) ? ReadValue(resolved, wide) : 0;
  }
  if (operand.kind == model::OperandKind::kPackedBit) {
    uint32_t bits = 0;
    auto source = operand.device;
    for (int bit = 0; bit < operand.immediate * 4; ++bit) {
      if (ReadBit(source))
        bits |= uint32_t{1} << bit;
      ++source.index;
    }
    return wide ? std::bit_cast<int32_t>(bits)
                : std::bit_cast<int16_t>(static_cast<uint16_t>(bits));
  }
  if (operand.kind == model::OperandKind::kImmediateDecimal ||
      operand.kind == model::OperandKind::kImmediateHex) {
    return wide ? operand.immediate
                : std::bit_cast<int16_t>(
                      static_cast<uint16_t>(operand.immediate));
  }
  const uint32_t index = operand.device.index;
  switch (operand.device.kind) {
    case model::DeviceKind::kZ:
    case model::DeviceKind::kV:
      if (index >= 8)
        return 0;
      if (wide)
        return std::bit_cast<int32_t>(
            static_cast<uint32_t>(static_cast<uint16_t>(memory_.Z[index])) |
            (static_cast<uint32_t>(static_cast<uint16_t>(memory_.V[index]))
             << 16));
      return operand.device.kind == model::DeviceKind::kZ ? memory_.Z[index]
                                                          : memory_.V[index];
    case model::DeviceKind::kD:
    case model::DeviceKind::kR:
    case model::DeviceKind::kBufferMemory: {
      if (operand.device.bit >= 0)
        return ReadBit(operand.device) ? 1 : 0;
      const auto storage = WordMemory(operand.device);
      if (index >= storage.size() || (wide && index + 1 >= storage.size()))
        return 0;
      if (!wide)
        return storage[index];
      return std::bit_cast<int32_t>(
          static_cast<uint32_t>(static_cast<uint16_t>(storage[index])) |
          (static_cast<uint32_t>(static_cast<uint16_t>(storage[index + 1]))
           << 16));
    }
    case model::DeviceKind::kT:
      if (index >= 512 || (wide && index + 1 >= 512))
        return 0;
      if (!wide)
        return std::bit_cast<int16_t>(static_cast<uint16_t>(memory_.T[index]));
      return std::bit_cast<int32_t>(
          static_cast<uint32_t>(static_cast<uint16_t>(memory_.T[index])) |
          (static_cast<uint32_t>(static_cast<uint16_t>(memory_.T[index + 1]))
           << 16));
    case model::DeviceKind::kC:
      if (index >= 256 || (wide && index == 199))
        return 0;
      if (index >= 200)
        return memory_.C[index];
      if (!wide)
        return std::bit_cast<int16_t>(static_cast<uint16_t>(memory_.C[index]));
      return std::bit_cast<int32_t>(
          static_cast<uint32_t>(static_cast<uint16_t>(memory_.C[index])) |
          (static_cast<uint32_t>(static_cast<uint16_t>(memory_.C[index + 1]))
           << 16));
    default:
      return ReadBit(operand.device) ? 1 : 0;
  }
}

bool CompiledPLCExecutor::WriteWord(model::DeviceAddress device, int64_t value,
                                    bool wide) {
  const uint32_t index = device.index;
  const uint32_t raw = static_cast<uint32_t>(value);
  if (device.kind == model::DeviceKind::kT) {
    if (index >= 512 || (wide && index == 511)) {
      SetError("Timer word destination exceeds memory");
      return false;
    }
    memory_.T[index] = std::bit_cast<int16_t>(static_cast<uint16_t>(raw));
    timer_remainders_ms_[index] = 0;
    if (wide) {
      memory_.T[index + 1] =
          std::bit_cast<int16_t>(static_cast<uint16_t>(raw >> 16));
      timer_remainders_ms_[index + 1] = 0;
    }
    return true;
  }
  if (device.kind == model::DeviceKind::kC) {
    if (index >= 256 || (wide && index == 199) || (!wide && index >= 200)) {
      SetError("Counter word destination exceeds memory or word width");
      return false;
    }
    if (index >= 200)
      memory_.C[index] = std::bit_cast<int32_t>(raw);
    else {
      memory_.C[index] = std::bit_cast<int16_t>(static_cast<uint16_t>(raw));
      if (wide)
        memory_.C[index + 1] =
            std::bit_cast<int16_t>(static_cast<uint16_t>(raw >> 16));
    }
    return true;
  }
  if (wide && (device.kind == model::DeviceKind::kV ||
               device.kind == model::DeviceKind::kZ)) {
    if (index >= 8) {
      SetError("Index register destination exceeds memory");
      return false;
    }
    memory_.Z[index] = std::bit_cast<int16_t>(static_cast<uint16_t>(raw));
    memory_.V[index] = std::bit_cast<int16_t>(static_cast<uint16_t>(raw >> 16));
    return true;
  }
  auto storage = WordMemory(device);
  if (device.bit >= 0 || index >= storage.size() ||
      (wide && index + 1 >= storage.size())) {
    SetError("Word destination exceeds register memory");
    return false;
  }
  storage[index] = std::bit_cast<int16_t>(static_cast<uint16_t>(raw));
  if (wide)
    storage[index + 1] =
        std::bit_cast<int16_t>(static_cast<uint16_t>(raw >> 16));
  return true;
}

bool CompiledPLCExecutor::WriteValue(const model::Operand& operand,
                                     int64_t value, bool wide) {
  if (operand.device.index_kind) {
    auto resolved = operand;
    return ResolveOperand(&resolved, wide) && WriteValue(resolved, value, wide);
  }
  if (operand.kind != model::OperandKind::kPackedBit)
    return WriteWord(operand.device, value, wide);
  const uint32_t bits = static_cast<uint32_t>(value);
  auto destination = operand.device;
  for (int bit = 0; bit < operand.immediate * 4; ++bit) {
    WriteBit(destination, ((bits >> bit) & 1) != 0);
    ++destination.index;
  }
  return true;
}

bool CompiledPLCExecutor::ExecuteStructuredInstruction(
    const model::OpenPLCInstruction& source_instruction, size_t index) {
  auto& instruction = resolved_program_.instructions[index];
  using model::GateKind;
  using model::Opcode;
  if (step_state_ >= 0 && !step_active_ && !step_cleanup_ &&
      instruction.opcode != Opcode::kStep &&
      instruction.opcode != Opcode::kStepEnd)
    return true;
  const size_t begin = instruction.evaluation_group >= 0 &&
                               instruction.evaluation_group == evaluated_group_
                           ? evaluated_gate_count_
                           : instruction.condition_begin;
  for (size_t gate_index = begin; gate_index < instruction.condition.size();
       ++gate_index) {
    const model::LogicGate& gate = instruction.condition[gate_index];
    bool value = false;
    auto first_operand = gate.first;
    auto second_operand = gate.second;
    if (!ResolveOperand(&first_operand, gate.wide) ||
        !ResolveOperand(&second_operand, gate.wide)) {
      SetError("Indexed contact exceeds device range");
      return false;
    }
    switch (gate.kind) {
      case GateKind::kConstant:
        value = gate.constant;
        break;
      case GateKind::kContact:
        value = ReadBit(first_operand.device);
        break;
      case GateKind::kNot:
        value = !gate_values_[gate.left];
        break;
      case GateKind::kAnd:
        value = gate_values_[gate.left] && gate_values_[gate.right];
        break;
      case GateKind::kOr:
        value = gate_values_[gate.left] || gate_values_[gate.right];
        break;
      case GateKind::kRising:
      case GateKind::kFalling: {
        const bool current = gate_values_[gate.left];
        const bool previous = edge_states_[index][gate_index];
        value = gate.kind == GateKind::kRising ? current && !previous
                                               : !current && previous;
        edge_states_[index][gate_index] = current;
        break;
      }
      default: {
        const int32_t first = ReadValue(first_operand, gate.wide);
        const int32_t second = ReadValue(second_operand, gate.wide);
        switch (gate.kind) {
          case GateKind::kEqual:
            value = first == second;
            break;
          case GateKind::kNotEqual:
            value = first != second;
            break;
          case GateKind::kGreater:
            value = first > second;
            break;
          case GateKind::kLess:
            value = first < second;
            break;
          case GateKind::kGreaterEqual:
            value = first >= second;
            break;
          case GateKind::kLessEqual:
            value = first <= second;
            break;
          default:
            return false;
        }
      }
    }
    gate_values_[gate_index] = value;
  }
  evaluated_group_ = instruction.evaluation_group;
  evaluated_gate_count_ = instruction.condition.size();
  const size_t root = instruction.condition_root == UINT32_MAX
                          ? instruction.condition.size() - 1
                          : instruction.condition_root;
  bool power = gate_values_[root];
  if (step_state_ >= 0 && instruction.opcode != Opcode::kStep &&
      instruction.opcode != Opcode::kStepEnd)
    power = power && step_active_;
  for (int level = 0; level <= master_level_; ++level)
    power = power && master_controls_[level];
  const bool previous_power = pulse_states_[index];
  pulse_states_[index] = power;
  if (instruction.pulse)
    power = power && !previous_power;
  instruction_power_[index] = power;
  std::copy_n(gate_values_.begin(), instruction.condition.size(),
              observed_gates_[index].begin());
  memory_.accumulator = power;
  for (size_t operand = 0; operand < source_instruction.operand_count;
       ++operand) {
    instruction.operands[operand].device =
        source_instruction.operands[operand].device;
    instruction.operands[operand].immediate =
        source_instruction.operands[operand].immediate;
    bool operand_wide = source_instruction.wide;
    if (instruction.operands[operand].kind == model::OperandKind::kWordDevice &&
        source_instruction.opcode >= Opcode::kToFloat &&
        source_instruction.opcode <= Opcode::kFloatDegrees) {
      operand_wide = true;
      if ((source_instruction.opcode == Opcode::kToFloat && operand == 0) ||
          (source_instruction.opcode == Opcode::kFromFloat && operand == 1))
        operand_wide = source_instruction.wide;
    }
    if (!ResolveOperand(&instruction.operands[operand], operand_wide)) {
      if (!power)
        return true;
      SetError("Indexed operand exceeds device range");
      return false;
    }
  }
  if (instruction.opcode == Opcode::kStep) {
    const int state = static_cast<int>(instruction.operands[0].device.index);
    if (index == 0 ||
        program_.instructions[index - 1].opcode != Opcode::kStep) {
      step_source_count_ = 0;
      step_active_ = step_group_previous_ = true;
    }
    if (step_source_count_ == step_sources_.size()) {
      SetError("STL parallel recombination exceeds eight states");
      return false;
    }
    step_sources_[step_source_count_++] = state;
    step_state_ = state;
    step_active_ = step_active_ && memory_.S[state];
    step_group_previous_ = step_group_previous_ && step_previous_[state];
    step_cleanup_ = !step_active_ && step_group_previous_;
    step_previous_[state] = memory_.S[state];
    evaluated_group_ = -1;
    return true;
  }
  if (instruction.opcode == Opcode::kStepEnd) {
    step_state_ = -1;
    step_active_ = step_cleanup_ = false;
    evaluated_group_ = -1;
    return true;
  }
  if (instruction.opcode >= Opcode::kLabel &&
      instruction.opcode <= Opcode::kNext)
    return true;  // The scan controller applies flow after observing its input.
  if (instruction.opcode == Opcode::kMasterControl ||
      instruction.opcode == Opcode::kMasterReset ||
      instruction.opcode == Opcode::kNop)
    return true;
  const auto& operands = instruction.operands;
  const model::DeviceAddress target = operands[0].device;
  if (step_state_ >= 0 && step_output_slots_[index] != SIZE_MAX) {
    if (instruction.opcode == Opcode::kOut || step_cleanup_)
      WriteStepOutput(index, power);
    else if (power)
      WriteStepOutput(index, instruction.opcode == Opcode::kSet);
    return true;
  }
  if (step_state_ >= 0 && target.kind == model::DeviceKind::kS &&
      (instruction.opcode == Opcode::kSet ||
       instruction.opcode == Opcode::kOut)) {
    if (power && !memory_.special_m[40]) {
      memory_.S[target.index] = true;
      for (size_t source = 0; source < step_source_count_; ++source)
        if (target.index != static_cast<uint32_t>(step_sources_[source]))
          step_pending_reset_[step_sources_[source]] = true;
    }
    return true;
  }
  if (instruction.opcode == Opcode::kPulseRise ||
      instruction.opcode == Opcode::kPulseFall) {
    bool pulse = instruction.opcode == Opcode::kPulseRise
                     ? power && !previous_power
                     : !power && previous_power;
    for (int level = 0; level <= master_level_; ++level)
      pulse = pulse && master_controls_[level];
    WriteBit(target, pulse);
    return true;
  }
  if (instruction.opcode == Opcode::kOut) {
    WriteBit(target, power);
    return true;
  }
  if (instruction.opcode == Opcode::kTimerOn ||
      instruction.opcode == Opcode::kCounterUp) {
    ParsedInstruction numeric;
    numeric.type = instruction.opcode == Opcode::kTimerOn
                       ? ParsedInstruction::PLC_TON
                       : ParsedInstruction::PLC_CTU;
    numeric.index = static_cast<int>(target.index);
    const bool wide_preset =
        instruction.opcode == Opcode::kCounterUp && target.index >= 200;
    const int32_t preset = ReadValue(operands[1], wide_preset);
    if (!wide_preset && preset < 0) {
      SetError("Timer or 16-bit counter preset is negative");
      return false;
    }
    numeric.preset = preset;
    return ExecuteInstruction(numeric);
  }
  if (instruction.opcode == Opcode::kInitialState)
    return ExecuteInitialState(instruction, power);
  if (instruction.opcode >= Opcode::kCfCreate &&
      instruction.opcode <= Opcode::kCfStatus)
    return ExecuteCfOperation(instruction, index, power);
  if (instruction.opcode == Opcode::kModbus)
    return ExecuteModbusOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kInverterCheck &&
      instruction.opcode <= Opcode::kInverterMulti)
    return ExecuteInverterOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kSerial &&
      instruction.opcode <= Opcode::kSerial2)
    return ExecuteSerialOperation(instruction, index, power);
  if (instruction.opcode == Opcode::kPrint)
    return ExecutePanelOperation(instruction, index, power);
  if (instruction.opcode == Opcode::kPid)
    return ExecutePidOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kPulseOutput)
    return ExecuteMotionOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kAbsoluteDrum)
    return ExecuteDrumOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kTenKey)
    return ExecutePanelOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kRefreshIo)
    return ExecuteHighSpeedOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kIndexPush)
    return ExecuteStorageOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kAnnunciatorSet)
    return ExecuteHandyOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kReadModule)
    return ExecuteModuleOperation(instruction, index, power);
  if (instruction.opcode >= Opcode::kTimeCompare)
    return ExecuteUtilityOperation(instruction, index, power);
  if (!power && instruction.opcode == Opcode::kTableSort)
    return ExecuteSortOperation(instruction, false);
  if (!power)
    return true;
  if (instruction.opcode >= Opcode::kTableSearch)
    return ExecuteTableOperation(instruction);
  if (instruction.opcode >= Opcode::kStringMove)
    return ExecuteStringOperation(instruction);
  if (instruction.opcode >= Opcode::kToFloat)
    return ExecuteFloatOperation(instruction);
  if (instruction.opcode >= Opcode::kComplement)
    return ExecuteCpuOperation(instruction);
  if (instruction.opcode >= Opcode::kRotateRight)
    return ExecuteDataOperation(instruction);
  switch (instruction.opcode) {
    case Opcode::kSet:
      WriteBit(target, true);
      return true;
    case Opcode::kReset:
      if (model::IsRegisterDevice(target))
        return WriteWord(target, 0);
      if (target.kind == model::DeviceKind::kT ||
          target.kind == model::DeviceKind::kC) {
        ParsedInstruction numeric;
        numeric.type = target.kind == model::DeviceKind::kT
                           ? ParsedInstruction::PLC_RST_T
                           : ParsedInstruction::PLC_RST_C;
        numeric.index = static_cast<int>(target.index);
        return ExecuteInstruction(numeric);
      }
      WriteBit(target, false);
      return true;
    case Opcode::kAlternate:
      WriteBit(target, !ReadBit(target));
      return true;
    case Opcode::kMov:
      return WriteValue(operands[1], ReadValue(operands[0], instruction.wide),
                        instruction.wide);
    case Opcode::kInc:
    case Opcode::kDec:
      return WriteValue(
          operands[0],
          static_cast<int64_t>(ReadValue(operands[0], instruction.wide)) +
              (instruction.opcode == Opcode::kInc ? 1 : -1),
          instruction.wide);
    case Opcode::kCompare:
    case Opcode::kZoneCompare: {
      const int32_t first = ReadValue(operands[0], instruction.wide);
      const int32_t second = ReadValue(operands[1], instruction.wide);
      const int32_t tested = instruction.opcode == Opcode::kCompare
                                 ? first
                                 : ReadValue(operands[2], instruction.wide);
      model::DeviceAddress result =
          operands[instruction.opcode == Opcode::kCompare ? 2 : 3].device;
      const std::array<bool, 3> values =
          instruction.opcode == Opcode::kCompare
              ? std::array<bool, 3>{first > second, first == second,
                                    first < second}
              : std::array<bool, 3>{tested < first,
                                    tested >= first && tested <= second,
                                    tested > second};
      for (const bool value : values) {
        WriteBit(result, value);
        result = model::OffsetBitAddress(result, 1);
      }
      return true;
    }
    case Opcode::kZeroReset: {
      model::DeviceAddress cursor = target;
      if (cursor.kind != operands[1].device.kind ||
          cursor.index > operands[1].device.index)
        return false;
      for (; cursor.index <= operands[1].device.index; ++cursor.index) {
        if (model::IsRegisterDevice(cursor)) {
          if (!WriteWord(cursor, 0))
            return false;
        } else if (cursor.kind == model::DeviceKind::kT ||
                   cursor.kind == model::DeviceKind::kC) {
          ParsedInstruction numeric;
          numeric.type = cursor.kind == model::DeviceKind::kT
                             ? ParsedInstruction::PLC_RST_T
                             : ParsedInstruction::PLC_RST_C;
          numeric.index = static_cast<int>(cursor.index);
          if (!ExecuteInstruction(numeric))
            return false;
        } else
          WriteBit(cursor, false);
      }
      return true;
    }
    case Opcode::kBlockMove:
    case Opcode::kFillMove: {
      const int32_t count = ReadValue(operands[2]);
      const uint32_t source = operands[0].device.index;
      const uint32_t destination = operands[1].device.index;
      auto destination_memory = WordMemory(operands[1].device);
      const auto source_memory = WordMemory(operands[0].device);
      if (count < 0 || count > 32767 ||
          destination + count > destination_memory.size() ||
          (instruction.opcode == Opcode::kBlockMove &&
           source + count > source_memory.size())) {
        SetError("Block operation exceeds register memory");
        return false;
      }
      if (instruction.opcode == Opcode::kBlockMove) {
        if (source_memory.data() == destination_memory.data() &&
            destination > source) {
          std::copy_backward(source_memory.begin() + source,
                             source_memory.begin() + source + count,
                             destination_memory.begin() + destination + count);
        } else {
          std::copy(source_memory.begin() + source,
                    source_memory.begin() + source + count,
                    destination_memory.begin() + destination);
        }
      } else {
        std::fill_n(destination_memory.begin() + destination, count,
                    static_cast<int16_t>(ReadValue(operands[0])));
      }
      return true;
    }
    default:
      break;
  }
  const int64_t first = ReadValue(operands[0], instruction.wide);
  const int64_t second = ReadValue(operands[1], instruction.wide);
  int64_t result = 0;
  switch (instruction.opcode) {
    case Opcode::kAdd:
      result = first + second;
      break;
    case Opcode::kSub:
      result = first - second;
      break;
    case Opcode::kWordAnd:
      result = first & second;
      break;
    case Opcode::kWordOr:
      result = first | second;
      break;
    case Opcode::kWordXor:
      result = first ^ second;
      break;
    case Opcode::kMul: {
      if (!instruction.wide)
        return WriteValue(operands[2], first * second, true);
      model::DeviceAddress upper = operands[2].device;
      upper.index += 2;
      if (upper.index + 1 >= model::WordDeviceLimit(upper))
        return false;
      const uint64_t product = static_cast<uint64_t>(first * second);
      return WriteValue(operands[2], static_cast<uint32_t>(product), true) &&
             WriteWord(upper, static_cast<uint32_t>(product >> 32), true);
    }
    case Opcode::kDiv: {
      if (second == 0) {
        SetError("Division by zero");
        return false;
      }
      model::DeviceAddress remainder = operands[2].device;
      remainder.index += instruction.wide ? 2 : 1;
      if (remainder.index + (instruction.wide ? 1 : 0) >=
          model::WordDeviceLimit(remainder))
        return false;
      return WriteValue(operands[2], first / second, instruction.wide) &&
             WriteWord(remainder, first % second, instruction.wide);
    }
    default:
      return false;
  }
  return WriteValue(operands[2], result, instruction.wide);
}
bool CompiledPLCExecutor::ExecuteDataOperation(
    const model::OpenPLCInstruction& instruction) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  const bool rotate = opcode <= Opcode::kRotateCarryLeft;
  const bool shifted =
      opcode >= Opcode::kBitShiftRight && opcode <= Opcode::kWordShiftLeft;
  const int32_t count =
      opcode == Opcode::kSumBits
          ? 0
          : ReadValue(operands[rotate ? 1 : 2], instruction.wide);
  const int32_t shift = shifted ? ReadValue(operands[3]) : 0;
  std::string error;
  if (!model::ValidateDataOperation(instruction, count, shift, &error)) {
    SetError(error);
    return false;
  }
  if (rotate) {
    const int width = instruction.wide ? 32 : 16;
    const uint32_t mask = instruction.wide ? UINT32_MAX : 0xffffu;
    uint32_t value =
        static_cast<uint32_t>(ReadValue(operands[0], instruction.wide)) & mask;
    const bool left =
        opcode == Opcode::kRotateLeft || opcode == Opcode::kRotateCarryLeft;
    const bool carry_rotation = opcode == Opcode::kRotateCarryRight ||
                                opcode == Opcode::kRotateCarryLeft;
    bool carry = memory_.special_m[22];
    for (int bit = 0; bit < count; ++bit) {
      const bool outgoing =
          left ? (value >> (width - 1)) != 0 : (value & 1) != 0;
      const bool incoming = carry_rotation ? carry : outgoing;
      value = left ? ((value << 1) | static_cast<uint32_t>(incoming)) & mask
                   : (value >> 1) |
                         (static_cast<uint32_t>(incoming) << (width - 1));
      carry = outgoing;
    }
    if (count > 0)
      memory_.special_m[22] = carry;
    return WriteValue(operands[0], value, instruction.wide);
  }
  if (opcode == Opcode::kSumBits) {
    const uint32_t value =
        static_cast<uint32_t>(ReadValue(operands[0], instruction.wide)) &
        (instruction.wide ? UINT32_MAX : 0xffffu);
    memory_.special_m[20] = value == 0;
    return WriteValue(operands[1], std::popcount(value), instruction.wide);
  }
  if (opcode == Opcode::kBitTest) {
    const uint32_t value =
        static_cast<uint32_t>(ReadValue(operands[0], instruction.wide));
    WriteBit(operands[1].device, ((value >> count) & 1) != 0);
    return true;
  }
  if (opcode == Opcode::kDecode || opcode == Opcode::kEncode) {
    if (count == 0)
      return true;
    uint32_t value = 0;
    if (opcode == Opcode::kDecode) {
      if (operands[0].kind != model::OperandKind::kBitDevice)
        value = static_cast<uint32_t>(ReadValue(operands[0]));
      else {
        auto source = operands[0].device;
        for (int bit = 0; bit < count; ++bit, ++source.index)
          value |= static_cast<uint32_t>(ReadBit(source)) << bit;
      }
      const uint32_t selected = value & ((1u << count) - 1);
      if (model::IsRegisterDevice(operands[1].device))
        return WriteValue(operands[1], uint64_t{1} << selected);
      auto destination = operands[1].device;
      for (uint32_t bit = 0; bit < (1u << count); ++bit, ++destination.index)
        WriteBit(destination, bit == selected);
      return true;
    }
    auto source = operands[0].device;
    const uint32_t word = static_cast<uint32_t>(ReadValue(operands[0]));
    for (uint32_t bit = 0; bit < (1u << count); ++bit, ++source.index) {
      const bool active = model::IsRegisterDevice(operands[0].device)
                              ? ((word >> bit) & 1) != 0
                              : ReadBit(source);
      if (active)
        value = bit;
    }
    return WriteValue(operands[1], value);
  }
  if (shifted) {
    const bool word = opcode >= Opcode::kWordShiftRight;
    const bool left =
        opcode == Opcode::kBitShiftLeft || opcode == Opcode::kWordShiftLeft;
    // Capture source before moving, including overlapping device ranges.
    std::array<int32_t, 1024> source_values{};
    auto source = operands[0].device;
    for (int index = 0; index < shift; ++index, ++source.index)
      source_values[index] =
          word ? WordMemory(source)[source.index] : ReadBit(source);
    const auto read = [this, word](model::DeviceAddress device) -> int32_t {
      return word ? WordMemory(device)[device.index] : ReadBit(device);
    };
    const auto write = [this, word](model::DeviceAddress device,
                                    int32_t value) {
      if (word)
        WriteWord(device, value);
      else
        WriteBit(device, value != 0);
    };
    const auto destination = operands[1].device;
    if (left) {
      for (int index = count - 1; index >= shift; --index) {
        auto from = destination;
        auto to = destination;
        from.index += index - shift;
        to.index += index;
        write(to, read(from));
      }
    } else {
      for (int index = 0; index < count - shift; ++index) {
        auto from = destination;
        auto to = destination;
        from.index += index + shift;
        to.index += index;
        write(to, read(from));
      }
    }
    for (int index = 0; index < shift; ++index) {
      auto to = destination;
      to.index += left ? index : count - shift + index;
      write(to, source_values[index]);
    }
    return true;
  }
  return false;
}
}  // namespace plc
