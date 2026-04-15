// app_layout.cpp
//
// Application layout helpers.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/core/component_transform.h"

#include <algorithm>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace plc {
namespace {

constexpr float kReferenceDisplayWidth = 1920.0f;
constexpr float kReferenceDisplayHeight = 1080.0f;
constexpr float kMinResolutionScale = 0.5f;
constexpr float kMaxResolutionScale = 3.0f;

float ClampResolutionScale(float width, float height) {
  if (width <= 0.0f || height <= 0.0f) {
    return 1.0f;
  }
  const float scale_x = width / kReferenceDisplayWidth;
  const float scale_y = height / kReferenceDisplayHeight;
  const float auto_scale = std::min(scale_x, scale_y);
  return std::clamp(auto_scale, kMinResolutionScale, kMaxResolutionScale);
}

}  // namespace

float Application::GetResolutionScale() const {
  if (ImGui::GetCurrentContext()) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    if (display_size.x > 0.0f && display_size.y > 0.0f) {
      return GetResolutionScaleForDisplaySize(display_size.x, display_size.y);
    }
  }

  if (window_) {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window_, &width, &height);
    if (width > 0 && height > 0) {
      return GetResolutionScaleForDisplaySize(static_cast<float>(width),
                                              static_cast<float>(height));
    }
  }
  return 1.0f;
}

float Application::GetResolutionScaleForDisplaySize(float width,
                                                    float height) const {
  return ClampResolutionScale(width, height);
}

float Application::GetLayoutScale() const {
  return ui_settings_.layout_scale * GetResolutionScale();
}

void Application::UpdatePortPositions() {
  port_positions_.clear();

  for (const auto& comp : placed_components_) {
    std::vector<Port> ports = GetRuntimePortsForComponent(comp);
    if (ports.empty()) {
      continue;
    }
    for (const auto& port_def : ports) {
      ImVec2 world_pos =
          LocalToWorld(comp,
                       ImVec2(port_def.relativePos.x, port_def.relativePos.y));
      Position world_pos_data = {world_pos.x, world_pos.y};
      port_positions_[{comp.instanceId, port_def.id}] = world_pos_data;
    }
  }
}

ImVec2 Application::WorldToScreen(const ImVec2& world_pos) const {
  const float zoom = camera_zoom_;
  const float half_w = canvas_size_.x * 0.5f;
  const float half_h = canvas_size_.y * 0.5f;
  const float screen_origin_x =
      canvas_top_left_.x + camera_offset_.x * zoom + half_w * (1.0f - zoom);
  const float screen_origin_y =
      canvas_top_left_.y + camera_offset_.y * zoom + half_h * (1.0f - zoom);
  // TODO: Migrate component positions from top-left anchors to center-based
  // anchors so camera transforms and render origins can share one model.
  float x = screen_origin_x + world_pos.x * zoom;
  float y = screen_origin_y + world_pos.y * zoom;
  return ImVec2(x, y);
}

ImVec2 Application::ScreenToWorld(const ImVec2& screen_pos) const {
  const float zoom = camera_zoom_;
  const float half_w = canvas_size_.x * 0.5f;
  const float half_h = canvas_size_.y * 0.5f;
  const float screen_origin_x =
      canvas_top_left_.x + camera_offset_.x * zoom + half_w * (1.0f - zoom);
  const float screen_origin_y =
      canvas_top_left_.y + camera_offset_.y * zoom + half_h * (1.0f - zoom);
  float x = (screen_pos.x - screen_origin_x) / zoom;
  float y = (screen_pos.y - screen_origin_y) / zoom;
  return ImVec2(x, y);
}

}  // namespace plc
