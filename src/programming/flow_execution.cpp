#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/project/gx_csv_codec.h"
#include "plc_emulator/project/instruction_codec.h"

#include <algorithm>
#include <array>
#include <chrono>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

struct LoopFrame {
  size_t start = 0;
  size_t next = 0;
  int remaining = 1;
};
}  // namespace

void CompiledPLCExecutor::BuildSourceSteps() {
  namespace model = plc_emulator::programming;
  instruction_step_numbers_.resize(program_.instructions.size());
  uint32_t step = 0;
  size_t index = 0;
  for (const auto& statement : model::SerializeInstructions(program_)) {
    std::string name = statement.mnemonic;
    const auto label = model::ParseOperand(name);
    const bool base = model::FindInstruction(name) ||
                      (name.size() > 1 && name.front() == 'D' &&
                       model::FindInstruction(name.substr(1)));
    if (!base && name.size() > 1 && name.back() == 'P')
      name.pop_back();
    const bool action =
        model::FindInstruction(name) ||
        (name.size() > 1 && name.front() == 'D' &&
         model::FindInstruction(name.substr(1))) ||
        (label && (label->kind == model::OperandKind::kPointer ||
                   label->kind == model::OperandKind::kInterruptPointer));
    if (action && index < instruction_step_numbers_.size())
      instruction_step_numbers_[index++] = step;
    step += model::GXInstructionStepCount(statement);
  }
}

