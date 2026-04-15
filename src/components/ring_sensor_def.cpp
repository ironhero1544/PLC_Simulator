#include "plc_emulator/components/ring_sensor_def.h"

#include "imgui.h"
#include "plc_emulator/components/component_input_resolver.h"
#include "plc_emulator/components/state_keys.h"

namespace plc {
namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

const ComponentPortDef kPorts[] = {
    {0, {10.0f, 60.0f}, PortType::ELECTRIC, true, {1, 0, 0, 1}, "POWER_24V"},
    {1, {30.0f, 60.0f}, PortType::ELECTRIC, false, {0.6f, 0.6f, 0.6f, 1}, "SIGNAL"},
    {2, {50.0f, 60.0f}, PortType::ELECTRIC, true, {0, 0, 0, 1}, "POWER_0V"},
};

void RenderRingSensor(ImDrawList* draw_list,
                      const PlacedComponent& comp,
                      ImVec2 pos,
                      float zoom) {
  (void)comp;
  ImVec2 base_min = {pos.x, pos.y + 50.0f * zoom};
  ImVec2 base_max = {pos.x + 60.0f * zoom, pos.y + 70.0f * zoom};

  draw_list->AddRectFilled(base_min, base_max, IM_COL32(245, 245, 245, 255));
  draw_list->AddRect(base_min, base_max, IM_COL32(0, 0, 0, 255), 0.0f, 0,
                     2.0f * zoom);

  ImVec2 center = {pos.x + 30.0f * zoom, pos.y + 28.0f * zoom};
  // Inner diameter 20.0f -> inner radius 10.0f, stroke 4.0f -> center radius 12.0f
  float radius = 20.0f * zoom;
  const float kPi = 3.14159265f;
  draw_list->PathArcTo(center, radius, -kPi * 0.35f, kPi * 1.35f);
  draw_list->PathStroke(IM_COL32(120, 120, 120, 255), 0, 4.0f * zoom);

  for (const auto& port : kPorts) {
    ImVec2 p = {pos.x + port.rel_pos.x * zoom,
                pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(p, 3.5f * zoom, ToImU32(port.color));
    draw_list->AddCircle(p, 3.5f * zoom, IM_COL32(50, 50, 50, 255), 0,
                         1.0f * zoom);
  }
}

void InitRingSensorDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  component_input::SetSensorDetected(comp, false);
  comp->internalStates[state_keys::kIsPowered] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::RING_SENSOR,
    "component.ring_sensor.name",
    "component.ring_sensor.desc",
    {60.0f, 70.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderRingSensor,
    &InitRingSensorDefaults,
    ComponentCategory::BOTH,
    "icon.ring_sensor",
    "Ring",
};

}  // namespace

const ComponentDefinition* GetRingSensorDefinition() {
  return &kDefinition;
}

}  // namespace plc
