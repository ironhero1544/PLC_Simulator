#pragma once

#include <algorithm>

#include "imgui.h"

#include "plc_emulator/application_physics/physics_helpers.h"
#include "plc_emulator/core/data_types.h"

namespace plc_emulator {

constexpr float kCylinderStroke = 160.0f;
constexpr float kWarehouseDriveStroke = 70.0f;
constexpr int kSensorSupplyPort = 2;
constexpr int kSensorGroundPort = 3;
constexpr int kForwardSensorPort = 4;
constexpr int kReverseSensorPort = 5;
constexpr int kVacuumPort = 6;

constexpr bool IsSensorCylinder(plc::ComponentType type) {
  return type == plc::ComponentType::kDrivenWarehouse ||
         type == plc::ComponentType::kSensorCylinder ||
         type == plc::ComponentType::kSuctionCylinder ||
         type == plc::ComponentType::kStopperCylinder ||
         type == plc::ComponentType::kLiftSuction;
}

constexpr bool IsLinearPneumaticCylinder(plc::ComponentType type) {
  return type == plc::ComponentType::CYLINDER ||
         type == plc::ComponentType::kSensorCylinder ||
         type == plc::ComponentType::kSuctionCylinder ||
         type == plc::ComponentType::kLiftSuction;
}

constexpr bool UsesSharedPneumaticRuntime(plc::ComponentType type) {
  return IsSensorCylinder(type) ||
         type == plc::ComponentType::kVacuumGenerator;
}

inline float ReadPneumaticState(const plc::PlacedComponent& component,
                               const char* key, float fallback = 0.0f) {
  const auto value = component.internalStates.find(key);
  return value == component.internalStates.end() ? fallback : value->second;
}

inline bool IsForwardEndpoint(const plc::PlacedComponent& component) {
  const float stroke = component.type == plc::ComponentType::kDrivenWarehouse
                           ? kWarehouseDriveStroke : kCylinderStroke;
  return ReadPneumaticState(component, "position") >= stroke - 0.1f;
}

inline bool IsReverseEndpoint(const plc::PlacedComponent& component) {
  return ReadPneumaticState(component, "position") <= 0.1f;
}

inline ImVec2 GetSuctionHeadLocal(const plc::PlacedComponent& component) {
  const float stroke = std::clamp(ReadPneumaticState(component, "position"),
                                  0.0f, kCylinderStroke);
  if (component.type == plc::ComponentType::kLiftSuction) {
    return ImVec2(119.0f + stroke, 161.0f);
  }
  return ImVec2(162.5f - stroke, 49.0f);
}

inline ImVec2 GetSuctionHeadWorld(const plc::PlacedComponent& component) {
  ImVec2 head = GetSuctionHeadLocal(component);
  if (component.type != plc::ComponentType::kLiftSuction) head.y = 61.0f;
  return plc::LocalToWorldCached(component, head);
}

inline plc::Position GetPneumaticPortPosition(
    const plc::PlacedComponent& component, int port_id,
    plc::Position definition_position) {
  if ((component.type == plc::ComponentType::kSuctionCylinder ||
       component.type == plc::ComponentType::kLiftSuction) &&
      port_id == kVacuumPort) {
    return GetSuctionHeadLocal(component);
  }
  return definition_position;
}

}  // namespace plc_emulator
