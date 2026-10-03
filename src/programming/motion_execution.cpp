#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

model::DeviceAddress Register(int index) {
  return {model::DeviceKind::kD, static_cast<uint32_t>(index)};
}

bool IsPulseCounter(model::Opcode opcode) {
  return opcode == model::Opcode::kPulseOutput ||
         opcode == model::Opcode::kPulseRamp;
}
}  // namespace

std::optional<CompiledPLCExecutor::AxisSnapshot>
CompiledPLCExecutor::GetAxisState(int axis) const {
  if (axis < 0 || axis >= 4)
    return std::nullopt;
  auto result = axes_[axis].status;
  result.position = GetWordValue(Register(8340 + axis * 10), true);
  return result;
}

void CompiledPLCExecutor::InitializeAxes(bool initialize_registers) {
  for (int axis = 0; axis < 4; ++axis) {
    if (axes_[axis].owner != SIZE_MAX) {
      memory_.Y[axis] = false;
      physical_outputs_[axis] = false;
    }
    if (axes_[axis].clear_remaining_ms > 0) {
      memory_.Y[axes_[axis].clear_output] = false;
      physical_outputs_[axes_[axis].clear_output] = false;
    }
  }
  axes_ = {};
  for (int axis = 0; axis < 4; ++axis) {
    const int base = 8340 + axis * 10;
    memory_.special_m[base - 8000] = false;
    memory_.special_m[base - 8000 + 8] = false;
    if (!initialize_registers)
      continue;
    WriteWord(Register(base + 3), 100000, true);
    memory_.D[base + 5] = 1000;
    WriteWord(Register(base + 6), 50000, true);
    memory_.D[base + 8] = 100;
    memory_.D[base + 9] = 100;
  }
  if (initialize_registers && positioning_table_enabled_)
    for (int axis = 0; axis < 4; ++axis)
      for (int entry = 0; entry < 100; ++entry) {
        const auto data = positioning_entries_[axis][entry];
        if ((data.opcode >= model::Opcode::kVariablePulse &&
             data.opcode <= model::Opcode::kAbsolutePosition) ||
            data.opcode == model::Opcode::kInterruptPosition)
          SetPositioningEntry(axis, entry + 1, data.opcode, data.pulses,
                              data.frequency);
      }
}

void CompiledPLCExecutor::FinishAxis(int axis, bool abnormal) {
  auto& state = axes_[axis];
  state.status.busy = false;
  state.status.frequency = 0;
  state.status.complete = !abnormal;
  state.status.abnormal = abnormal;
  memory_.special_m[340 + axis * 10] = false;
  memory_.Y[axis] = false;
  physical_outputs_[axis] = false;
  if (!abnormal &&
      (state.opcode == model::Opcode::kDogSearch ||
       state.opcode == model::Opcode::kZeroReturn) &&
      memory_.special_m[341 + axis * 10]) {
    state.clear_remaining_ms = 20 + current_elapsed_ms_;
    memory_.Y[state.clear_output] = true;
    physical_outputs_[state.clear_output] = true;
  }
  if (abnormal)
    memory_.special_m[329] = true;
  else
    memory_.special_m[29] = true;
}

