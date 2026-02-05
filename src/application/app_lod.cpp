// app_lod.cpp
//
// Viewport/zoom-based LOD calculation for physics.

#include "plc_emulator/core/application.h"

#include <algorithm>
#include <cmath>

namespace plc {

void Application::UpdatePhysicsLod() {
  PhysicsLodState next;
  next.zoom = camera_zoom_;

  bool has_canvas = canvas_size_.x > 1.0f && canvas_size_.y > 1.0f;
  next.has_view = has_canvas;

  if (has_canvas) {
    ImVec2 screen_max = {canvas_top_left_.x + canvas_size_.x,
                         canvas_top_left_.y + canvas_size_.y};
    ImVec2 world_a = ScreenToWorld(canvas_top_left_);
    ImVec2 world_b = ScreenToWorld(screen_max);
    next.view_min_x = std::min(world_a.x, world_b.x);
    next.view_max_x = std::max(world_a.x, world_b.x);
    next.view_min_y = std::min(world_a.y, world_b.y);
    next.view_max_y = std::max(world_a.y, world_b.y);
    float view_w = next.view_max_x - next.view_min_x;
    float view_h = next.view_max_y - next.view_min_y;
    next.view_radius = 0.5f * std::sqrt(view_w * view_w + view_h * view_h);
  }

  constexpr float kZoomHigh = 2.4f;
  constexpr float kZoomMedium = 1.2f;
  constexpr float kViewRadiusHigh = 700.0f;
  constexpr float kViewRadiusMedium = 1400.0f;

  next.tier = PhysicsLodTier::kLow;
  if ((next.zoom >= kZoomHigh) ||
      (next.has_view && next.view_radius <= kViewRadiusHigh)) {
    next.tier = PhysicsLodTier::kHigh;
  } else if ((next.zoom >= kZoomMedium) ||
             (next.has_view && next.view_radius <= kViewRadiusMedium)) {
    next.tier = PhysicsLodTier::kMedium;
  }

  physics_lod_state_ = next;
}

}  // namespace plc
