#include "plc_emulator/core/application.h"

#include "plc_emulator/physics/box2d_simulation.h"

namespace plc {

bool Application::UpdateWorkpieceInteractionsBox2d(float delta_time,
                                                 bool warmup_only) {
  const PhysicalSimulationView view{
      physics_lod_state_.has_view, physics_lod_state_.zoom,
      physics_lod_state_.view_min_x, physics_lod_state_.view_max_x,
      physics_lod_state_.view_min_y, physics_lod_state_.view_max_y};
  return UpdateWorkpiecesBox2d(this, &placed_components_, delta_time,
                              warmup_only, view);
}

bool Application::UpdateSensorsBox2d() {
  return UpdatePhysicalSensorsBox2d(this, &placed_components_);
}

}  // namespace plc