void CompiledPLCExecutor::HandleAxisInputs() {
  using model::Opcode;
  const auto input = [this](model::DeviceAddress device) {
    return device.kind == model::DeviceKind::kX ? physical_inputs_[device.index]
                                                : ReadBit(device);
  };
  for (int axis = 0; axis < 4; ++axis) {
    auto& state = axes_[axis];
    if (!state.status.busy)
      continue;
    const int relay = 340 + axis * 10;
    if (state.opcode == Opcode::kInterruptPosition && !state.finite) {
      const bool user = state.interrupt_input == 8;
      const bool level = user ? memory_.special_m[460 + axis]
                              : physical_inputs_[state.interrupt_input] ^
                                    memory_.special_m[relay + 7];
      if (level && !state.input_level) {
        state.remaining = state.interrupt_distance;
        state.finite = true;
        if (state.remaining == 0)
          FinishAxis(axis, false);
      }
      state.input_level = level;
    }
    if (state.opcode != Opcode::kDogSearch &&
        state.opcode != Opcode::kZeroReturn)
      continue;
    const bool dog = input(state.dog) ^ (state.opcode == Opcode::kDogSearch &&
                                         memory_.special_m[relay + 5]);
    if (state.home_phase == -1 && !dog) {
      state.home_phase = 0;
      state.sign = state.home_sign;
      WriteBit(state.direction, state.sign > 0);
    }
    if (state.home_phase == 0 && dog) {
      state.home_phase = 1;
      state.requested_frequency = state.creep_frequency;
    }
    if (state.home_phase == 1 && !dog)
      state.home_phase = 2;
    const bool zero = input(state.zero) ^ memory_.special_m[relay + 6];
    if (state.home_phase == 2 &&
        (state.opcode == Opcode::kZeroReturn || state.zero == state.dog ||
         (zero && !state.input_level))) {
      WriteWord(Register(8340 + axis * 10), 0, true);
      state.status.position = 0;
      FinishAxis(axis, false);
    }
    if (state.opcode == Opcode::kDogSearch)
      state.input_level = zero;
  }
}

void CompiledPLCExecutor::AdvanceAxes() {
  HandleAxisInputs();
  for (int axis = 0; axis < 4; ++axis) {
    auto& state = axes_[axis];
    if (state.clear_remaining_ms > 0) {
      state.clear_remaining_ms =
          std::max(0, state.clear_remaining_ms - current_elapsed_ms_);
      if (!state.clear_remaining_ms) {
        memory_.Y[state.clear_output] = false;
        physical_outputs_[state.clear_output] = false;
      }
    }
    if (!state.status.busy)
      continue;
    const int base = 8340 + axis * 10;
    const int relay = base - 8000;
    if (memory_.special_m[relay + 9]) {
      FinishAxis(axis, true);
      continue;
    }
    if (state.opcode == model::Opcode::kPulseWidth) {
      const int64_t elapsed = state.pwm_phase + int64_t{current_elapsed_ms_};
      state.status.generated_pulses += elapsed / state.pwm_period;
      state.pwm_phase = static_cast<int>(elapsed % state.pwm_period);
      memory_.Y[axis] = state.pwm_phase < state.pwm_high;
      continue;
    }
    if (memory_.special_m[relay + (state.sign > 0 ? 3 : 4)]) {
      if (state.opcode == model::Opcode::kDogSearch && state.home_phase == 0) {
        state.sign = -state.sign;
        state.home_sign = state.sign;
        WriteBit(state.direction, state.sign > 0);
      } else {
        state.stopping = true;
        state.status.abnormal = true;
      }
    }
    // Integrate in 1 ms intervals. This counts pulses without allocating or
    // emitting one UI event per pulse, including high-frequency adapter output.
    for (int elapsed = 0; elapsed < current_elapsed_ms_ && state.status.busy;
         ++elapsed) {
      double target = state.stopping ? 0 : state.requested_frequency;
      if (state.finite && state.deceleration_rate > 0)
        target =
            std::min(target, std::sqrt(2 * state.deceleration_rate *
                                       static_cast<double>(state.remaining)));
      double previous = state.status.frequency;
      const double rate =
          target < previous ? state.deceleration_rate : state.ramp_rate;
      state.status.frequency =
          rate <= 0 ? target
                    : previous + std::clamp(target - previous, -rate / 1000,
                                            rate / 1000);
      if (rate <= 0)
        previous = target;
      const double pulses =
          state.fraction + (previous + state.status.frequency) / 2000;
      int64_t count = static_cast<int64_t>(pulses);
      state.fraction = pulses - static_cast<double>(count);
      if (state.finite)
        count = std::min(count, state.remaining);
      state.status.generated_pulses += static_cast<uint64_t>(count);
      const int position_register =
          IsPulseCounter(state.opcode) ? 8140 + axis * 2 : base;
      const int32_t position = GetWordValue(Register(position_register), true);
      const uint32_t next = static_cast<uint32_t>(position) +
                            static_cast<uint32_t>(count * state.sign);
      WriteWord(Register(position_register), std::bit_cast<int32_t>(next),
                true);
      if (!IsPulseCounter(state.opcode))
        state.status.position = std::bit_cast<int32_t>(next);
      if (state.finite) {
        state.remaining -= count;
        if (state.remaining == 0)
          FinishAxis(axis, false);
      }
      if (state.stopping && state.status.frequency == 0)
        FinishAxis(axis, state.status.abnormal);
    }
    if (state.status.busy)
      memory_.Y[axis] = state.fraction < 0.5;
  }
}

