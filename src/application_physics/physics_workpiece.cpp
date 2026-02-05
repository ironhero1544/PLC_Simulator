// physics_workpiece.cpp
//
// Workpiece interactions and collision handling.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"

#include "plc_emulator/application_physics/physics_helpers.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace plc {
namespace {

constexpr float kSpatialCellSize = 120.0f;

struct SpatialCell {
  std::vector<int> conveyors;
  std::vector<int> cylinders;
  std::vector<int> processing;
  std::vector<int> boxes;
  std::vector<int> sensors;

  void Clear() {
    conveyors.clear();
    cylinders.clear();
    processing.clear();
    boxes.clear();
    sensors.clear();
  }
};

struct SpatialGridCache {
  std::unordered_map<uint64_t, size_t> lookup;
  std::vector<SpatialCell> cells;
  std::vector<size_t> used_cells;
  std::vector<uint32_t> conveyor_marks;
  std::vector<uint32_t> cylinder_marks;
  std::vector<uint32_t> processing_marks;
  std::vector<uint32_t> box_marks;
  std::vector<uint32_t> sensor_marks;
  uint32_t stamp = 1;
};

enum SpatialType {
  kGridConveyor = 0,
  kGridCylinder = 1,
  kGridProcessing = 2,
  kGridBox = 3,
  kGridSensor = 4
};

SpatialGridCache& GetSpatialGridCache() {
  static SpatialGridCache cache;
  return cache;
}

uint64_t MakeCellKey(int gx, int gy) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(gx)) << 32) |
         static_cast<uint32_t>(gy);
}

int ToCellCoord(float value) {
  return static_cast<int>(std::floor(value / kSpatialCellSize));
}

void ResetSpatialGrid(SpatialGridCache& grid, size_t reserve_cells) {
  grid.used_cells.clear();
  grid.lookup.clear();
  if (grid.lookup.bucket_count() < reserve_cells) {
    grid.lookup.reserve(reserve_cells);
  }
}

size_t AllocateCell(SpatialGridCache& grid) {
  size_t idx = grid.used_cells.size();
  if (idx < grid.cells.size()) {
    grid.cells[idx].Clear();
  } else {
    grid.cells.emplace_back();
  }
  grid.used_cells.push_back(idx);
  return idx;
}

SpatialCell& GetCell(SpatialGridCache& grid, int gx, int gy) {
  uint64_t key = MakeCellKey(gx, gy);
  auto it = grid.lookup.find(key);
  if (it != grid.lookup.end()) {
    return grid.cells[it->second];
  }
  size_t idx = AllocateCell(grid);
  grid.lookup[key] = idx;
  return grid.cells[idx];
}

void AddToGrid(SpatialGridCache& grid,
               const Aabb& box,
               int cache_index,
               SpatialType type) {
  int min_x = ToCellCoord(box.min_x);
  int max_x = ToCellCoord(box.max_x);
  int min_y = ToCellCoord(box.min_y);
  int max_y = ToCellCoord(box.max_y);
  for (int gy = min_y; gy <= max_y; ++gy) {
    for (int gx = min_x; gx <= max_x; ++gx) {
      SpatialCell& cell = GetCell(grid, gx, gy);
      switch (type) {
        case kGridConveyor:
          cell.conveyors.push_back(cache_index);
          break;
        case kGridCylinder:
          cell.cylinders.push_back(cache_index);
          break;
        case kGridProcessing:
          cell.processing.push_back(cache_index);
          break;
        case kGridBox:
          cell.boxes.push_back(cache_index);
          break;
        case kGridSensor:
          cell.sensors.push_back(cache_index);
          break;
        default:
          break;
      }
    }
  }
}

void PrepareMarks(std::vector<uint32_t>& marks, size_t size) {
  if (marks.size() < size) {
    marks.resize(size, 0);
  }
}

uint32_t NextStamp(SpatialGridCache& grid) {
  grid.stamp++;
  if (grid.stamp == 0) {
    std::fill(grid.conveyor_marks.begin(), grid.conveyor_marks.end(), 0);
    std::fill(grid.cylinder_marks.begin(), grid.cylinder_marks.end(), 0);
    std::fill(grid.processing_marks.begin(), grid.processing_marks.end(), 0);
    std::fill(grid.box_marks.begin(), grid.box_marks.end(), 0);
    std::fill(grid.sensor_marks.begin(), grid.sensor_marks.end(), 0);
    grid.stamp = 1;
  }
  return grid.stamp;
}

