#include "plc_emulator/components/sensor_cylinder_def.h"

#include <algorithm>

#include "plc_emulator/components/part_drawing.h"

#include "imgui.h"

#include "plc_emulator/components/cylinder_def.h"
#include "plc_emulator/components/pneumatic_component.h"

namespace plc_emulator {
namespace {

using plc::ComponentCategory;
using plc::ComponentDefinition;
using plc::ComponentPortDef;
using plc::ComponentType;
using plc::PlacedComponent;
using plc::PortType;

const ComponentPortDef kCylinderPorts[] = {
    {0, {175, 58}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_A"},
    {1, {305, 58}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_B"},
    {2, {242, 19}, PortType::ELECTRIC, true,
     {0.9f, 0.1f, 0.1f, 1}, "POWER_24V"},
    {3, {261, 19}, PortType::ELECTRIC, true,
     {0.05f, 0.05f, 0.05f, 1}, "POWER_0V"},
    {4, {280, 19}, PortType::ELECTRIC, false,
     {0.96f, 0.75f, 0.15f, 1}, "SENSOR_FWD"},
    {5, {299, 19}, PortType::ELECTRIC, false,
     {0.2f, 0.7f, 0.25f, 1}, "SENSOR_REV"},
    {6, {162.5f, 49}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_VACUUM"},
    {7, {322, 19}, PortType::ELECTRIC, false,
     {0.96f, 0.75f, 0.15f, 1}, "VACUUM_CONFIRMED"},
};
const ComponentPortDef kStopperPorts[] = {
    {0, {67, 23.226f}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_A"},
    {1, {67, 32}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_B"},
    {2, {43, 9.806f}, PortType::ELECTRIC, true,
     {0.9f, 0.1f, 0.1f, 1}, "POWER_24V"},
    {3, {49.333f, 9.806f}, PortType::ELECTRIC, true,
     {0.05f, 0.05f, 0.05f, 1}, "POWER_0V"},
    {4, {55.667f, 9.806f}, PortType::ELECTRIC, false,
     {0.96f, 0.75f, 0.15f, 1}, "SENSOR_DOWN"},
    {5, {62, 9.806f}, PortType::ELECTRIC, false,
     {0.2f, 0.7f, 0.25f, 1}, "SENSOR_UP"},
};
const ComponentPortDef kVacuumPorts[] = {
    {0, {8, 40}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_IN_P"},
    {1, {112, 40}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1}, "PNEUMATIC_VACUUM"},
};



void RenderSensorCylinder(ImDrawList* list, const PlacedComponent& component,
                          ImVec2 origin, float zoom) {
  plc::GetCylinderDefinition()->Render(
      list, component, {origin.x, origin.y + 18 * zoom}, zoom);
  const PartDrawing drawing{list, origin, zoom};
  drawing.Rect(232, 10, 78, 18, 220);
  for (int i = 2; i < 6; ++i) {
    drawing.Port(kCylinderPorts[i]);
  }
  drawing.Label(236, 0, "24V");
  drawing.Label(256, 0, "0V");
  drawing.Label(273, 0, "FWD");
  drawing.Label(292, 0, "REV");
  if (component.type == ComponentType::kSuctionCylinder) {
    const ImVec2 head = GetSuctionHeadLocal(component);
    drawing.Rect(head.x - 7.5f, 35, 15, 18, 210);
    drawing.Rect(head.x - 7.5f, 51, 15, 7, 180);
    drawing.Rect(head.x - 7.5f, 58, 15, 3, 40);
    drawing.Circle(head.x, head.y, 4, IM_COL32(33, 150, 243, 255));
    drawing.Label(head.x - 8, 24, "VAC");
    drawing.Port(kCylinderPorts[7]);
    drawing.Label(314, 0, "VAC");
  }
}

void RenderStopper(ImDrawList* list, const PlacedComponent& component,
                   ImVec2 origin, float zoom) {
  const PartDrawing drawing{list, origin, zoom};
  const float width_scale = component.size.height / 155.0f;
  const float length_scale = component.size.width / 240.0f;
  const float detail_scale = std::min(width_scale, length_scale);
  const auto draw_rect = [&](float x, float y, float width, float height,
                             int grey) {
    drawing.Rect(x * length_scale, y * width_scale,
                 width * length_scale, height * width_scale, grey);
  };
  draw_rect(6, 60, 200, 40, 180);
  draw_rect(20, 30, 172, 99, 210);
  draw_rect(20, 30, 172, 11, 170);
  draw_rect(20, 118, 172, 11, 170);
  draw_rect(96, 41, 20, 77, 110);
  for (const float x : {54.0f, 159.0f}) {
    drawing.Circle(x * length_scale, 80 * width_scale, 11 * detail_scale,
                   IM_COL32(170, 170, 170, 255));
    drawing.Circle(x * length_scale, 80 * width_scale, 6 * detail_scale,
                   IM_COL32(110, 110, 110, 255));
  }
  draw_rect(192, 34, 44, 40, 221);
  draw_rect(119, 10, 78, 20, 220);
  for (int i = 0; i < 6; ++i) {
    const auto& port = kStopperPorts[i];
    drawing.Circle(port.rel_pos.x, port.rel_pos.y, 2.5f,
                   ImGui::ColorConvertFloat4ToU32(
                       ImVec4(port.color.r, port.color.g, port.color.b, 1)));
    if (i < 2) drawing.Label(71, port.rel_pos.y - 5, i == 0 ? "A" : "B");
  }
}

void RenderVacuumGenerator(ImDrawList* list,
                           const PlacedComponent& component,
                           ImVec2 origin, float zoom) {
  (void)component;
  const PartDrawing drawing{list, origin, zoom};
  drawing.Rect(8, 12, 104, 50, 210);
  drawing.Rect(30, 18, 60, 12, 80);
  for (const auto& port : kVacuumPorts) {
    drawing.Port(port);
  }
  drawing.Label(4, 65, "P");
  drawing.Label(99, 65, "VAC");
}

void InitializeCylinder(PlacedComponent* component) {
  if (!component) {
    return;
  }
  for (const char* key : {"position", "velocity", "status", "pressure_a",
                         "pressure_b", "is_powered", "vacuum_pressure"}) {
    component->internalStates[key] = 0;
  }
  if (component->type == ComponentType::kStopperCylinder) {
    component->z_config = {1, 2, 2};
    component->z_state.current_height = 2;
  }
}

const ComponentDefinition kSensorDefinition = {
    ComponentType::kSensorCylinder, "component.sensor_cylinder.name",
    "component.sensor_cylinder.desc", {340, 84}, kCylinderPorts, 6,
    RenderSensorCylinder, InitializeCylinder, ComponentCategory::AUTOMATION,
    "icon.cylinder", "Sensor cylinder"};
const ComponentDefinition kSuctionDefinition = {
    ComponentType::kSuctionCylinder, "component.suction_cylinder.name",
    "component.suction_cylinder.desc", {340, 84}, kCylinderPorts, 8,
    RenderSensorCylinder, InitializeCylinder, ComponentCategory::AUTOMATION,
    "icon.cylinder", "Suction cylinder"};
const ComponentDefinition kStopperDefinition = {
    ComponentType::kStopperCylinder, "component.stopper_cylinder.name",
    "component.stopper_cylinder.desc", {80, 80}, kStopperPorts, 6,
    RenderStopper, InitializeCylinder, ComponentCategory::AUTOMATION,
    "icon.cylinder", "Stopper cylinder"};
const ComponentDefinition kVacuumDefinition = {
    ComponentType::kVacuumGenerator, "component.vacuum_generator.name",
    "component.vacuum_generator.desc", {120, 80}, kVacuumPorts, 2,
    RenderVacuumGenerator, nullptr, ComponentCategory::AUTOMATION,
    "icon.frl", "Vacuum generator"};

}  // namespace

const plc::ComponentDefinition* GetSensorCylinderDefinition() {
  return &kSensorDefinition;
}
const plc::ComponentDefinition* GetSuctionCylinderDefinition() {
  return &kSuctionDefinition;
}
const plc::ComponentDefinition* GetStopperCylinderDefinition() {
  return &kStopperDefinition;
}
const plc::ComponentDefinition* GetVacuumGeneratorDefinition() {
  return &kVacuumDefinition;
}

void RenderPneumaticFront(ImDrawList* list,
                         const plc::PlacedComponent& component,
                         ImVec2 origin, float zoom) {
  if (component.type != plc::ComponentType::kStopperCylinder) {
    RenderSensorCylinder(list, component, origin, zoom);
    return;
  }
  const PartDrawing drawing{list, origin, zoom};
  const float travel = std::clamp(ReadPneumaticState(component, "position") /
                                     kCylinderStroke, 0.0f, 1.0f) * 69;
  if (travel > 0.01f) {
    drawing.Rect(46, 123, 12, travel, 160);
    drawing.Rect(153, 123, 12, travel, 160);
  }
  drawing.Rect(20, 20, 172, 111, 210);
  drawing.Rect(25, 27, 29, 97, 170);
  drawing.Rect(158, 27, 29, 97, 170);
  drawing.Rect(95, 27, 22, 97, 100);
  drawing.Rect(119, 0, 78, 20, 220);
  for (int i = 2; i < 6; ++i) {
    drawing.Circle(129 + 19 * (i - 2), 10, 4,
        ImGui::ColorConvertFloat4ToU32(ImVec4(kStopperPorts[i].color.r,
            kStopperPorts[i].color.g, kStopperPorts[i].color.b, 1)));
  }
  drawing.Rect(6, 131 + travel, 200, 25, 180);
  drawing.Rect(6, 151 + travel, 200, 5, 80);
}

}  // namespace plc_emulator
