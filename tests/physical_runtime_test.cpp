#include "plc_emulator/physics/pneumatic_simulation.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "plc_emulator/application_physics/physics_helpers.h"
#include "plc_emulator/components/component_input_resolver.h"
#include "plc_emulator/physics/box2d_simulation.h"
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
  wire.isElectric = false;
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

}  // namespace

void RunPhysicalRuntimeTests() {
  ClosedLoopCylinder();
  AirAndPowerFailures();
  ElectricalDevices();
  ConveyorTransport();
}
