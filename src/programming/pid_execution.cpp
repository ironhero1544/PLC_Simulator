#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <cmath>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

int16_t TuningParameter(double value) {
  return static_cast<int16_t>(std::clamp(std::round(value), 1.0, 32767.0));
}
}  // namespace

bool CompiledPLCExecutor::ExecutePidTuning(PidState* state,
                                           std::span<int16_t> parameters,
                                           int target, int measured,
                                           int sampling_ms,
                                           model::DeviceAddress output) {
  auto& p = parameters;
  const double sample_seconds = sampling_ms / 1000.0;
  state->tuning_ms += sampling_ms;
  if (!(p[1] & 64)) {
    // Step response: capture the maximum tangent slope and its intercept.
    // The supplied MV remains unchanged throughout this tuning procedure.
    const double slope =
        std::abs(measured - state->previous_raw) / sample_seconds;
    if (slope > state->maximum_slope) {
      state->maximum_slope = slope;
      state->dead_time = state->tuning_ms / 1000.0 -
                         std::abs(measured - state->initial_pv) / slope;
    }
    state->previous_raw = measured;
    if (std::abs(measured - state->initial_pv) * 3 <
            std::abs(target - state->initial_pv) ||
        state->maximum_slope <= 0)
      return true;
    const double dead_time = std::max(sample_seconds, state->dead_time);
    p[3] = TuningParameter(120 * std::abs(state->initial_mv) /
                           (state->maximum_slope * dead_time));
    p[4] = TuningParameter(20 * dead_time);
    p[6] = TuningParameter(50 * dead_time);
    p[1] = static_cast<int16_t>((p[1] & ~17) |
                                (measured > state->initial_pv ? 1 : 0));
    state->tuning = false;
    state->filtered = state->previous_filtered = measured;
    state->previous_error = (p[1] & 1) ? target - measured : measured - target;
    state->derivative = 0;
    return true;
  }
  // Limit-cycle tuning switches the two output levels at SV +/- hysteresis.
  const bool reverse = p[1] & 1;
  const double error = reverse ? target - measured : measured - target;
  bool high = state->output_high;
  if (error > p[25])
    high = true;
  else if (error < -p[25])
    high = false;
  state->minimum_pv = std::min<double>(state->minimum_pv, measured);
  state->maximum_pv = std::max<double>(state->maximum_pv, measured);
  if (high != state->output_high) {
    ++state->transitions;
    if (high) {
      state->switch_ms = state->tuning_ms;
    } else {
      const int64_t period = state->tuning_ms - state->cycle_start_ms;
      const int64_t on = state->tuning_ms - state->switch_ms;
      const double amplitude = state->maximum_pv - state->minimum_pv;
      if (state->transitions >= 6 && period > on && on > 0 && amplitude > 0) {
        const double on_seconds = on / 1000.0;
        const double time = on_seconds * (1 - double(on) / double(period));
        p[3] = TuningParameter(120 * (p[26] - p[27]) / amplitude);
        p[4] = TuningParameter(20 * time);
        p[6] = TuningParameter(50 * time);
        const int wait_percent = p[28] >= -50 && p[28] <= 32717 ? p[28] : -50;
        state->wait_ms = (period - on) * (50 + wait_percent) / 100;
        p[1] &= ~80;
        state->tuning = false;
        state->filtered = state->previous_filtered = measured;
        state->previous_error = error;
        state->previous_raw = measured;
        state->derivative = 0;
      }
      state->cycle_start_ms = state->tuning_ms;
      state->minimum_pv = state->maximum_pv = measured;
    }
  }
  state->output_high = high;
  return WriteWord(output, high ? p[26] : p[27]);
}

