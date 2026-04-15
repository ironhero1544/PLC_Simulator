#include "plc_emulator/components/sensor_def.h"

#include "imgui.h"

#include "plc_emulator/components/component_input_resolver.h"
#include "plc_emulator/lang/lang_manager.h"

namespace plc {

namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

const char kPoweredKey[] = "is_powered";

const ComponentPortDef kPorts[] = {
    {0, {30.0f, 110.0f}, PortType::ELECTRIC, true,
     {1.0f, 0.0f, 0.0f, 1.0f}, "POWER_24V"},
    {1, {50.0f, 110.0f}, PortType::ELECTRIC, true,
     {0.0f, 0.0f, 0.0f, 1.0f}, "POWER_0V"},
    {2, {70.0f, 110.0f}, PortType::ELECTRIC, true,
     {0.6f, 0.6f, 0.6f, 1.0f}, "SIGNAL"},
};

void RenderSensor(ImDrawList* draw_list,
                  const PlacedComponent& comp,
                  ImVec2 screen_pos,
                  float zoom) {
  ImVec2 body_start = {screen_pos.x + 15 * zoom, screen_pos.y + 6 * zoom};
  ImVec2 body_end = {screen_pos.x + 85 * zoom, screen_pos.y + 107 * zoom};
  draw_list->AddRectFilled(body_start, body_end, IM_COL32(192, 192, 192, 255),
                           12.0f * zoom);
  draw_list->AddRect(body_start, body_end, IM_COL32(51, 51, 51, 255),
                     12.0f * zoom, 0, 2.0f);

  ImVec2 head_start = {screen_pos.x + 25 * zoom, screen_pos.y + 2 * zoom};
  ImVec2 head_end = {screen_pos.x + 75 * zoom, screen_pos.y + 15 * zoom};
  draw_list->AddRectFilled(head_start, head_end, IM_COL32(25, 118, 210, 255),
                           3.0f * zoom);
  draw_list->AddRect(head_start, head_end, IM_COL32(51, 51, 51, 255),
                     3.0f * zoom, 0, 2.0f);

  bool is_powered = comp.internalStates.count(kPoweredKey) &&
                    comp.internalStates.at(kPoweredKey) > 0.5f;
  bool is_detected = component_input::IsSensorDetected(comp);

  ImU32 led_color = IM_COL32(80, 80, 80, 255);
  if (is_powered) {
    led_color = is_detected ? IM_COL32(0, 255, 0, 255)
                            : IM_COL32(255, 0, 0, 255);
  }

  draw_list->AddCircleFilled(
      ImVec2(screen_pos.x + 50 * zoom, screen_pos.y + 19 * zoom), 4 * zoom,
      led_color);
  draw_list->AddCircle(
      ImVec2(screen_pos.x + 50 * zoom, screen_pos.y + 19 * zoom), 4 * zoom,
      IM_COL32(51, 51, 51, 255), 0, 1.0f);

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 4 * zoom, ToImU32(port.color));
  }

}

const ComponentDefinition kDefinition = {
    ComponentType::SENSOR,
    "component.sensor.name",
    "component.sensor.desc",
    {100.0f, 120.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderSensor,
    nullptr,
    ComponentCategory::BOTH,
    "icon.sensor",
    "Sensor",
};

}  // namespace

const ComponentDefinition* GetSensorDefinition() {
  return &kDefinition;
}

}  // namespace plc