bool CompiledPLCExecutor::ExecuteMotionOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  if (instruction.opcode == Opcode::kPositionTable)
    return ExecutePositionTable(instruction, index, power);
  if (instruction.opcode == Opcode::kAbsoluteRead)
    return ExecuteAbsoluteRead(instruction, index, power);
  const auto opcode = instruction.opcode;
  const auto& operands = instruction.operands;
  const int output_operand =
      opcode == Opcode::kPulseRamp || opcode == Opcode::kZeroReturn ? 3
      : opcode == Opcode::kVariablePulse                            ? 1
                                                                    : 2;
  const auto output = operands[output_operand].device;
  const auto fail = [this](const char* error) {
    memory_.special_m[67] = true;
    memory_.D[8067] = 6706;
    SetError(error);
    return false;
  };
  if (output.kind != model::DeviceKind::kY || output.index >= 4 ||
      (IsPulseCounter(opcode) && output.index >= 2))
    return fail("Pulse output requires Y0..Y3 (PLSY/PLSR Y0..Y1)");
  const int axis = static_cast<int>(output.index);
  auto& state = axes_[axis];
  const int base = 8340 + axis * 10;
  const int relay = base - 8000;
  if (!power) {
    if (state.owner != index)
      return true;
    memory_.special_m[29] = false;
    memory_.special_m[329] = false;
    if (state.status.busy && state.deceleration_rate > 0) {
      state.stopping = true;
      state.finite = false;
      return true;
    }
    if (state.status.busy)
      FinishAxis(axis, true);
    if (state.clear_remaining_ms > 0) {
      memory_.Y[state.clear_output] = false;
      physical_outputs_[state.clear_output] = false;
    }
    state = {};
    memory_.special_m[relay + 8] = false;
    memory_.special_m[29] = false;
    memory_.special_m[329] = false;
    return true;
  }
  if (state.owner != SIZE_MAX && state.owner != index) {
    if (state.status.busy || instruction_power_[state.owner])
      return fail("Two active positioning instructions share a pulse output");
    state = {};
  }
  const auto value = [&](int operand) {
    return int64_t{ReadValue(operands[operand], instruction.wide)};
  };
  if (state.owner == index) {
    if (state.status.complete)
      memory_.special_m[29] = true;
    if (state.status.abnormal)
      memory_.special_m[329] = true;
    if (state.status.busy && opcode == Opcode::kPulseOutput) {
      const int64_t frequency = value(0);
      if (frequency <= 0 || frequency > (instruction.wide ? 200000 : 32767))
        return fail("PLSY frequency exceeds output range");
      state.requested_frequency = static_cast<double>(frequency);
    }
    if (state.status.busy && opcode == Opcode::kVariablePulse &&
        !state.stopping) {
      const int64_t frequency = value(0);
      if (frequency == 0 ||
          std::abs(frequency) > (instruction.wide ? 200000 : 32768))
        return fail("PLSV frequency exceeds signed output range");
      const int sign = frequency < 0 ? -1 : 1;
      if (state.sign != sign && state.ramp_rate > 0 &&
          state.status.frequency > 0) {
        state.requested_frequency = 0;
      } else {
        state.sign = sign;
        WriteBit(state.direction, sign > 0);
        state.requested_frequency = static_cast<double>(std::abs(frequency));
      }
    }
    return true;
  }
  AxisState candidate;
  candidate.owner = index;
  candidate.opcode = opcode;
  candidate.status.busy = true;
  candidate.status.position = GetWordValue(Register(base), true);
  const bool directional =
      opcode == Opcode::kVariablePulse || opcode == Opcode::kRelativePosition ||
      opcode == Opcode::kAbsolutePosition ||
      opcode == Opcode::kInterruptPosition || opcode == Opcode::kDogSearch;
  if (directional) {
    candidate.direction =
        operands[opcode == Opcode::kVariablePulse ? 2 : 3].device;
    if (candidate.direction.kind == model::DeviceKind::kY &&
        candidate.direction.index == output.index)
      return fail("Pulse output and direction output must differ");
  }
  int64_t frequency = 0;
  int acceleration = 0;
  int deceleration = 0;
  if (opcode == Opcode::kPulseWidth) {
    const int64_t high = value(0);
    const int64_t period = value(1);
    if (period < 1 || period > 32767 || high < 0 || high > period)
      return fail("PWM requires 0 <= high time <= period (1..32767 ms)");
    candidate.pwm_high = static_cast<int>(high);
    candidate.pwm_period = static_cast<int>(period);
    candidate.status.frequency = 1000.0 / static_cast<double>(period);
  } else if (IsPulseCounter(opcode)) {
    frequency = value(0);
    const int64_t pulses = value(1);
    if (pulses < 0)
      return fail("PLSY/PLSR pulse quantity cannot be negative");
    candidate.finite = pulses != 0;
    candidate.remaining = pulses;
    if (opcode == Opcode::kPulseRamp) {
      const int64_t duration = value(2);
      if (duration < 50 || duration > 5000)
        return fail("PLSR acceleration/deceleration is 50..5000 ms");
      acceleration = deceleration = static_cast<int>(duration);
    }
  } else if (opcode == Opcode::kDogSearch || opcode == Opcode::kZeroReturn) {
    candidate.clear_output = axis + 4;
    if (memory_.special_m[464 + axis]) {
      const uint16_t specification = memory_.D[8464 + axis];
      int output_index = 0;
      for (int digit = 0; digit < 4; ++digit) {
        const int octal_digit = (specification >> (digit * 4)) & 15;
        if (octal_digit > 7)
          return fail("Zero-return CLEAR output requires octal Y digits");
        output_index += octal_digit << (digit * 3);
      }
      if (output_index >= 256 || output_index == axis)
        return fail(
            "Zero-return CLEAR output exceeds Y range or shares pulse output");
      candidate.clear_output = output_index;
    }
    candidate.home_sign =
        opcode == Opcode::kDogSearch && memory_.special_m[relay + 2] ? 1 : -1;
    candidate.sign = candidate.home_sign;
    candidate.dog = operands[opcode == Opcode::kDogSearch ? 0 : 2].device;
    candidate.zero =
        opcode == Opcode::kDogSearch ? operands[1].device : candidate.dog;
    if (candidate.dog.kind == model::DeviceKind::kX && candidate.dog.index > 7)
      return fail("Zero return DOG requires X0..X7 or an internal bit");
    if (candidate.zero.kind == model::DeviceKind::kX &&
        candidate.zero.index > 7)
      return fail("DSZR zero-phase input requires X0..X7 or an internal bit");
    frequency = opcode == Opcode::kDogSearch
                    ? GetWordValue(Register(base + 6), true)
                    : value(0);
    candidate.creep_frequency = opcode == Opcode::kDogSearch
                                    ? memory_.D[base + 5]
                                    : static_cast<double>(value(1));
    if (candidate.creep_frequency < 10 || candidate.creep_frequency > 32767 ||
        candidate.creep_frequency > frequency)
      return fail("Zero return creep speed is 10..32767 Hz below return speed");
    acceleration = memory_.D[base + 8];
    deceleration = memory_.D[base + 9];
    const bool dog = ReadBit(candidate.dog) ^ (opcode == Opcode::kDogSearch &&
                                               memory_.special_m[relay + 5]);
    if (dog && opcode == Opcode::kDogSearch) {
      candidate.home_phase = -1;
      candidate.sign = -candidate.home_sign;
    } else if (dog) {
      candidate.home_phase = 1;
      frequency = static_cast<int64_t>(candidate.creep_frequency);
    }
    candidate.input_level =
        ReadBit(candidate.zero) ^ memory_.special_m[relay + 6];
  } else {
    int64_t distance = opcode == Opcode::kVariablePulse ? 0 : value(0);
    frequency = value(opcode == Opcode::kVariablePulse ? 0 : 1);
    if (opcode == Opcode::kVariablePulse) {
      candidate.sign = frequency < 0 ? -1 : 1;
      frequency = std::abs(frequency);
      if (memory_.special_m[338]) {
        acceleration = memory_.D[base + 8];
        deceleration = memory_.D[base + 9];
      }
    } else {
      if (opcode == Opcode::kAbsolutePosition)
        distance -= candidate.status.position;
      candidate.sign = distance < 0 ? -1 : 1;
      candidate.remaining = std::abs(distance);
      candidate.finite = opcode != Opcode::kInterruptPosition;
      candidate.interrupt_distance = candidate.remaining;
      acceleration = memory_.D[base + 8];
      deceleration = memory_.D[base + 9];
      if (opcode == Opcode::kInterruptPosition) {
        candidate.interrupt_input =
            memory_.special_m[336]
                ? (static_cast<uint16_t>(memory_.D[8336]) >> (axis * 4)) & 15
                : axis;
        if (candidate.interrupt_input > 8)
          return fail(
              "DVIT interrupt selector must be X0..X7 or user command 8");
        candidate.input_level =
            candidate.interrupt_input == 8
                ? memory_.special_m[460 + axis]
                : physical_inputs_[candidate.interrupt_input] ^
                      memory_.special_m[relay + 7];
      }
    }
  }
  if (opcode != Opcode::kPulseWidth) {
    const int minimum =
        opcode == Opcode::kPulseOutput || opcode == Opcode::kVariablePulse ? 1
                                                                           : 10;
    int64_t maximum =
        instruction.wide || opcode == Opcode::kDogSearch ? 200000 : 32767;
    if (opcode == Opcode::kVariablePulse && !instruction.wide &&
        candidate.sign < 0)
      maximum = 32768;
    if (frequency < minimum || frequency > maximum || acceleration < 0 ||
        deceleration < 0 || memory_.D[base + 2] < 0)
      return fail("Positioning speed, bias or ramp setting exceeds range");
    candidate.requested_frequency = static_cast<double>(frequency);
    // Positioning acceleration times refer to the axis maximum speed, rather
    // than the frequency requested by an individual move.
    const double ramp_frequency = opcode == Opcode::kPulseRamp
                                      ? candidate.requested_frequency
                                      : GetWordValue(Register(base + 3), true);
    if ((acceleration || deceleration) && ramp_frequency <= 0)
      return fail("Axis maximum speed must be positive for acceleration");
    candidate.ramp_rate =
        acceleration ? ramp_frequency * 1000 / acceleration : 0;
    candidate.deceleration_rate =
        deceleration ? ramp_frequency * 1000 / deceleration : 0;
    candidate.status.frequency =
        acceleration ? std::min<double>(memory_.D[base + 2], frequency)
                     : static_cast<double>(frequency);
  }
  state = candidate;
  memory_.special_m[29] = false;
  memory_.special_m[329] = false;
  memory_.special_m[relay] = true;
  memory_.special_m[relay + 8] = true;
  if (directional)
    WriteBit(state.direction, state.sign > 0);
  if (state.opcode == Opcode::kPulseWidth)
    memory_.Y[axis] = state.pwm_high != 0;
  if (state.finite && state.remaining == 0)
    FinishAxis(axis, false);
  return true;
}
}  // namespace plc
