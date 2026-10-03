#pragma once

#include <vector>

#include "plc_emulator/core/data_types.h"

namespace plc {

struct PhysicalSimulationView {
  bool has_view = false;
  float zoom = 1.0f;
  float view_min_x = 0.0f;
  float view_max_x = 0.0f;
  float view_min_y = 0.0f;
  float view_max_y = 0.0f;
};

// owner identifies the simulation whose cached collision world is in use.
bool UpdateWorkpiecesBox2d(const void* owner,
                          std::vector<PlacedComponent>* components,
                          float delta_time, bool warmup_only,
                          const PhysicalSimulationView& view = {});
bool UpdatePhysicalSensorsBox2d(const void* owner,
                               std::vector<PlacedComponent>* components);

}  // namespace plc
