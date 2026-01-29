#include "plc_emulator/components/tower_lamp_def.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"

namespace plc {
namespace {

ImU32 ToImU32(const Color& c) {
  return IM_COL32(c.r * 255, c.g * 255, c.b * 255, c.a * 255);
}

const ComponentPortDef kPorts[] = {
    {0, {10.0f, 130.0f}, PortType::ELECTRIC, true, {0.6f, 0.6f, 0.6f, 1},
     "COM"},
    {1, {30.0f, 130.0f}, PortType::ELECTRIC, true, {1, 0, 0, 1}, "LAMP_R"},
    {2, {50.0f, 130.0f}, PortType::ELECTRIC, true, {1, 1, 0, 1}, "LAMP_Y"},
    {3, {70.0f, 130.0f}, PortType::ELECTRIC, true, {0, 1, 0, 1}, "LAMP_G"},
};

void RenderTowerLamp(ImDrawList* draw_list,
                     const PlacedComponent& comp,
                     ImVec2 pos,
                     float zoom) {
  draw_list->AddRectFilled({pos.x + 35 * zoom, pos.y + 90 * zoom},
                           {pos.x + 45 * zoom, pos.y + 120 * zoom},
                           IM_COL32(200, 200, 200, 255));
  draw_list->AddRectFilled({pos.x, pos.y + 120 * zoom},
                           {pos.x + 80 * zoom, pos.y + 140 * zoom},
                           IM_COL32(50, 50, 50, 255));

  bool r = comp.internalStates.count(state_keys::kLampRed) &&
           comp.internalStates.at(state_keys::kLampRed) > 0.5f;
  bool y = comp.internalStates.count(state_keys::kLampYellow) &&
           comp.internalStates.at(state_keys::kLampYellow) > 0.5f;
  bool g = comp.internalStates.count(state_keys::kLampGreen) &&
           comp.internalStates.at(state_keys::kLampGreen) > 0.5f;

  draw_list->AddRectFilled({pos.x + 20 * zoom, pos.y},
                           {pos.x + 60 * zoom, pos.y + 30 * zoom},
                           r ? IM_COL32(255, 0, 0, 255)
                             : IM_COL32(100, 0, 0, 255));
  draw_list->AddRect({pos.x + 20 * zoom, pos.y},
                     {pos.x + 60 * zoom, pos.y + 30 * zoom},
                     IM_COL32(0, 0, 0, 255), 0, 0, 2 * zoom);

  draw_list->AddRectFilled({pos.x + 20 * zoom, pos.y + 30 * zoom},
                           {pos.x + 60 * zoom, pos.y + 60 * zoom},
                           y ? IM_COL32(255, 255, 0, 255)
                             : IM_COL32(100, 100, 0, 255));
  draw_list->AddRect({pos.x + 20 * zoom, pos.y + 30 * zoom},
                     {pos.x + 60 * zoom, pos.y + 60 * zoom},
                     IM_COL32(0, 0, 0, 255), 0, 0, 2 * zoom);

  draw_list->AddRectFilled({pos.x + 20 * zoom, pos.y + 60 * zoom},
                           {pos.x + 60 * zoom, pos.y + 90 * zoom},
                           g ? IM_COL32(0, 255, 0, 255)
                             : IM_COL32(0, 100, 0, 255));
  draw_list->AddRect({pos.x + 20 * zoom, pos.y + 60 * zoom},
                     {pos.x + 60 * zoom, pos.y + 90 * zoom},
                     IM_COL32(0, 0, 0, 255), 0, 0, 2 * zoom);

  for (const auto& p : kPorts) {
    draw_list->AddCircleFilled(
        {pos.x + p.rel_pos.x * zoom, pos.y + p.rel_pos.y * zoom}, 4 * zoom,
        ToImU32(p.color));
  }
}

void InitTowerLampDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kLampRed] = 0.0f;
  comp->internalStates[state_keys::kLampYellow] = 0.0f;
  comp->internalStates[state_keys::kLampGreen] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::TOWER_LAMP,
    "component.tower_lamp.name",
    "component.tower_lamp.desc",
    {80.0f, 140.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderTowerLamp,
    &InitTowerLampDefaults,
    ComponentCategory::BOTH,
    "icon.tower_lamp",
    "Lamp",
};

}  // namespace

const ComponentDefinition* GetTowerLampDefinition() {
  return &kDefinition;
}

}  // namespace plc
