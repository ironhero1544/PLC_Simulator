#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/physics/ring_feed_simulation.h"
#include "plc_emulator/physics/servo_simulation.h"
#include "plc_emulator/physics/pneumatic_workpiece.h"
#include "plc_emulator/physics/pneumatic_simulation.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "plc_emulator/application_physics/physics_helpers.h"
#include "plc_emulator/components/component_input_resolver.h"
#include "plc_emulator/physics/box2d_simulation.h"
#include "plc_emulator/physics/z_layer.h"
#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/project/instruction_codec.h"

namespace {

void Check(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

plc::PlacedComponent Component(int id, plc::ComponentType type) {
  plc::PlacedComponent component{};
  component.instanceId = id;
  component.type = type;
  return component;
}

plc::Wire Hose(int from, int from_port, int to, int to_port) {
  plc::Wire wire{};
  wire.fromComponentId = from;
  wire.fromPortId = from_port;
  wire.toComponentId = to;
  wire.toPortId = to_port;
  wire.kind = plc::WireKind::kPneumatic;
  return wire;
}

struct CylinderFixture {
  std::vector<plc::PlacedComponent> components{
      Component(0, plc::ComponentType::FRL),
      Component(1, plc::ComponentType::VALVE_SINGLE),
      Component(2, plc::ComponentType::CYLINDER)};
  std::vector<plc::Wire> hoses{
      Hose(0, 0, 1, 2), Hose(1, 3, 2, 0), Hose(1, 4, 2, 1)};
  std::map<plc::PortRef, float> pressures;
  std::map<plc::PortRef, float> exhaust;
  std::map<plc::PortRef, bool> supply;
  std::map<plc::PortRef, bool> connected;
  bool powered = true;
  const plc::ComponentPhysicsAdapter* valve =
      plc::GetComponentPhysicsAdapter(plc::ComponentType::VALVE_SINGLE);
  const plc::ComponentPhysicsAdapter* cylinder =
      plc::GetComponentPhysicsAdapter(plc::ComponentType::CYLINDER);

  float position() const {
    const auto it = components[2].internalStates.find(plc::state_keys::kPosition);
    return it == components[2].internalStates.end() ? 0.0f : it->second;
  }

  void Step(bool output) {
    const std::map<plc::PortRef, float> voltages{
        {{1, 0}, powered && output ? 24.0f : 0.0f}, {{1, 1}, 0.0f}};
    valve->UpdateElectrical(&components[1], voltages);
    plc::SimulatePneumaticNetwork(hoses, components, &pressures, &supply,
                                 &exhaust, &connected);
    cylinder->UpdateCylinder(&components[2], pressures, exhaust, connected,
                             1.0f / 240.0f, 0.5f, 0.02f, 2.5f, 15.0f);
    Check(std::isfinite(position()) && position() >= 0.0f &&
              position() <= 160.0f,
          "Cylinder stroke must remain finite and within 0..160");
  }
};

void ClosedLoopCylinder() {
  namespace model = plc_emulator::programming;
  model::ExecutionProgram program;
  std::string error;
  Check(model::CompileStatements(
            {{"LD", {"X0"}}, {"SET", {"M0"}},
             {"LD", {"X1"}}, {"RST", {"M0"}},
             {"LD", {"M0"}}, {"OUT", {"Y0"}},
             {"LD", {"X1"}}, {"OUT", {"Y1"}}, {"END", {}}},
            &program, &error),
        "Compile physical cycle logic");
  plc::CompiledPLCExecutor executor;
  Check(executor.LoadProgram(program), "Load physical cycle logic");
  executor.SetContinuousExecution(true, 10);
  CylinderFixture fixture;
  auto rear = Component(3, plc::ComponentType::LIMIT_SWITCH);
  rear.size = {4.0f, 4.0f};
  rear.position = {168.0f, 48.0f};
  auto front = rear;
  front.instanceId = 4;
  front.position.x = 8.0f;
  fixture.components.push_back(rear);
  fixture.components.push_back(front);
  bool previous_front = false;
  bool previous_rear = true;
  int arrivals = 0;
  int returns = 0;
  int scans = 0;
  for (int tick = 0; tick < 30000 && returns < 20; ++tick) {
    Check(plc::UpdatePhysicalSensorsBox2d(&fixture, &fixture.components),
          "Box2D physical sensor update");
    const bool rear_detected =
        plc::component_input::IsLimitSwitchPressed(fixture.components[3]);
    const bool front_detected =
        plc::component_input::IsLimitSwitchPressed(fixture.components[4]);
    if (front_detected && !previous_front) {
      ++arrivals;
    }
    if (rear_detected && !previous_rear) {
      ++returns;
    }
    previous_front = front_detected;
    previous_rear = rear_detected;
    // 10 ms PLC scans, 240 Hz physics, with sampled physical feedback.
    if ((tick * 100) / 240 != ((tick - 1) * 100) / 240 || tick == 0) {
      executor.SetInput(0, rear_detected);
      executor.SetInput(1, front_detected);
      Check(executor.ExecuteScanCycle().success, "Physical cycle PLC scan");
      Check(executor.GetOutput(1) == front_detected,
            "Physical front sensor must reach PLC output Y1");
      ++scans;
    }
    fixture.Step(executor.GetOutput(0));
  }
  Check(arrivals == 20 && returns == 20,
        "PLC, solenoid, wired air, cylinder and sensors must cycle 20 times");
  std::cout << "PASS physical loop: 20 cylinder round trips, " << scans
            << " PLC scans, physical endpoint feedback\n";
}

void AirAndPowerFailures() {
  CylinderFixture fixture;
  for (int i = 0; i < 120; ++i) {
    fixture.Step(true);
  }
  Check(fixture.position() > 10.0f && fixture.position() < 150.0f,
        "Cylinder must move before fault injection");
  const float before = fixture.position();
  fixture.components[0].internalStates[plc::state_keys::kAirPressure] = 0.0f;
  for (int i = 0; i < 240; ++i) {
    fixture.Step(true);
  }
  Check(fixture.position() == before, "Loss of air must stop cylinder");
  fixture.components[0].internalStates[plc::state_keys::kAirPressure] = 6.0f;
  fixture.hoses.pop_back();
  fixture.Step(true);
  Check(fixture.position() == before, "Disconnected hose must stop cylinder");
  fixture.hoses.push_back(Hose(1, 4, 2, 1));
  fixture.powered = false;
  for (int i = 0; i < 480; ++i) {
    fixture.Step(true);
  }
  Check(fixture.position() == 0.0f,
        "Single valve must spring return after electrical power loss");
  std::cout << "PASS physical faults: air loss, hose disconnection, "
               "single valve power-loss return\n";
}

void ElectricalDevices() {
  auto conveyor = Component(5, plc::ComponentType::CONVEYOR);
  auto lamp = Component(6, plc::ComponentType::TOWER_LAMP);
  auto valve = Component(7, plc::ComponentType::VALVE_DOUBLE);
  const auto* conveyor_adapter = plc::GetComponentPhysicsAdapter(conveyor.type);
  const auto* lamp_adapter = plc::GetComponentPhysicsAdapter(lamp.type);
  const auto* valve_adapter = plc::GetComponentPhysicsAdapter(valve.type);
  conveyor_adapter->UpdateElectrical(&conveyor, {{{5, 0}, 24}, {{5, 1}, 0}});
  Check(conveyor.internalStates[plc::state_keys::kMotorActive] == 1,
        "Conveyor forward power");
  conveyor_adapter->UpdateElectrical(&conveyor, {{{5, 0}, 0}, {{5, 1}, 24}});
  Check(conveyor.internalStates[plc::state_keys::kMotorActive] == 1,
        "Conveyor reverse power");
  conveyor_adapter->UpdateElectrical(&conveyor, {{{5, 0}, 24}});
  Check(conveyor.internalStates[plc::state_keys::kMotorActive] == 0,
        "Conveyor open return wire must not power motor");
  lamp_adapter->UpdateElectrical(
      &lamp, {{{6, 0}, 0}, {{6, 1}, 24}, {{6, 2}, 0}, {{6, 3}, 24}});
  Check(lamp.internalStates[plc::state_keys::kLampRed] == 1 &&
            lamp.internalStates[plc::state_keys::kLampYellow] == 0 &&
            lamp.internalStates[plc::state_keys::kLampGreen] == 1,
        "Tower lamp separate color channels");
  valve_adapter->UpdateElectrical(
      &valve, {{{7, 0}, 24}, {{7, 1}, 0}, {{7, 2}, 0}, {{7, 3}, 0}});
  valve_adapter->UpdateValveLogic(&valve);
  valve_adapter->UpdateElectrical(&valve, {});
  valve_adapter->UpdateValveLogic(&valve);
  Check(valve.internalStates[plc::state_keys::kLastActiveSolenoid] == 1,
        "Double valve retains spool position without power");
  valve_adapter->UpdateElectrical(
      &valve, {{{7, 0}, 0}, {{7, 1}, 0}, {{7, 2}, 24}, {{7, 3}, 0}});
  valve_adapter->UpdateValveLogic(&valve);
  Check(valve.internalStates[plc::state_keys::kLastActiveSolenoid] == 2,
        "Double valve switches on opposite solenoid");
  std::cout << "PASS electrical devices: conveyor power/open return, "
               "tower lamp channels, double valve memory/reversal\n";
}

void ConveyorTransport() {
  auto conveyor = Component(10, plc::ComponentType::CONVEYOR);
  conveyor.size = {400.0f, 80.0f};
  auto workpiece = Component(11, plc::ComponentType::WORKPIECE_METAL);
  workpiece.position = {30.0f, 30.0f};
  workpiece.size = {10.0f, 10.0f};
  std::vector<plc::PlacedComponent> components{conveyor, workpiece};
  const auto* adapter = plc::GetComponentPhysicsAdapter(conveyor.type);
  adapter->UpdateElectrical(&components[0], {{{10, 0}, 24}, {{10, 1}, 0}});
  for (int i = 0; i < 240; ++i) {
    Check(plc::UpdateWorkpiecesBox2d(&components, &components,
                                   1.0f / 240.0f, false),
          "Box2D conveyor movement");
  }
  Check(std::abs(components[1].position.x - 90.0f) < 0.1f,
        "Powered conveyor must transport workpiece 60 units in one second");
  adapter->UpdateElectrical(&components[0], {});
  const float stopped_x = components[1].position.x;
  for (int i = 0; i < 240; ++i) {
    Check(plc::UpdateWorkpiecesBox2d(&components, &components,
                                   1.0f / 240.0f, false),
          "Box2D unpowered conveyor");
  }
  Check(std::abs(components[1].position.x - stopped_x) < 0.1f,
        "Conveyor power loss must stop workpiece transport");
  std::cout << "PASS Box2D conveyor: 60 units/second physical workpiece "
               "transport, power-loss stop\n";
}

void ZLayerIsolation() {
  auto conveyor = Component(110, plc::ComponentType::CONVEYOR);
  conveyor.size = {400.0f, 80.0f};
  auto workpiece = Component(111, plc::ComponentType::WORKPIECE_METAL);
  workpiece.position = {30.0f, 30.0f};
  workpiece.size = {10.0f, 10.0f};
  workpiece.z_state.current_height = 2.0f;
  std::vector<plc::PlacedComponent> components{conveyor, workpiece};
  plc::GetComponentPhysicsAdapter(conveyor.type)->UpdateElectrical(
      &components[0], {{{110, 0}, 24}, {{110, 1}, 0}});
  for (int tick = 0; tick < 240; ++tick) {
    Check(plc::UpdateWorkpiecesBox2d(&components, &components,
                                   1.0f / 240.0f, false), "Z query path");
  }
  Check(std::abs(components[1].position.x - 30.0f) < 0.1f,
        "Different Z must prevent conveyor transport");
  components[1].z_state.current_height = 1.49f;
  plc::UpdateWorkpiecesBox2d(&components, &components, 1.0f, false);
  Check(std::abs(components[1].position.x - 30.0f) < 0.1f,
        "Continuous Z must not round to adjacent layer");
  components[1].z_state.current_height = 1.0f;
  components[0].z_order = -5;
  components[1].z_order = 500;
  plc::UpdateWorkpiecesBox2d(&components, &components, 1.0f, false);
  Check(std::abs(components[1].position.x - 90.0f) < 0.1f,
        "Z-only change must refresh interaction independent of draw order");
  auto lower_box = Component(112, plc::ComponentType::BOX);
  lower_box.size = {80.0f, 80.0f};
  auto upper_box = Component(113, plc::ComponentType::BOX);
  upper_box.size = {120.0f, 120.0f};
  upper_box.z_state.current_height = 2.0f;
  workpiece.z_state.current_height = 1.0f;
  components = {workpiece, lower_box, upper_box};
  plc::UpdateWorkpiecesBox2d(&components, &components, 1.0f / 240.0f, false);
  Check(std::abs(components[0].position.x - 35.0f) < 0.1f,
        "Max-index query must skip wrong Z before selecting a box");
  auto sensor = Component(114, plc::ComponentType::SENSOR);
  sensor.size = {100.0f, 120.0f};
  const ImVec2 center = plc::GetSensorCenterWorld(sensor);
  workpiece.position = {center.x - 5.0f, center.y - 35.0f};
  workpiece.z_state.current_height = 2.0f;
  components = {sensor, workpiece};
  Check(plc::UpdatePhysicalSensorsBox2d(&components, &components),
        "Physical sensor Z query");
  Check(!plc::component_input::IsSensorDetected(components[0]),
        "Beam sensor must ignore a workpiece at another height");
  components[1].z_state.current_height = 1.0f;
  plc::UpdatePhysicalSensorsBox2d(&components, &components);
  Check(plc::component_input::IsSensorDetected(components[0]),
        "Beam sensor must detect after a Z-only change");
  auto processing = Component(120, plc::ComponentType::PROCESSING_CYLINDER);
  processing.z_config = {4.0f, 5.0f, 5.0f};
  processing.internalStates[plc::state_keys::kPosition] = 30.0f;
  Check(plc_emulator::ResolveContactHeight(processing) == 5.0f,
        "Raised processing head maps to configurable maximum Z");
  processing.internalStates[plc::state_keys::kPosition] = 15.0f;
  Check(plc_emulator::ResolveContactHeight(processing) == 4.0f,
        "Lowered processing head maps to configurable minimum Z");
  std::cout << "PASS physical Z: isolation, continuous height, Z-only refresh, "
               "draw order independence and processing stroke mapping\n";
}

void IntegratedPneumaticParts() {
  for (const auto type : {plc::ComponentType::kSensorCylinder,
                         plc::ComponentType::kSuctionCylinder,
                         plc::ComponentType::kStopperCylinder}) {
    CylinderFixture fixture;
    fixture.components[2].type = type;
    fixture.cylinder = plc::GetComponentPhysicsAdapter(type);
    for (int tick = 0; tick < 240; ++tick) {
      fixture.Step(true);
    }
    Check(plc_emulator::IsForwardEndpoint(fixture.components[2]),
          "All integrated axes share pressure-driven extension");
    fixture.cylinder->UpdateElectrical(&fixture.components[2],
        {{{2, 2}, 24}, {{2, 3}, 0}});
    Check(fixture.components[2].internalStates.at("is_powered") == 1,
          "Integrated sensor requires both power rails");
    fixture.cylinder->UpdateElectrical(&fixture.components[2], {{{2, 2}, 24}});
    Check(fixture.components[2].internalStates.at("is_powered") == 0,
          "Missing ground must disable integrated sensor");
    for (int tick = 0; tick < 240; ++tick) {
      fixture.Step(false);
    }
    Check(plc_emulator::IsReverseEndpoint(fixture.components[2]),
          "All integrated axes share pressure-driven return");
  }
  auto stopper = Component(130, plc::ComponentType::kStopperCylinder);
  stopper.size = {240, 155};
  stopper.z_config = {1, 2, 2};
  auto conveyor = Component(131, plc::ComponentType::CONVEYOR);
  conveyor.position = {-100, 60};
  conveyor.size = {600, 80};
  conveyor.internalStates["motor_active"] = 1;
  auto workpiece = Component(132, plc::ComponentType::WORKPIECE_METAL);
  workpiece.position = {-20, 70};
  workpiece.size = {10, 10};
  stopper.internalStates["position"] = 160;
  std::vector<plc::PlacedComponent> parts{stopper, conveyor, workpiece};
  for (int tick = 0; tick < 240; ++tick) {
    plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false);
  }
  Check(std::abs(parts[2].position.x - (-4)) < 0.1f,
        "Lowered plate stops workpiece without stopping conveyor");
  Check(parts[1].internalStates.at("motor_active") == 1,
        "Stopper must leave conveyor motor on");
  parts[0].internalStates["position"] = 0;
  for (int tick = 0; tick < 240; ++tick) {
    plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false);
  }
  Check(parts[2].position.x > 55,
        "Raised plate at another Z releases conveyor transport");
  parts[0].internalStates["position"] = 160;
  workpiece.position = {-80, 70};
  workpiece.internalStates["vel_x"] = 1000;
  std::vector<plc::PlacedComponent> fast{parts[0], workpiece};
  plc::UpdateWorkpiecesBox2d(&fast, &fast, 1, false);
  Check(fast[1].position.x <= -4 + 0.1f,
        "Swept plate collision prevents high-speed tunnelling");
  for (int rotation = 0; rotation < 4; ++rotation) {
    stopper.size = {80, 80};
    stopper.rotation_quadrants = rotation;
    const float lane_center = 80.0f * 80.0f / 155.0f;
    const ImVec2 before = plc::LocalToWorld(stopper, {-30, lane_center});
    const ImVec2 after = plc::LocalToWorld(stopper, {30, lane_center});
    workpiece.position = {after.x - 5, after.y - 5};
    std::vector<plc::PlacedComponent> compact{stopper, workpiece};
    plc_emulator::ResolveStopperCollision(
        compact, {before.x - 5, before.y - 5}, &compact[1]);
    const ImVec2 stopped = plc::WorldToLocal(
        stopper, plc::GetComponentWorldCenter(compact[1]));
    Check(stopped.x <= -2.9f,
          "Compact stopper keeps swept collision in all four rotations");
  }
  std::cout << "PASS integrated axes: shared stroke, powered sensors, "
               "lowered stopper, raised clearance and swept collision\n";
}

