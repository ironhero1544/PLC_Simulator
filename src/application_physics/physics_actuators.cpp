// physics_actuators.cpp
//
// Component logic and actuator updates.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/physics/component_physics_adapter.h"

#include "plc_emulator/application_physics/physics_helpers.h"
#include "plc_emulator/application_physics/physics_task_runner.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace plc {
namespace {

struct ValveLogicContext {
  std::vector<PlacedComponent>* components = nullptr;
};

void UpdateValveLogicComponent(size_t index, void* context) {
  auto* ctx = static_cast<ValveLogicContext*>(context);
  if (!ctx || !ctx->components) {
    return;
  }
  if (index >= ctx->components->size()) {
    return;
  }

  auto& comp = (*ctx->components)[index];
  const ComponentPhysicsAdapter* adapter =
      GetComponentPhysicsAdapter(comp.type);
  if (adapter && adapter->UpdateValveLogic) {
    adapter->UpdateValveLogic(&comp);
  }
}

struct CylinderUpdateContext {
  std::vector<PlacedComponent>* components = nullptr;
  const std::map<PortRef, float>* pressures = nullptr;
  const std::map<PortRef, float>* exhaust_quality = nullptr;
  const std::map<PortRef, bool>* pneumatic_connected = nullptr;
  float delta_time = 0.0f;
  float piston_mass = 0.0f;
  float friction_coeff = 0.0f;
  float activation_threshold = 0.0f;
  float force_scale = 0.0f;
};

void UpdateCylinderComponent(size_t index, void* context) {
  auto* ctx = static_cast<CylinderUpdateContext*>(context);
  if (!ctx || !ctx->components || !ctx->pressures ||
      !ctx->exhaust_quality || !ctx->pneumatic_connected) {
    return;
  }
  if (index >= ctx->components->size()) {
    return;
  }

  auto& comp = (*ctx->components)[index];
  const ComponentPhysicsAdapter* adapter =
      GetComponentPhysicsAdapter(comp.type);
  if (adapter && adapter->UpdateCylinder) {
    adapter->UpdateCylinder(&comp, *ctx->pressures, *ctx->exhaust_quality,
                            *ctx->pneumatic_connected, ctx->delta_time,
                            ctx->piston_mass, ctx->friction_coeff,
                            ctx->activation_threshold, ctx->force_scale);
  }
}

}  // namespace

void Application::UpdateComponentLogicImpl() {
  ValveLogicContext update_context;
  update_context.components = &placed_components_;
  RunComponentTasks(placed_components_.size(), UpdateValveLogicComponent,
                    &update_context);
}