bool CompiledPLCExecutor::ExecutePidOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  auto& state = pid_states_[index];
  if (!power) {
    state = {};
    return true;
  }
  const auto parameters = instruction.operands[2].device;
  const auto output = instruction.operands[3].device;
  const auto fail = [this](int code, const char* message) {
    memory_.special_m[67] = true;
    memory_.D[8067] = static_cast<int16_t>(code);
    SetError(message);
    return false;
  };
  if ((parameters.kind != model::DeviceKind::kD &&
       parameters.kind != model::DeviceKind::kR) ||
      parameters.index + 7 > WordMemory(parameters).size())
    return fail(6706, "PID parameters require a D/R register block");
  auto storage = WordMemory(parameters).subspan(parameters.index);
  const int act = static_cast<uint16_t>(storage[1]);
  const size_t size = (act & 64) ? 29 : (act & 38) ? 25 : 20;
  if (storage.size() < size)
    return fail(6706, "PID internal and alarm parameter block exceeds memory");
  auto p = storage.first(size);
  if (p[0] <= 0)
    return fail(6730, "PID sampling time must be positive");
  if ((act & ~119) || ((act & 4) && (act & 32)))
    return fail(6731, "PID operation flags conflict or use reserved bits");
  if (p[2] < 0 || p[2] > 99)
    return fail(6732, "PID input filter must be 0..99 percent");
  if (p[3] < 0)
    return fail(6733, "PID proportional gain cannot be negative");
  if (p[4] < 0)
    return fail(6734, "PID integral time cannot be negative");
  if (p[5] < 0 || p[5] > 100)
    return fail(6735, "PID derivative gain must be 0..100 percent");
  if (p[6] < 0)
    return fail(6736, "PID derivative time cannot be negative");
  if ((act & 32) && p[22] < p[23])
    return fail(6737, "PID output upper limit is below lower limit");
  if ((act & 2) && (p[20] < 0 || p[21] < 0))
    return fail(6737, "PID input alarm thresholds cannot be negative");
  if ((act & 4) && (p[22] < 0 || p[23] < 0))
    return fail(6737, "PID output alarm thresholds cannot be negative");
  const int target = ReadValue(instruction.operands[0]);
  const int measured = ReadValue(instruction.operands[1]);
  if (!state.active || !p[7]) {
    state = {};
    state.active = true;
    state.filtered = state.previous_filtered = measured;
    state.previous_raw = measured;
    state.previous_error = (act & 1) ? target - measured : measured - target;
    state.previous_output = GetWordValue(output);
    p[7] = 1;
  } else {
    state.elapsed_ms += current_elapsed_ms_;
  }
  if ((act & 16) && !state.tuning) {
    if (!(act & 64) && (p[0] < 1000 || std::abs(target - measured) < 150))
      return fail(6741,
                  "PID step tuning needs >=1000 ms sampling and >=150 PV/SV "
                  "difference");
    if ((act & 64) && (p[25] < 0 || p[26] <= p[27]))
      return fail(
          6741,
          "PID limit-cycle tuning needs nonnegative hysteresis and ULV > LLV");
    state.tuning = true;
    state.initial_pv = measured;
    state.initial_mv = GetWordValue(output);
    state.maximum_slope = 0;
    state.dead_time = 0;
    state.tuning_ms = state.cycle_start_ms = state.switch_ms = 0;
    state.transitions = 0;
    state.minimum_pv = state.maximum_pv = measured;
    state.output_high =
        ((act & 1) ? target - measured : measured - target) >= 0;
    if (act & 64)
      WriteWord(output, state.output_high ? p[26] : p[27]);
  }
  if (!(act & 16))
    state.tuning = false;
  const int sample = std::max<int>(p[0], current_elapsed_ms_);
  if (p[0] < current_elapsed_ms_) {
    memory_.special_m[67] = true;
    memory_.D[8067] = 6740;
  }
  if (state.elapsed_ms < sample)
    return true;
  state.elapsed_ms = 0;
  if (state.tuning)
    return ExecutePidTuning(&state, p, target, measured, sample, output);
  if (state.wait_ms > 0) {
    state.wait_ms = std::max<int64_t>(0, state.wait_ms - sample);
    return true;
  }
  const double filtered = measured + p[2] / 100.0 * (state.filtered - measured);
  const double direction = (act & 1) ? -1 : 1;
  const double error = direction * (filtered - target);
  const double integral = p[4] ? sample / (p[4] * 100.0) * error : 0;
  const double td = p[6] * 10.0;
  const double kd = p[5] / 100.0;
  const double derivative =
      td > 0
          ? td / (sample + kd * td) * direction *
                    (filtered - 2 * state.filtered + state.previous_filtered) +
                kd * td / (sample + kd * td) * state.derivative
          : 0;
  const double delta =
      p[3] / 100.0 * (error - state.previous_error + integral + derivative);
  const int previous = GetWordValue(output);
  if (previous != state.previous_output)
    state.output_fraction = 0;
  const double requested = previous + delta + state.output_fraction;
  const int upper = (act & 32) ? p[22] : 32767;
  const int lower = (act & 32) ? p[23] : -32768;
  const int result = static_cast<int>(
      std::clamp(std::round(requested), double(lower), double(upper)));
  state.previous_output = result;
  state.output_fraction =
      requested >= lower && requested <= upper ? requested - result : 0;
  if (act & 6) {
    int alarm = 0;
    if (act & 2) {
      if (measured - state.previous_raw > p[20])
        alarm |= 1;
      if (state.previous_raw - measured > p[21])
        alarm |= 2;
    }
    if (act & 4) {
      if (delta > p[22])
        alarm |= 4;
      if (-delta > p[23])
        alarm |= 8;
    }
    p[24] = static_cast<int16_t>(alarm);
  }
  state.previous_filtered = state.filtered;
  state.filtered = filtered;
  state.previous_error = error;
  state.derivative = derivative;
  state.previous_raw = measured;
  return WriteWord(output, result);
}
}  // namespace plc