void VacuumAndCarry() {
  auto frl = Component(140, plc::ComponentType::FRL);
  auto ejector = Component(141, plc::ComponentType::kVacuumGenerator);
  auto cylinder = Component(142, plc::ComponentType::kSuctionCylinder);
  cylinder.size = {340, 84};
  std::vector<plc::PlacedComponent> parts{frl, ejector, cylinder};
  std::vector<plc::Wire> hoses{Hose(140, 0, 141, 0), Hose(141, 1, 142, 6)};
  std::map<plc::PortRef, float> pressure, exhaust;
  std::map<plc::PortRef, bool> supply, connected;
  plc::SimulatePneumaticNetwork(hoses, parts, &pressure, &supply,
                               &exhaust, &connected);
  Check(pressure.at({142, 6}) < -0.2f,
        "Ejector makes negative pressure only on separate VAC circuit");
  Check(pressure.at({141, 0}) == 6,
        "Ejector must preserve positive supply pressure");
  parts[2].internalStates["vacuum_pressure"] = pressure.at({142, 6});
  auto workpiece = Component(143, plc::ComponentType::WORKPIECE_NONMETAL);
  workpiece.size = {10, 10};
  const ImVec2 head = plc_emulator::GetSuctionHeadWorld(parts[2]);
  workpiece.position = {head.x - 5, head.y - 5};
  parts.push_back(workpiece);
  parts[3].z_state.current_height = 2;
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[3].carry_state.owner_id == -1,
        "Vacuum cannot acquire workpiece at different Z");
  parts[3].z_state.current_height = 1;
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[3].carry_state.owner_id == 142,
        "Valid vacuum, contact and Z acquire one workpiece");
  const float initial_x = parts[3].position.x;
  parts[2].internalStates["position"] = 100;
  parts[2].z_state.current_height = 3;
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(std::abs(parts[3].position.x - (initial_x - 100)) < 0.1f &&
            parts[3].z_state.current_height == 3,
        "Carried workpiece follows head XY and Z");
  auto conveyor = Component(144, plc::ComponentType::CONVEYOR);
  conveyor.size = {400, 100};
  conveyor.z_state.current_height = 3;
  conveyor.internalStates["motor_active"] = 1;
  parts.push_back(conveyor);
  const float carried_x = parts[3].position.x;
  plc::UpdateWorkpiecesBox2d(&parts, &parts, 1, false);
  Check(std::abs(parts[3].position.x - carried_x) < 0.1f,
        "Conveyor cannot transport an owned workpiece");
  hoses.erase(hoses.begin());
  plc::SimulatePneumaticNetwork(hoses, parts, &pressure, &supply,
                               &exhaust, &connected);
  Check(!pressure.count({142, 6}), "Supply loss removes vacuum immediately");
  parts[2].internalStates["vacuum_pressure"] = 0;
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[3].carry_state.owner_id == -1 &&
            std::abs(parts[3].position.x - carried_x) < 0.1f,
        "Vacuum loss releases at current position, without teleporting");
  plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false);
  Check(parts[3].position.x > carried_x &&
            parts[3].position.x < carried_x + 1,
        "Released workpiece resumes conveyor without legacy rod snapping");
  parts[3].carry_state.owner_id = 142;
  parts.erase(parts.begin() + 2);
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[2].carry_state.owner_id == -1,
        "Deleting owner cannot leave a dangling ownership reference");
  cylinder.internalStates["vacuum_pressure"] = -0.6f;
  auto later = workpiece;
  later.instanceId = 146;
  std::vector<plc::PlacedComponent> contenders{cylinder, later, workpiece};
  plc_emulator::UpdateSuctionWorkpieces(&contenders);
  Check(contenders[1].carry_state.owner_id == -1 &&
            contenders[2].carry_state.owner_id == 142,
        "Overlapping acquisition chooses lowest stable workpiece ID");
  auto bad_ejector = Component(145, plc::ComponentType::kVacuumGenerator);
  parts = {frl, ejector, cylinder, bad_ejector};
  hoses = {Hose(140, 0, 141, 0), Hose(141, 1, 142, 6),
           Hose(140, 0, 145, 0), Hose(140, 0, 145, 1)};
  plc::SimulatePneumaticNetwork(hoses, parts, &pressure, &supply,
                               &exhaust, &connected);
  Check(pressure.at({142, 6}) < -0.2f && pressure.at({145, 1}) > 0,
        "One positive-contaminated VAC circuit must not disable another");
  std::cout << "PASS vacuum: separate pressure circuits, Z contact, "
               "ownership, tracking, conveyor isolation and loss release\n";
}

