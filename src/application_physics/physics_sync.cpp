// physics_sync.cpp
//
// Synchronization between PLC/application state and physics engine.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/component_transform.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <utility>

namespace plc {

void Application::SyncPLCOutputsToPhysicsEngine() {
  if (!physics_engine_ || !physics_engine_->isInitialized) {
    return;
  }

  ElectricalNetwork* elecNet = physics_engine_->electricalNetwork;
  if (!elecNet || elecNet->nodeCount == 0 || !elecNet->nodes) {
    return;
  }

  // Find PLC component.
  for (const auto& comp : placed_components_) {
    if (comp.type != ComponentType::PLC)
      continue;

    // PLC Y OUTPUT SYNCHRONIZATION (ports 16-31) with bounds checking
    for (int y = 0; y < 16; y++) {
      std::string yAddress = "Y" + std::to_string(y);
      bool yState = GetPlcDeviceState(yAddress);

      // SAFE NODE LOOKUP: Multiple bounds checks prevent array violations
      int nodeId = -1;
      if (comp.instanceId < physics_engine_->maxComponents) {
        // PLC Y ports mapped to port IDs 16+y
        int portId = 16 + y;
        if (portId < 32) {
          nodeId = physics_engine_
                       ->componentPortToElectricalNode[comp.instanceId][portId];
        }
      }

      if (nodeId >= 0 && nodeId < elecNet->nodeCount) {
        ElectricalNode& node = elecNet->nodes[nodeId];

        if (yState) {
          // Y OUTPUT ON: Low voltage (sinking type PLC output)
          node.isVoltageSource = true;
          node.sourceVoltage = 0.0f;
          node.sourceResistance = 0.1f;  // 0.1 ohm output resistance
        } else {
          // Y OUTPUT OFF: High impedance (open circuit)
          node.isVoltageSource = false;
          node.sourceVoltage = 0.0f;
          node.sourceResistance = 1e6f;  // 1M ohm (effectively open)
        }

        // PHYSICS STATE SYNCHRONIZATION with bounds checking
        int physicsIndex =
            physics_engine_->componentIdToPhysicsIndex[comp.instanceId];
        if (physicsIndex >= 0 &&
            physicsIndex < physics_engine_->activeComponents) {
          TypedPhysicsState& physState =
              physics_engine_->componentPhysicsStates[physicsIndex];
          if (physState.type == PHYSICS_STATE_PLC) {
            physState.state.plc.outputStates[y] = yState;
            physState.state.plc.outputVoltages[y] = yState ? 0.0f : -1.0f;
            physState.state.plc.outputCurrents[y] =
                yState ? 100.0f : 0.0f;  // 100mA when ON
          }
        }
      }
    }

    // PLC X input port voltage reading from physics engine with bounds checking
    for (int x = 0; x < 16; x++) {
      int nodeId = -1;
      if (comp.instanceId >= 0 &&
          comp.instanceId < physics_engine_->maxComponents && x >= 0 &&
          x < 32 && physics_engine_->componentPortToElectricalNode != nullptr) {
        nodeId =
            physics_engine_->componentPortToElectricalNode[comp.instanceId][x];
      }

      if (nodeId >= 0 && nodeId < elecNet->nodeCount) {
        ElectricalNode& node = elecNet->nodes[nodeId];

        // Set input impedance (high-impedance input)
        node.inputImpedance[0] = 10000.0f;  // 10k ohm
        node.responseTime = 0.001f;         // 1ms response time

        // PLCPhysicsState update with bounds checking
        int physicsIndex = -1;
        if (comp.instanceId >= 0 &&
            comp.instanceId < physics_engine_->maxComponents &&
            physics_engine_->componentIdToPhysicsIndex != nullptr) {
          physicsIndex =
              physics_engine_->componentIdToPhysicsIndex[comp.instanceId];
        }
        if (physicsIndex >= 0 &&
            physicsIndex < physics_engine_->activeComponents &&
            physics_engine_->componentPhysicsStates != nullptr) {
          TypedPhysicsState& physState =
              physics_engine_->componentPhysicsStates[physicsIndex];
          if (physState.type == PHYSICS_STATE_PLC) {
            physState.state.plc.inputVoltages[x] = node.voltage;
            physState.state.plc.inputCurrents[x] =
                node.current * 1000.0f;  // A -> mA
            physState.state.plc.inputStates[x] = (node.voltage > 0.0f);
          }
        }
      }
    }

    break;  // Assume a single PLC.
  }
}

// Synchronize physics engine results to Application
// Update internalStates, port_voltages_, port_pressures_ with simulation results
void Application::SyncPhysicsEngineToApplication() {
  // Basic null pointer checking
  if (!physics_engine_) {
    if (enable_debug_logging_) {
      std::cout << "[Physics] Cannot sync to application: engine is null"
                << std::endl;
    }
    return;
  }

  if (!physics_engine_->isInitialized) {
    if (enable_debug_logging_) {
      std::cout
          << "[Physics] Cannot sync to application: engine not initialized"
          << std::endl;
    }
    return;
  }

  // Network pointer safety checks
  ElectricalNetwork* elecNet = physics_engine_->electricalNetwork;
  PneumaticNetwork* pneuNet = physics_engine_->pneumaticNetwork;
  MechanicalSystem* mechSys = physics_engine_->mechanicalSystem;

  if (!elecNet || !pneuNet || !mechSys) {
    if (enable_debug_logging_) {
      std::cout << "[Physics] Cannot sync to application: one or more networks "
                   "are null"
                << std::endl;
    }
    return;
  }

  (void)mechSys;  // Mechanical system synchronization not yet implemented

  // 1. Sync electrical network results to port_voltages_
  if (elecNet && elecNet->nodeCount > 0) {
    port_voltages_.clear();

    for (int n = 0; n < elecNet->nodeCount; n++) {
      const ElectricalNode& node = elecNet->nodes[n];
      if (node.base.isActive) {
        std::pair<int, int> portKey =
            std::make_pair(node.base.componentId, node.base.portId);
        port_voltages_[portKey] = node.voltage;
      }
    }
  }

  // 2. Sync pneumatic network results to port_pressures_
  if (pneuNet && pneuNet->nodeCount > 0) {
    port_pressures_.clear();

    for (int n = 0; n < pneuNet->nodeCount; n++) {
      const PneumaticNode& node = pneuNet->nodes[n];
      if (node.base.isActive) {
        std::pair<int, int> portKey =
            std::make_pair(node.base.componentId, node.base.portId);
        // Pa -> bar
        port_pressures_[portKey] = node.gaugePressure / 100000.0f;
      }
    }
  }

  bool has_canvas = canvas_size_.x > 1.0f && canvas_size_.y > 1.0f;
  bool force_full_sync = debug_mode_ || enable_debug_logging_;
  float view_min_x = 0.0f;
  float view_max_x = 0.0f;
  float view_min_y = 0.0f;
  float view_max_y = 0.0f;
  float view_margin = 0.0f;
  if (has_canvas) {
    ImVec2 screen_max = {canvas_top_left_.x + canvas_size_.x,
                         canvas_top_left_.y + canvas_size_.y};
    ImVec2 world_a = ScreenToWorld(canvas_top_left_);
    ImVec2 world_b = ScreenToWorld(screen_max);
    view_min_x = std::min(world_a.x, world_b.x);
    view_max_x = std::max(world_a.x, world_b.x);
    view_min_y = std::min(world_a.y, world_b.y);
    view_max_y = std::max(world_a.y, world_b.y);
    float zoom = camera_zoom_ > 0.05f ? camera_zoom_ : 0.05f;
    view_margin = 300.0f / zoom;
  }
  auto is_component_visible = [&](const PlacedComponent& comp) -> bool {
    if (!has_canvas) {
      return true;
    }
    const Size display = GetComponentDisplaySize(comp);
    float min_x = comp.position.x;
    float min_y = comp.position.y;
    float max_x = comp.position.x + display.width;
    float max_y = comp.position.y + display.height;
    if (max_x < view_min_x - view_margin || min_x > view_max_x + view_margin ||
        max_y < view_min_y - view_margin || min_y > view_max_y + view_margin) {
      return false;
    }
    return true;
  };

  // Component physics state synchronization to internalStates with bounds
  // checking
  for (auto& comp : placed_components_) {
    if (!force_full_sync && !is_component_visible(comp)) {
      continue;
    }
    int physicsIndex = -1;
    if (comp.instanceId >= 0 &&
        comp.instanceId < physics_engine_->maxComponents &&
        physics_engine_->componentIdToPhysicsIndex != nullptr) {
      physicsIndex =
          physics_engine_->componentIdToPhysicsIndex[comp.instanceId];
    }

    if (physicsIndex >= 0 &&
        physicsIndex < physics_engine_->activeComponents &&
        physics_engine_->componentPhysicsStates != nullptr) {
      const TypedPhysicsState& physState =
          physics_engine_->componentPhysicsStates[physicsIndex];

      switch (physState.type) {
        case PHYSICS_STATE_CYLINDER: {
          const CylinderPhysicsState& cylState = physState.state.cylinder;

          // Sync position, velocity, acceleration
          comp.internalStates["position"] = cylState.position;
          comp.internalStates["velocity"] = cylState.velocity;
          comp.internalStates["acceleration"] = cylState.acceleration;

          // Sync pressure
          comp.internalStates["pressure_a"] = cylState.pressureA;
          comp.internalStates["pressure_b"] = cylState.pressureB;

          // Sync state
          float status = 0.0f;
          if (cylState.isExtending)
            status = 1.0f;
          else if (cylState.isRetracting)
            status = -1.0f;
          comp.internalStates["status"] = status;

          // Sync force
          comp.internalStates["force"] = cylState.totalForce;
          comp.internalStates["pneumatic_force_a"] = cylState.pneumaticForceA;
          comp.internalStates["pneumatic_force_b"] = cylState.pneumaticForceB;
          comp.internalStates["friction_force"] = cylState.frictionForce;

          break;
        }

        case PHYSICS_STATE_LIMIT_SWITCH: {
          // Prefer basic directional detection for limit switches.
          break;
        }

        case PHYSICS_STATE_SENSOR: {
          const SensorPhysicsState& sensorState = physState.state.sensor;

          // Sync detection state
          bool prefer_basic_sensor =
              (comp.type == ComponentType::SENSOR ||
               comp.type == ComponentType::INDUCTIVE_SENSOR ||
               comp.type == ComponentType::RING_SENSOR);
          if (!prefer_basic_sensor) {
            comp.internalStates[state_keys::kIsDetected] =
                sensorState.detectionState ? 1.0f : 0.0f;
            comp.internalStates[state_keys::kIsPowered] =
                (sensorState.powerConsumption > 0.1f) ? 1.0f : 0.0f;
          } else {
            bool is_detected =
                comp.internalStates.count(state_keys::kIsDetected) &&
                comp.internalStates.at(state_keys::kIsDetected) > 0.5f;
            bool is_powered =
                comp.internalStates.count(state_keys::kIsPowered) &&
                comp.internalStates.at(state_keys::kIsPowered) > 0.5f;
            comp.internalStates["output_voltage"] =
                (is_detected && is_powered) ? 24.0f : 0.0f;
          }
          comp.internalStates["target_distance"] = sensorState.targetDistance;
          if (!prefer_basic_sensor) {
            comp.internalStates["output_voltage"] = sensorState.outputVoltage;
          }

          // Sync sensor performance
          comp.internalStates["sensitivity"] = sensorState.sensitivity;
          comp.internalStates["response_time"] = sensorState.responseTime;

          break;
        }

        case PHYSICS_STATE_VALVE_SINGLE: {
          const ValveSinglePhysicsState& valveState =
              physState.state.valveSingle;

          // Sync solenoid state
          comp.internalStates["solenoid_a_active"] =
              valveState.solenoidEnergized ? 1.0f : 0.0f;
          comp.internalStates["solenoid_voltage"] = valveState.solenoidVoltage;
          comp.internalStates["solenoid_current"] = valveState.solenoidCurrent;

          // Sync valve position/flow
          comp.internalStates["valve_position"] = valveState.valvePosition;
          comp.internalStates["flow_coefficient_pa"] =
              valveState.flowCoefficientPA;
          comp.internalStates["flow_coefficient_ar"] =
              valveState.flowCoefficientAR;

          break;
        }

        case PHYSICS_STATE_VALVE_DOUBLE: {
          const ValveDoublePhysicsState& valveState =
              physState.state.valveDouble;

          // Sync double-solenoid state
          comp.internalStates["solenoid_a_active"] =
              valveState.solenoidEnergizedA ? 1.0f : 0.0f;
          comp.internalStates["solenoid_b_active"] =
              valveState.solenoidEnergizedB ? 1.0f : 0.0f;
          comp.internalStates["last_active_solenoid"] =
              valveState.lastActivePosition;

          // Synchronize electrical characteristics
          comp.internalStates["solenoid_voltage_a"] =
              valveState.solenoidVoltageA;
          comp.internalStates["solenoid_voltage_b"] =
              valveState.solenoidVoltageB;
          comp.internalStates["solenoid_current_a"] =
              valveState.solenoidCurrentA;
          comp.internalStates["solenoid_current_b"] =
              valveState.solenoidCurrentB;

          // Sync valve position
          comp.internalStates["valve_position"] = valveState.valvePosition;

          break;
        }

        case PHYSICS_STATE_BUTTON_UNIT: {
          const ButtonUnitPhysicsState& buttonState =
              physState.state.buttonUnit;

          // Synchronize button states
          for (int i = 0; i < 3; i++) {
            comp.internalStates["is_pressed_" + std::to_string(i)] =
                buttonState.buttonStates[i] ? 1.0f : 0.0f;
            comp.internalStates["lamp_on_" + std::to_string(i)] =
                buttonState.ledStates[i] ? 1.0f : 0.0f;
            comp.internalStates["button_voltage_" + std::to_string(i)] =
                buttonState.buttonVoltages[i];
            comp.internalStates["led_brightness_" + std::to_string(i)] =
                buttonState.ledBrightness[i];
          }

          break;
        }

        // Other component types as needed...
        default:
          break;
      }
    }
  }

  // 4. Update PLC X inputs from physics results
  bool plc_error = false;
  if (programming_mode_ && programming_mode_->HasCompileAttempted()) {
    if (!programming_mode_->GetLastCompileError().empty() ||
        !programming_mode_->GetLastScanError().empty()) {
      plc_error = true;
    }
    if (is_plc_running_ && !programming_mode_->GetLastScanSuccess() &&
        !programming_mode_->GetLastScanError().empty()) {
      plc_error = true;
    }
  }

  for (auto& comp : placed_components_) {
    if (comp.type == ComponentType::PLC) {
      std::map<std::string, bool>
          xInputs;  // Buffer X inputs for ProgrammingMode
      for (int x = 0; x < 16; x++) {
        std::string xAddress = "X" + std::to_string(x);
        auto portKey = std::make_pair(comp.instanceId, x);

        bool x_active = false;
        if (port_voltages_.count(portKey)) {
          float voltage = port_voltages_.at(portKey);
          bool state = (voltage > 12.0f);  // 12V threshold
          SetPlcDeviceState(xAddress, state);
          // Forward to ProgrammingMode (single entry point)
          xInputs[xAddress] = state;
          x_active = state;
        } else {
          x_active = GetPlcDeviceState(xAddress);
        }
        comp.internalStates[std::string(state_keys::kPlcXPrefix) +
                            std::to_string(x)] =
            x_active ? 1.0f : 0.0f;
      }
      for (int y = 0; y < 16; ++y) {
        std::string yAddress = "Y" + std::to_string(y);
        comp.internalStates[std::string(state_keys::kPlcYPrefix) +
                            std::to_string(y)] =
            GetPlcDeviceState(yAddress) ? 1.0f : 0.0f;
      }
      // Apply input sync
      if (programming_mode_ && !xInputs.empty()) {
        programming_mode_->UpdateInputsFromSystem(xInputs);
      }
      comp.internalStates[state_keys::kPlcRunning] =
          is_plc_running_ ? 1.0f : 0.0f;
      comp.internalStates[state_keys::kPlcError] = plc_error ? 1.0f : 0.0f;
      break;
    }
  }
}

}  // namespace plc