bool CompiledPLCExecutor::ExecuteProgramScan(int* executed_instructions) {
  using model::Opcode;
  const size_t end = program_.instructions.size();
  std::array<LoopFrame, 5> loops{};
  size_t loop_depth = 0;
  size_t frame_depth = 0;
  bool in_interrupt = false;
  size_t position = 0;
  master_controls_.fill(true);
  master_level_ = -1;
  auto watchdog = std::chrono::steady_clock::now();
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  const auto reset_condition = [this] {
    evaluated_group_ = -1;
    evaluated_gate_count_ = 0;
  };
  const auto push = [&](size_t next, bool interrupt) {
    if (frame_depth == flow_frames_.size() ||
        (!interrupt && frame_depth >= (in_interrupt ? 6u : 5u)))
      return false;
    auto& frame = flow_frames_[frame_depth++];
    frame.return_position = next;
    frame.loop_depth = loop_depth;
    frame.interrupt = interrupt;
    frame.master_controls = master_controls_;
    frame.master_level = master_level_;
    frame.evaluation_group = evaluated_group_;
    frame.gate_count = evaluated_gate_count_;
    std::copy(gate_values_.begin(), gate_values_.end(), frame.gates.begin());
    return true;
  };
  // Timed interrupt pointers I6nn/I7nn/I8nn specify intervals in milliseconds.
  for (int pointer = 601; pointer < 900; ++pointer) {
    const int interval = pointer % 100;
    if (!interval || interrupt_targets_[pointer] >= end)
      continue;
    interrupt_elapsed_ms_[pointer] += std::min(current_elapsed_ms_, 32767);
    if (interrupt_elapsed_ms_[pointer] >= interval) {
      pending_interrupts_[pointer] = true;
      interrupt_elapsed_ms_[pointer] %= interval;
    }
  }
  std::fill(instruction_power_.begin(), instruction_power_.end(), 0);
  for (auto& gates : observed_gates_)
    std::fill(gates.begin(), gates.end(), 0);
  while (position < end) {
    if (*executed_instructions >= 1000000)
      return fail("Scan exceeded execution limit (possible infinite loop)");
    if ((*executed_instructions & 255) == 0 &&
        std::chrono::steady_clock::now() - watchdog >
            std::chrono::milliseconds{std::max<int>(1, memory_.D[8000])})
      return fail("PLC watchdog expired");
    if (interrupts_enabled_ && !in_interrupt) {
      const auto pending = std::find(pending_interrupts_.begin(),
                                     pending_interrupts_.end(), true);
      if (pending != pending_interrupts_.end()) {
        const size_t pointer = pending - pending_interrupts_.begin();
        if (!push(position, true))
          return fail("Interrupt stack overflow");
        pending_interrupts_[pointer] = false;
        in_interrupt = true;
        position = interrupt_targets_[pointer];
        reset_condition();
      }
    }
    if (!ExecuteStructuredInstruction(program_.instructions[position],
                                      position))
      return false;
    const auto& instruction = resolved_program_.instructions[position];
    ++*executed_instructions;
    const bool power = instruction_power_[position] != 0;
    switch (instruction.opcode) {
      case Opcode::kJump:
        if (power) {
          const int pointer = instruction.operands[0].immediate;
          const size_t target = pointer_targets_[pointer];
          const bool call = instruction.opcode == Opcode::kCall;
          if ((target == end && (call || pointer != 63)) ||
              (call ? target <= main_program_end_
                    : target > main_program_end_ && target != end))
            return fail(
                "Indexed pointer does not identify a valid routine or jump "
                "label");
          position = target;
          reset_condition();
          continue;
        }
        break;
      case Opcode::kCall:
        if (power) {
          if (!push(position + 1, false))
            return fail("CALL exceeds five nesting levels");
          const int pointer = instruction.operands[0].immediate;
          const size_t target = pointer_targets_[pointer];
          const bool call = instruction.opcode == Opcode::kCall;
          if ((target == end && (call || pointer != 63)) ||
              (call ? target <= main_program_end_
                    : target > main_program_end_ && target != end))
            return fail(
                "Indexed pointer does not identify a valid routine or jump "
                "label");
          position = target;
          reset_condition();
          continue;
        }
        break;
      case Opcode::kReturn:
      case Opcode::kInterruptReturn: {
        const bool interrupt = instruction.opcode == Opcode::kInterruptReturn;
        if (!frame_depth ||
            flow_frames_[frame_depth - 1].interrupt != interrupt)
          return fail("Routine return without matching CALL or interrupt");
        const auto& frame = flow_frames_[--frame_depth];
        if (loop_depth != frame.loop_depth)
          return fail("Routine returned with unfinished FOR/NEXT");
        position = frame.return_position;
        evaluated_group_ = frame.evaluation_group;
        evaluated_gate_count_ = frame.gate_count;
        master_controls_ = frame.master_controls;
        master_level_ = frame.master_level;
        std::copy(frame.gates.begin(), frame.gates.end(), gate_values_.begin());
        if (interrupt)
          in_interrupt = false;
        continue;
      }
      case Opcode::kFend:
        if (frame_depth || loop_depth)
          return fail("FEND reached inside an active routine or loop");
        return true;
      case Opcode::kFor:
        if (loop_depth == loops.size())
          return fail("FOR/NEXT exceeds five nesting levels");
        loops[loop_depth++] = {position + 1, flow_targets_[position],
                               std::max(1, ReadValue(instruction.operands[0]))};
        break;
      case Opcode::kNext:
        if (!loop_depth || loops[loop_depth - 1].next != position ||
            (frame_depth &&
             loop_depth <= flow_frames_[frame_depth - 1].loop_depth))
          return fail("NEXT entered without executing its FOR");
        if (--loops[loop_depth - 1].remaining > 0) {
          position = loops[loop_depth - 1].start;
          reset_condition();
          continue;
        }
        --loop_depth;
        break;
      case Opcode::kEnableInterrupts:
        interrupts_enabled_ = true;
        break;
      case Opcode::kDisableInterrupts:
        interrupts_enabled_ = false;
        break;
      case Opcode::kWatchdog:
        if (power)
          watchdog = std::chrono::steady_clock::now();
        break;
      case Opcode::kMasterControl: {
        const int level = instruction.operands[0].immediate;
        if (level != master_level_ + 1)
          return fail("MC nesting levels must increase in order");
        master_controls_[level] = power;
        master_level_ = level;
        WriteBit(instruction.operands[1].device, power);
        break;
      }
      case Opcode::kMasterReset: {
        const int level = instruction.operands[0].immediate;
        if (level > master_level_)
          return fail("MCR without matching MC level");
        for (int cleared = level; cleared < 8; ++cleared)
          master_controls_[cleared] = true;
        master_level_ = level - 1;
        break;
      }
      default:
        break;
    }
    ++position;
  }
  return frame_depth || loop_depth
             ? fail("END reached with unfinished routine or loop")
             : true;
}
}  // namespace plc