void BoxAlignment() {
  for (int rotation = 0; rotation < 4; ++rotation) {
    auto box = Component(160, plc::ComponentType::BOX);
    box.size = {80, 100};
    box.position = {200, 300};
    box.rotation_quadrants = rotation;
    box.flip_x = rotation % 2 != 0;
    std::vector<plc::PlacedComponent> parts{box};
    for (int index = 0; index < 4; ++index) {
      auto piece = Component(164 - index, plc::ComponentType::WORKPIECE_NONMETAL);
      piece.size = {30, 30};
      const ImVec2 center = plc::GetComponentWorldCenter(box);
      piece.position = {center.x - 15, center.y - 15};
      parts.push_back(piece);
      Check(plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false),
            "Actual box capture path runs");
      for (size_t first = 1; first < parts.size(); ++first) {
        const auto& stored = parts[first];
        Check(plc_emulator::ReadPneumaticState(stored, "is_stuck_box") > 0.5f &&
                  plc_emulator::ReadPneumaticState(stored, "box_slot") == first,
              "Box preserves arrival order independent of component IDs");
        const ImVec2 local = plc::WorldToLocal(
            box, plc::GetComponentWorldCenter(stored));
        Check(local.x >= 19 && local.x <= 61 &&
                  local.y >= 19 && local.y <= 81,
              "Every stored workpiece stays inside the rotated box");
        for (size_t second = first + 1; second < parts.size(); ++second) {
          const ImVec2 other = plc::GetComponentWorldCenter(parts[second]);
          const ImVec2 center = plc::GetComponentWorldCenter(stored);
          Check(std::abs(center.x - other.x) >= 33.9f ||
                    std::abs(center.y - other.y) >= 33.9f,
                "Stored pieces have a gap and never overlap");
        }
      }
    }
    auto other_layer = Component(170, plc::ComponentType::WORKPIECE_NONMETAL);
    other_layer.size = {30, 30};
    other_layer.position = parts[1].position;
    other_layer.z_state.current_height = 2;
    parts.push_back(other_layer);
    plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false);
    Check(plc_emulator::ReadPneumaticState(parts.back(), "is_stuck_box") < 0.5f,
          "Box does not capture a workpiece on another Z layer");
    parts[1].internalStates["is_stuck_box"] = 0;
    parts[1].position = {500, 500};
    parts.back().z_state.current_height = 1;
    const ImVec2 center = plc::GetComponentWorldCenter(box);
    parts.back().position = {center.x - 15, center.y - 15};
    plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false);
    Check(plc_emulator::ReadPneumaticState(parts.back(), "is_stuck_box") > 0.5f,
          "Removing a piece frees space for the next arrival");
  }
  std::cout << "PASS box alignment: arrival order, spacing, rotation, Z, reuse\n";
}

