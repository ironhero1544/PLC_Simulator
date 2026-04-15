// physics_ports.cpp
//
// Port mapping helpers for the physics layer.

#include "plc_emulator/core/application.h"

#include <utility>
#include <vector>

namespace plc {

std::vector<std::pair<int, bool>> Application::GetPortsForComponent(
    const PlacedComponent& comp) {
  std::vector<std::pair<int, bool>> ports;
  int max_ports = 0;
  switch (comp.type) {
    case ComponentType::PLC:
      max_ports = 32;
      break;
    case ComponentType::FRL:
      max_ports = 1;
      break;
    case ComponentType::MANIFOLD:
      max_ports = 5;
      break;
    case ComponentType::LIMIT_SWITCH:
      max_ports = 3;
      break;
    case ComponentType::SENSOR:
      max_ports = 3;
      break;
    case ComponentType::CYLINDER:
      max_ports = 2;
      break;
    case ComponentType::VALVE_SINGLE:
      max_ports = 5;
      break;
    case ComponentType::VALVE_DOUBLE:
      max_ports = 7;
      break;
    case ComponentType::BUTTON_UNIT:
      max_ports = 15;
      break;
    case ComponentType::POWER_SUPPLY:
      max_ports = 2;
      break;
    case ComponentType::WORKPIECE_METAL:
    case ComponentType::WORKPIECE_NONMETAL:
      max_ports = 0;
      break;
    case ComponentType::RING_SENSOR:
      max_ports = 3;
      break;
    case ComponentType::METER_VALVE:
      max_ports = 2;
      break;
    case ComponentType::INDUCTIVE_SENSOR:
      max_ports = 3;
      break;
    case ComponentType::CONVEYOR:
      max_ports = 2;
      break;
    case ComponentType::PROCESSING_CYLINDER:
      max_ports = 7;
      break;
    case ComponentType::BOX:
      max_ports = 0;
      break;
    case ComponentType::TOWER_LAMP:
      max_ports = 4;
      break;
    case ComponentType::EMERGENCY_STOP:
      max_ports = 4;
      break;
    case ComponentType::RTL_MODULE:
      for (const auto& port : comp.runtimePorts) {
        ports.push_back({port.id, port.isInput});
      }
      return ports;
    default:
      max_ports = 0;
      break;
  }
  for (int i = 0; i < max_ports; ++i) {
    ports.push_back({i, true});
  }
  return ports;
}


}  // namespace plc
