#include "plc_emulator/components/plc_def.h"

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

const char kRunningKey[] = "is_running";

const ComponentPortDef kPorts[] = {
    {0, {30.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X0"},
    {1, {55.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X1"},
    {2, {80.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X2"},
    {3, {105.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X3"},
    {4, {130.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X4"},
    {5, {155.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X5"},
    {6, {180.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X6"},
    {7, {205.0f, 25.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X7"},
    {8, {30.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X8"},
    {9, {55.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X9"},
    {10, {80.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X10"},
    {11, {105.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X11"},
    {12, {130.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X12"},
    {13, {155.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X13"},
    {14, {180.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X14"},
    {15, {205.0f, 45.0f}, PortType::ELECTRIC, true,
     {0.3f, 0.7f, 0.3f, 1.0f}, "X15"},
    {16, {30.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y0"},
    {17, {55.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y1"},
    {18, {80.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y2"},
    {19, {105.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y3"},
    {20, {130.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y4"},
    {21, {155.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y5"},
    {22, {180.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y6"},
    {23, {205.0f, 135.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y7"},
    {24, {30.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y8"},
    {25, {55.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y9"},
    {26, {80.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y10"},
    {27, {105.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y11"},
    {28, {130.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y12"},
    {29, {155.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y13"},
    {30, {180.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y14"},
    {31, {205.0f, 155.0f}, PortType::ELECTRIC, false,
     {1.0f, 0.6f, 0.0f, 1.0f}, "Y15"},
};

void RenderPlc(ImDrawList* draw_list,
               const PlacedComponent& comp,
               ImVec2 screen_pos,
               float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(240, 240, 240, 255),
                           5.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                     5.0f * zoom, 0, 2.0f);

  ImVec2 input_start = {screen_pos.x + 20 * zoom, screen_pos.y + 15 * zoom};
  ImVec2 input_end = {screen_pos.x + 230 * zoom, screen_pos.y + 65 * zoom};
  draw_list->AddRectFilled(input_start, input_end, IM_COL32(232, 232, 232, 255),
                           3.0f * zoom);
  draw_list->AddRect(input_start, input_end, IM_COL32(100, 100, 100, 255),
                     3.0f * zoom, 0, 1.0f);

  ImVec2 output_start = {screen_pos.x + 20 * zoom, screen_pos.y + 115 * zoom};
  ImVec2 output_end = {screen_pos.x + 230 * zoom, screen_pos.y + 165 * zoom};
  draw_list->AddRectFilled(output_start, output_end,
                           IM_COL32(232, 232, 232, 255), 3.0f * zoom);
  draw_list->AddRect(output_start, output_end, IM_COL32(100, 100, 100, 255),
                     3.0f * zoom, 0, 1.0f);

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 6 * zoom, ToImU32(port.color));
    draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0,
                         2.0f);
  }

  ImVec2 power_block_start = {screen_pos.x + 245 * zoom,
                              screen_pos.y + 15 * zoom};
  ImVec2 power_block_end = {screen_pos.x + 310 * zoom,
                            screen_pos.y + 140 * zoom};
  draw_list->AddRectFilled(power_block_start, power_block_end,
                           IM_COL32(232, 232, 232, 255), 3.0f * zoom);
  draw_list->AddRect(power_block_start, power_block_end,
                     IM_COL32(100, 100, 100, 255), 3.0f * zoom, 0, 1.0f);

  const char* labels[] = {"24V", "0V", "COM0", "COM1"};
  ImU32 colors[] = {IM_COL32(255, 0, 0, 255), IM_COL32(0, 0, 0, 255),
                    IM_COL32(128, 128, 128, 255), IM_COL32(128, 128, 128, 255)};
  for (int i = 0; i < 4; ++i) {
    ImVec2 port_pos = {screen_pos.x + 265 * zoom,
                       screen_pos.y + (40.0f + static_cast<float>(i) * 25.0f) *
                                           zoom};
    draw_list->AddCircleFilled(port_pos, 6 * zoom, colors[i]);
    draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0,
                         2.0f);
    if (zoom > 0.4f) {
      draw_list->AddText(ImVec2(port_pos.x + 10 * zoom, port_pos.y - 8 * zoom),
                         IM_COL32(50, 50, 50, 255), labels[i]);
    }
  }

  ImVec2 status_led_area_start = {screen_pos.x + 250 * zoom,
                                  screen_pos.y + 145 * zoom};
  ImVec2 status_led_area_end = {screen_pos.x + 300 * zoom,
                                screen_pos.y + 165 * zoom};
  draw_list->AddRectFilled(status_led_area_start, status_led_area_end,
                           IM_COL32(44, 62, 80, 255), 3.0f * zoom);

  bool is_running = comp.internalStates.count(kRunningKey) &&
                    comp.internalStates.at(kRunningKey) > 0.5f;
  ImVec2 led_pos = {screen_pos.x + 275 * zoom, screen_pos.y + 155 * zoom};
  draw_list->AddCircleFilled(
      led_pos, 5 * zoom,
      is_running ? IM_COL32(0, 255, 0, 255) : IM_COL32(100, 100, 100, 255));

  if (zoom > 0.5f) {
    draw_list->AddText(
        ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 5 * zoom),
        IM_COL32(50, 50, 50, 255),
        TR("component.plc.label", "FX3U-32M"));
  }
}

void InitPlcDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[kRunningKey] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::PLC,
    "component.plc.name",
    "component.plc.desc",
    {320.0f, 180.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderPlc,
    &InitPlcDefaults,
    ComponentCategory::AUTOMATION,
    "icon.plc",
    "PLC",
};

}  // namespace

const ComponentDefinition* GetPlcDefinition() {
  return &kDefinition;
}

}  // namespace plc
