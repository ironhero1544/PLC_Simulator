#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/physics/z_layer.h"

namespace plc_emulator {

// Stock rests on a ring sensor: one piece at its contact height, all waiting
// pieces one layer above. Legacy magazines remain readable in saved projects.
inline bool IsGravityFeedSource(plc::ComponentType type) {
  return type == plc::ComponentType::RING_SENSOR ||
         type == plc::ComponentType::kMagazine;
}
inline void UpdateRingFeedWorkpieces(
    std::vector<plc::PlacedComponent>* components) {
  if (!components)
    return;
  for (const auto& feed_sensor : *components) {
    if (!IsGravityFeedSource(feed_sensor.type) ||
        feed_sensor.z_config.max_layer - feed_sensor.z_config.min_layer < 1)
      continue;
    const ImVec2 outlet = plc::LocalToWorld(
        feed_sensor, feed_sensor.type == plc::ComponentType::RING_SENSOR
                      ? ImVec2(30, 28) : ImVec2(50, 50));
    int stock_count = 0;
    for (const auto& piece : *components) {
      if (plc::IsWorkpiece(piece.type) &&
          piece.carry_state.owner_id == feed_sensor.instanceId)
        stock_count = std::max(stock_count, piece.carry_state.slot_id);
    }
    for (auto& piece : *components) {
      if (!plc::IsWorkpiece(piece.type) || piece.carry_state.owner_id >= 0 ||
          piece.carry_state.release_source_id == feed_sensor.instanceId ||
          ReadPneumaticState(piece, "is_stuck_box") > 0.5f ||
          ReadPneumaticState(piece, "is_manual_drag") > 0.5f)
        continue;
      const ImVec2 center = plc::GetComponentWorldCenter(piece);
      if (piece.z_state.current_height < feed_sensor.z_state.current_height - 0.01f ||
          piece.z_state.current_height > feed_sensor.z_state.current_height + 1.01f ||
          std::abs(ReadPneumaticState(piece, "vel_x")) > 0.01f ||
          std::abs(ReadPneumaticState(piece, "vel_y")) > 0.01f ||
          std::hypot(center.x - outlet.x, center.y - outlet.y) > 18)
        continue;
      piece.carry_state.owner_id = feed_sensor.instanceId;
      piece.carry_state.slot_id = ++stock_count;
    }
    bool outlet_clear = true;
    bool drive_clear = true;
    for (const auto& part : *components) {
      if (plc::IsWorkpiece(part.type) &&
          part.carry_state.owner_id != feed_sensor.instanceId &&
          CanInteractAtHeight(part, feed_sensor)) {
        const ImVec2 center = plc::GetComponentWorldCenter(part);
        if (std::hypot(center.x - outlet.x, center.y - outlet.y) < 36)
          outlet_clear = false;
      }
      if (part.type != plc::ComponentType::CYLINDER &&
          part.type != plc::ComponentType::kSensorCylinder)
        continue;
      const ImVec2 tip = plc::LocalToWorld(
          part,
          {170, part.type == plc::ComponentType::CYLINDER ? 25.0f : 43.0f});
      if (std::hypot(tip.x - outlet.x, tip.y - outlet.y) <= 45 &&
          CanInteractAtHeight(part, feed_sensor) &&
          (ReadPneumaticState(part, "position") >= 1 ||
           std::abs(ReadPneumaticState(part, "velocity")) > 0.01f))
        drive_clear = false;
    }
    int first_id = -1;
    int first_slot = 2147483647;
    for (const auto& piece : *components) {
      if (piece.carry_state.owner_id == feed_sensor.instanceId &&
          piece.carry_state.slot_id < first_slot) {
        first_slot = piece.carry_state.slot_id;
        first_id = piece.instanceId;
      }
    }
    for (auto& piece : *components) {
      if (piece.carry_state.owner_id != feed_sensor.instanceId)
        continue;
      const plc::Size size = plc::GetComponentDisplaySize(piece);
      piece.position = {outlet.x - size.width / 2, outlet.y - size.height / 2};
      piece.z_state.current_height = feed_sensor.z_state.current_height + 1;
      piece.internalStates["vel_x"] = 0;
      piece.internalStates["vel_y"] = 0;
      if (piece.instanceId == first_id && outlet_clear && drive_clear) {
        piece.carry_state = {};
        piece.carry_state.release_source_id = feed_sensor.instanceId;
        piece.z_state.current_height = feed_sensor.z_state.current_height;
      }
    }
  }
}

}  // namespace plc_emulator
