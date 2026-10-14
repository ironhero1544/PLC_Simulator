#pragma once

#include <algorithm>
#include <vector>

#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/physics/z_layer.h"
#include "plc_emulator/physics/storage_sensor_simulation.h"

namespace plc_emulator {

inline void UpdateSuctionWorkpieces(
    std::vector<plc::PlacedComponent>* components) {
  if (!components) {
    return;
  }
  UpdateWarehouseWorkpieces(components);
  for (auto& workpiece : *components) {
    if (!plc::IsWorkpiece(workpiece.type)) {
      continue;
    }
    const bool manual = ReadPneumaticState(workpiece, "is_manual_drag") > 0.5f;
    auto owner = std::find_if(components->begin(), components->end(),
        [&](const plc::PlacedComponent& component) {
          return component.instanceId == workpiece.carry_state.owner_id &&
                 (component.type == plc::ComponentType::kSuctionCylinder ||
                  component.type == plc::ComponentType::kLiftSuction);
        });
    if (owner != components->end() && !manual &&
        ReadPneumaticState(*owner, "vacuum_pressure") <= -0.2f) {
      ImVec2 head = GetSuctionHeadLocal(*owner);
      if (owner->type != plc::ComponentType::kLiftSuction) head.y = 61;
      const ImVec2 center = plc::LocalToWorld(*owner,
          {head.x + workpiece.carry_state.local_offset.x,
           head.y + workpiece.carry_state.local_offset.y});
      const plc::Size size = plc::GetComponentDisplaySize(workpiece);
      workpiece.position = {center.x - size.width * 0.5f,
                            center.y - size.height * 0.5f};
      workpiece.z_state.current_height = ResolveContactHeight(*owner);
      workpiece.internalStates["vel_x"] = 0;
      workpiece.internalStates["vel_y"] = 0;
      continue;
    }
    if (workpiece.carry_state.owner_id >= 0 &&
        workpiece.carry_state.slot_id < 0) {
      workpiece.carry_state.release_source_id = workpiece.carry_state.owner_id;
      workpiece.carry_state.owner_id = -1;
      workpiece.internalStates["is_contacted"] = 0;
    }
    if (manual || ReadPneumaticState(workpiece, "is_stuck_box") > 0.5f) {
      continue;
    }
    for (const auto& cylinder : *components) {
      if ((cylinder.type != plc::ComponentType::kSuctionCylinder &&
           cylinder.type != plc::ComponentType::kLiftSuction) ||
          ReadPneumaticState(cylinder, "vacuum_pressure") > -0.2f ||
          !CanInteractAtHeight(cylinder, workpiece)) {
        continue;
      }
      const bool occupied = std::any_of(components->begin(), components->end(),
          [&](const plc::PlacedComponent& other) {
            return other.carry_state.owner_id == cylinder.instanceId;
          });
      if (occupied) {
        continue;
      }
      const ImVec2 head = GetSuctionHeadWorld(cylinder);
      const plc::Aabb bounds = plc::GetAabb(workpiece);
      if (head.x < bounds.min_x || head.x > bounds.max_x ||
          head.y < bounds.min_y || head.y > bounds.max_y) {
        continue;
      }
      const ImVec2 center = plc::WorldToLocal(
          cylinder, plc::GetComponentWorldCenter(workpiece));
      ImVec2 local_head = GetSuctionHeadLocal(cylinder);
      if (cylinder.type != plc::ComponentType::kLiftSuction) local_head.y = 61;
      // Choose by stable ID rather than canvas order when candidates overlap.
      const bool earlier_candidate = std::any_of(
          components->begin(), components->end(),
          [&](const plc::PlacedComponent& candidate) {
            if (!plc::IsWorkpiece(candidate.type) ||
                candidate.instanceId >= workpiece.instanceId ||
                candidate.carry_state.owner_id >= 0 ||
                ReadPneumaticState(candidate, "is_manual_drag") > 0.5f ||
                ReadPneumaticState(candidate, "is_stuck_box") > 0.5f ||
                !CanInteractAtHeight(cylinder, candidate)) {
              return false;
            }
            const plc::Aabb box = plc::GetAabb(candidate);
            return head.x >= box.min_x && head.x <= box.max_x &&
                   head.y >= box.min_y && head.y <= box.max_y;
          });
      if (earlier_candidate) {
        continue;
      }
      for (auto& [key, value] : workpiece.internalStates) {
        if (key.starts_with("snap_lock_") || key.starts_with("proc_ready_")) {
          value = 0;
        }
      }
      workpiece.carry_state.owner_id = cylinder.instanceId;
      workpiece.carry_state.slot_id = -1;
      workpiece.carry_state.release_source_id = -1;
      workpiece.carry_state.local_offset = {center.x - local_head.x,
                                           center.y - local_head.y};
      workpiece.internalStates["vel_x"] = 0;
      workpiece.internalStates["vel_y"] = 0;
      break;
    }
  }
  UpdateWarehouseWorkpieces(components);
}

// Sweep in the stopper's local plane so rotation and mirroring use the same
// plate geometry as the drawing. Only crossing from outside stops movement.
inline void ResolveStopperCollision(
    const std::vector<plc::PlacedComponent>& components,
    plc::Position previous_position, plc::PlacedComponent* workpiece) {
  if (!workpiece || workpiece->carry_state.owner_id >= 0) {
    return;
  }
  const plc::Size size = plc::GetComponentDisplaySize(*workpiece);
  for (const auto& stopper : components) {
    if (stopper.type != plc::ComponentType::kStopperCylinder ||
        !CanInteractAtHeight(stopper, *workpiece)) {
      continue;
    }
    const ImVec2 previous = plc::WorldToLocal(stopper,
        {previous_position.x + size.width * 0.5f,
         previous_position.y + size.height * 0.5f});
    const ImVec2 current = plc::WorldToLocal(
        stopper, plc::GetComponentWorldCenter(*workpiece));
    const int rotation = plc::NormalizeRotationQuadrants(
        workpiece->rotation_quadrants - stopper.rotation_quadrants);
    const float half_x = (rotation % 2 ? workpiece->size.height
                                      : workpiece->size.width) * 0.5f;
    const float half_y = (rotation % 2 ? workpiece->size.width
                                      : workpiece->size.height) * 0.5f;
    const float width_scale = stopper.size.height / 155.0f;
    const float length_scale = stopper.size.width / 240.0f;
    const float minimum[] = {6 * length_scale - half_x,
                             60 * width_scale - half_y};
    const float maximum[] = {206 * length_scale + half_x,
                             100 * width_scale + half_y};
    const float start[] = {previous.x, previous.y};
    const float movement[] = {current.x - previous.x, current.y - previous.y};
    float entry = 0;
    float exit = 1;
    bool hit = true;
    const bool inside = start[0] > minimum[0] && start[0] < maximum[0] &&
                        start[1] > minimum[1] && start[1] < maximum[1];
    if (inside) {
      workpiece->position = previous_position;
      workpiece->internalStates["vel_x"] = 0;
      workpiece->internalStates["vel_y"] = 0;
      workpiece->internalStates["is_contacted"] = 1;
      return;
    }
    for (int axis = 0; axis < 2; ++axis) {
      if (std::abs(movement[axis]) < 0.0001f) {
        hit &= start[axis] >= minimum[axis] && start[axis] <= maximum[axis];
      } else {
        const float first = (minimum[axis] - start[axis]) / movement[axis];
        const float second = (maximum[axis] - start[axis]) / movement[axis];
        entry = std::max(entry, std::min(first, second));
        exit = std::min(exit, std::max(first, second));
      }
    }
    if (hit && entry <= exit && exit > 0.00001f && entry >= 0 && entry < 1) {
      const ImVec2 center = plc::LocalToWorld(stopper,
          {previous.x + movement[0] * entry,
           previous.y + movement[1] * entry});
      workpiece->position = {center.x - size.width * 0.5f,
                             center.y - size.height * 0.5f};
      workpiece->internalStates["vel_x"] = 0;
      workpiece->internalStates["vel_y"] = 0;
      workpiece->internalStates["is_contacted"] = 1;
    }
  }
}

}  // namespace plc_emulator
