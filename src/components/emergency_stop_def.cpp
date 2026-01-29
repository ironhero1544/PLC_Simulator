#include "plc_emulator/components/emergency_stop_def.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"

#include <cmath>

namespace plc {
namespace {

ImU32 ToImU32(const Color& c) {
  return IM_COL32(c.r * 255, c.g * 255, c.b * 255, c.a * 255);
}

void DrawCircularArrows(ImDrawList* draw_list,
                        ImVec2 center,
                        float radius,
                        float thickness,
                        ImU32 color) {
  if (!draw_list || radius <= 0.0f) {
    return;
  }

  const float kPi = 3.14159265358979323846f;
  const float gap_deg = 22.0f * (kPi / 180.0f);
  const float arc_deg = 95.0f * (kPi / 180.0f);
  const float arrow_len = radius * 0.24f;
  const float tip_offset = thickness * 0.6f;
  const float arrow_half_w = thickness * 1.0f;

  for (int i = 0; i < 3; ++i) {
    float phase = -kPi * 0.5f + i * (2.0f * kPi / 3.0f);
    float a0 = phase + gap_deg * 0.5f;
    float a1 = a0 + arc_deg;

    float arc_radius = radius - thickness * 0.2f;
    float head_angle = (arrow_len + arrow_half_w) / arc_radius;
    float base_angle = a1 - head_angle;
    if (base_angle < a0) {
      base_angle = a0;
    }
    draw_list->PathClear();
    draw_list->PathArcTo(center, arc_radius, a0, base_angle, 16);
    draw_list->PathStroke(color, false, thickness);

    ImVec2 tip = {center.x + std::cos(a1) * (arc_radius + tip_offset),
                  center.y + std::sin(a1) * (arc_radius + tip_offset)};
    ImVec2 base_center = {center.x + std::cos(base_angle) * arc_radius,
                          center.y + std::sin(base_angle) * arc_radius};
    ImVec2 radial = {std::cos(base_angle), std::sin(base_angle)};
    ImVec2 left = {base_center.x + radial.x * arrow_half_w,
                   base_center.y + radial.y * arrow_half_w};
    ImVec2 right = {base_center.x - radial.x * arrow_half_w,
                    base_center.y - radial.y * arrow_half_w};
    draw_list->AddTriangleFilled(tip, left, right, color);
  }
}

const ComponentPortDef kPorts[] = {
    {0, {10.0f, 70.0f}, PortType::ELECTRIC, true, {0.6f, 0.6f, 0.6f, 1},
     "NC_1"},
    {1, {70.0f, 70.0f}, PortType::ELECTRIC, true, {0.6f, 0.6f, 0.6f, 1},
     "NC_2"},
    {2, {10.0f, 90.0f}, PortType::ELECTRIC, true, {0.6f, 0.6f, 0.6f, 1},
     "NO_1"},
    {3, {70.0f, 90.0f}, PortType::ELECTRIC, true, {0.6f, 0.6f, 0.6f, 1},
     "NO_2"},
};

void RenderEmergencyStop(ImDrawList* draw_list,
                         const PlacedComponent& comp,
                         ImVec2 pos,
                         float zoom) {
  bool is_pressed = comp.internalStates.count(state_keys::kIsPressed) &&
                    comp.internalStates.at(state_keys::kIsPressed) > 0.5f;
  draw_list->AddRectFilled(pos, {pos.x + 80 * zoom, pos.y + 100 * zoom},
                           IM_COL32(240, 240, 240, 255));
  draw_list->AddRect(pos, {pos.x + 80 * zoom, pos.y + 100 * zoom},
                     IM_COL32(0, 0, 0, 255), 0, 0, 2 * zoom);

  ImVec2 center = {pos.x + 40 * zoom, pos.y + 35 * zoom};
  if (!is_pressed) {
    draw_list->AddCircleFilled(
        ImVec2(center.x + 2.0f * zoom, center.y + 2.0f * zoom), 25 * zoom,
        IM_COL32(0, 0, 0, 50));
  }
  draw_list->AddCircleFilled(center, 25 * zoom, IM_COL32(255, 0, 0, 255));
  draw_list->AddCircle(center, 25 * zoom, IM_COL32(0, 0, 0, 255), 0,
                       2 * zoom);
  DrawCircularArrows(draw_list, center, 17 * zoom, 3 * zoom,
                     IM_COL32(255, 220, 220, 255));

  draw_list->AddLine({pos.x + 10 * zoom, pos.y + 70 * zoom},
                     {pos.x + 30 * zoom, pos.y + 70 * zoom},
                     IM_COL32(0, 0, 0, 255), 2 * zoom);
  draw_list->AddLine({pos.x + 50 * zoom, pos.y + 70 * zoom},
                     {pos.x + 70 * zoom, pos.y + 70 * zoom},
                     IM_COL32(0, 0, 0, 255), 2 * zoom);
  draw_list->AddLine({pos.x + 30 * zoom, pos.y + 75 * zoom},
                     {pos.x + 50 * zoom, pos.y + 75 * zoom},
                     IM_COL32(0, 0, 0, 255), 2 * zoom);
  draw_list->AddLine({pos.x + 40 * zoom, pos.y + 68 * zoom},
                     {pos.x + 40 * zoom, pos.y + 75 * zoom},
                     IM_COL32(0, 0, 0, 255), 2 * zoom);

  draw_list->AddLine({pos.x + 10 * zoom, pos.y + 90 * zoom},
                     {pos.x + 30 * zoom, pos.y + 90 * zoom},
                     IM_COL32(0, 0, 0, 255), 2 * zoom);
  draw_list->AddLine({pos.x + 50 * zoom, pos.y + 90 * zoom},
                     {pos.x + 70 * zoom, pos.y + 90 * zoom},
                     IM_COL32(0, 0, 0, 255), 2 * zoom);

  for (const auto& p : kPorts) {
    draw_list->AddCircleFilled(
        {pos.x + p.rel_pos.x * zoom, pos.y + p.rel_pos.y * zoom}, 4 * zoom,
        ToImU32(p.color));
  }
}

void InitEmergencyStopDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kIsPressed] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::EMERGENCY_STOP,
    "component.emergency_stop.name",
    "component.emergency_stop.desc",
    {80.0f, 100.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderEmergencyStop,
    &InitEmergencyStopDefaults,
    ComponentCategory::BOTH,
    "icon.emergency_stop",
    "E-Stop",
};

}  // namespace

const ComponentDefinition* GetEmergencyStopDefinition() {
  return &kDefinition;
}

}  // namespace plc
