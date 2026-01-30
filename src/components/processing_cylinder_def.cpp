#include "plc_emulator/components/processing_cylinder_def.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"

#include <algorithm>
#include <cmath>

namespace plc {
namespace {

ImU32 ToImU32(const Color& c) {
  return IM_COL32(c.r * 255, c.g * 255, c.b * 255, c.a * 255);
}

const ImU32 kBodyColor = IM_COL32(247, 247, 247, 255);
const ImU32 kFrameColor = IM_COL32(28, 28, 28, 255);
const ImU32 kShadowSoft = IM_COL32(0, 0, 0, 90);
const ImU32 kHeaderColor = IM_COL32(220, 220, 220, 255);
const ImU32 kBaseColor = IM_COL32(232, 232, 232, 255);
const ImU32 kRailColor = IM_COL32(70, 70, 70, 255);
const ImU32 kPlateOuter = IM_COL32(180, 180, 180, 255);

const ComponentPortDef kPorts[] = {
    {0, {20.0f, 169.0f}, PortType::ELECTRIC, true, {0.90f, 0.10f, 0.10f, 1},
     "POWER_24V"},
    {1, {40.0f, 169.0f}, PortType::ELECTRIC, true, {0.96f, 0.75f, 0.15f, 1},
     "SENSOR_FWD"},
    {2, {60.0f, 169.0f}, PortType::ELECTRIC, true, {0.20f, 0.70f, 0.25f, 1},
     "SENSOR_REV"},
    {3, {80.0f, 169.0f}, PortType::ELECTRIC, true, {0.95f, 0.45f, 0.10f, 1},
     "MOTOR_CTRL"},
    {4, {100.0f, 169.0f}, PortType::ELECTRIC, true, {0.05f, 0.05f, 0.05f, 1},
     "POWER_0V"},
    {5, {92.0f, 13.0f}, PortType::PNEUMATIC, true, {0.15f, 0.55f, 0.90f, 1},
     "PNEUMATIC_IN_A"},
    {6, {110.0f, 13.0f}, PortType::PNEUMATIC, true, {0.15f, 0.55f, 0.90f, 1},
     "PNEUMATIC_IN_B"},
};

void RenderProcessingCylinderBody(ImDrawList* draw_list,
                                  ImVec2 pos,
                                  float zoom) {
  const float w = 120.0f * zoom;
  const float h = 180.0f * zoom;
  ImVec2 body_min = pos;
  ImVec2 body_max = {pos.x + w, pos.y + h};

  draw_list->AddRectFilled(body_min, body_max, kBodyColor);
  draw_list->AddRect(body_min, body_max, kFrameColor, 0, 2.0f * zoom);

  ImVec2 header_min = {body_max.x - 38.0f * zoom, body_min.y};
  ImVec2 header_max = {body_max.x, body_min.y + 26.0f * zoom};
  draw_list->AddRectFilled(header_min, header_max, kHeaderColor);
  draw_list->AddRect(header_min, header_max, kFrameColor, 0, 2.0f * zoom);

  ImVec2 base_min = {body_min.x, body_max.y - 22.0f * zoom};
  ImVec2 base_max = body_max;
  draw_list->AddRectFilled(base_min, base_max, kBaseColor);
  draw_list->AddRect(base_min, base_max, kFrameColor, 0, 2.0f * zoom);
}

void RenderProcessingCylinderPorts(ImDrawList* draw_list,
                                   ImVec2 pos,
                                   float zoom) {
  for (const auto& p : kPorts) {
    ImVec2 pc = {pos.x + p.rel_pos.x * zoom, pos.y + p.rel_pos.y * zoom};
    draw_list->AddCircleFilled(pc, 4.2f * zoom, ToImU32(p.color));
    draw_list->AddCircle(pc, 4.2f * zoom, kFrameColor, 0, 1.4f * zoom);
  }
}

void RenderProcessingCylinderHeadInternal(ImDrawList* draw_list,
                                          const PlacedComponent& comp,
                                          ImVec2 pos,
                                          float zoom) {
  constexpr float kDrillUpPos = 30.0f;
  constexpr float kDrillDownPos = 15.0f;

  float pos_val = comp.internalStates.count(state_keys::kPosition)
                      ? comp.internalStates.at(state_keys::kPosition)
                      : kDrillUpPos;
  float pos_norm =
      (kDrillUpPos - pos_val) / (kDrillUpPos - kDrillDownPos);
  pos_norm = std::clamp(pos_norm, 0.0f, 1.0f);
  const float outer_r = 42.0f * zoom;
  const float inner_r = 32.0f * zoom;
  const float inner_most_base = 30.0f * zoom;
  float drill_scale = 1.0f - (0.35f * pos_norm);
  drill_scale = std::clamp(drill_scale, 0.65f, 1.0f);
  const float inner_most_r = inner_most_base * drill_scale;
  float cur_y = 90.0f;
  ImVec2 center = {pos.x + 60.0f * zoom, pos.y + cur_y * zoom};

  float rail_top = pos.y + 0.0f * zoom;
  float rail_bottom = center.y - outer_r + 6.0f * zoom;
  float rail_offset = 14.0f * zoom;
  float rail_left_x = center.x - rail_offset;
  float rail_right_x = center.x + rail_offset;

  draw_list->AddLine({rail_left_x + 4.0f * zoom, rail_top + 4.0f * zoom},
                     {rail_left_x + 4.0f * zoom, rail_bottom + 4.0f * zoom},
                     kShadowSoft, 2.4f * zoom);
  draw_list->AddLine({rail_right_x + 4.0f * zoom, rail_top + 4.0f * zoom},
                     {rail_right_x + 4.0f * zoom, rail_bottom + 4.0f * zoom},
                     kShadowSoft, 2.4f * zoom);
  draw_list->AddLine({rail_left_x, rail_top}, {rail_left_x, rail_bottom},
                     kRailColor, 2.0f * zoom);
  draw_list->AddLine({rail_right_x, rail_top}, {rail_right_x, rail_bottom},
                     kRailColor, 2.0f * zoom);

  ImVec2 rod_min = {center.x - 14.0f * zoom, rail_top};
  ImVec2 rod_max = {center.x + 14.0f * zoom, rail_bottom};
  draw_list->AddRectFilled({rod_min.x + 4.0f * zoom, rod_min.y + 4.0f * zoom},
                           {rod_max.x + 4.0f * zoom, rod_max.y + 4.0f * zoom},
                           kShadowSoft);
  draw_list->AddRectFilled(rod_min, rod_max, IM_COL32(210, 210, 210, 255));
  draw_list->AddRect(rod_min, rod_max, kFrameColor, 0, 1.5f * zoom);

  draw_list->AddCircleFilled({center.x + 5.0f * zoom, center.y + 5.0f * zoom},
                             outer_r + 3.0f * zoom, kShadowSoft);
  draw_list->AddCircleFilled(center, outer_r, kPlateOuter);
  draw_list->AddCircle(center, outer_r, kFrameColor, 0, 2.0f * zoom);
  draw_list->AddCircleFilled(center, inner_r, kBodyColor);
  draw_list->AddCircle(center, inner_r, kFrameColor, 0, 2.0f * zoom);
  draw_list->AddCircle(center, inner_most_r, kFrameColor, 0, 2.0f * zoom);

  float motor_angle = 0.0f;
  if (comp.internalStates.count(state_keys::kMotorOn) &&
      comp.internalStates.at(state_keys::kMotorOn) > 0.5f) {
    motor_angle = comp.internalStates.count(state_keys::kRotationAngle)
                      ? comp.internalStates.at(state_keys::kRotationAngle)
                      : 0.0f;
    draw_list->AddCircleFilled(center, 12.0f * zoom * drill_scale,
                               IM_COL32(200, 200, 200, 110));
  }
  float base_angle = -0.78539816339f;
  float final_angle = base_angle + motor_angle;
  float drill_len = inner_most_r - 2.0f * zoom;
  draw_list->AddLine({center.x - drill_len * std::cos(final_angle),
                      center.y - drill_len * std::sin(final_angle)},
                     {center.x + drill_len * std::cos(final_angle),
                      center.y + drill_len * std::sin(final_angle)},
                     kFrameColor, 2.4f * zoom);
}

void RenderProcessingCylinder(ImDrawList* draw_list,
                              const PlacedComponent& comp,
                              ImVec2 pos,
                              float zoom) {
  RenderProcessingCylinderBody(draw_list, pos, zoom);
  RenderProcessingCylinderHeadInternal(draw_list, comp, pos, zoom);
  RenderProcessingCylinderPorts(draw_list, pos, zoom);
}

void InitProcessingCylinderDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kPosition] = 30.0f;
  comp->internalStates[state_keys::kMotorOn] = 0.0f;
  comp->internalStates[state_keys::kRotationAngle] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::PROCESSING_CYLINDER,
    "component.processing_cylinder.name",
    "component.processing_cylinder.desc",
    {120.0f, 180.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderProcessingCylinder,
    &InitProcessingCylinderDefaults,
    ComponentCategory::BOTH,
    "icon.processing_cylinder",
    "Proc Cyl",
};

}  // namespace

const ComponentDefinition* GetProcessingCylinderDefinition() {
  return &kDefinition;
}

void RenderProcessingCylinderHead(ImDrawList* draw_list,
                                  const PlacedComponent& comp,
                                  ImVec2 pos,
                                  float zoom) {
  RenderProcessingCylinderHeadInternal(draw_list, comp, pos, zoom);
}

}  // namespace plc
