#include "plc_emulator/components/plc_def.h"

#include "imgui.h"

#include "plc_emulator/lang/lang_manager.h"
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

  const char* labels[] = {
      TR("component.plc.indicator_power", "POWER"),
      TR("component.plc.indicator_run", "RUN"),
      TR("component.plc.indicator_batt", "BATT"),
      TR("component.plc.indicator_error", "ERROR")};
  bool run_on = comp.internalStates.count(state_keys::kPlcRunning) &&
                comp.internalStates.at(state_keys::kPlcRunning) > 0.5f;
  bool error_on = comp.internalStates.count(state_keys::kPlcError) &&
                  comp.internalStates.at(state_keys::kPlcError) > 0.5f;
  ImU32 on_green = IM_COL32(0, 200, 0, 255);
  ImU32 on_red = IM_COL32(220, 40, 40, 255);
  ImU32 off_gray = IM_COL32(100, 100, 100, 255);
  ImU32 colors[] = {on_green, run_on ? on_green : off_gray, on_green,
                    error_on ? on_red : off_gray};
  float text_scale = zoom / 1.4f;
  float font_size = 16.0f * text_scale;
  const float max_font_size =
      ImGui::GetFontSize() * std::max(1.0f, zoom * 0.75f);
  if (font_size > max_font_size) {
    font_size = max_font_size;
  }
  for (int i = 0; i < 4; ++i) {
    ImVec2 port_pos = {screen_pos.x + 265 * zoom - 2.0f,
                       screen_pos.y + (40.0f + static_cast<float>(i) * 25.0f) *
                                           zoom};
    draw_list->AddCircleFilled(port_pos, 6 * zoom, colors[i]);
    draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0,
                         2.0f);
    if (zoom > 0.4f) {
      draw_list->AddText(ImGui::GetFont(), font_size,
                         ImVec2(port_pos.x + 10 * zoom, port_pos.y - 8 * zoom),
                         IM_COL32(50, 50, 50, 255), labels[i]);
    }
  }

  ImVec2 status_led_area_start = {screen_pos.x + 250 * zoom,
                                  screen_pos.y + 145 * zoom};
  ImVec2 status_led_area_end = {screen_pos.x + 300 * zoom,
                                screen_pos.y + 165 * zoom};
  draw_list->AddRectFilled(status_led_area_start, status_led_area_end,
                           IM_COL32(44, 62, 80, 255), 3.0f * zoom);
  float panel_padding = 1.0f * zoom;
  ImVec2 led_area_start = {status_led_area_start.x + panel_padding,
                           status_led_area_start.y + panel_padding};
  ImVec2 led_area_end = {status_led_area_end.x - panel_padding,
                         status_led_area_end.y - panel_padding};
  float panel_width = led_area_end.x - led_area_start.x;
  float panel_height = led_area_end.y - led_area_start.y;
  float cell_width = panel_width / 16.0f;
  float cell_height = panel_height / 2.0f;
  ImU32 x_on = IM_COL32(0, 200, 0, 255);
  ImU32 y_on = IM_COL32(255, 165, 0, 255);
  ImU32 off = IM_COL32(60, 60, 60, 255);
  for (int i = 0; i < 16; ++i) {
    float x0 = led_area_start.x + cell_width * i;
    float x1 = led_area_start.x + cell_width * (i + 1);
    float x_row0 = led_area_start.y;
    float x_row1 = led_area_start.y + cell_height;
    float y_row0 = led_area_start.y + cell_height;
    float y_row1 = led_area_start.y + cell_height * 2.0f;
    std::string x_key = std::string(state_keys::kPlcXPrefix) +
                        std::to_string(i);
    std::string y_key = std::string(state_keys::kPlcYPrefix) +
                        std::to_string(i);
    bool x_active =
        comp.internalStates.count(x_key) &&
        comp.internalStates.at(x_key) > 0.5f;
    bool y_active =
        comp.internalStates.count(y_key) &&
        comp.internalStates.at(y_key) > 0.5f;
    draw_list->AddRectFilled({x0, x_row0}, {x1, x_row1},
                             x_active ? x_on : off);
    draw_list->AddRectFilled({x0, y_row0}, {x1, y_row1},
                             y_active ? y_on : off);
  }

  if (zoom > 0.5f) {
    draw_list->AddText(
        ImGui::GetFont(), font_size,
        ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 5 * zoom),
        IM_COL32(50, 50, 50, 255),
        TR("component.plc.label", "FX3U-32M"));
  }
}

void InitPlcDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kPlcRunning] = 0.0f;
  comp->internalStates[state_keys::kPlcError] = 0.0f;
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
