#include "plc_emulator/components/cylinder_def.h"

#include "imgui.h"

#include "plc_emulator/lang/lang_manager.h"

#include <cmath>

namespace plc {

namespace {

const char kPositionKey[] = "position";
const char kStatusKey[] = "status";
const char kPressureAKey[] = "pressure_a";
const char kPressureBKey[] = "pressure_b";

const ComponentPortDef kPorts[] = {
    {0, {175.0f, 40.0f}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_IN_A"},
    {1, {305.0f, 40.0f}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_IN_B"},
};

void RenderCylinder(ImDrawList* draw_list,
                    const PlacedComponent& comp,
                    ImVec2 screen_pos,
                    float zoom) {
  float position = 0.0f;
  float status = 0.0f;
  float pressure_a = 0.0f;
  float pressure_b = 0.0f;

  if (comp.internalStates.count(kPositionKey)) {
    position = comp.internalStates.at(kPositionKey);
  }
  if (comp.internalStates.count(kStatusKey)) {
    status = comp.internalStates.at(kStatusKey);
  }
  if (comp.internalStates.count(kPressureAKey)) {
    pressure_a = comp.internalStates.at(kPressureAKey);
  }
  if (comp.internalStates.count(kPressureBKey)) {
    pressure_b = comp.internalStates.at(kPressureBKey);
  }

  ImVec2 body_start = {screen_pos.x + 170 * zoom, screen_pos.y + 10 * zoom};
  ImVec2 body_end = {screen_pos.x + 310 * zoom, screen_pos.y + 50 * zoom};
  draw_list->AddRectFilled(body_start, body_end, IM_COL32(210, 210, 210, 255),
                           4.0f * zoom);
  draw_list->AddRect(body_start, body_end, IM_COL32(100, 100, 100, 255),
                     4.0f * zoom, 0, 2.0f);

  draw_list->AddRectFilled(ImVec2(body_start.x, body_start.y),
                           ImVec2(body_start.x + 15 * zoom, body_end.y),
                           IM_COL32(180, 180, 180, 255), 4.0f * zoom,
                           ImDrawFlags_RoundCornersLeft);
  draw_list->AddRectFilled(ImVec2(body_end.x - 15 * zoom, body_start.y),
                           ImVec2(body_end.x, body_end.y),
                           IM_COL32(180, 180, 180, 255), 4.0f * zoom,
                           ImDrawFlags_RoundCornersRight);

  float rod_extended_length = position;
  ImVec2 rod_base_pos = {body_start.x, screen_pos.y + 25 * zoom};
  ImVec2 rod_tip_pos = {rod_base_pos.x - rod_extended_length * zoom,
                        rod_base_pos.y};

  draw_list->AddRectFilled(
      ImVec2(rod_tip_pos.x - 15 * zoom, rod_tip_pos.y - 5 * zoom),
      ImVec2(rod_tip_pos.x, rod_tip_pos.y + 5 * zoom),
      IM_COL32(150, 150, 150, 255));
  draw_list->AddLine(rod_tip_pos,
                     ImVec2(body_start.x + 125 * zoom, rod_base_pos.y),
                     IM_COL32(120, 120, 120, 255), 5.0f * zoom);

  if (std::abs(status) > 0.1f && zoom > 0.4f) {
    ImVec2 arrow_center = {screen_pos.x + 240 * zoom, screen_pos.y + 5 * zoom};
    ImU32 arrow_color =
        (status > 0) ? IM_COL32(0, 200, 0, 200) : IM_COL32(255, 0, 0, 200);
    float dir = (status > 0) ? -1.0f : 1.0f;

    draw_list->AddLine(ImVec2(arrow_center.x - 10 * dir * zoom, arrow_center.y),
                       ImVec2(arrow_center.x + 10 * dir * zoom, arrow_center.y),
                       arrow_color, 3.0f * zoom);
    draw_list->AddTriangleFilled(
        ImVec2(arrow_center.x + 10 * dir * zoom, arrow_center.y - 5 * zoom),
        ImVec2(arrow_center.x + 10 * dir * zoom, arrow_center.y + 5 * zoom),
        ImVec2(arrow_center.x + 18 * dir * zoom, arrow_center.y), arrow_color);
  }

  ImU32 port_a_color = (pressure_a > 1.0f) ? IM_COL32(0, 180, 255, 255)
                                           : IM_COL32(33, 150, 243, 128);
  ImU32 port_b_color = (pressure_b > 1.0f) ? IM_COL32(0, 180, 255, 255)
                                           : IM_COL32(33, 150, 243, 128);

  float text_scale = zoom / 1.3f;
  float font_size = ImGui::GetFontSize() * text_scale;
  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    ImU32 color = (port.id == 0) ? port_a_color : port_b_color;
    draw_list->AddCircleFilled(port_pos, 5 * zoom, color);
    draw_list->AddCircle(port_pos, 5 * zoom, IM_COL32(50, 50, 50, 255), 12,
                         1.5f * zoom);

    if (zoom > 0.5f) {
      const char* label = (port.id == 0) ? "A" : "B";
      draw_list->AddText(ImGui::GetFont(), font_size,
                         ImVec2(port_pos.x - 5 * zoom, port_pos.y - 20 * zoom),
                         IM_COL32(50, 50, 50, 255), label);
    }
  }

  if (comp.selected && zoom > 0.3f) {
    float detection_range_pixels = 50.0f * zoom;
    draw_list->AddCircle(rod_tip_pos, detection_range_pixels,
                         IM_COL32(255, 165, 0, 100), 32, 2.0f);
    draw_list->AddCircleFilled(rod_tip_pos, 4.0f, IM_COL32(255, 165, 0, 200));
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::CYLINDER,
    "component.cylinder.name",
    "component.cylinder.desc",
    {340.0f, 60.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderCylinder,
    nullptr,
    ComponentCategory::BOTH,
    "icon.cylinder",
    "Cylinder",
};

}  // namespace

const ComponentDefinition* GetCylinderDefinition() {
  return &kDefinition;
}

}  // namespace plc
