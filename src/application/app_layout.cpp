// app_layout.cpp
//
// Application layout helpers.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/core/component_transform.h"

namespace plc {

float Application::GetLayoutScale() const {
  return ui_settings_.layout_scale;
}

void Application::UpdatePortPositions() {
  port_positions_.clear();

  for (const auto& comp : placed_components_) {
    const ComponentDefinition* def = GetComponentDefinition(comp.type);
    if (!def || !def->ports || def->port_count <= 0) {
      continue;
    }
    for (int i = 0; i < def->port_count; ++i) {
      const ComponentPortDef& port_def = def->ports[i];
      ImVec2 world_pos =
          LocalToWorld(comp, ImVec2(port_def.rel_pos.x, port_def.rel_pos.y));
      Position world_pos_data = {world_pos.x, world_pos.y};
      port_positions_[{comp.instanceId, port_def.id}] = world_pos_data;
    }
  }
}

ImVec2 Application::WorldToScreen(const ImVec2& world_pos) const {
  float x = canvas_top_left_.x + (world_pos.x + camera_offset_.x) * camera_zoom_;
  float y = canvas_top_left_.y + (world_pos.y + camera_offset_.y) * camera_zoom_;
  return ImVec2(x, y);
}

ImVec2 Application::ScreenToWorld(const ImVec2& screen_pos) const {
  float x =
      (screen_pos.x - canvas_top_left_.x) / camera_zoom_ - camera_offset_.x;
  float y =
      (screen_pos.y - canvas_top_left_.y) / camera_zoom_ - camera_offset_.y;
  return ImVec2(x, y);
}

}  // namespace plc