void ServoAxes() {
  auto plc_component = Component(150, plc::ComponentType::PLC);
  auto ac = Component(151, plc::ComponentType::kAcSupply);
  auto amp = Component(152, plc::ComponentType::kServoAmplifier);
  auto lift = Component(153, plc::ComponentType::kLiftSuction);
  lift.size = {340, 200};
  lift.z_config = {1, 3, 3};
  plc_emulator::InitializeZLayerState(&lift);
  std::vector<plc::PlacedComponent> parts{plc_component, ac, amp, lift};
  std::vector<plc::Wire> wires;
  const auto electrical = [&](int from, int port, int to, int target) {
    auto wire = Hose(from, port, to, target);
    wire.kind = plc::WireKind::kElectric;
    wires.push_back(wire);
  };
  for (int phase = 0; phase < 4; ++phase) electrical(151, phase, 152, phase);
  for (int phase = 0; phase < 3; ++phase) electrical(152, 14 + phase, 153, 10 + phase);
  electrical(152, 3, 153, 13);
  electrical(152, 17, 153, 14);
  electrical(150, 16, 152, 6);
  electrical(150, 20, 152, 8);
  std::map<plc::PortRef, float> volts{
      {{152, 4}, 24}, {{152, 5}, 0}, {{152, 7}, 0},
      {{152, 8}, 24}, {{152, 9}, 24}};
  std::vector<plc_emulator::PulseSource> pulses{{150, 0, 0, 0, 1, 4}};
  const auto tick = [&]() {
    plc_emulator::StepServoNetwork(&parts, wires, volts, pulses, 1.0 / 240, false);
  };
  tick();
  Check(parts[2].axis_state.ready, "Valid phase, PE, encoder and control chain");
  pulses[0].reverse = 1000;
  tick();
  Check(!parts[2].axis_state.in_position && parts[2].axis_state.target == 19,
        "1000 sub-tick pulses set one reverse revolution; INP waits for motion");
  for (int i = 0; i < 240; ++i) tick();
  Check(parts[2].axis_state.in_position &&
            std::abs(parts[3].z_state.current_height - 2.9f) < 0.001f,
        "Lead and travel map the real servo position to continuous Z");
  const ImVec2 before = plc_emulator::GetSuctionHeadWorld(parts[3]);
  parts[3].internalStates["position"] = 100;
  const ImVec2 after = plc_emulator::GetSuctionHeadWorld(parts[3]);
  Check(after.x - before.x == 100 && after.y == before.y,
        "Integrated cylinder extends right without changing lift height");
  parts[3].internalStates["vacuum_pressure"] = -0.6f;
  auto workpiece = Component(154, plc::ComponentType::WORKPIECE_NONMETAL);
  workpiece.size = {10, 10};
  workpiece.position = {after.x - 5, after.y - 5};
  workpiece.z_state.current_height = parts[3].z_state.current_height;
  parts.push_back(workpiece);
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[4].carry_state.owner_id == 153, "Integrated head acquires workpiece");
  pulses[0].reverse += 1000;
  for (int i = 0; i < 240; ++i) {
    tick();
    plc_emulator::UpdateSuctionWorkpieces(&parts);
  }
  Check(std::abs(parts[4].z_state.current_height - 2.8f) < 0.001f,
        "Owned workpiece follows the servo Z axis and pneumatic XY axis");
  const double held = parts[3].axis_state.position;
  wires.erase(wires.begin() + 8); // Encoder, after AC and phase/earth connections.
  tick();
  Check(!parts[2].axis_state.ready && parts[2].axis_state.alarm == 3 &&
            parts[3].axis_state.position == held,
        "Encoder loss alarms and holds actual position");
  volts[{152, 10}] = 24;
  tick();
  Check(parts[2].axis_state.alarm != 0,
        "Reset cannot clear a still-disconnected encoder");
  electrical(152, 17, 153, 14);
  tick();
  Check(parts[2].axis_state.ready && parts[2].axis_state.alarm == 0,
        "Reconnect and RESET restores valid chain");
  volts[{152, 9}] = 0;
  pulses[0].reverse += 5000;
  tick();
  volts[{152, 9}] = 24;
  tick();
  Check(parts[3].axis_state.position == held,
        "SON-off pulses are discarded, never replayed on re-enable");
  pulses[0].epoch = 2;
  pulses[0].forward = 0;
  pulses[0].reverse = 0;
  tick();
  Check(parts[3].axis_state.position == held,
        "PLC reset rebases pulse cursor without moving load");
  pulses[0].reverse = 100000;
  tick();
  Check(parts[2].axis_state.alarm == 4 && !parts[2].axis_state.in_position,
        "Mechanical travel over-command alarms instead of false completion");
  parts[3].axis_settings = {60, 1, 100, 100, 200};
  plc_emulator::InitializeZLayerState(&parts[3]);
  parts[2].axis_state = {};
  pulses[0] = {150, 0, 0, 0, 3, 4};
  tick();
  for (int pulse = 0; pulse < 6000; ++pulse) {
    ++pulses[0].reverse;
    tick();
  }
  for (int i = 0; i < 2400; ++i) tick();
  Check(parts[2].axis_state.alarm == 0 && parts[2].axis_state.in_position &&
            std::abs(parts[3].z_state.current_height - 1) < 0.00001f,
        "Fractional-turn pulse accumulation reaches exact lift endpoint without false limit alarm");
  auto dc = Component(155, plc::ComponentType::kDcMotor);
  std::vector<plc::PlacedComponent> dc_parts{dc};
  volts = {{{155, 0}, 24}, {{155, 1}, 0}};
  for (int i = 0; i < 240; ++i)
    plc_emulator::StepServoNetwork(&dc_parts, {}, volts, {}, 1.0 / 240, false);
  Check(dc_parts[0].axis_state.velocity > 9.9, "DC voltage drives axis speed");
  for (int i = 0; i < 240; ++i)
    plc_emulator::StepServoNetwork(&dc_parts, {}, {}, {}, 1.0 / 240, false);
  Check(std::abs(dc_parts[0].axis_state.velocity) < 0.001,
        "DC power loss decelerates without resetting angle");
  std::cout << "PASS servo: high-rate pulses, tracking/INP, lift Z, integrated "
               "suction, encoder fault/reset, SON gating, limits and DC stop\n";
}

