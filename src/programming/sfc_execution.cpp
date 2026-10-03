#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

bool IsStepOutput(const model::OpenPLCInstruction& instruction) {
  return instruction.opcode == model::Opcode::kOut &&
         instruction.operands[0].device.kind == model::DeviceKind::kY;
}
}  // namespace

void CompiledPLCExecutor::InitializeSteps() {
  const size_t size = program_.instructions.size();
  instruction_steps_.assign(size, -1);
  step_output_slots_.assign(size, SIZE_MAX);
  step_output_values_.assign(size, 0);
  step_output_peers_.assign(size, {});
  step_previous_.fill(false);
  step_pending_reset_.fill(false);
  step_state_ = -1;
  int state = -1;
  for (size_t index = 0; index < size; ++index) {
    const auto& instruction = program_.instructions[index];
    if (instruction.opcode == model::Opcode::kStep)
      state = static_cast<int>(instruction.operands[0].device.index);
    else if (instruction.opcode == model::Opcode::kStepEnd)
      state = -1;
    else
      instruction_steps_[index] = state;
    if (state < 0 || !IsStepOutput(instruction))
      continue;
    size_t slot = index;
    for (size_t previous = 0; previous < index; ++previous)
      if (instruction_steps_[previous] == state &&
          step_output_slots_[previous] != SIZE_MAX &&
          program_.instructions[previous].operands[0].device ==
              instruction.operands[0].device) {
        slot = step_output_slots_[previous];
        break;
      }
    step_output_slots_[index] = slot;
  }
  for (size_t index = 0; index < size; ++index) {
    if (step_output_slots_[index] != index)
      continue;
    for (size_t peer = 0; peer < size; ++peer)
      if (step_output_slots_[peer] == peer &&
          program_.instructions[peer].operands[0].device ==
              program_.instructions[index].operands[0].device)
        step_output_peers_[index].push_back(peer);
  }
}

void CompiledPLCExecutor::BeginStepScan() {
  step_state_ = -1;
  step_active_ = step_cleanup_ = false;
  for (size_t state = 0; state < step_pending_reset_.size(); ++state)
    if (step_pending_reset_[state]) {
      memory_.S[state] = false;
      step_pending_reset_[state] = false;
    }
}

void CompiledPLCExecutor::WriteStepOutput(size_t index, bool value) {
  const size_t slot = step_output_slots_[index];
  step_output_values_[slot] = value;
  bool combined = false;
  for (const size_t peer : step_output_peers_[slot])
    combined = combined || step_output_values_[peer];
  WriteBit(resolved_program_.instructions[index].operands[0].device, combined);
}

void CompiledPLCExecutor::FinishStepScan() {
  memory_.special_m[46] = false;
  if (!memory_.special_m[47])
    return;
  std::fill(memory_.D + 8040, memory_.D + 8048, -1);
  int slot = 0;
  for (int state = 0; state < 4096; ++state) {
    if ((state >= 900 && state <= 999) || !memory_.S[state] ||
        step_pending_reset_[state])
      continue;
    memory_.special_m[46] = true;
    if (slot < 8)
      memory_.D[8040 + slot++] = static_cast<int16_t>(state);
  }
}
}  // namespace plc
