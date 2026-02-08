#include "plc_emulator/components/limit_switch_def.h"

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

const char kPressedKey[] = "is_pressed";

const ComponentPortDef kPorts[] = {
    {0, {15.0f, 55.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "COM"},
    {1, {30.0f, 55.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NO"},
    {2, {45.0f, 55.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NC"},
};

void RenderLimitSwitch(ImDrawList* draw_list,
                       const PlacedComponent& comp,
                       ImVec2 screen_pos,
                       float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                           3.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(80, 80, 80, 255),
                     3.0f * zoom, 0, 2.0f);

  bool is_pressed = comp.internalStates.count(kPressedKey) &&
                    comp.internalStates.at(kPressedKey) > 0.5f;
  float actuator_y_offset = is_pressed ? 5.0f * zoom : -5.0f * zoom;
  ImU32 wheel_color =
      is_pressed ? IM_COL32(255, 220, 50, 255) : IM_COL32(255, 193, 7, 255);

  ImVec2 actuator_base_pos = {screen_pos.x + 30 * zoom, screen_pos.y};
  draw_list->AddRectFilled(
      ImVec2(actuator_base_pos.x - 5 * zoom,
             actuator_base_pos.y + actuator_y_offset),
      ImVec2(actuator_base_pos.x + 5 * zoom,
             actuator_base_pos.y + 15 * zoom + actuator_y_offset),
      IM_COL32(150, 150, 150, 255));
  draw_list->AddCircleFilled(
      ImVec2(actuator_base_pos.x, actuator_base_pos.y + actuator_y_offset),
      5 * zoom, wheel_color);

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 3 * zoom, ToImU32(port.color));
  }

  if (zoom > 0.5f) {
    float text_scale = zoom / 1.3f;
    float font_size = 16.0f * text_scale;
    const float max_font_size =
        ImGui::GetFontSize() * std::max(1.0f, zoom * 0.75f);
    if (font_size > max_font_size) {
      font_size = max_font_size;
    }
    draw_list->AddText(
        ImGui::GetFont(), font_size,
        ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 25 * zoom),
        IM_COL32(255, 255, 255, 255),
        TR("component.limit_switch.label", "LIMIT"));
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::LIMIT_SWITCH,
    "component.limit_switch.name",
    "component.limit_switch.desc",
    {60.0f, 60.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderLimitSwitch,
    nullptr,
    ComponentCategory::BOTH,
    "icon.limit_switch",
    "Limit",
};

}  // namespace

const ComponentDefinition* GetLimitSwitchDefinition() {
  return &kDefinition;
}

}  // namespace plc