void StorageAndOptical() {
  using plc::ComponentType;
  auto warehouse = Component(300, ComponentType::kWarehouse);
  warehouse.size = {160, 100}; warehouse.z_config = {1, 3, 3};
  auto indexing_cylinder = Component(399, ComponentType::kSensorCylinder);
  indexing_cylinder.size = {340, 84};
  indexing_cylinder.flip_x = true;
  warehouse.warehouse_cylinder_id = 399;
  auto stored_on_base = Component(398, ComponentType::WORKPIECE_METAL);
  stored_on_base.size = {40, 40};
  stored_on_base.carry_state.owner_id = 300;
  stored_on_base.carry_state.slot_id = 2;
  std::vector<plc::PlacedComponent> indexing{warehouse, indexing_cylinder, stored_on_base};
  indexing[1].internalStates["position"] = 160;
  plc_emulator::UpdateWarehouseWorkpieces(&indexing, false);
  Check(std::abs(indexing[0].position.x - 70) < 0.001 &&
            std::abs(indexing[2].position.x - 165) < 0.001 &&
            indexing[2].carry_state.slot_id == 2,
        "Warehouse base follows indexed cylinder; stored slot moves with base");
  plc_emulator::UpdateWarehouseWorkpieces(&indexing, false);
  Check(std::abs(indexing[0].position.x - 70) < 0.001,
        "Repeated warehouse updates do not accumulate displacement");
  indexing[1].internalStates["position"] = 0;
  plc_emulator::UpdateWarehouseWorkpieces(&indexing, false);
  Check(std::abs(indexing[0].position.x) < 0.001 &&
            std::abs(indexing[2].position.x - 95) < 0.001,
        "Retracting warehouse cylinder returns base and stored workpiece");
  warehouse.warehouse_cylinder_id = -1;
  auto lift = Component(301, ComponentType::kLiftSuction);
  lift.size = {340, 200}; lift.position = {-74, -111};
  lift.z_config = {1, 3, 2}; plc_emulator::InitializeZLayerState(&lift);
  auto piece = Component(302, ComponentType::WORKPIECE_NONMETAL);
  piece.size = {40, 40}; piece.position = {25, 30};
  piece.z_state.current_height = 2; piece.carry_state.owner_id = 301;
  std::vector<plc::PlacedComponent> parts{warehouse, lift, piece};
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[2].carry_state.owner_id == 300 && parts[2].carry_state.slot_id == 3,
        "Vacuum release at stopped Z2 deposits in slot 3");
  auto duplicate = piece; duplicate.instanceId = 303;
  duplicate.carry_state = {}; duplicate.carry_state.release_source_id = 301;
  parts.push_back(duplicate);
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[3].carry_state.owner_id == -1 && parts[2].carry_state.slot_id == 3,
        "Occupied slot cannot be overwritten");
  parts.erase(parts.begin() + 3);
  auto second = piece; second.instanceId = 304; second.position.x = 95;
  second.z_state.current_height = 1; second.carry_state = {};
  second.carry_state.release_source_id = 301; parts.push_back(second);
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[3].carry_state.slot_id == 2 && parts[2].carry_state.slot_id == 3,
        "Slot 3 then slot 2 use separate heights and columns");
  auto incoming = piece;
  incoming.instanceId = 305;
  incoming.position = {-29, 30};
  incoming.carry_state = {};
  incoming.z_state.current_height = 1.5f;
  parts.push_back(incoming);
  plc_emulator::UpdateWarehouseWorkpieces(&parts);
  Check(parts.back().carry_state.owner_id == -1,
        "Overlapping free piece at a different height remains free");
  parts.back().z_state.current_height = 2;
  parts.back().position.x = 95;
  parts.back().internalStates["is_manual_drag"] = 1;
  plc_emulator::UpdateWarehouseWorkpieces(&parts);
  Check(parts.back().carry_state.owner_id == -1,
        "Manual drag does not capture a piece before drop");
  parts.back().internalStates["is_manual_drag"] = 0;
  plc_emulator::UpdateWarehouseWorkpieces(&parts);
  Check(parts.back().carry_state.owner_id == 300 &&
            parts.back().carry_state.slot_id == 4,
        "Manual drop stores a free piece without a suction release event");
  parts.pop_back();
  auto receiving = warehouse;
  receiving.position = {100, 0};
  receiving.z_config = {1, 3, 1};
  auto conveyor = Component(306, ComponentType::CONVEYOR);
  conveyor.position = {0, 0};
  conveyor.size = {200, 100};
  plc::GetComponentPhysicsAdapter(conveyor.type)->UpdateElectrical(
      &conveyor, {{{306, 0}, 24}, {{306, 1}, 0}});
  auto delivered = incoming;
  delivered.position = {60, 30};
  delivered.z_state.current_height = 1;
  std::vector<plc::PlacedComponent> delivery{receiving, conveyor, delivered};
  for (int tick = 0; tick < 240 && delivery.back().carry_state.owner_id < 0;
       ++tick) {
    Check(plc::UpdateWorkpiecesBox2d(&delivery, &delivery, 1.0f / 240, false),
          "Conveyor delivery uses the actual Box2D path");
  }
  Check(delivery.back().carry_state.owner_id == 300 &&
            delivery.back().carry_state.slot_id == 1,
        "Conveyor entering a warehouse opening stores its free workpiece");
  for (const int slot : {1, 4, 5, 6}) {
    auto extra = piece; extra.instanceId = 310 + slot;
    extra.carry_state = {};
    extra.position.x = slot % 2 ? 25.0f : 95.0f;
    extra.z_state.current_height = plc_emulator::WarehouseSlotHeight(parts[0], slot);
    parts.push_back(extra);
  }
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  int occupied = 0;
  for (const auto& part : parts) if (part.carry_state.owner_id == 300) ++occupied;
  Check(occupied == 6, "Six-slot capacity, two per service height");
  parts[1].internalStates["vacuum_pressure"] = -0.5f;
  plc_emulator::UpdateSuctionWorkpieces(&parts);
  Check(parts[2].carry_state.owner_id == 301 && parts[2].carry_state.slot_id == -1,
        "Real head contact atomically transfers stored piece back to suction");
  Check(plc::UpdateWorkpiecesBox2d(&parts, &parts, 1.0f / 240, false),
        "Stored and carried pieces survive Box2D path");
  Check(parts[3].carry_state.slot_id == 2, "Storage excluded from free conveyor movement");
  auto amp = Component(400, ComponentType::kFiberAmplifier);
  auto tx = Component(401, ComponentType::kFiberTransmitter);
  auto rx = Component(402, ComponentType::kFiberReceiver);
  tx.size = rx.size = {98, 30}; tx.position = {0, 0}; tx.flip_x = true;
  rx.position = {200, 0};
  auto blocker = Component(403, ComponentType::WORKPIECE_METAL);
  blocker.size = {40, 40}; blocker.position = {130, -5};
  std::vector<plc::PlacedComponent> optical{amp, tx, rx, blocker};
  auto first = Hose(400, 3, 401, 0); first.kind = plc::WireKind::kOptical;
  auto last = Hose(400, 4, 402, 0); last.kind = plc::WireKind::kOptical;
  std::vector<plc::Wire> wires{first, last};
  std::map<plc::PortRef, float> volts{{{400, 0}, 24}, {{400, 1}, 0}};
  const auto scan = [&] { plc_emulator::UpdateFiberSensors(&optical, wires, volts); };
  scan(); Check(optical[0].fiber_state.output && optical[0].fiber_state.fault == 0,
                "Facing powered optical heads detect same-height obstruction");
  optical[3].z_state.current_height = 2; scan();
  Check(!optical[0].fiber_state.output && optical[0].fiber_state.received == 1000,
        "Different Z remains optically clear");
  optical[0].fiber_settings.dark_on = false; scan();
  Check(optical[0].fiber_state.output, "Light-ON reverses valid detection");
  optical[2].position.y = 10; scan();
  Check(optical[0].fiber_state.fault == 3 && !optical[0].fiber_state.output,
        "Misalignment is a fault, not an object detection");
  optical[2].position.y = 0; optical[2].z_state.current_height = 2; scan();
  Check(optical[0].fiber_state.fault == 3, "Sloped Z beam not projected into XY");
  optical[2].z_state.current_height = 1; wires.pop_back(); scan();
  Check(optical[0].fiber_state.fault == 2, "Missing optical channel fault");
  volts.clear(); scan(); Check(optical[0].fiber_state.fault == 1 &&
      !optical[0].fiber_state.output, "Power loss disables optical output");
}

