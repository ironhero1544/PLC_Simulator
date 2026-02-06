#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_APPLICATION_PHYSICS_PHYSICS_HELPERS_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_APPLICATION_PHYSICS_PHYSICS_HELPERS_H_

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/core/data_types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace plc {

struct Aabb {
  float min_x = 0.0f;
  float min_y = 0.0f;
  float max_x = 0.0f;
  float max_y = 0.0f;
};

namespace detail {

struct TransformCacheEntry {
  bool valid = false;
  float pos_x = 0.0f;
  float pos_y = 0.0f;
  float size_w = 0.0f;
  float size_h = 0.0f;
  int rotation_quadrants = 0;
  bool flip_x = false;
  bool flip_y = false;
  Size display{};
  ImVec2 world_center{};
  ImVec2 base_center{};
  Aabb aabb{};
};

struct TransformCache {
  std::vector<TransformCacheEntry> entries;
};

inline TransformCache& GetTransformCache() {
  static TransformCache cache;
  return cache;
}

inline TransformCacheEntry& GetTransformCacheEntry(
    const PlacedComponent& comp) {
  TransformCache& cache = GetTransformCache();
  if (comp.instanceId < 0) {
    static TransformCacheEntry temp;
    const int rotation = NormalizeRotationQuadrants(comp.rotation_quadrants);
    temp.valid = true;
    temp.pos_x = comp.position.x;
    temp.pos_y = comp.position.y;
    temp.size_w = comp.size.width;
    temp.size_h = comp.size.height;
    temp.rotation_quadrants = rotation;
    temp.flip_x = comp.flip_x;
    temp.flip_y = comp.flip_y;
    temp.base_center =
        ImVec2(comp.size.width * 0.5f, comp.size.height * 0.5f);
    Size display = comp.size;
    if (rotation % 2 != 0) {
      display = {comp.size.height, comp.size.width};
    }
    temp.display = display;
    temp.world_center =
        ImVec2(comp.position.x + display.width * 0.5f,
               comp.position.y + display.height * 0.5f);
    temp.aabb = {comp.position.x, comp.position.y,
                 comp.position.x + display.width,
                 comp.position.y + display.height};
    return temp;
  }

  const size_t index = static_cast<size_t>(comp.instanceId);
  if (index >= cache.entries.size()) {
    cache.entries.resize(index + 1u);
  }

  TransformCacheEntry& entry = cache.entries[index];
  const int rotation = NormalizeRotationQuadrants(comp.rotation_quadrants);
  const bool changed = !entry.valid || entry.pos_x != comp.position.x ||
                       entry.pos_y != comp.position.y ||
                       entry.size_w != comp.size.width ||
                       entry.size_h != comp.size.height ||
                       entry.rotation_quadrants != rotation ||
                       entry.flip_x != comp.flip_x || entry.flip_y != comp.flip_y;
  if (changed) {
    entry.pos_x = comp.position.x;
    entry.pos_y = comp.position.y;
    entry.size_w = comp.size.width;
    entry.size_h = comp.size.height;
    entry.rotation_quadrants = rotation;
    entry.flip_x = comp.flip_x;
    entry.flip_y = comp.flip_y;
    entry.base_center =
        ImVec2(comp.size.width * 0.5f, comp.size.height * 0.5f);

    Size display = comp.size;
    if (rotation % 2 != 0) {
      display = {comp.size.height, comp.size.width};
    }
    entry.display = display;
    entry.world_center =
        ImVec2(comp.position.x + display.width * 0.5f,
               comp.position.y + display.height * 0.5f);
    entry.aabb = {comp.position.x, comp.position.y,
                  comp.position.x + display.width,
                  comp.position.y + display.height};
    entry.valid = true;
  }

  return entry;
}

}  /* namespace detail */

inline ImVec2 LocalToWorldCached(const PlacedComponent& comp, ImVec2 local) {
  const auto& entry = detail::GetTransformCacheEntry(comp);
  ImVec2 offset(local.x - entry.base_center.x, local.y - entry.base_center.y);
  if (entry.flip_x) {
    offset.x = -offset.x;
  }
  if (entry.flip_y) {
    offset.y = -offset.y;
  }
  ImVec2 rotated = RotatePoint(offset, entry.rotation_quadrants);
  return ImVec2(entry.world_center.x + rotated.x,
                entry.world_center.y + rotated.y);
}

inline ImVec2 LocalDirToWorldCached(const PlacedComponent& comp,
                                    ImVec2 local_dir) {
  const auto& entry = detail::GetTransformCacheEntry(comp);
  if (entry.flip_x) {
    local_dir.x = -local_dir.x;
  }
  if (entry.flip_y) {
    local_dir.y = -local_dir.y;
  }
  return RotatePoint(local_dir, entry.rotation_quadrants);
}

inline bool IsWorkpiece(ComponentType type) {
  return type == ComponentType::WORKPIECE_METAL ||
         type == ComponentType::WORKPIECE_NONMETAL;
}

inline float Clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

inline ImVec2 GetCylinderRodTipWorld(const PlacedComponent& cylinder) {
  float piston_position = cylinder.internalStates.count(state_keys::kPosition)
                              ? cylinder.internalStates.at(state_keys::kPosition)
                              : 0.0f;
  return LocalToWorldCached(cylinder,
                            ImVec2(170.0f - piston_position, 25.0f));
}

inline ImVec2 GetLimitSwitchCenterWorld(const PlacedComponent& sensor) {
  return LocalToWorldCached(sensor,
                            ImVec2(sensor.size.width * 0.5f,
                                   sensor.size.height * 0.5f));
}

inline ImVec2 GetSensorCenterWorld(const PlacedComponent& sensor) {
  return LocalToWorldCached(sensor, ImVec2(sensor.size.width * 0.5f, 7.5f));
}

inline ImVec2 LocalDirToWorld(const PlacedComponent& comp, ImVec2 local_dir) {
  return LocalDirToWorldCached(comp, local_dir);
}

inline float Dot(ImVec2 a, ImVec2 b) {
  return a.x * b.x + a.y * b.y;
}

inline bool IsDirectionalHit(ImVec2 sensor_center,
                             ImVec2 forward_dir,
                             ImVec2 target,
                             float max_distance,
                             float beam_half_width,
                             float* out_distance) {
  float len = std::sqrt(forward_dir.x * forward_dir.x +
                        forward_dir.y * forward_dir.y);
  if (len < 1e-4f) {
    return false;
  }
  forward_dir.x /= len;
  forward_dir.y /= len;
  ImVec2 to_target = {target.x - sensor_center.x, target.y - sensor_center.y};
  float proj = Dot(to_target, forward_dir);
  if (proj < 0.0f || proj > max_distance) {
    return false;
  }
  ImVec2 perp = {-forward_dir.y, forward_dir.x};
  float lateral = std::abs(Dot(to_target, perp));
  if (lateral > beam_half_width) {
    return false;
  }
  if (out_distance) {
    *out_distance =
        std::sqrt(to_target.x * to_target.x + to_target.y * to_target.y);
  }
  return true;
}

inline Aabb GetAabb(const PlacedComponent& comp) {
  return detail::GetTransformCacheEntry(comp).aabb;
}

inline bool AabbOverlaps(const Aabb& a, const Aabb& b) {
  return a.min_x < b.max_x && a.max_x > b.min_x && a.min_y < b.max_y &&
         a.max_y > b.min_y;
}

inline Aabb ExpandAabb(const Aabb& box, float pad_x, float pad_y) {
  Aabb expanded = box;
  expanded.min_x -= pad_x;
  expanded.max_x += pad_x;
  expanded.min_y -= pad_y;
  expanded.max_y += pad_y;
  return expanded;
}

inline Aabb GetWorkpieceDetectionAabb(const PlacedComponent& workpiece) {
  Aabb box = GetAabb(workpiece);
  float width = box.max_x - box.min_x;
  float height = box.max_y - box.min_y;
  float min_dim = (width < height) ? width : height;
  float pad = min_dim * 1.5f;
  if (pad < 20.0f) {
    pad = 20.0f;
  } else if (pad > 60.0f) {
    pad = 60.0f;
  }
  return ExpandAabb(box, pad, pad);
}

inline float GetCenterX(const Aabb& box) {
  return (box.min_x + box.max_x) * 0.5f;
}

inline float GetCenterY(const Aabb& box) {
  return (box.min_y + box.max_y) * 0.5f;
}

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_APPLICATION_PHYSICS_PHYSICS_HELPERS_H_ */
