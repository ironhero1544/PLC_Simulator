#include "plc_emulator/core/application.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include "plc_emulator/components/component_input_resolver.h"

namespace plc {
namespace {

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

LadderProgram TestLadder() {
  using Type = LadderInstructionType;
  const auto instruction = [](Type type, const char* address,
                              const char* preset = "") {
    LadderInstruction cell;
    cell.type = type;
    cell.address = address;
    cell.preset = preset;
    return cell;
  };
  const auto rung = [&instruction](const LadderInstruction& contact,
                                  const LadderInstruction& output) {
    Rung row;
    row.cells.assign(12, instruction(Type::HLINE, ""));
    row.cells.front() = contact;
    row.cells.back() = output;
    return row;
  };
  LadderProgram ladder;
  ladder.rungs.clear();
  const Rung drive = rung(instruction(Type::XIC, "X0"),
                         instruction(Type::OTE, "Y0"));
  ladder.rungs.push_back(drive);
  const Rung sensor = rung(instruction(Type::XIC, "X1"),
                          instruction(Type::OTE, "Y1"));
  ladder.rungs.push_back(sensor);
  const Rung timer = rung(instruction(Type::XIC, "X0"),
                         instruction(Type::TON, "T0", "K2"));
  ladder.rungs.push_back(timer);
  const Rung delay = rung(instruction(Type::XIC, "T0"),
                         instruction(Type::OTE, "Y2"));
  ladder.rungs.push_back(delay);
  Rung end;
  end.isEndRung = true;
  ladder.rungs.push_back(end);
  return ladder;
}

// SOURCE X inputs close to 0V; SINK Y outputs provide the 0V load return.
constexpr const char* kLayout = R"json({
  "electrical_config": {"plc_input_mode":"source", "plc_output_mode":"sink"},
  "components": [
    {"instance_id":0,"type":"PLC","x":300,"y":50},
    {"instance_id":1,"type":"POWER_SUPPLY","x":50,"y":50},
    {"instance_id":2,"type":"BUTTON_UNIT","x":50,"y":180},
    {"instance_id":3,"type":"FRL","x":50,"y":340},
    {"instance_id":4,"type":"VALVE_SINGLE","x":280,"y":340},
    {"instance_id":5,"type":"CYLINDER","x":470,"y":340},
    {"instance_id":6,"type":"LIMIT_SWITCH","x":450,"y":385},
    {"instance_id":7,"type":"TOWER_LAMP","x":700,"y":220}
  ],
  "wires": [
    {"id":0,"from_component_id":1,"from_port_id":1,"to_component_id":2,"to_port_id":10,"is_electric":true},
    {"id":1,"from_component_id":2,"from_port_id":11,"to_component_id":0,"to_port_id":0,"is_electric":true},
    {"id":2,"from_component_id":1,"from_port_id":0,"to_component_id":4,"to_port_id":0,"is_electric":true},
    {"id":3,"from_component_id":0,"from_port_id":16,"to_component_id":4,"to_port_id":1,"is_electric":true},
    {"id":4,"from_component_id":3,"from_port_id":0,"to_component_id":4,"to_port_id":2,"is_electric":false},
    {"id":5,"from_component_id":4,"from_port_id":3,"to_component_id":5,"to_port_id":0,"is_electric":false},
    {"id":6,"from_component_id":4,"from_port_id":4,"to_component_id":5,"to_port_id":1,"is_electric":false},
    {"id":7,"from_component_id":1,"from_port_id":1,"to_component_id":6,"to_port_id":0,"is_electric":true},
    {"id":8,"from_component_id":6,"from_port_id":1,"to_component_id":0,"to_port_id":1,"is_electric":true},
    {"id":9,"from_component_id":1,"from_port_id":0,"to_component_id":7,"to_port_id":0,"is_electric":true},
    {"id":10,"from_component_id":0,"from_port_id":17,"to_component_id":7,"to_port_id":1,"is_electric":true}
  ]
})json";

}  // namespace

// Accesses test seams only; all compile, scan, wiring and monitor code is
// production code. In wiring tests, neither X nor Y is assigned by the test.
class ApplicationRuntimeTest {
 public:
  static void Run() {
    Application app(false);
    Require(app.Initialize(), "Initialize complete application");
    try {
      ProgrammingMode& editor = *app.programming_mode_;
      editor.SetLadderProgram(TestLadder());
      Require(editor.CompileLadderToOpenPLC(), "Editor compile button path");
      Require(editor.IsConverted() && editor.IsUsingCompiledEngine(),
              "Editor must report successfully loaded compiled ladder");
      Require(!app.loaded_ladder_program_.rungs.empty(),
              "Editor compilation must load application runtime too");
      Monitor(&editor);
      Wiring(&app, &editor);
      app.Shutdown();
    } catch (...) {
      app.Shutdown();
      throw;
    }
  }