void CollectCandidates(SpatialGridCache& grid,
                       const Aabb& box,
                       uint32_t stamp,
                       std::vector<int>* conveyors,
                       std::vector<int>* cylinders,
                       std::vector<int>* processing,
                       std::vector<int>* boxes,
                       std::vector<int>* sensors) {
  if (conveyors) conveyors->clear();
  if (cylinders) cylinders->clear();
  if (processing) processing->clear();
  if (boxes) boxes->clear();
  if (sensors) sensors->clear();

  int min_x = ToCellCoord(box.min_x);
  int max_x = ToCellCoord(box.max_x);
  int min_y = ToCellCoord(box.min_y);
  int max_y = ToCellCoord(box.max_y);
  for (int gy = min_y; gy <= max_y; ++gy) {
    for (int gx = min_x; gx <= max_x; ++gx) {
      auto it = grid.lookup.find(MakeCellKey(gx, gy));
      if (it == grid.lookup.end()) {
        continue;
      }
      const SpatialCell& cell = grid.cells[it->second];
      if (conveyors) {
        for (int idx : cell.conveyors) {
          if (idx < 0 || static_cast<size_t>(idx) >= grid.conveyor_marks.size()) {
            continue;
          }
          if (grid.conveyor_marks[idx] == stamp) {
            continue;
          }
          grid.conveyor_marks[idx] = stamp;
          conveyors->push_back(idx);
        }
      }
      if (cylinders) {
        for (int idx : cell.cylinders) {
          if (idx < 0 || static_cast<size_t>(idx) >= grid.cylinder_marks.size()) {
            continue;
          }
          if (grid.cylinder_marks[idx] == stamp) {
            continue;
          }
          grid.cylinder_marks[idx] = stamp;
          cylinders->push_back(idx);
        }
      }
      if (processing) {
        for (int idx : cell.processing) {
          if (idx < 0 || static_cast<size_t>(idx) >= grid.processing_marks.size()) {
            continue;
          }
          if (grid.processing_marks[idx] == stamp) {
            continue;
          }
          grid.processing_marks[idx] = stamp;
          processing->push_back(idx);
        }
      }
      if (boxes) {
        for (int idx : cell.boxes) {
          if (idx < 0 || static_cast<size_t>(idx) >= grid.box_marks.size()) {
            continue;
          }
          if (grid.box_marks[idx] == stamp) {
            continue;
          }
          grid.box_marks[idx] = stamp;
          boxes->push_back(idx);
        }
      }
      if (sensors) {
        for (int idx : cell.sensors) {
          if (idx < 0 || static_cast<size_t>(idx) >= grid.sensor_marks.size()) {
            continue;
          }
          if (grid.sensor_marks[idx] == stamp) {
            continue;
          }
          grid.sensor_marks[idx] = stamp;
          sensors->push_back(idx);
        }
      }
    }
  }
}

}  // namespace