void DrivenWarehouse() {
  for (int rotation = 0; rotation < 4; ++rotation) {
    CylinderFixture fixture;
    auto& warehouse = fixture.components[2];
    warehouse.type = plc::ComponentType::kDrivenWarehouse;
    warehouse.size = {430, 100};
    warehouse.position = {100, 200};
    warehouse.rotation_quadrants = rotation;
    warehouse.z_config = {1, 3, 1};
    auto piece = Component(3, plc::ComponentType::WORKPIECE_METAL);
    piece.size = {30, 30};
    piece.carry_state.owner_id = 2;
    piece.carry_state.slot_id = 2;
    fixture.components.push_back(piece);
    plc_emulator::UpdateWarehouseWorkpieces(&fixture.components, false);
    const ImVec2 start = plc::GetComponentWorldCenter(fixture.components[3]);
    for (int tick = 0; tick < 1200; ++tick) {
      fixture.Step(true);
      plc_emulator::UpdateWarehouseWorkpieces(&fixture.components, false);
    }
    const auto& driven = fixture.components[2];
    const ImVec2 end = plc::GetComponentWorldCenter(fixture.components[3]);
    const ImVec2 direction = plc::LocalDirToWorld(driven, {1, 0});
    Check(std::abs(fixture.position() - 70) < 0.01f,
          "Driven warehouse cylinder reaches its physical 70 mm stop");
    Check(driven.position.x == 100 && driven.position.y == 200,
          "Driven warehouse frame and pneumatic terminals stay fixed");
    Check(std::abs(end.x - start.x - direction.x * 70) < 0.01f &&
              std::abs(end.y - start.y - direction.y * 70) < 0.01f,
          "Stored workpiece and carriage follow the rod one-to-one in all rotations");
    const auto* adapter = plc::GetComponentPhysicsAdapter(driven.type);
    adapter->UpdateElectrical(&fixture.components[2],
                              {{{2, 2}, 24}, {{2, 3}, 0}});
    Check(fixture.components[2].internalStates["sensor_fwd"] == 1,
          "Integrated forward switch detects the 70 mm endpoint");
    fixture.hoses.clear();
    fixture.Step(true);
    Check(fixture.position() == 70, "No air cannot move the carriage");
    adapter->UpdateElectrical(&fixture.components[2], {});
    Check(fixture.components[2].internalStates["sensor_fwd"] == 0,
          "Endpoint output needs sensor power");
    fixture.hoses = {Hose(0, 0, 1, 2), Hose(1, 3, 2, 0), Hose(1, 4, 2, 1)};
    for (int tick = 0; tick < 1200; ++tick) {
      fixture.Step(false);
      plc_emulator::UpdateWarehouseWorkpieces(&fixture.components, false);
    }
    Check(fixture.position() == 0 &&
              std::abs(plc::GetComponentWorldCenter(fixture.components[3]).x - start.x) < 0.01f &&
              std::abs(plc::GetComponentWorldCenter(fixture.components[3]).y - start.y) < 0.01f,
          "Reverse air returns the carriage and retained workpiece to home");
  }
}

