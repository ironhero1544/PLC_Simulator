#include "plc_emulator/components/servo_def.h"

#include "plc_emulator/components/part_drawing.h"
#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/components/servo_component.h"
#include "plc_emulator/physics/z_layer.h"

#include <array>
#include <cmath>

namespace plc_emulator {
namespace {
using plc::ComponentCategory;
using plc::ComponentDefinition;
using plc::ComponentType;
using plc::PlacedComponent;
using plc::PortType;

constexpr plc::Color kPower{0.9f, 0.1f, 0.1f, 1};
constexpr plc::Color kGround{0.05f, 0.05f, 0.05f, 1};
constexpr plc::Color kDrive{0.95f, 0.5f, 0.1f, 1};
constexpr plc::Color kEarth{0.2f, 0.7f, 0.25f, 1};
constexpr plc::Color kAir{0.13f, 0.59f, 0.95f, 1};
constexpr plc::Color kSignal{0.96f, 0.75f, 0.15f, 1};
const ComponentPortDef kDcPorts[] = {
    {0, {65, 18}, PortType::ELECTRIC, true, kPower, "DC_MOTOR_PLUS"},
    {1, {90, 18}, PortType::ELECTRIC, true, kGround, "DC_MOTOR_MINUS"}};
const ComponentPortDef kDcDriverPorts[] = {
    {0, {30, 16}, PortType::ELECTRIC, true, kPower, "POWER_24V"},
    {1, {58, 16}, PortType::ELECTRIC, true, kGround, "POWER_0V"},
    {2, {86, 16}, PortType::ELECTRIC, true, kSignal, "DC_ENABLE"},
    {3, {114, 16}, PortType::ELECTRIC, true, kSignal, "DC_DIRECTION"},
    {4, {142, 16}, PortType::ELECTRIC, true, kSignal, "DC_BRAKE"},
    {5, {170, 16}, PortType::ELECTRIC, true, kSignal, "DC_PWM"},
    {7, {242, 16}, PortType::ELECTRIC, false, kDrive, "DC_DRIVE_PLUS"},
    {8, {270, 16}, PortType::ELECTRIC, false, kDrive, "DC_DRIVE_MINUS"}};
const ComponentPortDef kMotorPorts[] = {
    {0, {46, 18}, PortType::ELECTRIC, true, kDrive, "SERVO_U"},
    {1, {62, 18}, PortType::ELECTRIC, true, kDrive, "SERVO_V"},
    {2, {78, 18}, PortType::ELECTRIC, true, kDrive, "SERVO_W"},
    {3, {94, 18}, PortType::ELECTRIC, true, kEarth, "PROTECTIVE_EARTH"},
    {4, {126, 18}, PortType::ELECTRIC, true, kEarth, "ENCODER"}};
const ComponentPortDef kLiftPorts[] = {
    {0, {49, 12}, PortType::ELECTRIC, true, kDrive, "SERVO_U"},
    {1, {65, 12}, PortType::ELECTRIC, true, kDrive, "SERVO_V"},
    {2, {81, 12}, PortType::ELECTRIC, true, kDrive, "SERVO_W"},
    {3, {97, 12}, PortType::ELECTRIC, true, kEarth, "PROTECTIVE_EARTH"},
    {4, {115, 12}, PortType::ELECTRIC, true, kEarth, "ENCODER"}};
const ComponentPortDef kIntegratedPorts[] = {
    {0, {43, 169}, PortType::PNEUMATIC, true, kAir, "PNEUMATIC_IN_A"},
    {1, {103, 169}, PortType::PNEUMATIC, true, kAir, "PNEUMATIC_IN_B"},
    {2, {60, 137}, PortType::ELECTRIC, true, kPower, "POWER_24V"},
    {3, {77, 137}, PortType::ELECTRIC, true, kGround, "POWER_0V"},
    {4, {94, 137}, PortType::ELECTRIC, false, kSignal, "SENSOR_FWD"},
    {5, {111, 137}, PortType::ELECTRIC, false, kEarth, "SENSOR_REV"},
    {6, {119, 161}, PortType::PNEUMATIC, true, kAir, "PNEUMATIC_VACUUM"},
    {7, {128, 137}, PortType::ELECTRIC, false, kSignal, "VACUUM_CONFIRMED"},
    {10, {49, 12}, PortType::ELECTRIC, true, kDrive, "SERVO_U"},
    {11, {65, 12}, PortType::ELECTRIC, true, kDrive, "SERVO_V"},
    {12, {81, 12}, PortType::ELECTRIC, true, kDrive, "SERVO_W"},
    {13, {97, 12}, PortType::ELECTRIC, true, kEarth, "PROTECTIVE_EARTH"},
    {14, {115, 12}, PortType::ELECTRIC, true, kEarth, "ENCODER"}};
// Match the PLC body's 180-unit height while preserving the amplifier shape.
constexpr float kAmplifierHeight = 180;
constexpr float kAmplifierScale = kAmplifierHeight / 354;
const ComponentPortDef kAmpDesignPorts[] = {
    {0, {64, 99}, PortType::ELECTRIC, true, kPower, "AC_L1"},
    {1, {64, 115}, PortType::ELECTRIC, true, kPower, "AC_L2"},
    {2, {64, 131}, PortType::ELECTRIC, true, kPower, "AC_L3"},
    {3, {64, 147}, PortType::ELECTRIC, true, kEarth, "PROTECTIVE_EARTH"},
    {4, {64, 180}, PortType::ELECTRIC, true, kPower, "POWER_24V"},
    {5, {64, 196}, PortType::ELECTRIC, true, kGround, "POWER_0V"},
    {6, {130, 100}, PortType::ELECTRIC, true, kSignal, "PULSE"},
    {7, {130, 116}, PortType::ELECTRIC, true, kGround, "PULSE_COMMON"},
    {8, {130, 132}, PortType::ELECTRIC, true, kSignal, "DIRECTION"},
    {9, {130, 148}, PortType::ELECTRIC, true, kSignal, "SERVO_ON"},
    {10, {130, 164}, PortType::ELECTRIC, true, kSignal, "ALARM_RESET"},
    {11, {130, 186}, PortType::ELECTRIC, false, kSignal, "READY"},
    {12, {130, 202}, PortType::ELECTRIC, false, kSignal, "IN_POSITION"},
    {13, {130, 218}, PortType::ELECTRIC, false, kPower, "ALARM"},
    {14, {64, 250}, PortType::ELECTRIC, false, kDrive, "SERVO_U"},
    {15, {64, 266}, PortType::ELECTRIC, false, kDrive, "SERVO_V"},
    {16, {64, 282}, PortType::ELECTRIC, false, kDrive, "SERVO_W"},
    {17, {130, 260}, PortType::ELECTRIC, false, kEarth, "ENCODER"}};
const auto kAmpPorts = [] {
  auto ports = std::to_array(kAmpDesignPorts);
  for (auto& port : ports) {
    port.rel_pos.x = (port.rel_pos.x - 40) * kAmplifierScale;
    port.rel_pos.y = (port.rel_pos.y - 8) * kAmplifierScale;
  }
  return ports;
}();
const ComponentPortDef kAcPorts[] = {
    {0, {25, 65}, PortType::ELECTRIC, false, kPower, "AC_L1"},
    {1, {50, 65}, PortType::ELECTRIC, false, kPower, "AC_L2"},
    {2, {75, 65}, PortType::ELECTRIC, false, kPower, "AC_L3"},
    {3, {100, 65}, PortType::ELECTRIC, false, kEarth, "PROTECTIVE_EARTH"}};

void Shaft(const PartDrawing& drawing, const PlacedComponent& component,
           float x, float y) {
  drawing.Circle(x, y, 26, IM_COL32(160, 160, 160, 255));
  drawing.Circle(x, y, 13, IM_COL32(210, 210, 210, 255));
  const double angle = component.axis_state.position * 6.283185307;
  drawing.list->AddLine(
      drawing.Point(x, y),
      drawing.Point(x + static_cast<float>(std::sin(angle)) * 12,
                    y - static_cast<float>(std::cos(angle)) * 12),
      IM_COL32(40, 40, 40, 255), 5 * drawing.zoom);
}

void RenderMotor(ImDrawList* list, const PlacedComponent& component,
                 ImVec2 origin, float zoom) {
  const PartDrawing drawing{list, origin, zoom};
  const bool servo = component.type == ComponentType::kServoMotor;
  if (servo) {
    drawing.Rect(10, 28, 140, 140, 195);
    for (const float x : {20.0f, 140.0f}) {
      for (const float y : {38.0f, 158.0f}) {
        drawing.Circle(x, y, 4, IM_COL32(245, 245, 245, 255));
      }
    }
  } else {
    drawing.Rect(42, 156, 76, 16, 170);
    drawing.Circle(80, 96, 70, IM_COL32(190, 190, 190, 255));
  }
  drawing.Circle(80, 96, 56, IM_COL32(220, 220, 220, 255));
  Shaft(drawing, component, 80, 96);
  drawing.Rect(servo ? 38 : 54, 8, servo ? 65 : 48, 20, 75);
  if (servo) {
    drawing.Rect(114, 8, 24, 20, 75);
  }
  const auto* ports = servo ? kMotorPorts : kDcPorts;
  const char* labels[] = {"U", "V", "W", "PE", "ENC"};
  for (int i = 0; i < (servo ? 5 : 2); ++i) {
    drawing.Port(ports[i]);
    drawing.Label(ports[i].rel_pos.x - 5, 0,
                  servo ? labels[i] : (i ? "-" : "+"));
  }
}

void LiftBody(const PartDrawing& drawing, const PlacedComponent& component) {
  constexpr double kRotationDisplayScale = 1.0 / 30.0;
  drawing.Rect(40, 70, 84, 65, 220);
  drawing.Rect(52, 20, 60, 60, 38);
  drawing.Circle(82, 50, 22, IM_COL32(120, 120, 120, 255));
  drawing.Circle(82, 50, 12, IM_COL32(170, 170, 170, 255));
  const double angle =
      component.axis_state.position * kRotationDisplayScale * 6.283185307;
  drawing.list->AddLine(
      drawing.Point(82, 50),
      drawing.Point(82 + static_cast<float>(std::sin(angle)) * 10,
                    50 - static_cast<float>(std::cos(angle)) * 10),
      IM_COL32(40, 40, 40, 255), 2 * drawing.zoom);
  drawing.Rect(46, 83, 16, 18, 120);
  drawing.Rect(102, 83, 16, 18, 120);
  drawing.Circle(82, 93, 8, IM_COL32(155, 155, 155, 255));
  drawing.Rect(37, 105, 90, 36, 48);
  drawing.Rect(41, 4, 84, 16, 75);
}

void DrawIntegratedCylinder(const PartDrawing& drawing,
                            const PlacedComponent& component, float y,
                            bool top_view) {
  drawing.Rect(37, y, 72, 31, 210);
  drawing.Rect(37, y, 9, 31, 180);
  drawing.Rect(100, y, 9, 31, 180);
  const float stroke =
      std::clamp(ReadPneumaticState(component, "position"), 0.0f, 160.0f);
  drawing.list->AddLine(drawing.Point(45, y + 15),
                        drawing.Point(119 + stroke, y + 15),
                        IM_COL32(120, 120, 120, 255), 4 * drawing.zoom);
  if (top_view) {
    drawing.Circle(119 + stroke, y + 15, 8, IM_COL32(180, 180, 180, 255));
    drawing.Circle(119 + stroke, y + 15, 4, IM_COL32(33, 150, 243, 255));
  } else {
    drawing.Rect(111.5f + stroke, y + 10, 15, 18, 210);
    drawing.Rect(111.5f + stroke, y + 26, 15, 7, 180);
    drawing.Rect(111.5f + stroke, y + 33, 15, 3, 40);
  }
  drawing.Rect(52, y - 18, 85, 18, 220);
  ComponentPortDef vacuum_sensor = kIntegratedPorts[7];
  vacuum_sensor.rel_pos.y += y - 146;
  drawing.Port(vacuum_sensor);
  if (!top_view) {
    for (int i = 0; i < 6; ++i) {
      ComponentPortDef port = kIntegratedPorts[i];
      port.rel_pos.y += y - 146;
      drawing.Port(port);
    }
  }
}

void RenderLift(ImDrawList* list, const PlacedComponent& component,
                ImVec2 origin, float zoom) {
  const PartDrawing drawing{list, origin, zoom};
  LiftBody(drawing, component);
  if (component.type == ComponentType::kLiftSuction) {
    DrawIntegratedCylinder(drawing, component, 146, true);
    for (int i = 0; i < 6; ++i) {
      drawing.Port(kIntegratedPorts[i]);
    }
  }
  const char* labels[] = {"U", "V", "W", "PE", "ENC"};
  for (int i = 0; i < 5; ++i) {
    drawing.Port(kLiftPorts[i]);
    drawing.Label(kLiftPorts[i].rel_pos.x - 4, 0, labels[i]);
  }
}

void RenderAmp(ImDrawList* list, const PlacedComponent& component,
               ImVec2 origin, float zoom) {
  zoom *= kAmplifierScale;
  const PartDrawing drawing{
      list, {origin.x - 40 * zoom, origin.y - 8 * zoom}, zoom};
  drawing.Rect(40, 8, 140, 354, 67);
  drawing.Rect(180, 8, 12, 354, 120);
  for (float x = 182; x < 192; x += 3) {
    drawing.Rect(x, 9, 1, 350, 90);
  }
  drawing.Rect(55, 24, 100, 40, 40);
  drawing.Circle(165, 40, 4,
                 component.axis_state.alarm ? IM_COL32(220, 30, 30, 255)
                                            : IM_COL32(50, 160, 60, 255));
  for (const auto& port : kAmpDesignPorts) {
    drawing.Rect(port.rel_pos.x - 9, port.rel_pos.y - 7, 18, 14, 20);
    drawing.Port(port);
  }
  const char* labels[] = {"L1",    "L2",    "L3",  "PE",  "24V",   "0V",
                          "PULSE", "P-COM", "DIR", "SON", "RESET", "READY",
                          "INP",   "ALARM", "U",   "V",   "W",     "ENC"};
  for (int i = 0; i < 18; ++i) {
    list->AddText(ImGui::GetFont(), 8 * zoom,
                  drawing.Point(kAmpDesignPorts[i].rel_pos.x + 11,
                                kAmpDesignPorts[i].rel_pos.y - 4),
                  IM_COL32(235, 235, 235, 255), labels[i]);
  }
  list->AddText(ImGui::GetFont(), ImGui::GetFontSize() * zoom,
                drawing.Point(54, 30), IM_COL32(235, 235, 235, 255), "SERVO");
}

void RenderAc(ImDrawList* list, const PlacedComponent& component, ImVec2 origin,
              float zoom) {
  (void)component;
  const PartDrawing drawing{list, origin, zoom};
  drawing.Rect(10, 10, 105, 68, 210);
  drawing.Rect(20, 20, 85, 22, 75);
  list->AddText(ImGui::GetFont(), 8 * zoom, drawing.Point(25, 25),
                IM_COL32(235, 235, 235, 255), "AC 3-phase");
  const char* labels[] = {"L1", "L2", "L3", "PE"};
  for (int i = 0; i < 4; ++i) {
    drawing.Port(kAcPorts[i]);
    drawing.Label(kAcPorts[i].rel_pos.x - 5, 49, labels[i]);
  }
}

void Initialize(PlacedComponent* component) {
  if (!component) {
    return;
  }
  if (IsServoLift(component->type)) {
    component->z_config = {1, 3, 3};
    InitializeZLayerState(component);
  }
  if (component->type == ComponentType::kLiftSuction) {
    for (const char* key : {"position", "velocity", "status", "pressure_a",
                            "pressure_b", "is_powered", "vacuum_pressure"}) {
      component->internalStates[key] = 0;
    }
  }
}

void RenderDcDriver(ImDrawList* list, const PlacedComponent& component,
                    ImVec2 origin, float zoom) {
  const PartDrawing drawing{list, origin, zoom};
  drawing.Rect(0, 25, 300, 145, 45);
  drawing.Rect(8, 25, 284, 140, 65);
  for (int x = 10; x < 290; x += 8)
    drawing.Rect(static_cast<float>(x), 154, 3, 16, 90);
  drawing.Rect(16, 4, 168, 24, 155);
  drawing.Rect(228, 4, 56, 24, 155);
  const char* labels[] = {"24V", "0V", "EN", "DIR", "BRK", "PWM", "M+", "M-"};
  for (int i = 0; i < 8; ++i) {
    drawing.Port(kDcDriverPorts[i]);
    list->AddText(ImGui::GetFont(), 8 * zoom,
                  drawing.Point(kDcDriverPorts[i].rel_pos.x - 9, 32),
                  IM_COL32(230, 230, 230, 255), labels[i]);
  }
  list->AddText(ImGui::GetFont(), 16 * zoom, drawing.Point(30, 62),
                IM_COL32(235, 235, 235, 255), "DC MOTOR DRIVER");
  list->AddText(ImGui::GetFont(), 10 * zoom, drawing.Point(30, 88),
                IM_COL32(210, 210, 210, 255), "24V / PWM + DIR / 2-WIRE");
  drawing.Rect(30, 110, 162, 30, 85);
  list->AddText(ImGui::GetFont(), 9 * zoom, drawing.Point(37, 120),
                IM_COL32(235, 235, 235, 255), "PWM: DUTY    DIR: F/R");
  drawing.Circle(250, 112, 5,
                  component.axis_state.alarm ? IM_COL32(210, 40, 40, 255)
                  : component.axis_state.ready ? IM_COL32(50, 190, 65, 255)
                                               : IM_COL32(100, 100, 100, 255));
}

const ComponentDefinition kDcDriverDefinition = {
    ComponentType::kDcMotorDriver, "component.dc_driver.name",
    "component.dc_driver.desc", {300, 170}, kDcDriverPorts, 8,
    RenderDcDriver, nullptr, ComponentCategory::AUTOMATION,
    "icon.power_supply", "DC driver"};
const ComponentDefinition kDcDefinition = {ComponentType::kDcMotor,
                                           "component.dc_motor.name",
                                           "component.dc_motor.desc",
                                           {160, 180},
                                           kDcPorts,
                                           2,
                                           RenderMotor,
                                           Initialize,
                                           ComponentCategory::AUTOMATION,
                                           "icon.cylinder",
                                           "DC motor"};
const ComponentDefinition kMotorDefinition = {ComponentType::kServoMotor,
                                              "component.servo_motor.name",
                                              "component.servo_motor.desc",
                                              {160, 180},
                                              kMotorPorts,
                                              5,
                                              RenderMotor,
                                              Initialize,
                                              ComponentCategory::AUTOMATION,
                                              "icon.cylinder",
                                              "Servo motor"};
const ComponentDefinition kAmpDefinition = {
    ComponentType::kServoAmplifier,
    "component.servo_amp.name",
    "component.servo_amp.desc",
    {152 * kAmplifierScale, kAmplifierHeight},
    kAmpPorts.data(),
    18,
    RenderAmp,
    Initialize,
    ComponentCategory::AUTOMATION,
    "icon.cylinder",
    "Servo amplifier"};
const ComponentDefinition kLiftDefinition = {ComponentType::kServoLift,
                                             "component.servo_lift.name",
                                             "component.servo_lift.desc",
                                             {160, 150},
                                             kLiftPorts,
                                             5,
                                             RenderLift,
                                             Initialize,
                                             ComponentCategory::AUTOMATION,
                                             "icon.cylinder",
                                             "Servo lift"};
const ComponentDefinition kIntegratedDefinition = {
    ComponentType::kLiftSuction,
    "component.lift_suction.name",
    "component.lift_suction.desc",
    {340, 200},
    kIntegratedPorts,
    static_cast<int>(std::size(kIntegratedPorts)),
    RenderLift,
    Initialize,
    ComponentCategory::AUTOMATION,
    "icon.cylinder",
    "Lift suction"};
const ComponentDefinition kAcDefinition = {ComponentType::kAcSupply,
                                           "component.ac_supply.name",
                                           "component.ac_supply.desc",
                                           {125, 85},
                                           kAcPorts,
                                           4,
                                           RenderAc,
                                           Initialize,
                                           ComponentCategory::AUTOMATION,
                                           "icon.power_supply",
                                           "AC supply"};
}  // namespace

const plc::ComponentDefinition* GetDcMotorDriverDefinition() {
  return &kDcDriverDefinition;
}
const plc::ComponentDefinition* GetDcMotorDefinition() {
  return &kDcDefinition;
}
const plc::ComponentDefinition* GetServoMotorDefinition() {
  return &kMotorDefinition;
}
const plc::ComponentDefinition* GetServoAmplifierDefinition() {
  return &kAmpDefinition;
}
const plc::ComponentDefinition* GetServoLiftDefinition() {
  return &kLiftDefinition;
}
const plc::ComponentDefinition* GetLiftSuctionDefinition() {
  return &kIntegratedDefinition;
}
const plc::ComponentDefinition* GetAcSupplyDefinition() {
  return &kAcDefinition;
}

void RenderServoFront(ImDrawList* list, const PlacedComponent& component,
                      ImVec2 origin, float zoom) {
  if (!IsServoLift(component.type)) {
    RenderMotor(list, component, origin, zoom);
    return;
  }
  const PartDrawing drawing{list, origin, zoom};
  drawing.Rect(40, 60, 84, 200, 220);
  drawing.Rect(46, 64, 8, 190, 160);
  drawing.Rect(110, 64, 8, 190, 160);
  drawing.Rect(78, 64, 8, 190, 150);
  for (float y = 65; y < 254; y += 7) {
    list->AddLine(drawing.Point(78, y + 5), drawing.Point(86, y),
                  IM_COL32(70, 70, 70, 255), zoom);
  }
  drawing.Rect(52, 10, 60, 50, 40);
  drawing.Rect(41, 4, 84, 16, 75);
  const char* labels[] = {"U", "V", "W", "PE", "ENC"};
  for (int i = 0; i < 5; ++i) {
    drawing.Port(kLiftPorts[i]);
    drawing.Label(kLiftPorts[i].rel_pos.x - 4, 0, labels[i]);
  }
  const float span =
      component.z_config.max_layer - component.z_config.min_layer;
  const float fraction =
      span > 0
          ? (ResolveContactHeight(component) - component.z_config.min_layer) /
                span
          : 0;
  const float carriage_y = 65 + (1 - fraction) * 140;
  drawing.Rect(37, carriage_y, 90, 45, 48);
  if (component.type == ComponentType::kLiftSuction) {
    DrawIntegratedCylinder(drawing, component, carriage_y + 25, false);
  }
}
}  // namespace plc_emulator
