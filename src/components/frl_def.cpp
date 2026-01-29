#include "plc_emulator/components/frl_def.h"

#include "imgui.h"

#include "plc_emulator/lang/lang_manager.h"

#include <cmath>
#include <cstdio>

namespace plc {

namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

const char kPressureKey[] = "air_pressure";

const ComponentPortDef kPorts[] = {
    {0, {40.0f, 90.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_OUT"},
};

void RenderFrl(ImDrawList* draw_list,
               const PlacedComponent& comp,
               ImVec2 screen_pos,
               float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(232, 248, 253, 255),
                           5.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                     5.0f * zoom, 0, 2.0f);

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 6 * zoom, ToImU32(port.color));
    draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0,
                         2.0f);
  }

  float pressure = 6.0f;
  if (comp.internalStates.count(kPressureKey)) {
    pressure = comp.internalStates.at(kPressureKey);
  }
  float angle = (pressure / 10.0f) * 2.0f * 3.14159265f;

  ImVec2 handle_center = {screen_pos.x + 40 * zoom,
                          screen_pos.y + 25 * zoom};
  float handle_radius = 15 * zoom;
  draw_list->AddCircleFilled(handle_center, handle_radius,
                             IM_COL32(80, 80, 80, 255));
  draw_list->AddCircle(handle_center, handle_radius, IM_COL32(50, 50, 50, 255),
                       12, 1.5f * zoom);
  ImVec2 line_end = {
      static_cast<float>(handle_center.x +
                         (handle_radius - 2 * zoom) * cos(angle - 1.57f)),
      static_cast<float>(handle_center.y +
                         (handle_radius - 2 * zoom) * sin(angle - 1.57f))};
  draw_list->AddLine(handle_center, line_end, IM_COL32(255, 255, 255, 200),
                     2.0f * zoom);

  if (zoom > 0.5f) {
    draw_list->AddText(
        ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 50 * zoom),
        IM_COL32(50, 50, 50, 255),
        TR("component.frl.label", "FRL UNIT"));
    char pressure_text[16];
    std::snprintf(pressure_text, sizeof(pressure_text), "%.1f bar", pressure);
    draw_list->AddText(
        ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 65 * zoom),
        IM_COL32(50, 50, 50, 255), pressure_text);
  }
}

void InitFrlDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[kPressureKey] = 6.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::FRL,
    "component.frl.name",
    "component.frl.desc",
    {80.0f, 100.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderFrl,
    &InitFrlDefaults,
    ComponentCategory::BOTH,
    "icon.frl",
    "FRL",
};

}  // namespace

const ComponentDefinition* GetFrlDefinition() {
  return &kDefinition;
}

}  // namespace plc
