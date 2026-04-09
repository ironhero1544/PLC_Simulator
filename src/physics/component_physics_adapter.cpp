#include "plc_emulator/physics/component_physics_adapter.h"

#include "plc_emulator/components/state_keys.h"

#include <cmath>
#include <string>

namespace plc {
namespace {

void UpdateButtonUnitElectrical(PlacedComponent* comp,
                                const std::map<PortRef, float>& voltages) {
  if (!comp) {
    return;
  }
  for (int i = 0; i < 3; ++i) {
    PortRef plus_port = std::make_pair(comp->instanceId, i * 5 + 3);
    PortRef minus_port = std::make_pair(comp->instanceId, i * 5 + 4);
    bool has_24v = voltages.count(plus_port) &&
                   voltages.at(plus_port) > 23.0f;
    bool has_0v = voltages.count(minus_port) &&
                  voltages.at(minus_port) > -0.1f &&
                  voltages.at(minus_port) < 0.1f;
    std::string key = std::string(state_keys::kLampOnPrefix) +
                      std::to_string(i);
    comp->internalStates[key] = (has_24v && has_0v) ? 1.0f : 0.0f;
  }
}

void UpdateValveElectrical(PlacedComponent* comp,
                           const std::map<PortRef, float>& voltages) {
  if (!comp) {
    return;
  }
  PortRef sol_a_plus = std::make_pair(comp->instanceId, 0);
  PortRef sol_a_minus = std::make_pair(comp->instanceId, 1);
  bool sol_a_powered = voltages.count(sol_a_plus) &&
                       voltages.at(sol_a_plus) > 23.0f &&
                       voltages.count(sol_a_minus) &&
                       voltages.at(sol_a_minus) > -0.1f &&
                       voltages.at(sol_a_minus) < 0.1f;
  comp->internalStates[state_keys::kSolenoidAActive] =
      sol_a_powered ? 1.0f : 0.0f;

  if (comp->type == ComponentType::VALVE_DOUBLE) {
    PortRef sol_b_plus = std::make_pair(comp->instanceId, 2);
    PortRef sol_b_minus = std::make_pair(comp->instanceId, 3);
    bool sol_b_powered = voltages.count(sol_b_plus) &&
                         voltages.at(sol_b_plus) > 23.0f &&
                         voltages.count(sol_b_minus) &&
                         voltages.at(sol_b_minus) > -0.1f &&
                         voltages.at(sol_b_minus) < 0.1f;
    comp->internalStates[state_keys::kSolenoidBActive] =
        sol_b_powered ? 1.0f : 0.0f;
  }
}

void UpdateSensorElectrical(PlacedComponent* comp,
                            const std::map<PortRef, float>& voltages) {
  if (!comp) {
    return;
  }
  PortRef plus_port = std::make_pair(comp->instanceId, 0);
  int minus_id = (comp->type == ComponentType::RING_SENSOR) ? 2 : 1;
  PortRef minus_port = std::make_pair(comp->instanceId, minus_id);
  bool has_24v = voltages.count(plus_port) &&
                 voltages.at(plus_port) > 23.0f;
  bool has_0v = voltages.count(minus_port) &&
                voltages.at(minus_port) > -0.1f &&
                voltages.at(minus_port) < 0.1f;
  comp->internalStates[state_keys::kIsPowered] =
      (has_24v && has_0v) ? 1.0f : 0.0f;
}

void UpdateConveyorElectrical(PlacedComponent* comp,
                              const std::map<PortRef, float>& voltages) {
  if (!comp) {
    return;
  }
  PortRef plus_port = std::make_pair(comp->instanceId, 0);
  PortRef minus_port = std::make_pair(comp->instanceId, 1);
  bool p0_24 =
      voltages.count(plus_port) && voltages.at(plus_port) > 23.0f;
  bool p1_24 =
      voltages.count(minus_port) && voltages.at(minus_port) > 23.0f;
  bool p0_0 = voltages.count(plus_port) &&
              voltages.at(plus_port) > -0.1f &&
              voltages.at(plus_port) < 0.1f;
  bool p1_0 = voltages.count(minus_port) &&
              voltages.at(minus_port) > -0.1f &&
              voltages.at(minus_port) < 0.1f;
  bool motor_on = (p0_24 && p1_0) || (p1_24 && p0_0);
  comp->internalStates[state_keys::kMotorActive] = motor_on ? 1.0f : 0.0f;
}

void UpdateProcessingCylinderElectrical(
    PlacedComponent* comp,
    const std::map<PortRef, float>& voltages) {
  if (!comp) {
    return;
  }
  PortRef supply_plus = std::make_pair(comp->instanceId, 0);
  PortRef supply_minus = std::make_pair(comp->instanceId, 4);
  PortRef motor_ctrl = std::make_pair(comp->instanceId, 3);
  bool has_24v = voltages.count(supply_plus) &&
                 voltages.at(supply_plus) > 23.0f;
  bool has_0v = voltages.count(supply_minus) &&
                voltages.at(supply_minus) > -0.1f &&
                voltages.at(supply_minus) < 0.1f;
  bool ctrl_high = voltages.count(motor_ctrl) &&
                   voltages.at(motor_ctrl) > 23.0f;
  bool ctrl_low = voltages.count(motor_ctrl) &&
                  voltages.at(motor_ctrl) > -0.1f &&
                  voltages.at(motor_ctrl) < 0.1f;
  bool motor_on = has_24v && has_0v && (ctrl_low || ctrl_high);
  comp->internalStates[state_keys::kMotorOn] = motor_on ? 1.0f : 0.0f;
}

void UpdateTowerLampElectrical(PlacedComponent* comp,
                               const std::map<PortRef, float>& voltages) {
  if (!comp) {
    return;
  }
  PortRef com_port = std::make_pair(comp->instanceId, 0);
  auto read_voltage = [&](const PortRef& port) -> float {
    auto it = voltages.find(port);
    if (it == voltages.end()) {
      return -1.0f;
    }
    return it->second;
  };
  auto is_high = [&](float value) { return value > 23.0f; };
  auto is_low = [&](float value) {
    return value > -0.1f && value < 0.1f;
  };
  float com_voltage = read_voltage(com_port);
  bool red_on = false;
  bool yellow_on = false;
  bool green_on = false;
  if (com_voltage >= -0.1f) {
    PortRef red_port = std::make_pair(comp->instanceId, 1);
    PortRef yellow_port = std::make_pair(comp->instanceId, 2);
    PortRef green_port = std::make_pair(comp->instanceId, 3);
    float red_voltage = read_voltage(red_port);
    float yellow_voltage = read_voltage(yellow_port);
    float green_voltage = read_voltage(green_port);
    red_on = (is_high(com_voltage) && is_low(red_voltage)) ||
             (is_low(com_voltage) && is_high(red_voltage));
    yellow_on = (is_high(com_voltage) && is_low(yellow_voltage)) ||
                (is_low(com_voltage) && is_high(yellow_voltage));
    green_on = (is_high(com_voltage) && is_low(green_voltage)) ||
               (is_low(com_voltage) && is_high(green_voltage));
  }
  comp->internalStates[state_keys::kLampRed] = red_on ? 1.0f : 0.0f;
  comp->internalStates[state_keys::kLampYellow] = yellow_on ? 1.0f : 0.0f;
  comp->internalStates[state_keys::kLampGreen] = green_on ? 1.0f : 0.0f;
}

void UpdateValveLogic(PlacedComponent* comp) {
  if (!comp || comp->type != ComponentType::VALVE_DOUBLE) {
    return;
  }

  bool sol_a_active = comp->internalStates.count(state_keys::kSolenoidAActive) &&
                      comp->internalStates.at(state_keys::kSolenoidAActive) > 0.5f;
  bool sol_b_active = comp->internalStates.count(state_keys::kSolenoidBActive) &&
                      comp->internalStates.at(state_keys::kSolenoidBActive) > 0.5f;

  if (sol_a_active && !sol_b_active) {
    comp->internalStates[state_keys::kLastActiveSolenoid] = 1.0f;
  } else if (sol_b_active && !sol_a_active) {
    comp->internalStates[state_keys::kLastActiveSolenoid] = 2.0f;
  }
}

float Clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float GetExhaustQuality(const std::map<PortRef, float>& exhaust_quality,
                        const PortRef& port) {
  auto it = exhaust_quality.find(port);
  if (it == exhaust_quality.end()) {
    return 0.0f;
  }
  return Clamp01(it->second);
}

bool IsPortConnected(const std::map<PortRef, bool>& pneumatic_connected,
                     const PortRef& port) {
  auto it = pneumatic_connected.find(port);
  if (it == pneumatic_connected.end()) {
    return false;
  }
  return it->second;
}

void UpdateCylinderPhysics(PlacedComponent* comp,
                           const std::map<PortRef, float>& pressures,
                           const std::map<PortRef, float>& exhaust_quality,
                           const std::map<PortRef, bool>& pneumatic_connected,
                           float delta_time,
                           float piston_mass,
                           float friction_coeff,
                           float activation_threshold,
                           float force_scale) {
  if (!comp || comp->type != ComponentType::CYLINDER) {
    return;
  }

  if (!comp->internalStates.count(state_keys::kPosition)) {
    comp->internalStates[state_keys::kPosition] = 0.0f;
    comp->internalStates[state_keys::kVelocity] = 0.0f;
  }

  float current_position = comp->internalStates.at(state_keys::kPosition);
  float current_velocity = comp->internalStates.at(state_keys::kVelocity);

  PortRef port_a = {comp->instanceId, 0};
  PortRef port_b = {comp->instanceId, 1};
  float pressure_a =
      pressures.count(port_a) ? pressures.at(port_a) : 0.0f;
  float pressure_b =
      pressures.count(port_b) ? pressures.at(port_b) : 0.0f;

  comp->internalStates[state_keys::kPressureA] = pressure_a;
  comp->internalStates[state_keys::kPressureB] = pressure_b;

  bool connected_a = IsPortConnected(pneumatic_connected, port_a);
  bool connected_b = IsPortConnected(pneumatic_connected, port_b);
  float exhaust_quality_a = GetExhaustQuality(exhaust_quality, port_a);
  float exhaust_quality_b = GetExhaustQuality(exhaust_quality, port_b);
  float effective_a = pressure_a * exhaust_quality_b;
  float effective_b = pressure_b * exhaust_quality_a;
  // Invert A/B drive direction to match cylinder orientation.
  float pressure_diff = effective_b - effective_a;
  if (!connected_a || !connected_b ||
      std::abs(pressure_diff) < activation_threshold) {
    comp->internalStates[state_keys::kVelocity] = 0.0f;
    comp->internalStates[state_keys::kStatus] = 0.0f;
    return;
  }
  float force = 0.0f;

  if (std::abs(pressure_diff) >= activation_threshold) {
    float effective_pressure =
        std::abs(pressure_diff) - activation_threshold;
    float force_magnitude =
        std::pow(effective_pressure, 2.0f) * force_scale;
    force = std::copysign(force_magnitude, pressure_diff);
  }

  float friction_force = -friction_coeff * current_velocity;
  float total_force = force + friction_force;

  float acceleration = total_force / piston_mass;

  current_velocity += acceleration * delta_time;
  current_position += current_velocity * delta_time;

  const float max_stroke = 160.0f;
  if (current_position < 0.0f) {
    current_position = 0.0f;
    if (current_velocity < 0) {
      current_velocity = 0;
    }
  } else if (current_position > max_stroke) {
    current_position = max_stroke;
    if (current_velocity > 0) {
      current_velocity = 0;
    }
  }

  comp->internalStates[state_keys::kPosition] = current_position;
  comp->internalStates[state_keys::kVelocity] = current_velocity;

  const float velocity_threshold = 1.0f;
  if (current_velocity > velocity_threshold) {
    comp->internalStates[state_keys::kStatus] = 1.0f;
  } else if (current_velocity < -velocity_threshold) {
    comp->internalStates[state_keys::kStatus] = -1.0f;
  } else {
    comp->internalStates[state_keys::kStatus] = 0.0f;
  }
}

}  // namespace

const ComponentPhysicsAdapter* GetComponentPhysicsAdapter(ComponentType type) {
  constexpr int kComponentTypeCount = 20;
  static_assert(static_cast<int>(ComponentType::EMERGENCY_STOP) ==
                    kComponentTypeCount - 1,
                "Update physics adapter table for new component types.");

  static const ComponentPhysicsAdapter kAdapters[kComponentTypeCount] = {
      // PLC
      {nullptr, nullptr, nullptr},
      // FRL
      {nullptr, nullptr, nullptr},
      // MANIFOLD
      {nullptr, nullptr, nullptr},
      // LIMIT_SWITCH
      {nullptr, nullptr, nullptr},
      // SENSOR
      {UpdateSensorElectrical, nullptr, nullptr},
      // CYLINDER
      {nullptr, nullptr, UpdateCylinderPhysics},
      // VALVE_SINGLE
      {UpdateValveElectrical, nullptr, nullptr},
      // VALVE_DOUBLE
      {UpdateValveElectrical, UpdateValveLogic, nullptr},
      // BUTTON_UNIT
      {UpdateButtonUnitElectrical, nullptr, nullptr},
      // POWER_SUPPLY
      {nullptr, nullptr, nullptr},
      // WORKPIECE_METAL
      {nullptr, nullptr, nullptr},
      // WORKPIECE_NONMETAL
      {nullptr, nullptr, nullptr},
      // RING_SENSOR
      {UpdateSensorElectrical, nullptr, nullptr},
      // METER_VALVE
      {nullptr, nullptr, nullptr},
      // INDUCTIVE_SENSOR
      {UpdateSensorElectrical, nullptr, nullptr},
      // CONVEYOR
      {UpdateConveyorElectrical, nullptr, nullptr},
      // PROCESSING_CYLINDER
      {UpdateProcessingCylinderElectrical, nullptr, nullptr},
      // BOX
      {nullptr, nullptr, nullptr},
      // TOWER_LAMP
      {UpdateTowerLampElectrical, nullptr, nullptr},
      // EMERGENCY_STOP
      {nullptr, nullptr, nullptr}};

  int index = static_cast<int>(type);
  if (index < 0 || index >= kComponentTypeCount) {
    return nullptr;
  }
  return &kAdapters[index];
}

}  // namespace plc
