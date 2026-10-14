#include "plc_emulator/components/storage_sensor_def.h"

#include "plc_emulator/components/part_drawing.h"
#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/physics/z_layer.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/core/render_layer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace plc_emulator {
namespace {
using plc::ComponentType;
using plc::PortType;
constexpr plc::Color kPower{0.9f, 0.1f, 0.1f, 1};
constexpr plc::Color kGround{0.05f, 0.05f, 0.05f, 1};
constexpr plc::Color kSignal{0.96f, 0.75f, 0.15f, 1};
constexpr plc::Color kOptical{0.6f, 0.35f, 0.8f, 1};
const ComponentPortDef kFiberPorts[] = {
    {0, {165, 17}, PortType::ELECTRIC, true, kPower, "POWER_24V"},
    {1, {165, 47}, PortType::ELECTRIC, true, kGround, "POWER_0V"},
    {2, {165, 32}, PortType::ELECTRIC, false, kSignal, "SENSOR_SIGNAL"},
    {3, {9, 22}, PortType::kOptical, false, kOptical, "OPTICAL_TX"},
    {4, {9, 42}, PortType::kOptical, true, kOptical, "OPTICAL_RX"}};
const ComponentPortDef kTxPorts[] = {
    {0, {94, 15}, PortType::kOptical, true, kOptical, "OPTICAL_TX_HEAD"}};
const ComponentPortDef kRxPorts[] = {
    {0, {94, 15}, PortType::kOptical, false, kOptical, "OPTICAL_RX_HEAD"}};
const ComponentPortDef kHmiPorts[] = {
    {0, {52, 245}, PortType::ELECTRIC, true, kPower, "POWER_24V"},
    {1, {77, 245}, PortType::ELECTRIC, true, kGround, "POWER_0V"},
};

void RenderWarehouse(ImDrawList* list, const plc::PlacedComponent& component,
                     ImVec2 origin, float zoom) {
  (void)component;
  const PartDrawing draw{list, origin, zoom};
  draw.Rect(0, 0, 160, 100, 210);
  draw.Rect(10, 10, 70, 80, 245);
  draw.Rect(80, 10, 70, 80, 245);
  draw.Rect(0, 90, 160, 10, 170);
}
const ComponentPortDef kDrivenWarehousePorts[] = {
    {0, {109, 64}, PortType::PNEUMATIC, true, {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_A"},
    {1, {21, 64}, PortType::PNEUMATIC, true, {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_B"},
    {2, {20, 19}, PortType::ELECTRIC, true, kPower, "POWER_24V"},
    {3, {40, 19}, PortType::ELECTRIC, true, kGround, "POWER_0V"},
    {4, {60, 19}, PortType::ELECTRIC, false, kSignal, "SENSOR_FWD"},
    {5, {80, 19}, PortType::ELECTRIC, false, kSignal, "SENSOR_REV"},
};
void RenderDrivenWarehouse(ImDrawList* list,
                           const plc::PlacedComponent& component,
                           ImVec2 origin, float zoom) {
  const PartDrawing draw{list, origin, zoom};
  const float displacement = std::clamp(
      ReadPneumaticState(component, "position"), 0.0f, kWarehouseDriveStroke);
  // Fixed frame, two guide rails, and a double-acting cylinder.
  draw.Rect(170, 0, 8, 100, 170);
  draw.Rect(422, 0, 8, 100, 170);
  draw.Rect(170, 7, 255, 6, 115);
  draw.Rect(170, 87, 255, 6, 115);
  // Use the sensor cylinder's compact terminal strip, flush with the housing.
  draw.Rect(14, 10, 78, 18, 220);
  draw.Rect(15, 28, 100, 44, 185);
  draw.Rect(15, 28, 12, 44, 130);
  draw.Rect(103, 28, 12, 44, 130);
  // Rod and carriage share exactly one displacement, without a ratio.
  draw.Rect(115, 45, 75 + displacement, 10, 140);
  draw.Rect(180 + displacement, 20, 12, 60, 100);
  draw.Rect(190 + displacement, 0, 160, 100, 210);
  draw.Rect(200 + displacement, 10, 70, 80, 245);
  draw.Rect(270 + displacement, 10, 70, 80, 245);
  draw.Rect(190 + displacement, 90, 160, 10, 170);
  for (const auto& port : kDrivenWarehousePorts) draw.Port(port);
  draw.Label(18, 75, "B");
  draw.Label(106, 75, "A");
  draw.Label(16, 0, "24V");
  draw.Label(36, 0, "0V");
  draw.Label(56, 0, "FWD");
  draw.Label(76, 0, "REV");
  draw.Label(32, 42, "70 mm");
}
void RenderMagazine(ImDrawList* list, const plc::PlacedComponent& component,
                    ImVec2 origin, float zoom) {
  (void)component;
  const PartDrawing draw{list, origin, zoom};
  draw.Rect(0, 0, 100, 12, 175);
  draw.Rect(0, 88, 100, 12, 175);
  draw.Rect(12, 12, 10, 76, 210);
  draw.Rect(78, 12, 10, 76, 210);
  draw.Circle(50, 50, 25, IM_COL32(70, 70, 70, 255));
  draw.Circle(7, 6, 2, IM_COL32(40, 40, 40, 255));
  draw.Circle(93, 6, 2, IM_COL32(40, 40, 40, 255));
  draw.Circle(7, 94, 2, IM_COL32(40, 40, 40, 255));
  draw.Circle(93, 94, 2, IM_COL32(40, 40, 40, 255));
}
void RenderFiber(ImDrawList* list, const plc::PlacedComponent& component,
                 ImVec2 origin, float zoom) {
  const PartDrawing draw{list, origin, zoom};
  draw.Rect(0, 12, 18, 40, 160);
  draw.Rect(18, 0, 138, 64, 85);
  draw.Rect(156, 8, 18, 48, 210);
  draw.Rect(30, 10, 67, 30, 35);
  char value[16];
  std::snprintf(value, sizeof(value), "%04.0f", component.fiber_state.received);
  list->AddText(ImGui::GetFont(), 15 * zoom, draw.Point(36, 17),
                IM_COL32(255, 140, 30, 255), value);
  draw.Circle(111, 20, 3,
              component.fiber_state.fault ? IM_COL32(230, 40, 30, 255)
                                          : IM_COL32(40, 170, 60, 255));
  draw.Rect(125, 12, 16, 12, 160);
  draw.Rect(34, 46, 24, 12, 180);
  draw.Rect(66, 46, 30, 12, 180);
  draw.Label(37, 48, "SET");
  draw.Label(68, 48, "MODE");
  for (const auto& port : kFiberPorts)
    draw.Port(port);
  list->AddText(ImGui::GetFont(), 8 * zoom, draw.Point(20, 18),
                IM_COL32(235, 235, 235, 255), "TX");
  list->AddText(ImGui::GetFont(), 8 * zoom, draw.Point(20, 38),
                IM_COL32(235, 235, 235, 255), "RX");
  draw.Label(175, 13, "24V");
  draw.Label(175, 28, "SIG");
  draw.Label(175, 43, "0V");
}
void RenderHead(ImDrawList* list, const plc::PlacedComponent& component,
                ImVec2 origin, float zoom) {
  const PartDrawing draw{list, origin, zoom};
  draw.Rect(0, 6, 9, 18, 35);
  draw.Rect(9, 0, 73, 30, 195);
  for (float x = 14; x < 80; x += 5)
    draw.Rect(x, 2, 1, 26, 150);
  draw.Rect(82, 4, 10, 22, 120);
  draw.Rect(92, 8, 6, 14, 170);
  draw.Port(component.type == ComponentType::kFiberTransmitter ? kTxPorts[0]
                                                               : kRxPorts[0]);
  draw.Label(34, 10,
             component.type == ComponentType::kFiberTransmitter ? "TX" : "RX");
}
void RenderHmi(ImDrawList* list, const plc::PlacedComponent& component,
               ImVec2 origin, float zoom) {
  const PartDrawing draw{list, origin, zoom};
  draw.Rect(0, 0, 360, 270, 195);
  RenderHmiScreen(list, component.hmi_settings, component.hmi_state,
                  draw.Point(36, 12), zoom);
  draw.Rect(41, 236, 48, 18, 160);
  for (const auto& port : kHmiPorts)
    draw.Port(port);
  draw.Label(41, 257, "24V   0V");
  draw.Label(
      220, 245,
      component.hmi_state.online ? "PLC AUTO / ONLINE" : "PLC AUTO / OFFLINE");
}
void Initialize(plc::PlacedComponent* component) {
  if (!component)
    return;
  if (plc::IsWarehouseType(component->type)) {
    component->z_config = {1, 3, 3};
    if (component->type == ComponentType::kDrivenWarehouse) {
      component->internalStates["position"] = 0;
      component->internalStates["velocity"] = 0;
      component->internalStates["status"] = 0;
    }
    InitializeZLayerState(component);
  }
}
const plc::ComponentDefinition kWarehouse = {ComponentType::kWarehouse,
                                             "component.warehouse.name",
                                             "component.warehouse.desc",
                                             {160, 100},
                                             nullptr,
                                             0,
                                             RenderWarehouse,
                                             Initialize,
                                             plc::ComponentCategory::AUTOMATION,
                                             "icon.box",
                                             "Warehouse"};
const plc::ComponentDefinition kDrivenWarehouse = {
    ComponentType::kDrivenWarehouse, "component.driven_warehouse.name",
    "component.driven_warehouse.desc", {430, 100}, kDrivenWarehousePorts, 6,
    RenderDrivenWarehouse, Initialize, plc::ComponentCategory::AUTOMATION,
    "icon.box", "Pneumatic warehouse"};
const plc::ComponentDefinition kFiber = {ComponentType::kFiberAmplifier,
                                         "component.fiber_amp.name",
                                         "component.fiber_amp.desc",
                                         {205, 64},
                                         kFiberPorts,
                                         5,
                                         RenderFiber,
                                         Initialize,
                                         plc::ComponentCategory::AUTOMATION,
                                         "icon.sensor",
                                         "Fiber amp"};
const plc::ComponentDefinition kTx = {ComponentType::kFiberTransmitter,
                                      "component.fiber_tx.name",
                                      "component.fiber_tx.desc",
                                      {98, 30},
                                      kTxPorts,
                                      1,
                                      RenderHead,
                                      Initialize,
                                      plc::ComponentCategory::AUTOMATION,
                                      "icon.sensor",
                                      "Fiber TX"};
const plc::ComponentDefinition kRx = {ComponentType::kFiberReceiver,
                                      "component.fiber_rx.name",
                                      "component.fiber_rx.desc",
                                      {98, 30},
                                      kRxPorts,
                                      1,
                                      RenderHead,
                                      Initialize,
                                      plc::ComponentCategory::AUTOMATION,
                                      "icon.sensor",
                                      "Fiber RX"};
const plc::ComponentDefinition kHmi = {ComponentType::kTouchPanel,
                                       "component.touch_panel.name",
                                       "component.touch_panel.desc",
                                       {360, 270},
                                       kHmiPorts,
                                       2,
                                       RenderHmi,
                                       Initialize,
                                       plc::ComponentCategory::AUTOMATION,
                                       "icon.plc",
                                       "Touch panel"};
}  // namespace
const plc::ComponentDefinition* GetMagazineDefinition() {
  static const plc::ComponentDefinition kDefinition{
      ComponentType::kMagazine, "component.magazine.name",
      "component.magazine.desc", {100, 100}, nullptr, 0, RenderMagazine,
      nullptr, plc::ComponentCategory::AUTOMATION, "icon.box", "Magazine"};
  return &kDefinition;
}
const plc::ComponentDefinition* GetDrivenWarehouseDefinition() {
  return &kDrivenWarehouse;
}
const plc::ComponentDefinition* GetWarehouseDefinition() {
  return &kWarehouse;
}
const plc::ComponentDefinition* GetFiberAmplifierDefinition() {
  return &kFiber;
}
const plc::ComponentDefinition* GetFiberTransmitterDefinition() {
  return &kTx;
}
const plc::ComponentDefinition* GetFiberReceiverDefinition() {
  return &kRx;
}
const plc::ComponentDefinition* GetTouchPanelDefinition() {
  return &kHmi;
}

void RenderHmiScreen(ImDrawList* list, const plc::HmiSettings& settings,
                     const plc::HmiState& state, ImVec2 origin, float zoom,
                     bool show_grid) {
  if (!list || zoom <= 0)
    return;
  const PartDrawing screen{list, origin, zoom};
  screen.Rect(0, 0, plc::kHmiScreenWidth, plc::kHmiScreenHeight, 245);
  list->PushClipRect(
      origin, screen.Point(plc::kHmiScreenWidth, plc::kHmiScreenHeight), true);
  if (show_grid) {
    for (float x = 5; x < plc::kHmiScreenWidth; x += 5)
      list->AddLine(screen.Point(x, 0), screen.Point(x, plc::kHmiScreenHeight),
                    IM_COL32(150, 170, 190, 55));
    for (float y = 5; y < plc::kHmiScreenHeight; y += 5)
      list->AddLine(screen.Point(0, y), screen.Point(plc::kHmiScreenWidth, y),
                    IM_COL32(150, 170, 190, 55));
  }
  for (size_t i = 0; i < settings.widgets.size() && i < plc::kMaxHmiWidgets;
       ++i) {
    const auto& widget = settings.widgets[i];
    if (widget.page != state.active_page || state.hidden[i])
      continue;
    const float x = widget.position.x;
    const float y = widget.position.y;
    const ImVec2 start = screen.Point(x, y);
    const ImVec2 end =
        screen.Point(x + widget.size.width, y + widget.size.height);
    list->PushClipRect(start, end, true);
    char number[96];
    const char* text = widget.text.c_str();
    ImU32 color = IM_COL32(40, 40, 40, 255);
    float text_x = x + 3;
    if (widget.type == plc::HmiWidgetType::kButton) {
      screen.Rect(x, y, widget.size.width, widget.size.height,
                  state.online && state.values[i] ? 170 : 230);
    } else if (widget.type == plc::HmiWidgetType::kLamp) {
      const float radius = std::min(widget.size.width, widget.size.height) / 3;
      screen.Circle(x + radius + 2, y + widget.size.height / 2, radius,
                    state.online && state.values[i]
                        ? widget.on_color
                        : IM_COL32(110, 110, 110, 255));
      text_x = x + radius * 2 + 5;
    } else if (widget.type == plc::HmiWidgetType::kNumber) {
      screen.Rect(x, y, widget.size.width, widget.size.height, 40);
      if (state.number_valid[i])
        std::snprintf(number, sizeof(number), "%.*f %s", widget.decimals,
                      state.numbers[i] / std::pow(10.0, widget.decimals),
                      widget.unit.c_str());
      else
        std::snprintf(number, sizeof(number), "--- %s", widget.unit.c_str());
      color = IM_COL32(220, 235, 235, 255);
      if (!widget.text.empty()) {
        list->AddText(ImGui::GetFont(), widget.font_size * zoom,
                      screen.Point(text_x, y + 2), color, text);
      }
      text = number;
    }
    if (widget.type == plc::HmiWidgetType::kMessage)
      text =
          state.number_valid[i] && state.numbers[i] >= 0 && state.numbers[i] < 3
              ? widget.messages[state.numbers[i]].c_str()
              : "---";
    const float text_y =
        y + (widget.type == plc::HmiWidgetType::kNumber && !widget.text.empty()
                 ? widget.font_size + 3
                 : std::max(2.0f, (widget.size.height - widget.font_size) / 2));
    list->AddText(ImGui::GetFont(), widget.font_size * zoom,
                  screen.Point(text_x, text_y), color, text);
    list->PopClipRect();
  }
  list->PopClipRect();
}

ImVec2 WarehouseOverlayOrigin(const plc::PlacedComponent& warehouse,
                              float camera_zoom) {
  return {warehouse.position.x + plc::GetComponentDisplaySize(warehouse).width +
              8.0f / camera_zoom, warehouse.position.y};
}

int FindWarehouseOverlayWorkpiece(
    const std::vector<plc::PlacedComponent>& parts, ImVec2 world_position,
    float camera_zoom) {
  if (camera_zoom <= 0) return -1;
  for (auto warehouse = parts.rbegin(); warehouse != parts.rend(); ++warehouse) {
    if (!plc::IsWarehouseType(warehouse->type) ||
        !IsRenderVisible(*warehouse)) continue;
    const ImVec2 origin = WarehouseOverlayOrigin(*warehouse, camera_zoom);
    const ImVec2 local((world_position.x - origin.x) / kWarehouseOverlayScale,
                       (world_position.y - origin.y) / kWarehouseOverlayScale);
    if (local.x < 0 || local.x > 160 || local.y < 0 || local.y > 220) continue;
    for (size_t index = 0; index < parts.size(); ++index) {
      const auto& piece = parts[index];
      const int slot = piece.carry_state.slot_id;
      if (!IsRenderVisible(piece) ||
          !plc::IsWorkpiece(piece.type) || slot < 1 || slot > 6 ||
          piece.carry_state.owner_id != warehouse->instanceId) continue;
      const float x = 45.0f + ((slot - 1) % 2) * 70.0f;
      const float y = 38.0f + (2 - (slot - 1) / 2) * 65.0f;
      if (std::hypot(local.x - x, local.y - y) <= 18.0f)
        return static_cast<int>(index);
    }
  }
  return -1;
}

void RenderStorageSensorFront(ImDrawList* list,
                              const plc::PlacedComponent& component,
                              ImVec2 origin, float zoom,
                              const std::vector<plc::PlacedComponent>& parts) {
  if (!plc::IsWarehouseType(component.type)) {
    const auto* definition =
        component.type == ComponentType::kTouchPanel         ? &kHmi
        : component.type == ComponentType::kFiberAmplifier   ? &kFiber
        : component.type == ComponentType::kFiberTransmitter ? &kTx
                                                             : &kRx;
    definition->Render(list, component, origin, zoom);
    return;
  }
  const PartDrawing draw{list, origin, zoom};
  draw.Rect(0, 0, 160, 220, 210);
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 2; ++column) {
      const float x = 10 + column * 70.0f;
      const float y = 10 + (2 - row) * 65.0f;
      draw.Rect(x, y, 70, 55, 245);
      for (const auto& piece : parts) {
        if (IsRenderVisible(piece) &&
            (piece.type == ComponentType::WORKPIECE_METAL ||
             piece.type == ComponentType::WORKPIECE_NONMETAL) &&
            piece.carry_state.owner_id == component.instanceId &&
            piece.carry_state.slot_id == row * 2 + column + 1) {
          draw.Circle(x + 35, y + 28, 18,
                      piece.type == ComponentType::WORKPIECE_METAL
                          ? IM_COL32(160, 160, 170,
                              static_cast<int>(255 * piece.render_opacity))
                          : IM_COL32(230, 230, 230,
                              static_cast<int>(255 * piece.render_opacity)));
        }
      }
    }
  }
}
}  // namespace plc_emulator
