#include "plc_emulator/components/button_unit_def.h"

#include "imgui.h"

#include "plc_emulator/lang/lang_manager.h"

#include <string>

namespace plc {

namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

const ComponentPortDef kPorts[] = {
    {0, {80.0f, 20.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "COM"},
    {1, {100.0f, 20.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NO"},
    {2, {120.0f, 20.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NC"},
    {3, {155.0f, 20.0f}, PortType::ELECTRIC, true,
     {1.0f, 0.0f, 0.0f, 1.0f}, "LAMP_24V"},
    {4, {175.0f, 20.0f}, PortType::ELECTRIC, true,
     {0.0f, 0.0f, 0.0f, 1.0f}, "LAMP_0V"},
    {5, {80.0f, 50.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "COM"},
    {6, {100.0f, 50.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NO"},
    {7, {120.0f, 50.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NC"},
    {8, {155.0f, 50.0f}, PortType::ELECTRIC, true,
     {1.0f, 0.0f, 0.0f, 1.0f}, "LAMP_24V"},
    {9, {175.0f, 50.0f}, PortType::ELECTRIC, true,
     {0.0f, 0.0f, 0.0f, 1.0f}, "LAMP_0V"},
    {10, {80.0f, 80.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "COM"},
    {11, {100.0f, 80.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NO"},
    {12, {120.0f, 80.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "NC"},
    {13, {155.0f, 80.0f}, PortType::ELECTRIC, true,
     {1.0f, 0.0f, 0.0f, 1.0f}, "LAMP_24V"},
    {14, {175.0f, 80.0f}, PortType::ELECTRIC, true,
     {0.0f, 0.0f, 0.0f, 1.0f}, "LAMP_0V"},
};

void RenderButtonUnit(ImDrawList* draw_list,
                      const PlacedComponent& comp,
                      ImVec2 screen_pos,
                      float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(240, 240, 240, 255),
                           5.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                     5.0f * zoom, 0, 2.0f);

  ImU32 colors_off[] = {IM_COL32(180, 40, 30, 255),
                        IM_COL32(190, 170, 40, 255),
                        IM_COL32(50, 130, 50, 255)};
  ImU32 colors_on[] = {IM_COL32(255, 80, 70, 255),
                       IM_COL32(255, 235, 60, 255),
                       IM_COL32(90, 220, 90, 255)};

  for (int btn = 0; btn < 3; ++btn) {
    float y_offset =
        screen_pos.y + (20.0f + static_cast<float>(btn) * 30.0f) * zoom;
    ImVec2 button_pos = ImVec2(screen_pos.x + 25 * zoom, y_offset);

    std::string lamp_key = "lamp_on_" + std::to_string(btn);
    std::string pressed_key = "is_pressed_" + std::to_string(btn);
    bool lamp_on = comp.internalStates.count(lamp_key) &&
                   comp.internalStates.at(lamp_key) > 0.5f;
    bool is_pressed = comp.internalStates.count(pressed_key) &&
                      comp.internalStates.at(pressed_key) > 0.5f;

    if (!is_pressed) {
      draw_list->AddCircleFilled(
          ImVec2(button_pos.x + 2 * zoom, button_pos.y + 2 * zoom), 10 * zoom,
          IM_COL32(0, 0, 0, 50));
    }

    draw_list->AddCircleFilled(button_pos, 8 * zoom,
                               lamp_on ? colors_on[btn] : colors_off[btn]);
    draw_list->AddCircle(button_pos, 8 * zoom, IM_COL32(50, 50, 50, 255), 0,
                         2.0f);
  }

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 5 * zoom, ToImU32(port.color));
  }

  if (zoom > 0.5f) {
    float text_scale = zoom / 1.3f;
    float font_size = ImGui::GetFontSize() * text_scale;
    draw_list->AddText(
        ImGui::GetFont(), font_size,
        ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 5 * zoom),
        IM_COL32(50, 50, 50, 255),
        TR("component.button_unit.label", "BUTTON UNIT"));
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::BUTTON_UNIT,
    "component.button_unit.name",
    "component.button_unit.desc",
    {200.0f, 100.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderButtonUnit,
    nullptr,
    ComponentCategory::BOTH,
    "icon.button_unit",
    "Buttons",
};

}  // namespace

const ComponentDefinition* GetButtonUnitDefinition() {
  return &kDefinition;
}

}  // namespace plc