void Application::UpdateActuatorsImpl(float delta_time,
                                      bool skip_cylinder_update) {
  const float deltaTime = delta_time;
  const float pistonMass = 0.5f;
  const float frictionCoeff = 0.02f;

  const float ACTIVATION_PRESSURE_THRESHOLD = 2.5f;
  const float FORCE_SCALING_FACTOR = 15.0f;

  CylinderUpdateContext update_context;
  update_context.components = &placed_components_;
  update_context.pressures = &port_pressures_;
  update_context.exhaust_quality = &port_exhaust_quality_;
  update_context.pneumatic_connected = &port_pneumatic_connected_;
  update_context.delta_time = deltaTime;
  update_context.piston_mass = pistonMass;
  update_context.friction_coeff = frictionCoeff;
  update_context.activation_threshold = ACTIVATION_PRESSURE_THRESHOLD;
  update_context.force_scale = FORCE_SCALING_FACTOR;
  if (!skip_cylinder_update) {
    RunComponentTasks(placed_components_.size(), UpdateCylinderComponent,
                      &update_context);
  }

  constexpr float kProcDrillUp = 30.0f;
  constexpr float kProcDrillDown = 15.0f;
  constexpr float kProcDrillSpeed = 20.0f;
  constexpr float kProcPressureThreshold = 2.5f;
  constexpr float kProcRotationSpeed = 6.0f;

  for (auto& comp : placed_components_) {
    if (comp.type != ComponentType::PROCESSING_CYLINDER) {
      continue;
    }
    if (!comp.internalStates.count(state_keys::kPosition)) {
      comp.internalStates[state_keys::kPosition] = kProcDrillUp;
    }

    float pos_val = comp.internalStates.at(state_keys::kPosition);
    PortRef port_a = {comp.instanceId, 5};
    PortRef port_b = {comp.instanceId, 6};
    float pressure_a =
        port_pressures_.count(port_a) ? port_pressures_.at(port_a) : 0.0f;
    float pressure_b =
        port_pressures_.count(port_b) ? port_pressures_.at(port_b) : 0.0f;
    bool connected_a = port_pneumatic_connected_.count(port_a) &&
                       port_pneumatic_connected_.at(port_a);
    bool connected_b = port_pneumatic_connected_.count(port_b) &&
                       port_pneumatic_connected_.at(port_b);
    float exhaust_quality_a =
        port_exhaust_quality_.count(port_a)
            ? Clamp01(port_exhaust_quality_.at(port_a))
            : 0.0f;
    float exhaust_quality_b =
        port_exhaust_quality_.count(port_b)
            ? Clamp01(port_exhaust_quality_.at(port_b))
            : 0.0f;
    float effective_a = pressure_a * exhaust_quality_b;
    float effective_b = pressure_b * exhaust_quality_a;

    bool up_cmd = connected_a && connected_b &&
                  effective_a >= kProcPressureThreshold;
    bool down_cmd = connected_a && connected_b &&
                    effective_b >= kProcPressureThreshold;
    float target = pos_val;
    if (up_cmd && !down_cmd) {
      target = kProcDrillUp;
    } else if (down_cmd && !up_cmd) {
      target = kProcDrillDown;
    } else if (up_cmd && down_cmd) {
      target = (effective_a >= effective_b) ? kProcDrillUp : kProcDrillDown;
    }

    if (target > pos_val) {
      pos_val += kProcDrillSpeed * deltaTime;
      if (pos_val > target) {
        pos_val = target;
      }
    } else if (target < pos_val) {
      pos_val -= kProcDrillSpeed * deltaTime;
      if (pos_val < target) {
        pos_val = target;
      }
    }
    pos_val = std::max(kProcDrillDown, std::min(kProcDrillUp, pos_val));
    comp.internalStates[state_keys::kPosition] = pos_val;

    bool motor_on = comp.internalStates.count(state_keys::kMotorOn) &&
                    comp.internalStates.at(state_keys::kMotorOn) > 0.5f;
    if (motor_on) {
      float angle = comp.internalStates.count(state_keys::kRotationAngle)
                        ? comp.internalStates.at(state_keys::kRotationAngle)
                        : 0.0f;
      angle += kProcRotationSpeed * deltaTime;
      if (angle > 6.28318530718f) {
        angle -= 6.28318530718f;
      }
      comp.internalStates[state_keys::kRotationAngle] = angle;
    }
  }

  // Sensor and limit switch detection system.
  bool sensors_from_box2d = UpdateSensorsBox2d();
  if (!sensors_from_box2d) {
    static int debugCounter = 0;
    bool enableDetailedDebug = (++debugCounter % 60 == 0);

    for (auto& sensor : placed_components_) {
      if (sensor.type == ComponentType::LIMIT_SWITCH ||
          sensor.type == ComponentType::SENSOR) {
        float detectionRange =
            (sensor.type == ComponentType::LIMIT_SWITCH) ? 100.0f : 75.0f;
        float beamHalfWidth =
            (sensor.type == ComponentType::LIMIT_SWITCH) ? 10.0f : 15.0f;
        ImVec2 sensor_center =
            (sensor.type == ComponentType::LIMIT_SWITCH)
                ? GetLimitSwitchCenterWorld(sensor)
                : GetSensorCenterWorld(sensor);
        ImVec2 sensor_forward = LocalDirToWorld(sensor, ImVec2(0.0f, -1.0f));
        bool isActivatedByPhysics = false;
        bool isActivatedByWorkpiece = false;
        float minDistance = 999999.0f;
        int closestCylinderId = -1;

        for (const auto& workpiece : placed_components_) {
          if (!IsWorkpiece(workpiece.type)) {
            continue;
          }
          ImVec2 workpiece_center = {
              workpiece.position.x + workpiece.size.width * 0.5f,
              workpiece.position.y + workpiece.size.height * 0.5f};
          if (IsDirectionalHit(sensor_center, sensor_forward, workpiece_center,
                               detectionRange, beamHalfWidth, nullptr)) {
            isActivatedByWorkpiece = true;
            break;
          }
        }

        for (const auto& cylinder : placed_components_) {
          if (cylinder.type == ComponentType::CYLINDER) {
            ImVec2 piston_tip = GetCylinderRodTipWorld(cylinder);
            float distance = 0.0f;
            if (IsDirectionalHit(sensor_center, sensor_forward, piston_tip,
                                 detectionRange, beamHalfWidth, &distance)) {
              isActivatedByPhysics = true;
              closestCylinderId = cylinder.instanceId;

              if (enableDetailedDebug && enable_debug_logging_) {
                std::cout << "[SENSOR] "
                          << (sensor.type == ComponentType::LIMIT_SWITCH
                                  ? "LimitSwitch"
                                  : "Sensor")
                          << "[" << sensor.instanceId
                          << "] detected at distance " << distance << "mm"
                          << std::endl;
              }
              break;
            }
            if (distance < minDistance) {
              minDistance = distance;
            }
          }
        }

        (void)closestCylinderId;  // Unused variable

        if (sensor.type == ComponentType::LIMIT_SWITCH) {
          bool isPressedManually =
              sensor.internalStates.count(state_keys::kIsPressedManual) &&
              sensor.internalStates.at(state_keys::kIsPressedManual) > 0.5f;
          sensor.internalStates["is_pressed"] =
              (isActivatedByPhysics || isActivatedByWorkpiece ||
               isPressedManually)
                  ? 1.0f
                  : 0.0f;
        } else {
          sensor.internalStates["is_detected"] =
              (isActivatedByPhysics || isActivatedByWorkpiece) ? 1.0f : 0.0f;
        }
      }
    }
  }

  UpdateWorkpieceInteractions(deltaTime);
}

