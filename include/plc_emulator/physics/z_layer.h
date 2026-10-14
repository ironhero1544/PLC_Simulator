#pragma once

#include <algorithm>

#include "plc_emulator/components/servo_component.h"
#include <cmath>

#include "plc_emulator/core/data_types.h"

namespace plc_emulator {

inline bool IsValidZLayerConfig(const plc::ZLayerConfig& config) {
  return std::isfinite(config.min_layer) &&
         std::isfinite(config.max_layer) &&
         std::isfinite(config.initial_height) &&
         config.min_layer >= 1.0f && config.min_layer <= config.max_layer &&
         config.initial_height >= config.min_layer &&
         config.initial_height <= config.max_layer;
}

inline float ResolveContactHeight(const plc::PlacedComponent& component) {
  if (component.type == plc::ComponentType::PROCESSING_CYLINDER) {
    const auto position = component.internalStates.find("position");
    const float stroke = position == component.internalStates.end()
                             ? 30.0f : position->second;
    const float fraction = std::clamp((stroke - 15.0f) / 15.0f, 0.0f, 1.0f);
    return component.z_config.min_layer + fraction *
        (component.z_config.max_layer - component.z_config.min_layer);
  }
  if (component.type == plc::ComponentType::kStopperCylinder) {
    const auto position = component.internalStates.find("position");
    const float stroke = position == component.internalStates.end()
                             ? 0.0f : position->second;
    const float fraction = std::clamp(stroke / 160.0f, 0.0f, 1.0f);
    return component.z_config.max_layer - fraction *
        (component.z_config.max_layer - component.z_config.min_layer);
  }
  return component.z_state.current_height;
}

inline bool CanInteractAtHeight(float first, float second) {
  constexpr float kHeightTolerance = 0.01f;
  return std::isfinite(first) && std::isfinite(second) &&
         std::abs(first - second) <= kHeightTolerance;
}

inline bool CanInteractAtHeight(const plc::PlacedComponent& first,
                               const plc::PlacedComponent& second) {
  return CanInteractAtHeight(ResolveContactHeight(first),
                            ResolveContactHeight(second));
}

inline void InitializeZLayerState(plc::PlacedComponent* component) {
  if (!component) {
    return;
  }
  const auto& config = component->z_config;
  component->z_state.current_height = config.initial_height;
  if (IsServoLift(component->type)) {
    const float span = config.max_layer - config.min_layer;
    const double fraction = span > 0
        ? (config.initial_height - config.min_layer) / span : 0;
    component->axis_state = {};
    component->axis_state.position = fraction * component->axis_settings.stroke_mm /
                                     component->axis_settings.lead_mm;
    component->axis_state.target = component->axis_state.position;
  } else if (component->type == plc::ComponentType::PROCESSING_CYLINDER) {
    const float span = config.max_layer - config.min_layer;
    const float fraction = span > 0.0f
        ? (config.initial_height - config.min_layer) / span : 0.0f;
    component->internalStates["position"] = 15.0f + fraction * 15.0f;
  } else if (component->type == plc::ComponentType::kStopperCylinder) {
    const float span = config.max_layer - config.min_layer;
    const float fraction = span > 0
        ? (config.max_layer - config.initial_height) / span : 0;
    component->internalStates["position"] = fraction * 160;
  }
}

}  // namespace plc_emulator
