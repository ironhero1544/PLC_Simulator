#include "plc_emulator/components/valve_single_def.h"

#include "imgui.h"

#include "plc_emulator/lang/lang_manager.h"

namespace plc {

namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

const ComponentPortDef kPorts[] = {
    {0, {15.0f, 15.0f}, PortType::ELECTRIC, true,
     {1.0f, 0.0f, 0.0f, 1.0f}, "SOL_A_24V"},
    {1, {15.0f, 30.0f}, PortType::ELECTRIC, true,
     {0.0f, 0.0f, 0.0f, 1.0f}, "SOL_A_0V"},
    {2, {40.0f, 90.0f}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1.0f}, "P_IN"},
    {3, {25.0f, 55.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PORT_A_OUT"},
    {4, {55.0f, 55.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PORT_B_OUT"},
};

void RenderValveSingle(ImDrawList* draw_list,
                       const PlacedComponent& comp,
                       ImVec2 screen_pos,
                       float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(224, 224, 224, 255),
                           5.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                     5.0f * zoom, 0, 2.0f);

  ImVec2 coil_start = {screen_pos.x + 5 * zoom, screen_pos.y + 10 * zoom};
  ImVec2 coil_end = {screen_pos.x + 25 * zoom, screen_pos.y + 40 * zoom};
  draw_list->AddRectFilled(coil_start, coil_end, IM_COL32(200, 200, 200, 255),
                           3.0f * zoom);
  draw_list->AddRect(coil_start, coil_end, IM_COL32(100, 100, 100, 255),
                     3.0f * zoom, 0, 1.0f);

  draw_list->AddCircleFilled(
      ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 15 * zoom), 5 * zoom,
      IM_COL32(244, 67, 54, 255));
  draw_list->AddCircleFilled(
      ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 30 * zoom), 5 * zoom,
      IM_COL32(0, 0, 0, 255));

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 6 * zoom, ToImU32(port.color));
  }

  if (zoom > 0.5f) {
    draw_list->AddText(
        ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 40 * zoom),
        IM_COL32(50, 50, 50, 255),
        TR("component.valve.label_5_2", "5/2WAY"));
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::VALVE_SINGLE,
    "component.valve_single.name",
    "component.valve_single.desc",
    {80.0f, 100.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderValveSingle,
    nullptr,
    ComponentCategory::BOTH,
    "icon.valve_single",
    "Valve S",
};

}  // namespace

const ComponentDefinition* GetValveSingleDefinition() {
  return &kDefinition;
}

}  // namespace plc