void RingSensorFeed() {
  for (int rotation = 0; rotation < 4; ++rotation) {
    auto sensor = Component(0, plc::ComponentType::RING_SENSOR);
    sensor.size = {60, 70};
    sensor.rotation_quadrants = rotation;
    sensor.z_config = {3, 4, 3};
    sensor.z_state.current_height = 3;
    const ImVec2 outlet = plc::LocalToWorld(sensor, {30, 28});
    auto cylinder = Component(1, plc::ComponentType::CYLINDER);
    cylinder.size = {340, 66};
    cylinder.rotation_quadrants = rotation;
    cylinder.z_state.current_height = 3;
    const ImVec2 tip = plc::LocalToWorld(cylinder, {170, 25});
    cylinder.position = {outlet.x - tip.x, outlet.y - tip.y};
    std::vector<plc::PlacedComponent> parts{sensor, cylinder};
    for (int id = 2; id < 6; ++id) {
      auto piece = Component(id, plc::ComponentType::WORKPIECE_NONMETAL);
      piece.size = {30, 30};
      piece.position = {outlet.x - 15, outlet.y - 15};
      piece.z_state.current_height = 3;
      parts.push_back(piece);
    }
    for (int id = 2; id < 6; ++id) {
      plc_emulator::UpdateRingFeedWorkpieces(&parts);
      plc_emulator::UpdateSuctionWorkpieces(&parts);
      Check(parts[id].carry_state.owner_id == -1 &&
                parts[id].z_state.current_height == 3,
            "Ring sensor releases one bottom piece at its base layer");
      for (int waiting = id + 1; waiting < 6; ++waiting) {
        Check(parts[waiting].carry_state.owner_id == 0 &&
                  parts[waiting].z_state.current_height == 4,
              "All waiting stock stays at the second layer after storage update");
      }
      parts[1].internalStates["position"] = 0;
      plc_emulator::UpdateRingFeedWorkpieces(&parts);
      if (id < 5) {
        Check(parts[id + 1].carry_state.owner_id == 0,
              "Occupied outlet prevents another piece falling");
      }
      parts[id].position.x += 160;
      parts[1].internalStates["position"] = 160;
      plc_emulator::UpdateRingFeedWorkpieces(&parts);
      if (id < 5) {
        Check(parts[id + 1].carry_state.owner_id == 0,
              "Extended pusher blocks gravity feed even when stopped");
        parts[1].internalStates["position"] = 0;
        parts[1].internalStates["velocity"] = -10;
        plc_emulator::UpdateRingFeedWorkpieces(&parts);
        Check(parts[id + 1].carry_state.owner_id == 0,
              "Moving pusher blocks gravity feed at its retracted endpoint");
      }
      parts[1].internalStates["position"] = 0;
      parts[1].internalStates["velocity"] = 0;
    }
  }
}

}  // namespace

void RunPhysicalRuntimeTests() {
  ClosedLoopCylinder();
  AirAndPowerFailures();
  ElectricalDevices();
  ConveyorTransport();
  ZLayerIsolation();
  IntegratedPneumaticParts();
  VacuumAndCarry();
  ServoAxes();
  BoxAlignment();
  StorageAndOptical();
  DrivenWarehouse();
  RingSensorFeed();
}