void Application::UpdateWorkpieceInteractionsImpl(float delta_time) {
  if (UpdateWorkpieceInteractionsBox2d(delta_time, false)) {
    return;
  }

  const float kSnapDistance = 10.0f;
  const float kConveyorSpeed = 60.0f;
  const int kWorkpieceSubsteps = 4;
  const float kSleepVelocity = 0.05f;
  constexpr float kRingCenterX = 30.0f;
  constexpr float kRingCenterY = 28.0f;
  constexpr float kRingDetectRadius = 18.0f;

  std::vector<int> workpiece_indices;
  std::vector<int> conveyor_indices;
  std::vector<int> cylinder_indices;
  std::vector<int> processing_indices;
  std::vector<int> box_indices;
  std::vector<int> sensor_indices;
  workpiece_indices.reserve(placed_components_.size());
  conveyor_indices.reserve(placed_components_.size());
  cylinder_indices.reserve(placed_components_.size());
  processing_indices.reserve(placed_components_.size());
  box_indices.reserve(placed_components_.size());
  sensor_indices.reserve(placed_components_.size());

  for (size_t i = 0; i < placed_components_.size(); ++i) {
    auto& comp = placed_components_[i];
    if (IsWorkpiece(comp.type)) {
      workpiece_indices.push_back(static_cast<int>(i));
      continue;
    }

    switch (comp.type) {
      case ComponentType::CONVEYOR:
        conveyor_indices.push_back(static_cast<int>(i));
        break;
      case ComponentType::CYLINDER:
        cylinder_indices.push_back(static_cast<int>(i));
        break;
      case ComponentType::PROCESSING_CYLINDER:
        processing_indices.push_back(static_cast<int>(i));
        break;
      case ComponentType::BOX:
        box_indices.push_back(static_cast<int>(i));
        break;
      case ComponentType::RING_SENSOR:
      case ComponentType::INDUCTIVE_SENSOR:
        comp.internalStates[state_keys::kIsDetected] = 0.0f;
        sensor_indices.push_back(static_cast<int>(i));
        break;
      default:
        break;
    }
  }

  if (workpiece_indices.empty()) {
    return;
  }

  const int substeps = kWorkpieceSubsteps;
  float step_dt = delta_time / static_cast<float>(substeps);

  struct SensorCache {
    int index = -1;
    ComponentType type = ComponentType::PLC;
    Aabb box{};
    ImVec2 center{};
    ImVec2 forward{};
    ImVec2 ring_center{};
    std::string snap_key;
  };

  struct ConveyorCache {
    int index = -1;
    Aabb box{};
    ImVec2 dir{};
  };

  struct CylinderCache {
    int index = -1;
    Aabb box{};
    float velocity = 0.0f;
    ImVec2 rod_tip{};
    ImVec2 rod_dir{};
    ImVec2 rod_vel{};
    Aabb rod_swept{};
    std::string snap_key;
  };

  struct ProcCylinderCache {
    int index = -1;
    Aabb head_box{};
    float center_x = 0.0f;
    float center_y = 0.0f;
    bool is_down = false;
    bool motor_on = false;
    std::string ready_key;
    std::string snap_key;
  };

  struct BoxCache {
    Aabb box{};
  };

  std::vector<SensorCache> sensor_caches;
  sensor_caches.reserve(sensor_indices.size());
  for (int idx : sensor_indices) {
    const auto& sensor = placed_components_[idx];
    SensorCache cache;
    cache.index = idx;
    cache.type = sensor.type;
    cache.box = GetAabb(sensor);
    if (sensor.type == ComponentType::RING_SENSOR) {
      cache.ring_center =
          LocalToWorld(sensor, ImVec2(kRingCenterX, kRingCenterY));
      cache.snap_key =
          std::string("snap_lock_ring_") + std::to_string(sensor.instanceId);
    } else if (sensor.type == ComponentType::INDUCTIVE_SENSOR) {
      cache.center = GetSensorCenterWorld(sensor);
      cache.forward = LocalDirToWorld(sensor, ImVec2(0.0f, -1.0f));
    }
    sensor_caches.push_back(cache);
  }

  std::vector<ConveyorCache> conveyor_caches;
  conveyor_caches.reserve(conveyor_indices.size());
  for (int idx : conveyor_indices) {
    const auto& comp = placed_components_[idx];
    bool active = comp.internalStates.count(state_keys::kMotorActive) &&
                  comp.internalStates.at(state_keys::kMotorActive) > 0.5f;
    if (!active) {
      continue;
    }
    ConveyorCache cache;
    cache.index = idx;
    cache.box = GetAabb(comp);
    cache.dir = LocalDirToWorld(comp, ImVec2(1.0f, 0.0f));
    conveyor_caches.push_back(cache);
  }

  float max_drive_speed = kConveyorSpeed;
  std::vector<CylinderCache> cylinder_caches;
  cylinder_caches.reserve(cylinder_indices.size());
  for (int idx : cylinder_indices) {
    const auto& comp = placed_components_[idx];
    CylinderCache cache;
    cache.index = idx;
    cache.box = GetAabb(comp);
    cache.velocity =
        comp.internalStates.count(state_keys::kVelocity)
            ? comp.internalStates.at(state_keys::kVelocity)
            : 0.0f;
    float abs_velocity = std::abs(cache.velocity);
    if (abs_velocity > max_drive_speed) {
      max_drive_speed = abs_velocity;
    }
    cache.rod_tip = GetCylinderRodTipWorld(comp);
    cache.rod_dir = LocalDirToWorld(comp, ImVec2(-1.0f, 0.0f));
    cache.rod_vel =
        ImVec2(cache.rod_dir.x * cache.velocity,
               cache.rod_dir.y * cache.velocity);
    ImVec2 sweep(cache.rod_vel.x * step_dt, cache.rod_vel.y * step_dt);
    const float kRodHitLeft = 12.0f;
    const float kRodHitRight = 20.0f;
    float rod_radius = std::max(kRodHitLeft, kRodHitRight);
    float rod_min_x =
        std::min(cache.rod_tip.x, cache.rod_tip.x + sweep.x) - rod_radius;
    float rod_max_x =
        std::max(cache.rod_tip.x, cache.rod_tip.x + sweep.x) + rod_radius;
    float rod_min_y =
        std::min(cache.rod_tip.y, cache.rod_tip.y + sweep.y) - rod_radius;
    float rod_max_y =
        std::max(cache.rod_tip.y, cache.rod_tip.y + sweep.y) + rod_radius;
    cache.rod_swept = {rod_min_x, rod_min_y, rod_max_x, rod_max_y};
    cache.snap_key =
        std::string("snap_lock_cyl_") + std::to_string(comp.instanceId);
    cylinder_caches.push_back(cache);
  }

  std::vector<ProcCylinderCache> proc_cylinder_caches;
  proc_cylinder_caches.reserve(processing_indices.size());
  for (int idx : processing_indices) {
    const auto& comp = placed_components_[idx];
    constexpr float kProcDrillUp = 30.0f;
    constexpr float kProcDrillDown = 15.0f;
    constexpr float kProcEpsilon = 0.1f;
    ProcCylinderCache cache;
    cache.index = idx;
    Aabb cyl_box = GetAabb(comp);
    float pos_val =
        comp.internalStates.count(state_keys::kPosition)
            ? comp.internalStates.at(state_keys::kPosition)
            : kProcDrillUp;
    cache.is_down = pos_val <= (kProcDrillDown + kProcEpsilon);
    cache.motor_on =
        comp.internalStates.count(state_keys::kMotorOn) &&
        comp.internalStates.at(state_keys::kMotorOn) > 0.5f;
    cache.center_x = cyl_box.min_x + 60.0f;
    cache.center_y = cyl_box.min_y + 90.0f;
    float head_radius = 30.0f;
    cache.head_box.min_x = cache.center_x - head_radius;
    cache.head_box.max_x = cache.center_x + head_radius;
    cache.head_box.min_y = cache.center_y - head_radius;
    cache.head_box.max_y = cache.center_y + head_radius;
    cache.ready_key =
        std::string("proc_ready_") + std::to_string(comp.instanceId);
    cache.snap_key =
        std::string("snap_lock_proc_") + std::to_string(comp.instanceId);
    proc_cylinder_caches.push_back(cache);
  }

  std::vector<BoxCache> box_caches;
  box_caches.reserve(box_indices.size());
  for (int idx : box_indices) {
    const auto& comp = placed_components_[idx];
    BoxCache cache;
    cache.box = GetAabb(comp);
    box_caches.push_back(cache);
  }

  float query_pad = max_drive_speed * step_dt + kSnapDistance;
  SpatialGridCache& grid = GetSpatialGridCache();
  size_t reserve_cells =
      (sensor_caches.size() + conveyor_caches.size() + cylinder_caches.size() +
       proc_cylinder_caches.size() + box_caches.size()) *
          2u +
      16u;
  ResetSpatialGrid(grid, reserve_cells);
  PrepareMarks(grid.conveyor_marks, conveyor_caches.size());
  PrepareMarks(grid.cylinder_marks, cylinder_caches.size());
  PrepareMarks(grid.processing_marks, proc_cylinder_caches.size());
  PrepareMarks(grid.box_marks, box_caches.size());
  PrepareMarks(grid.sensor_marks, sensor_caches.size());

  for (size_t i = 0; i < conveyor_caches.size(); ++i) {
    AddToGrid(grid, conveyor_caches[i].box, static_cast<int>(i),
              kGridConveyor);
  }
  for (size_t i = 0; i < cylinder_caches.size(); ++i) {
    const auto& cylinder = cylinder_caches[i];
    Aabb combined = cylinder.box;
    combined.min_x = std::min(combined.min_x, cylinder.rod_swept.min_x);
    combined.min_y = std::min(combined.min_y, cylinder.rod_swept.min_y);
    combined.max_x = std::max(combined.max_x, cylinder.rod_swept.max_x);
    combined.max_y = std::max(combined.max_y, cylinder.rod_swept.max_y);
    AddToGrid(grid, combined, static_cast<int>(i), kGridCylinder);
  }
  for (size_t i = 0; i < proc_cylinder_caches.size(); ++i) {
    AddToGrid(grid, proc_cylinder_caches[i].head_box, static_cast<int>(i),
              kGridProcessing);
  }
  for (size_t i = 0; i < box_caches.size(); ++i) {
    AddToGrid(grid, box_caches[i].box, static_cast<int>(i), kGridBox);
  }
  for (size_t i = 0; i < sensor_caches.size(); ++i) {
    AddToGrid(grid, sensor_caches[i].box, static_cast<int>(i), kGridSensor);
  }

  std::vector<int> candidate_conveyors;
  std::vector<int> candidate_cylinders;
  std::vector<int> candidate_processing;
  std::vector<int> candidate_boxes;
  std::vector<int> candidate_sensors;

  candidate_conveyors.reserve(conveyor_caches.size());
  candidate_cylinders.reserve(cylinder_caches.size());
  candidate_processing.reserve(proc_cylinder_caches.size());
  candidate_boxes.reserve(box_caches.size());
  candidate_sensors.reserve(sensor_caches.size());

  auto is_in_view = [&](const Aabb& box) -> bool {
    if (!physics_lod_state_.has_view) {
      return true;
    }
    float zoom = physics_lod_state_.zoom;
    if (zoom < 0.05f) {
      zoom = 0.05f;
    }
    float margin = 300.0f / zoom;
    if (box.max_x < physics_lod_state_.view_min_x - margin ||
        box.min_x > physics_lod_state_.view_max_x + margin ||
        box.max_y < physics_lod_state_.view_min_y - margin ||
        box.min_y > physics_lod_state_.view_max_y + margin) {
      return false;
    }
    return true;
  };

  auto apply_sensor_detection = [&](PlacedComponent& workpiece,
                                    bool allow_snap,
                                    const std::vector<int>& sensor_candidates) {
    Aabb workpiece_detection = GetWorkpieceDetectionAabb(workpiece);
    Aabb workpiece_box = GetAabb(workpiece);
    for (int cache_index : sensor_candidates) {
      if (cache_index < 0 ||
          cache_index >= static_cast<int>(sensor_caches.size())) {
        continue;
      }
      const auto& cache = sensor_caches[cache_index];
      auto& sensor = placed_components_[cache.index];
      const bool is_ring = cache.type == ComponentType::RING_SENSOR;
      const bool is_inductive = cache.type == ComponentType::INDUCTIVE_SENSOR;
      Aabb sensor_box = cache.box;
      const bool overlaps_detection =
          AabbOverlaps(workpiece_detection, sensor_box);
      const bool overlaps_body = AabbOverlaps(workpiece_box, sensor_box);
      std::string snap_key;
      if (is_ring) {
        snap_key = cache.snap_key;
        if (!overlaps_body) {
          workpiece.internalStates[snap_key] = 0.0f;
        }
        if (!overlaps_detection) {
          continue;
        }
        ImVec2 ring_center = cache.ring_center;
        ImVec2 work_center = {workpiece.position.x +
                                  workpiece.size.width * 0.5f,
                              workpiece.position.y +
                                  workpiece.size.height * 0.5f};
        float dx = work_center.x - ring_center.x;
        float dy = work_center.y - ring_center.y;
        if (dx * dx + dy * dy >
            kRingDetectRadius * kRingDetectRadius) {
          continue;
        }
      } else if (!overlaps_detection) {
        continue;
      }
      if (is_inductive) {
        bool is_metal =
            workpiece.type == ComponentType::WORKPIECE_METAL ||
            (workpiece.internalStates.count(state_keys::kIsMetal) &&
             workpiece.internalStates.at(state_keys::kIsMetal) > 0.5f);
        if (!is_metal) {
          continue;
        }
        ImVec2 sensor_center = cache.center;
        ImVec2 sensor_forward = cache.forward;
        ImVec2 work_center = {workpiece.position.x +
                                  workpiece.size.width * 0.5f,
                              workpiece.position.y +
                                  workpiece.size.height * 0.5f};
        const float kDetectionRange = 75.0f;
        const float kBeamHalfWidth = 15.0f;
        if (!IsDirectionalHit(sensor_center, sensor_forward, work_center,
                              kDetectionRange, kBeamHalfWidth, nullptr)) {
          continue;
        }
      }
      sensor.internalStates[state_keys::kIsDetected] = 1.0f;
      if (allow_snap && is_ring) {
        bool snap_locked =
            workpiece.internalStates.count(snap_key) &&
            workpiece.internalStates.at(snap_key) > 0.5f;
        if (snap_locked) {
          continue;
        }
        ImVec2 ring_center = cache.ring_center;
        float center_x = ring_center.x;
        float center_y = ring_center.y;
        float delta_x =
            (workpiece.position.x + workpiece.size.width * 0.5f) - center_x;
        float delta_y =
            (workpiece.position.y + workpiece.size.height * 0.5f) - center_y;
        if (std::abs(delta_x) <= kSnapDistance &&
            std::abs(delta_y) <= kSnapDistance) {
          workpiece.position.x = center_x - workpiece.size.width * 0.5f;
          workpiece.position.y = center_y - workpiece.size.height * 0.5f;
          workpiece.internalStates[state_keys::kVelX] = 0.0f;
          workpiece.internalStates[state_keys::kVelY] = 0.0f;
          workpiece.internalStates[snap_key] = 1.0f;
        }
      }
    }
  };

  for (int step = 0; step < substeps; ++step) {
    for (int workpiece_index : workpiece_indices) {
      auto& workpiece = placed_components_[workpiece_index];

      uint32_t stamp = NextStamp(grid);
      Aabb workpiece_box = GetAabb(workpiece);
      Aabb query_box = ExpandAabb(workpiece_box, query_pad, query_pad);
      CollectCandidates(grid, query_box, stamp, &candidate_conveyors,
                        &candidate_cylinders, &candidate_processing,
                        &candidate_boxes, nullptr);

      Aabb detection_box = GetWorkpieceDetectionAabb(workpiece);
      detection_box = ExpandAabb(detection_box, query_pad, query_pad);
      CollectCandidates(grid, detection_box, stamp, nullptr, nullptr, nullptr,
                        nullptr, &candidate_sensors);

      std::sort(candidate_cylinders.begin(), candidate_cylinders.end());
      std::sort(candidate_processing.begin(), candidate_processing.end());
      std::sort(candidate_sensors.begin(), candidate_sensors.end());

      bool manual_drag =
          workpiece.internalStates.count(state_keys::kIsManualDrag) &&
          workpiece.internalStates.at(state_keys::kIsManualDrag) > 0.5f;
      if (manual_drag) {
        apply_sensor_detection(workpiece, false, candidate_sensors);
        continue;
      }

      bool stuck_box =
          workpiece.internalStates.count(state_keys::kIsStuckBox) &&
          workpiece.internalStates.at(state_keys::kIsStuckBox) > 0.5f;
      if (stuck_box) {
        workpiece.internalStates[state_keys::kVelX] = 0.0f;
        workpiece.internalStates[state_keys::kVelY] = 0.0f;
        continue;
      }

      workpiece.internalStates[state_keys::kIsContacted] = 0.0f;
      float vel_x = workpiece.internalStates.count(state_keys::kVelX)
                        ? workpiece.internalStates.at(state_keys::kVelX)
                        : 0.0f;
      float vel_y = workpiece.internalStates.count(state_keys::kVelY)
                        ? workpiece.internalStates.at(state_keys::kVelY)
                        : 0.0f;
      bool stable =
          std::abs(vel_x) <= kSleepVelocity && std::abs(vel_y) <= kSleepVelocity;
      bool offscreen = !is_in_view(workpiece_box);
      if (stable && offscreen &&
          candidate_conveyors.empty() && candidate_cylinders.empty() &&
          candidate_processing.empty() && candidate_boxes.empty() &&
          candidate_sensors.empty()) {
        if (workpiece.internalStates.count(state_keys::kVelX)) {
          workpiece.internalStates[state_keys::kVelX] = 0.0f;
        }
        if (workpiece.internalStates.count(state_keys::kVelY)) {
          workpiece.internalStates[state_keys::kVelY] = 0.0f;
        }
        continue;
      }

      bool on_conveyor = false;
      bool pushed_by_cylinder = false;

      int conveyor_hit = -1;
      for (int cache_index : candidate_conveyors) {
        if (cache_index < 0 ||
            cache_index >= static_cast<int>(conveyor_caches.size())) {
          continue;
        }
        const auto& conveyor = conveyor_caches[cache_index];
        if (AabbOverlaps(workpiece_box, conveyor.box)) {
          conveyor_hit = std::max(conveyor_hit, cache_index);
        }
      }
      if (conveyor_hit >= 0) {
        const auto& conveyor = conveyor_caches[conveyor_hit];
        on_conveyor = true;
        vel_x = conveyor.dir.x * kConveyorSpeed;
        vel_y = conveyor.dir.y * kConveyorSpeed;
      }

      if (!on_conveyor) {
        vel_x = 0.0f;
        vel_y = 0.0f;
      }

      for (int cache_index : candidate_cylinders) {
        if (cache_index < 0 ||
            cache_index >= static_cast<int>(cylinder_caches.size())) {
          continue;
        }
        const auto& cylinder = cylinder_caches[cache_index];
        float velocity = cylinder.velocity;
        const std::string& snap_key = cylinder.snap_key;
        const float kSnapReleaseVelocity = 0.5f;
        if (std::abs(velocity) <= kSnapReleaseVelocity) {
          workpiece.internalStates[snap_key] = 0.0f;
          continue;
        }
        ImVec2 rod_tip = cylinder.rod_tip;
        ImVec2 rod_dir = cylinder.rod_dir;
        ImVec2 rod_vel = cylinder.rod_vel;
        Aabb rod_swept = cylinder.rod_swept;
        bool snapped_to_cylinder = false;
        Aabb cylinder_box = cylinder.box;
        if (!AabbOverlaps(workpiece_box, cylinder_box)) {
          workpiece.internalStates[snap_key] = 0.0f;
        }
        if (!AabbOverlaps(workpiece_box, rod_swept)) {
          continue;
        }
        workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
        ImVec2 workpiece_center(
            workpiece.position.x + workpiece.size.width * 0.5f,
            workpiece.position.y + workpiece.size.height * 0.5f);
        ImVec2 to_work(workpiece_center.x - rod_tip.x,
                       workpiece_center.y - rod_tip.y);
        ImVec2 rod_perp(-rod_dir.y, rod_dir.x);
        float lateral = Dot(to_work, rod_perp);
        bool snap_locked =
            workpiece.internalStates.count(snap_key) &&
            workpiece.internalStates.at(snap_key) > 0.5f;
        if (!snap_locked && std::abs(lateral) <= kSnapDistance) {
          workpiece_center.x -= rod_perp.x * lateral;
          workpiece_center.y -= rod_perp.y * lateral;
          workpiece.position.x =
              workpiece_center.x - workpiece.size.width * 0.5f;
          workpiece.position.y =
              workpiece_center.y - workpiece.size.height * 0.5f;
          to_work.x = workpiece_center.x - rod_tip.x;
          to_work.y = workpiece_center.y - rod_tip.y;
          vel_x = 0.0f;
          vel_y = 0.0f;
          snapped_to_cylinder = true;
          workpiece.internalStates[snap_key] = 1.0f;
        }
        const float kContactGap = 1.0f;
        if (!snapped_to_cylinder && std::abs(velocity) > 1.0f) {
          float proj = Dot(to_work, rod_dir);
          float sign = (velocity >= 0.0f) ? 1.0f : -1.0f;
          float half_extent =
              0.5f * (std::abs(rod_dir.x) * workpiece.size.width +
                      std::abs(rod_dir.y) * workpiece.size.height);
          float desired_offset = sign * (half_extent + kContactGap);
          bool in_front = (sign > 0.0f) ? (proj >= 0.0f) : (proj <= 0.0f);
          if (in_front) {
            if ((sign > 0.0f && proj < desired_offset) ||
                (sign < 0.0f && proj > desired_offset)) {
              workpiece_center.x = rod_tip.x + rod_dir.x * desired_offset;
              workpiece_center.y = rod_tip.y + rod_dir.y * desired_offset;
              workpiece.position.x =
                  workpiece_center.x - workpiece.size.width * 0.5f;
              workpiece.position.y =
                  workpiece_center.y - workpiece.size.height * 0.5f;
            }
            vel_x = rod_vel.x;
            vel_y = rod_vel.y;
            pushed_by_cylinder = true;
          }
        }
      }

      if (pushed_by_cylinder) {
        on_conveyor = false;
      }

      for (int cache_index : candidate_processing) {
        if (cache_index < 0 ||
            cache_index >= static_cast<int>(proc_cylinder_caches.size())) {
          continue;
        }
        const auto& proc = proc_cylinder_caches[cache_index];
        bool is_down = proc.is_down;
        bool motor_on = proc.motor_on;
        const std::string& ready_key = proc.ready_key;
        const std::string& snap_key = proc.snap_key;
        bool ready = workpiece.internalStates.count(ready_key) &&
                     workpiece.internalStates.at(ready_key) > 0.5f;
        bool snap_locked = workpiece.internalStates.count(snap_key) &&
                           workpiece.internalStates.at(snap_key) > 0.5f;
        Aabb proc_workpiece_box = GetAabb(workpiece);
        bool overlapping = AabbOverlaps(proc_workpiece_box, proc.head_box);

        if (overlapping && !snap_locked) {
          workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
          workpiece.position.x = proc.center_x - workpiece.size.width * 0.5f;
          workpiece.position.y = proc.center_y - workpiece.size.height * 0.5f;
          vel_x = 0.0f;
          vel_y = 0.0f;
          proc_workpiece_box = GetAabb(workpiece);
          ready = true;
          workpiece.internalStates[snap_key] = 1.0f;
        } else if (overlapping) {
          workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
          ready = true;
        } else if (!overlapping) {
          ready = false;
        }

        if (ready && is_down) {
          if (motor_on) {
            workpiece.internalStates[state_keys::kIsProcessed] = 1.0f;
            ready = false;
          } else {
            ready = false;
          }
        }

        workpiece.internalStates[ready_key] = ready ? 1.0f : 0.0f;
      }

      int box_hit = -1;
      for (int cache_index : candidate_boxes) {
        if (cache_index < 0 ||
            cache_index >= static_cast<int>(box_caches.size())) {
          continue;
        }
        const auto& box_cache = box_caches[cache_index];
        if (AabbOverlaps(workpiece_box, box_cache.box)) {
          box_hit = std::max(box_hit, cache_index);
        }
      }
      if (box_hit >= 0) {
        const auto& box_cache = box_caches[box_hit];
        workpiece.internalStates[state_keys::kIsStuckBox] = 1.0f;
        workpiece.position.x =
            GetCenterX(box_cache.box) - workpiece.size.width * 0.5f;
        workpiece.position.y =
            GetCenterY(box_cache.box) - workpiece.size.height * 0.5f;
        vel_x = 0.0f;
        vel_y = 0.0f;
        workpiece_box = GetAabb(workpiece);
      }

      workpiece.position.x += vel_x * step_dt;
      workpiece.position.y += vel_y * step_dt;
      workpiece.internalStates[state_keys::kVelX] = vel_x;
      workpiece.internalStates[state_keys::kVelY] = vel_y;

      workpiece_box = GetAabb(workpiece);
      apply_sensor_detection(workpiece, true, candidate_sensors);
    }
  }
}

}  // namespace plc
