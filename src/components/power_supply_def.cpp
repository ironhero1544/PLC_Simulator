#include "plc_emulator/components/power_supply_def.h"

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
    {0, {85.0f, 20.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.0f, 0.0f, 1.0f}, "POWER_24V"},
    {1, {85.0f, 40.0f}, PortType::ELECTRIC, false,
     {0.0f, 0.0f, 0.0f, 1.0f}, "POWER_0V"},
};

void RenderPowerSupply(ImDrawList* draw_list,
                       const PlacedComponent& comp,
                       ImVec2 screen_pos,
                       float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(44, 62, 80, 255),
                           5.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                     5.0f * zoom, 0, 2.0f);

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 6 * zoom, ToImU32(port.color));
  }

  if (zoom > 0.5f) {
    draw_list->AddText(
        ImVec2(screen_pos.x + 10 * zoom, screen_pos.y + 25 * zoom),
        IM_COL32(255, 255, 255, 255),
        TR("component.power_supply.label", "24V DC"));
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::POWER_SUPPLY,
    "component.power_supply.name",
    "component.power_supply.desc",
    {100.0f, 80.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderPowerSupply,
    nullptr,
    ComponentCategory::BOTH,
    "icon.power_supply",
    "Power",
};

}  // namespace

const ComponentDefinition* GetPowerSupplyDefinition() {
  return &kDefinition;
}

}  // namespace plc