void Application::UpdateBasicPhysicsImpl(float delta_time) {
  // Cylinder physics simulation.
  const float deltaTime = delta_time;
  const float pistonMass = 0.5f;
  const float frictionCoeff = 0.02f;
  const float ACTIVATION_PRESSURE_THRESHOLD = 2.5f;
  const float FORCE_SCALING_FACTOR = 15.0f;

  for (auto& comp : placed_components_) {
    if (comp.type == ComponentType::CYLINDER) {
      if (!comp.internalStates.count("position")) {
        comp.internalStates["position"] = 0.0f;
        comp.internalStates["velocity"] = 0.0f;
      }

      float currentPosition = comp.internalStates.at("position");
      float currentVelocity = comp.internalStates.at("velocity");

      float pressureA = port_pressures_.count({comp.instanceId, 0})
                            ? port_pressures_.at({comp.instanceId, 0})
                            : 0.0f;
      float pressureB = port_pressures_.count({comp.instanceId, 1})
                            ? port_pressures_.at({comp.instanceId, 1})
                            : 0.0f;

      float pressureDiff = pressureA - pressureB;
      float force = 0.0f;

      if (std::abs(pressureDiff) >= ACTIVATION_PRESSURE_THRESHOLD) {
        float effectivePressure =
            std::abs(pressureDiff) - ACTIVATION_PRESSURE_THRESHOLD;
        float forceMagnitude =
            std::pow(effectivePressure, 2.0f) * FORCE_SCALING_FACTOR;
        force = std::copysign(forceMagnitude, pressureDiff);
      }

      float frictionForce = -frictionCoeff * currentVelocity;
      float totalForce = force + frictionForce;
      float acceleration = totalForce / pistonMass;

      currentVelocity += acceleration * deltaTime;
      currentPosition += currentVelocity * deltaTime;

      const float maxStroke = 160.0f;
      if (currentPosition < 0.0f) {
        currentPosition = 0.0f;
        if (currentVelocity < 0) {
          currentVelocity = 0;
        }
      } else if (currentPosition > maxStroke) {
        currentPosition = maxStroke;
        if (currentVelocity > 0) {
          currentVelocity = 0;
        }
      }

      comp.internalStates["position"] = currentPosition;
      comp.internalStates["velocity"] = currentVelocity;
      comp.internalStates["pressure_a"] = pressureA;
      comp.internalStates["pressure_b"] = pressureB;

      const float velocityThreshold = 1.0f;
      if (currentVelocity > velocityThreshold) {
        comp.internalStates["status"] = 1.0f;
      } else if (currentVelocity < -velocityThreshold) {
        comp.internalStates["status"] = -1.0f;
      } else {
        comp.internalStates["status"] = 0.0f;
      }
    }
  }

  // Sensor detection logic (independent of PLC state).
  bool sensors_from_box2d = UpdateSensorsBox2d();
  if (!sensors_from_box2d) {
    static int debugCounter = 0;
    (void)(++debugCounter);  // Suppress unused variable warning.

    for (auto& sensor : placed_components_) {
      if (sensor.type == ComponentType::LIMIT_SWITCH ||
          sensor.type == ComponentType::SENSOR) {
        float detectionRange =
            (sensor.type == ComponentType::LIMIT_SWITCH) ? 100.0f : 75.0f;
        float beamHalfWidth =
            (sensor.type == ComponentType::LIMIT_SWITCH) ? 10.0f : 15.0f;
        ImVec2 sensor_center =
            (sensor.type == ComponentType::LIMIT_SWITCH)
                ? GetLimitSwitchCenterWorld(sensor)
                : GetSensorCenterWorld(sensor);
        ImVec2 sensor_forward = LocalDirToWorld(sensor, ImVec2(0.0f, -1.0f));
        bool isActivatedByPhysics = false;
        bool isActivatedByWorkpiece = false;
        float minDistance = 999999.0f;
        int closestCylinderId = -1;
        (void)closestCylinderId;  // Unused variable.
        for (const auto& workpiece : placed_components_) {
          if (!IsWorkpiece(workpiece.type)) {
            continue;
          }
          ImVec2 workpiece_center = {
              workpiece.position.x + workpiece.size.width * 0.5f,
              workpiece.position.y + workpiece.size.height * 0.5f};
          if (IsDirectionalHit(sensor_center, sensor_forward, workpiece_center,
                               detectionRange, beamHalfWidth, nullptr)) {
            isActivatedByWorkpiece = true;
            break;
          }
        }

        for (const auto& cylinder : placed_components_) {
          if (cylinder.type == ComponentType::CYLINDER) {
            ImVec2 piston_tip = GetCylinderRodTipWorld(cylinder);
            float distance = 0.0f;
            if (IsDirectionalHit(sensor_center, sensor_forward, piston_tip,
                                 detectionRange, beamHalfWidth, &distance)) {
              isActivatedByPhysics = true;
              closestCylinderId = cylinder.instanceId;
              break;
            }
            if (distance < minDistance) {
              minDistance = distance;
            }
          }
        }

        // Update sensor state.
        if (sensor.type == ComponentType::LIMIT_SWITCH) {
          bool isPressedManually =
              sensor.internalStates.count("is_pressed_manual") &&
              sensor.internalStates.at("is_pressed_manual") > 0.5f;
          sensor.internalStates["is_pressed"] =
              (isActivatedByPhysics || isActivatedByWorkpiece ||
               isPressedManually)
                  ? 1.0f
                  : 0.0f;
        } else {
          sensor.internalStates["is_detected"] =
              (isActivatedByPhysics || isActivatedByWorkpiece) ? 1.0f : 0.0f;
        }
      }
    }
  }

  // Basic simulation for PLC STOP state.
  if (!is_plc_running_) {
    SimulateElectrical();
    UpdateComponentLogic();
    SimulatePneumatic();
  }
}

}  // namespace plc