 private:
  static void Monitor(ProgrammingMode* editor) {
    editor->SetMonitorMode(true);
    editor->UpdateInputsFromSystem({{"X0", false}, {"X1", false}});
    MonitorTick(editor);
    Require(!editor->GetDeviceStates().at("Y0"), "Monitor initial output OFF");
    editor->UpdateInputsFromSystem({{"X0", true}});
    for (int i = 0; i < 30; ++i) {
      MonitorTick(editor);
    }
    Require(editor->GetDeviceStates().at("Y0") &&
                editor->GetDeviceStates().at("Y2"),
            "Compiled monitor coil and timer output ON");
    Require(editor->GetLadderProgram().rungs[0].cells[0].isActive &&
                editor->GetLadderProgram().rungs[0].cells.back().isActive,
            "Monitor contact and coil highlight must reflect execution");
    Require(editor->GetTimerStates().at("T0").done,
            "Monitor timer must display completion");
    editor->UpdateInputsFromSystem({{"X0", false}});
    MonitorTick(editor);
    Require(!editor->GetDeviceStates().at("Y0") &&
                !editor->GetDeviceStates().at("Y2") &&
                !editor->GetLadderProgram().rungs[0].cells.back().isActive,
            "Monitor output and visual coil must clear on input OFF");
    std::cout << "PASS application monitor: editor compile, X0/Y0, "
                 "T0 K2/Y2, contact/coil highlight, OFF reset\n";
  }

  static void MonitorTick(ProgrammingMode* editor) {
    editor->scan_time_initialized_ = true;
    editor->last_scan_time_ = std::chrono::steady_clock::now() -
                              std::chrono::milliseconds(10);
    editor->UpdateWithPlcState(false);
  }

  static void Wiring(Application* app, ProgrammingMode* editor) {
    Require(app->DeserializeLayoutJson(kLayout), "Load real wiring fixture");
    app->SyncLadderProgramFromProgrammingMode();
    app->compiled_plc_executor_->ResetMemory();
    app->CompileAndLoadLadderProgram();
    app->is_plc_running_ = true;
    app->current_mode_ = Mode::WIRING;
    for (int i = 0; i < 5; ++i) {
      WiringTick(app, editor);
    }
    Require(!app->GetPlcDeviceState("X0") && !app->GetPlcDeviceState("Y0"),
            "Open physical button must leave input and coil OFF");
    auto& button = app->placed_components_[2];
    auto& cylinder = app->placed_components_[5];
    component_input::SetButtonUnitPressed(&button, 2, true);
    for (int i = 0; i < 130; ++i) {
      WiringTick(app, editor);
    }
    Require(app->GetPlcDeviceState("X0") && app->GetPlcDeviceState("Y0"),
            "Physical button wiring must drive compiled X0/Y0");
    Require(cylinder.internalStates.at(state_keys::kPosition) > 145.0f,
            "Compiled coil must move wired cylinder to forward endpoint");
    Require(app->GetPlcDeviceState("X1") && app->GetPlcDeviceState("Y1"),
            "Physical endpoint switch wiring must drive compiled X1/Y1");
    Require(app->placed_components_[7].internalStates.at(state_keys::kLampRed)
                > 0.5f,
            "Compiled Y1 must light electrically wired tower lamp");
    Require(editor->GetDeviceStates().at("X1") &&
                editor->GetLadderProgram().rungs[1].cells.back().isActive,
            "Monitor must reflect external wiring runtime sensor and coil");
    component_input::SetButtonUnitPressed(&button, 2, false);
    for (int i = 0; i < 180; ++i) {
      WiringTick(app, editor);
    }
    Require(!app->GetPlcDeviceState("X0") && !app->GetPlcDeviceState("Y0") &&
                !app->GetPlcDeviceState("X1") && !app->GetPlcDeviceState("Y1"),
            "Button release and physical return must clear wired feedback");
    Require(cylinder.internalStates.at(state_keys::kPosition) < 1.0f,
            "Physical cylinder must return after compiled output turns OFF");
    for (int cycle = 0; cycle < 9; ++cycle) {
      component_input::SetButtonUnitPressed(&button, 2, true);
      for (int tick = 0; tick < 130; ++tick) {
        WiringTick(app, editor);
      }
      Require(app->GetPlcDeviceState("Y0") && app->GetPlcDeviceState("X1") &&
                  app->GetPlcDeviceState("Y1") &&
                  cylinder.internalStates.at(state_keys::kPosition) > 145.0f,
              "Repeated compiled cycle must reach wired forward sensor");
      component_input::SetButtonUnitPressed(&button, 2, false);
      for (int tick = 0; tick < 180; ++tick) {
        WiringTick(app, editor);
      }
      Require(!app->GetPlcDeviceState("Y0") && !app->GetPlcDeviceState("X1") &&
                  !app->GetPlcDeviceState("Y1") &&
                  cylinder.internalStates.at(state_keys::kPosition) < 1.0f,
              "Repeated compiled cycle must return and clear sensor feedback");
    }
    std::cout << "PASS application wiring: button -> electrical X0 -> "
                 "compiled Y0 -> valve -> hoses -> cylinder -> limit X1 -> "
                 "compiled Y1 -> wired lamp and monitor; 10 round trips\n";
  }

  static void WiringTick(Application* app, ProgrammingMode* editor) {
    app->physics_time_initialized_ = true;
    app->last_physics_time_ = std::chrono::steady_clock::now() -
                             std::chrono::milliseconds(10);
    app->Update();
    editor->UpdateWithPlcState(true);
  }
};

}  // namespace plc

int main() {
  try {
    plc::ApplicationRuntimeTest::Run();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL application runtime: " << error.what() << '\n';
    return 1;
  }
}
