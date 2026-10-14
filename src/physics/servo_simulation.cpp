#include "plc_emulator/physics/servo_simulation.h"

#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/components/servo_component.h"

#include <algorithm>
#include <cmath>

namespace plc_emulator {
namespace {

bool HasWire(const std::vector<plc::Wire>& wires, plc::PortRef first,
             plc::PortRef second) {
  return std::any_of(wires.begin(), wires.end(), [&](const auto& wire) {
    const plc::PortRef from{wire.fromComponentId, wire.fromPortId};
    const plc::PortRef to{wire.toComponentId, wire.toPortId};
    return wire.kind == plc::WireKind::kElectric &&
           ((from == first && to == second) || (from == second && to == first));
  });
}

bool SignalConnected(const std::vector<plc::Wire>& wires, plc::PortRef start,
                     plc::PortRef destination) {
  static thread_local std::vector<plc::PortRef> visited;
  visited.clear();
  visited.push_back(start);
  visited.reserve(wires.size() * 2 + 1);
  for (size_t index = 0; index < visited.size(); ++index) {
    if (visited[index] == destination) {
      return true;
    }
    for (const auto& wire : wires) {
      if (wire.kind != plc::WireKind::kElectric) {
        continue;
      }
      const plc::PortRef from{wire.fromComponentId, wire.fromPortId};
      const plc::PortRef to{wire.toComponentId, wire.toPortId};
      const auto next = from == visited[index] ? to : from;
      if ((from == visited[index] || to == visited[index]) &&
          std::find(visited.begin(), visited.end(), next) == visited.end()) {
        visited.push_back(next);
      }
    }
  }
  return false;
}

float Voltage(const std::map<plc::PortRef, float>& voltages, int id, int port) {
  const auto found = voltages.find({id, port});
  return found == voltages.end() ? -1 : found->second;
}

bool Active(float voltage, bool active_low) {
  return active_low ? std::abs(voltage) < 0.1f : voltage > 23;
}

const plc::PlacedComponent* LoadFor(
    const std::vector<plc::PlacedComponent>& parts,
    const std::vector<plc::Wire>& wires, int amplifier) {
  const plc::PlacedComponent* result = nullptr;
  for (const auto& part : parts) {
    if (!IsServoLoad(part.type)) {
      continue;
    }
    const int base = ServoPortBase(part.type);
    bool linked = HasWire(wires, {amplifier, 17}, {part.instanceId, base + 4});
    for (int phase = 0; phase < 3; ++phase) {
      linked &= HasWire(wires, {amplifier, 14 + phase},
                        {part.instanceId, base + phase});
    }
    if (linked) {
      if (result) {
        return nullptr;
      }
      result = &part;
    }
  }
  return result;
}

const plc::PlacedComponent* AcFor(
    const std::vector<plc::PlacedComponent>& parts,
    const std::vector<plc::Wire>& wires, int amplifier) {
  for (const auto& part : parts) {
    if (part.type != plc::ComponentType::kAcSupply ||
        ReadPneumaticState(part, "ac_enabled", 1) < 0.5f) {
      continue;
    }
    bool connected = true;
    for (int phase = 0; phase < 4; ++phase) {
      connected &= HasWire(wires, {amplifier, phase}, {part.instanceId, phase});
    }
    if (connected) {
      return &part;
    }
  }
  return nullptr;
}

void StepPosition(plc::PlacedComponent* amp, plc::PlacedComponent* load,
                  double pulses, double dt) {
  auto& state = amp->axis_state;
  const auto& settings = load->axis_settings;
  state.target += pulses / settings.pulses_per_turn;
  if (IsServoLift(load->type)) {
    const double maximum = settings.stroke_mm / settings.lead_mm;
    const double tolerance = 1e-9 * std::max(1.0, maximum);
    if (state.target < -tolerance || state.target > maximum + tolerance) {
      state.alarm = 4;
      state.target = state.position;
      state.velocity = 0;
      state.ready = false;
      state.in_position = false;
      return;
    }
    state.target = std::clamp(state.target, 0.0, maximum);
  }
  const double error = state.target - state.position;
  const double speed =
      std::min(settings.max_speed,
               std::sqrt(2 * settings.acceleration * std::abs(error)));
  const double desired = std::copysign(speed, error);
  state.velocity +=
      std::clamp(desired - state.velocity, -settings.acceleration * dt,
                 settings.acceleration * dt);
  const double movement = state.velocity * dt;
  if (std::abs(error) < 0.00001 ||
      (movement * error >= 0 && std::abs(movement) >= std::abs(error))) {
    state.position = state.target;
    state.velocity = 0;
  } else {
    state.position += movement;
  }
  if (IsServoLift(load->type)) {
    const double maximum = settings.stroke_mm / settings.lead_mm;
    if (state.position < 0 || state.position > maximum) {
      state.position = std::clamp(state.position, 0.0, maximum);
      state.target = state.position;
      state.velocity = 0;
      state.alarm = 4;
      state.ready = false;
      state.in_position = false;
      return;
    }
  }
  state.in_position = std::abs(state.target - state.position) < 0.0001 &&
                      std::abs(state.velocity) < 0.001;
}

void StepAmplifier(plc::PlacedComponent* amp,
                   std::vector<plc::PlacedComponent>* parts,
                   const std::vector<plc::Wire>& wires,
                   const std::map<plc::PortRef, float>& voltages,
                   const std::vector<PulseSource>& pulses, double dt,
                   bool active_low) {
  auto& state = amp->axis_state;
  state.ready = false;
  state.in_position = false;
  const int id = amp->instanceId;
  const bool powered = Voltage(voltages, id, 4) > 23 &&
                       std::abs(Voltage(voltages, id, 5)) < 0.1f;
  const auto* ac = AcFor(*parts, wires, id);
  const auto* load = LoadFor(*parts, wires, id);
  const PulseSource* source = nullptr;
  bool ambiguous = false;
  for (const auto& pulse : pulses) {
    if (SignalConnected(wires, {id, 6},
                        {pulse.component_id, 16 + pulse.output})) {
      ambiguous |= source != nullptr;
      source = &pulse;
    }
  }
  double delta = 0;
  if (source) {
    const bool same = state.pulse_baseline &&
                      state.pulse_source_id == source->component_id &&
                      state.pulse_output == source->output &&
                      state.pulse_epoch == source->epoch;
    if (same && source->forward >= state.pulse_forward &&
        source->reverse >= state.pulse_reverse) {
      if (source->direction_output >= 0) {
        delta = static_cast<double>(source->forward - state.pulse_forward) -
                static_cast<double>(source->reverse - state.pulse_reverse);
      } else {
        delta = static_cast<double>(source->forward - state.pulse_forward) +
                static_cast<double>(source->reverse - state.pulse_reverse);
        if (!Active(Voltage(voltages, id, 8), active_low)) {
          delta = -delta;
        }
      }
    }
    state.pulse_baseline = true;
    state.pulse_source_id = source->component_id;
    state.pulse_output = source->output;
    state.pulse_epoch = source->epoch;
    state.pulse_forward = source->forward;
    state.pulse_reverse = source->reverse;
  } else {
    state.pulse_baseline = false;
  }
  int cause = !powered ? 1 : !ac ? 2 : !load ? 3 : 0;
  if (cause == 0 && !IsValidAxisSettings(load->axis_settings))
    cause = 8;
  if (cause == 0) {
    const int base = ServoPortBase(load->type);
    const bool earth =
        HasWire(wires, {load->instanceId, base + 3}, {id, 3}) ||
        HasWire(wires, {load->instanceId, base + 3}, {ac->instanceId, 3});
    if (!earth) {
      cause = 5;
    } else if (!source || ambiguous ||
               !Active(Voltage(voltages, id, 7), !active_low)) {
      cause = 6;
    } else if (source->direction_output >= 0 &&
               !SignalConnected(
                   wires, {id, 8},
                   {source->component_id, 16 + source->direction_output})) {
      cause = 7;
    }
  }
  if (cause != 0 && (powered || state.alarm != 0 || state.velocity != 0)) {
    state.alarm = cause;
  }
  if (cause == 0 && Active(Voltage(voltages, id, 10), active_low)) {
    state.alarm = 0;
  }
  if (cause != 0 || state.alarm != 0 ||
      !Active(Voltage(voltages, id, 9), active_low)) {
    state.velocity = 0;
    state.target = state.position;
    return;
  }
  auto* driven = &(*parts)[static_cast<size_t>(load - parts->data())];
  if (state.load_id != driven->instanceId) {
    state.load_id = driven->instanceId;
    state.position = driven->axis_state.position;
    state.target = state.position;
  }
  state.ready = true;
  StepPosition(amp, driven, delta, dt);
  driven->axis_state.position = state.position;
  driven->axis_state.velocity = state.velocity;
  driven->axis_state.target = state.target;
  driven->axis_state.ready = state.ready;
  driven->axis_state.in_position = state.in_position;
  driven->axis_state.alarm = state.alarm;
  driven->axis_state.amplifier_id = id;
  if (IsServoLift(driven->type)) {
    const double fraction = state.position * driven->axis_settings.lead_mm /
                            driven->axis_settings.stroke_mm;
    driven->z_state.current_height =
        driven->z_config.min_layer +
        static_cast<float>(fraction) *
            (driven->z_config.max_layer - driven->z_config.min_layer);
  }
}

}  // namespace

bool IsServoSignalRole(const std::string& role) {
  return role != "HMI_COM" && role != "PLC_COM" && role != "SERVO_U" && role != "SERVO_V" && role != "SERVO_W" &&
         !role.starts_with("AC_") && role != "PROTECTIVE_EARTH" &&
         role != "ENCODER";
}

void StepServoNetwork(std::vector<plc::PlacedComponent>* parts,
                      const std::vector<plc::Wire>& wires,
                      const std::map<plc::PortRef, float>& voltages,
                      const std::vector<PulseSource>& pulses, double dt,
                      bool active_low) {
  if (!parts || !std::isfinite(dt) || dt <= 0) {
    return;
  }
  for (auto& part : *parts) {
    if (IsServoLoad(part.type)) {
      part.axis_state.ready = false;
      part.axis_state.in_position = false;
      part.axis_state.velocity = 0;
    } else if (part.type == plc::ComponentType::kDcMotor) {
      const float plus = Voltage(voltages, part.instanceId, 0);
      const float minus = Voltage(voltages, part.instanceId, 1);
      const double target =
          plus >= 0 && minus >= 0
              ? std::clamp(static_cast<double>(plus - minus) / 24, -1.0, 1.0) *
                    part.axis_settings.max_speed
              : 0;
      part.axis_state.velocity +=
          std::clamp(target - part.axis_state.velocity,
                     -part.axis_settings.acceleration * dt,
                     part.axis_settings.acceleration * dt);
      part.axis_state.position += part.axis_state.velocity * dt;
    }
  }
  for (auto& part : *parts) {
    if (part.type == plc::ComponentType::kServoAmplifier) {
      StepAmplifier(&part, parts, wires, voltages, pulses, dt, active_low);
    }
  }
}

}  // namespace plc_emulator
