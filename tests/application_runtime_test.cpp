#include "plc_emulator/programming/monitor_values.h"
#include "plc_emulator/components/storage_sensor_def.h"
#include "plc_emulator/physics/storage_sensor_simulation.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "plc_emulator/components/servo_def.h"
#include <fstream>
#include "nlohmann/json.hpp"
#include "plc_emulator/project/instruction_codec.h"
#include "plc_emulator/project/gx_csv_codec.h"
#include "plc_emulator/physics/servo_simulation.h"
#include "plc_emulator/physics/z_layer.h"
#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/core/render_layer.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
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
  static void VerifyExam3(const std::filesystem::path& folder,
                          bool vary_frame_time = false) {
    vary_frame_time_ = vary_frame_time;
    Application app(false);
    Require(app.Initialize(), "Initialize exam verification application");
    try {
      const auto read = [](const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
      };
      Require(app.DeserializeLayoutJson(
                  read(folder / "automation-exam-03-answer.json")),
              "Load exam 3 wiring configuration");
      LadderProgram ladder;
      std::string error;
      Require(plc_emulator::programming::ImportGXCSV(
                  read(folder / "automation-exam-03-answer.csv"), &ladder,
                  &error),
              error.c_str());
      app.programming_mode_->SetLadderProgram(ladder);
      Require(app.programming_mode_->CompileLadderToOpenPLC(),
              "Compile exam 3 ladder");
      app.SyncLadderProgramFromProgrammingMode();
      app.CompileAndLoadLadderProgram();
      Require(app.compiled_plc_executor_ != nullptr,
              "Load exam 3 runtime program");
      app.compiled_plc_executor_->ResetMemory();
      app.is_plc_running_ = true;
      app.current_mode_ = Mode::WIRING;
      for (int tick = 0; tick < 250; ++tick)
        WiringTick(&app, app.programming_mode_.get());
      Require(app.SetHmiButton(2, 5, true), "JOG before the first metal cycle");
      for (int tick = 0; tick < 100; ++tick)
        WiringTick(&app, app.programming_mode_.get());
      app.SetHmiButton(2, 5, false);
      for (int tick = 0; tick < 50; ++tick)
        WiringTick(&app, app.programming_mode_.get());
      Require(app.SetHmiButton(2, 12, true) && app.SetHmiButton(2, 14, true),
              "HMI navigates and starts single cycle");
      for (int tick = 0; tick < 10; ++tick)
        WiringTick(&app, app.programming_mode_.get());
      app.SetHmiButton(2, 14, false);
      int previous_stage = -1;
      int processing_started = -1;
      bool motor_started_first = false;
      bool processing_hold_verified = false;
      for (int tick = 0; tick < 9000; ++tick) {
        WiringTick(&app, app.programming_mode_.get());
        const int stage = app.compiled_plc_executor_->GetMemory().D[10];
        const int applied_stage = app.compiled_plc_executor_->GetMemory().D[11];
        if (applied_stage == 1) {
          Require(std::abs(app.FindComponentById(14)->z_state.current_height -
                           3) < 0.01f,
                  "Metal cycle after JOG must restore home before feeding");
        }
        if (applied_stage == 33) {
          Require(app.GetPlcDeviceState("Y3") && !app.GetPlcDeviceState("Y5"),
                  "Motor must start before processing cylinder descends");
          motor_started_first = true;
        }
        // A long render frame can include both PLC scans around stage 33.
        // Observe the physical motor/head too, rather than requiring that
        // this one-scan stage coincides with the end of a render frame.
        if (applied_stage == 3 && app.GetPlcDeviceState("Y3") &&
            plc_emulator::ReadPneumaticState(*app.FindComponentById(12),
                                             "position") >= 29.9f) {
          motor_started_first = true;
        }
        if (applied_stage >= 3 && applied_stage <= 5)
          Require(app.GetPlcDeviceState("Y3"),
                  "Motor must keep running until processing cylinder rises");
        if (applied_stage == 4 && processing_started < 0) processing_started = tick;
        if (applied_stage == 5 && !processing_hold_verified) {
          Require(processing_started >= 0 && tick - processing_started >= 290,
                  "Processing holds down for the required three seconds");
          processing_hold_verified = true;
        }
        if (stage != previous_stage) {
          const auto* current_piece = app.FindComponentById(50);
          const auto* current_lift = app.FindComponentById(14);
          std::cout << "EXAM3 stage=" << stage << " tick=" << tick
                    << " piece=" << current_piece->position.x << ','
                    << current_piece->position.y
                    << " owner=" << current_piece->carry_state.owner_id
                    << " slot=" << current_piece->carry_state.slot_id
                    << " liftZ=" << current_lift->z_state.current_height
                    << " vacuum="
                    << current_lift->internalStates.at("vacuum_pressure")
                    << '\n';
          previous_stage = stage;
        }
        if (tick > 10 && stage == 0 && !app.GetPlcDeviceState("M201"))
          break;
      }
      Require(motor_started_first && processing_hold_verified,
              "Observe the original processing order and timed hold");
      const auto* piece = app.FindComponentById(50);
      std::cout << "EXAM3 piece=" << piece->position.x << ','
                << piece->position.y << " Z=" << piece->z_state.current_height
                << " owner=" << piece->carry_state.owner_id
                << " slot=" << piece->carry_state.slot_id << '\n';
      for (const int id : {10, 11, 12, 13, 14, 16})
        std::cout << "EXAM3 component=" << id << " position="
                  << app.FindComponentById(id)->internalStates["position"]
                  << '\n';
      Require(
          piece->carry_state.owner_id == 16 && piece->carry_state.slot_id == 2,
          "Single metal cycle stores the actual workpiece in warehouse slot 2");
      app.is_plc_running_ = false;
      Require(app.DeserializeLayoutJson(
                  read(folder / "automation-exam-03-continuous.json")),
              "Load four physical stock pieces for continuous operation");
      app.compiled_plc_executor_->ResetMemory();
      app.is_plc_running_ = true;
      for (int tick = 0; tick < 250; ++tick) {
        WiringTick(&app, app.programming_mode_.get());
      }
      Require(app.SetHmiButton(2, 12, true) && app.SetHmiButton(2, 15, true),
              "Start continuous operation from HMI");
      for (int tick = 0; tick < 10; ++tick)
        WiringTick(&app, app.programming_mode_.get());
      app.SetHmiButton(2, 15, false);
      int next_stock = 51;
      bool was_active = false;
      int continuous_stage = -1;
      for (int tick = 0; tick < 20000; ++tick) {
        WiringTick(&app, app.programming_mode_.get());
        const int stage = app.compiled_plc_executor_->GetMemory().D[10];
        if (stage != continuous_stage) {
          continuous_stage = stage;
          std::cout << "EXAM3 continuous stage=" << stage << " tick=" << tick << '\n';
          for (int id = 50; id <= 53; ++id) {
            const auto* stock = app.FindComponentById(id);
            std::cout << "STOCK " << id << " XY=" << stock->position.x << ','
                      << stock->position.y << " Z=" << stock->z_state.current_height
                      << " owner=" << stock->carry_state.owner_id
                      << " slot=" << stock->carry_state.slot_id << '\n';
          }
        }
        if (stage != 0)
          was_active = true;
        if (stage == 0 && was_active) {
          was_active = false;
          std::cout << "EXAM3 continuous completed piece=" << next_stock - 1
                    << " metal="
                    << app.compiled_plc_executor_->GetMemory().D[101] << '\n';
          if (!app.GetPlcDeviceState("M201"))
            break;
          Require(next_stock <= 53,
                  "Four preloaded magazine pieces complete without manual feed");
          ++next_stock;
        }
      }
      Require(!app.GetPlcDeviceState("M201") &&
                  app.compiled_plc_executor_->GetMemory().D[101] == 2,
              "Continuous mode automatically stops after two metal pieces");
      for (const int id : {50, 52}) {
        Require(
            app.FindComponentById(id)->internalStates["is_stuck_box"] > 0.5f,
            "Both nonmetal pieces physically reach the storage box");
      }
      Require(
          app.FindComponentById(51)->carry_state.owner_id == 16 &&
              app.FindComponentById(51)->carry_state.slot_id == 2 &&
              app.FindComponentById(53)->carry_state.owner_id == 16 &&
              app.FindComponentById(53)->carry_state.slot_id == 5,
          "Continuous metal pieces physically occupy warehouse slots 2 and 5");
      const auto ticks = [&app](int count) {
        for (int tick = 0; tick < count; ++tick) {
          WiringTick(&app, app.programming_mode_.get());
        }
      };
      const auto press = [&app, &ticks](int widget) {
        Require(app.SetHmiButton(2, widget, true), "Press active HMI button");
        // A 10 ms frame can fall between PLC scans. Hold a realistic button
        // press across several scans, including the release before a repeat.
        ticks(10);
        app.SetHmiButton(2, widget, false);
        ticks(10);
      };
      press(22);
      for (const auto [widget, speed] :
           {std::pair{7, 500}, std::pair{8, 1000}, std::pair{9, 1500}}) {
        press(widget);
        Require(app.compiled_plc_executor_->GetMemory().D[100] == speed,
                "HMI speed selection updates PLC register");
      }
      press(3);
      Require(app.GetPlcDeviceState("Y3"), "Manual motor starts");
      ticks(450);
      Require(app.GetPlcDeviceState("Y3"),
              "Manual motor remains on before five seconds");
      ticks(70);
      Require(!app.GetPlcDeviceState("Y3"),
              "Manual motor stops after five seconds");
      const double home_position =
          app.FindComponentById(14)->axis_state.position;
      Require(app.SetHmiButton(2, 5, true), "Hold manual JOG down");
      ticks(100);
      app.SetHmiButton(2, 5, false);
      ticks(50);
      Require(app.FindComponentById(14)->axis_state.position < home_position,
              "JOG physically lowers servo lift while held");
      press(6);
      ticks(600);
      Require(app.GetPlcDeviceState("M209") &&
                  std::abs(app.FindComponentById(14)->z_state.current_height -
                           3) < 0.01f,
              "Manual home returns physically and lights completion lamp");
      for (const int widget : {5, 4}) {
        Require(app.SetHmiButton(2, widget, true), "Hold JOG to travel limit");
        ticks(1000);
        app.SetHmiButton(2, widget, false);
        ticks(50);
        const auto* lift = app.FindComponentById(14);
        const auto* amplifier = app.FindComponentById(3);
        std::cout << "EXAM3 JOG limit widget=" << widget
                  << " position=" << lift->axis_state.position
                  << " target=" << amplifier->axis_state.target
                  << " alarm=" << amplifier->axis_state.alarm << '\n';
        Require(amplifier->axis_state.alarm == 0,
                "Holding manual JOG at either travel limit must not alarm");
      }
      press(6);
      ticks(600);
      Require(std::abs(app.FindComponentById(14)->z_state.current_height - 3) <
                  0.01f,
              "Home after full manual travel restores the physical origin");
      Require(app.SetHmiButton(2, 5, true), "Move lift away from home");
      ticks(100);
      app.SetHmiButton(2, 5, false);
      ticks(50);
      Require(app.FindComponentById(14)->z_state.current_height < 2.9f,
              "Manual JOG leaves the lift away from the physical origin");
      press(12);
      const auto feed_nonmetal = [&app] {
        const auto* stock = app.FindComponentById(50);
        const ImVec2 origin(stock->position.x, stock->position.y);
        app.BeginComponentMoveSession(
            app.BuildComponentMoveSession(50, origin));
        Application::CanvasInteractionCommand move;
        move.type =
            Application::CanvasInteractionCommand::Type::UpdateComponentMove;
        const auto* feed_sensor = app.FindComponentById(23);
        const ImVec2 center = LocalToWorld(*feed_sensor, {30, 28});
        move.world_pos = {center.x - 15, center.y - 15};
        app.ApplyCanvasInteractionCommand(move);
        app.EndComponentMoveSession();
      };
      const auto wait_stage = [&app, &ticks](int wanted) {
        for (int tick = 0; tick < 5000; ++tick) {
          if (app.compiled_plc_executor_->GetMemory().D[10] == wanted)
            return;
          ticks(1);
        }
        Require(false, "Reach requested physical sequence stage");
      };
      feed_nonmetal();
      press(15);
      Require(app.compiled_plc_executor_->GetMemory().D[10] == 0 &&
                  !app.GetPlcDeviceState("Y1"),
              "Automatic start after JOG must home before feeding stock");
      wait_stage(4);
      Require(std::abs(app.FindComponentById(14)->z_state.current_height - 3) <
                  0.01f,
              "Automatic start restores the physical origin after JOG");
      press(16);
      Require(app.GetPlcDeviceState("M201"),
              "End request lets current cycle finish");
      ticks(5000);
      Require(
          !app.GetPlcDeviceState("M201") &&
              app.FindComponentById(50)->internalStates["is_stuck_box"] > 0.5f,
          "End request stops after physical nonmetal box delivery");
      feed_nonmetal();
      press(15);
      wait_stage(4);
      press(17);
      Require(app.GetPlcDeviceState("M204") && !app.GetPlcDeviceState("Y3") &&
                  !app.GetPlcDeviceState("Y17"),
              "Pause immediately cuts motion and motor output");
      const float stopped_position =
          app.FindComponentById(12)->internalStates["position"];
      ticks(200);
      Require(app.GetPlcDeviceState("M204") &&
                  app.FindComponentById(12)->internalStates["position"] ==
                      stopped_position,
              "Pause holds physical cylinder before five second reset");
      ticks(1800);
      Require(!app.GetPlcDeviceState("M201") &&
                  !app.GetPlcDeviceState("M204") &&
                  !app.GetPlcDeviceState("M205") &&
                  app.compiled_plc_executor_->GetMemory().D[10] == 0,
              "Five second pause initializes the full physical plant without "
              "app reset");
      std::cout << "EXAM3 PASS single, continuous, manual, home, end, pause, "
                   "repeat\n";
      app.Shutdown();
    } catch (...) {
      app.Shutdown();
      throw;
    }
  }
  static void Run(bool render_parts = false, bool render_hmi = false,
                  bool render_list = false, bool render_feedback = false,
                  bool render_symbols = false, bool render_doctor = false) {
    Application app(false);
    Require(app.Initialize(), "Initialize complete application");
    try {
      if (render_parts || render_hmi || render_list || render_feedback || render_symbols || render_doctor) {
        RenderPartsPreview(&app, render_hmi, render_list, render_feedback, render_symbols, render_doctor);
        app.Shutdown();
        return;
      }
      ProjectRtlPackaging(&app);
      ProgrammingMode& editor = *app.programming_mode_;
      InstructionShortcuts(&editor);
      InstructionSpans(&editor);
      MonitorLookupBenchmark(&editor);
      MonitorOutputReset(&app, &editor);
      MonitorRegisterPresets(&app, &editor);
      NewContacts(&editor);
      NewContacts(&editor, true);
      editor.SetLadderProgram(TestLadder());
      Require(editor.CompileLadderToOpenPLC(), "Editor compile button path");
      Require(editor.IsConverted() && editor.IsUsingCompiledEngine(),
              "Editor must report successfully loaded compiled ladder");
      Require(!app.loaded_ladder_program_.rungs.empty(),
              "Editor compilation must load application runtime too");
      DoctorCompletion(&app);
      WireTool(&app);
      IntegratedSensors(&app);
      ServoWiring(&app);
      DcDriverWiring(&app);
      StorageSensors(&app);
      WarehouseMouseRemoval(&app);
      RenderLayers(&app);
      Monitor(&editor);
      Wiring(&app, &editor);
      PulseContactWiring(&app, &editor);
      PulseContactWiring(&app, &editor, true);
      app.Shutdown();
    } catch (...) {
      app.Shutdown();
      throw;
    }
  }

 private:
  static void RenderLayers(Application* app) {
    app->placed_components_.clear();
    app->wires_.clear();
    app->wiring_undo_stack_.clear();
    app->wiring_redo_stack_.clear();
    app->physics_paused_ = false;
    for (int id = 1; id <= 4; ++id) {
      PlacedComponent part{};
      part.instanceId = id;
      part.type = ComponentType::WORKPIECE_METAL;
      part.size = GetComponentDefinition(part.type)->size;
      part.position = {100, 100};
      part.z_order = id;
      part.z_config = {2, 2, 2};
      part.z_state.current_height = 2;
      part.internalStates["velocity"] = 37;
      part.selected = id == 2 || id == 3;
      app->placed_components_.push_back(part);
    }
    using Edit = Application::RenderLayerEdit;
    app->ApplyRenderLayerEdit(Edit::kGroup, 0, "Lift group");
    app->ApplyRenderLayerEdit(Edit::kOpacity, 0.4f);
    app->ApplyRenderLayerEdit(Edit::kVisibility, 1);
    Require(app->placed_components_[1].render_hidden &&
                app->placed_components_[2].render_group == "Lift group" &&
                std::abs(app->placed_components_[1].render_opacity - 0.4f) < 0.001f &&
                app->placed_components_[0].render_opacity == 1 &&
                app->placed_components_[1].z_state.current_height == 2 &&
                app->placed_components_[1].internalStates["velocity"] == 37 &&
                !app->physics_paused_,
            "Bulk display edits leave physics and unselected parts unchanged");
    app->UndoWiringState();
    Require(!app->placed_components_[1].render_hidden,
            "Visibility edit can be undone");
    app->RedoWiringState();
    Require(app->placed_components_[1].render_hidden,
            "Visibility edit can be redone");
    app->ApplyRenderLayerEdit(Edit::kVisibility, 0);
    app->MoveSelectedRenderLayers(1);
    Require(app->placed_components_[1].z_order == 2 &&
                app->placed_components_[2].z_order == 3 &&
                app->placed_components_[3].z_order == 1,
            "Move up preserves the selected block's internal order");
    app->MoveSelectedRenderLayers(-2);
    Require(app->placed_components_[1].z_order == 0 &&
                app->placed_components_[2].z_order == 1,
            "Send selected group behind all other parts");
    app->MoveSelectedRenderLayers(0, 1);
    Require(app->placed_components_[1].z_order == 1 &&
                app->placed_components_[2].z_order == 2 &&
                app->next_z_order_ == 4,
            "Drag-drop moves the whole selection above its target");
    plc_emulator::SortRenderLayerIndices(app->placed_components_,
                                         &app->render_layer_order_);
    app->render_layer_anchor_id_ = 1;
    app->SelectRenderLayerRow(3, false, true);
    Require(app->placed_components_[0].selected &&
                app->placed_components_[1].selected &&
                app->placed_components_[2].selected &&
                !app->placed_components_[3].selected,
            "Shift selection follows render order rather than vector order");

    const auto saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(saved) &&
                app->placed_components_[1].render_group == "Lift group" &&
                std::abs(app->placed_components_[1].render_opacity - 0.4f) < 0.001f,
            "Project round trip retains grouping, opacity and order");
    auto legacy = nlohmann::json::parse(saved);
    for (auto& part : legacy["components"]) part.erase("render_layer");
    Require(app->DeserializeLayoutJson(legacy.dump()) &&
                app->placed_components_[1].render_opacity == 1 &&
                !app->placed_components_[1].render_hidden &&
                app->placed_components_[1].render_group.empty(),
            "Old projects load visible and opaque without groups");

    const auto panel_frame = [&](ImVec2 mouse, bool mouse_down) {
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::GetIO().AddMousePosEvent(mouse.x, mouse.y);
      ImGui::GetIO().AddMouseButtonEvent(0, mouse_down);
      ImGui::NewFrame();
      ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
      ImGui::SetNextWindowSize({600, 700}, ImGuiCond_Always);
      app->show_render_layer_panel_ = true;
      app->RenderLayerPanel();
      ImGui::Render();
    };
    panel_frame({-100, -100}, false);
    panel_frame({-100, -100}, false);
    ImGuiWindow* list_window = nullptr;
    for (auto* window : ImGui::GetCurrentContext()->Windows) {
      if (std::string(window->Name).find("RenderLayerList") != std::string::npos)
        list_window = window;
    }
    Require(list_window != nullptr, "Display layer list has a native child window");
    const ImVec2 first_row(list_window->DC.CursorStartPos.x + 8,
        list_window->DC.CursorStartPos.y + ImGui::GetTextLineHeight() / 2);
    const ImVec2 second_row(first_row.x,
        first_row.y + ImGui::GetTextLineHeightWithSpacing());
    panel_frame(first_row, false);
    panel_frame(first_row, true);
    panel_frame(second_row, true);
    panel_frame(second_row, false);
    Require(app->placed_components_[3].selected &&
                app->placed_components_[2].selected &&
                !app->placed_components_[1].selected,
            "Dragging native list selection gutter selects multiple rows");

    app->placed_components_.resize(1);
    auto& part = app->placed_components_[0];
    part.selected = false;
    app->camera_offset_ = {0, 0};
    app->camera_zoom_ = 1;
    app->canvas_top_left_ = {0, 0};
    app->canvas_size_ = {1000, 800};
    int opaque_vertices = 0;
    for (int mode = 0; mode < 3; ++mode) {
      part.render_opacity = mode == 1 ? 0.5f : 1;
      part.render_hidden = mode == 2;
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      ImGui::Begin("Layer render test");
      ImDrawList* list = ImGui::GetWindowDrawList();
      const int start = list->VtxBuffer.Size;
      app->RenderPlacedComponents(list);
      const int count = list->VtxBuffer.Size - start;
      if (mode == 0) {
        opaque_vertices = count;
        Require(count > 0, "Visible part emits vertices");
      } else if (mode == 1) {
        Require(count == opaque_vertices, "Opacity preserves geometry");
        for (int index = start; index < list->VtxBuffer.Size; ++index)
          Require((list->VtxBuffer[index].col >> IM_COL32_A_SHIFT) <= 127,
                  "Every part vertex uses requested opacity");
      } else {
        Require(count == 0, "Hidden component emits no canvas vertices");
      }
      ImGui::End();
      app->show_render_layer_panel_ = true;
      app->RenderLayerPanel();
      ImGui::Render();
    }
    app->show_render_layer_panel_ = false;
    std::cout << "PASS render layers: bulk edits, group reorder, undo/redo, "
                 "legacy/save and actual alpha/hidden rendering\n";
  }

  static void ProjectRtlPackaging(Application* app) {
    const size_t library_size = app->rtl_project_manager_->GetLibrary().size();
    const auto clean = app->project_file_manager_->LoadProjectPackage(
        "docs/examples/automation-exam-03.plcproj");
    Require(clean.success && clean.rtlArtifacts.empty() &&
                nlohmann::json::parse(clean.rtlLibraryJson)["entries"].empty(),
            "Published exam example contains no unrelated RTL payload");
    const std::string legacy_path = "tmp/rtl-legacy-scope.plcproj";
    const std::string legacy_library = R"({"entries":[{
      "module_id":"unrelated_test","display_name":"RTL Test",
      "top_module":"test","component_enabled":true,
      "source_files":[{"path":"test.v","content":"module test; endmodule"}],
      "ports":[]}]})";
    const auto legacy = app->project_file_manager_->SaveProjectPackage(
        clean.program, clean.layoutJson, legacy_library,
        {{"unrelated_test", {{"rtl_worker.exe", "Unused regression payload"}}}},
        legacy_path, "Legacy RTL scope regression");
    Require(legacy.success, "Generate legacy fixture outside published examples");
    for (int repeat = 0; repeat < 2; ++repeat) {
      Require(app->LoadProjectPackage(legacy_path),
              "Load legacy package containing unrelated global RTL");
      Require(app->rtl_project_manager_->GetLibrary().size() == library_size,
              "Legacy project must not import unrelated RTL Test or clones");
    }
    const std::string path = "tmp/rtl-project-scope.plcproj";
    Require(app->SaveProjectPackage(path, "RTL scope regression"),
            "Save project without RTL components");
    const auto package = app->project_file_manager_->LoadProjectPackage(path);
    Require(package.success && package.rtlArtifacts.empty() &&
                nlohmann::json::parse(package.rtlLibraryJson)["entries"].empty(),
            "Project without RTL must exclude global workers and library");
  }

  static void DoctorCompletion(Application* app) {
    using namespace plc_emulator;
    app->show_rtl_toolchain_panel_ = false;
    app->rtl_doctor_pending_action_ = WslAction::kStop;
    app->rtl_doctor_action_future_ = std::async(std::launch::async, [] {
      return WslProcessResult{true, false, 0, "", ""};
    });
    app->rtl_doctor_action_future_.wait();
    app->RenderRtlToolchainPanel();
    Require(!app->rtl_doctor_action_future_.valid() &&
                app->rtl_toolchain_status_valid_,
            "Closing Doctor must still complete stop without probing WSL");
    std::cout << "PASS Tool Doctor closed-panel completion without WSL restart\n";
  }

  static void MonitorLookupBenchmark(ProgrammingMode* editor) {
    LadderProgram ladder;
    ladder.rungs.clear();
    for (int row = 0; row < 300; ++row) {
      const std::string output = "OUT M" + std::to_string(row);
      ladder.rungs.push_back(ContactRow({"LD X0"}, output.c_str()));
    }
    Rung end;
    end.isEndRung = true;
    ladder.rungs.push_back(end);
    editor->SetLadderProgram(ladder);
    Require(editor->CompileLadderToOpenPLC(), "Compile monitor benchmark");
    auto* executor = editor->plc_executor_.get();
    executor->SetContinuousExecution(true, 10);
    executor->SetInput(0, true);
    Require(executor->ExecuteScanCycle().success, "Execute monitor benchmark");
    const auto start = std::chrono::steady_clock::now();
    int active = 0;
    for (int frame = 0; frame < 20; ++frame) {
      for (int row = 0; row < 300; ++row) {
        for (int cell = 0; cell < 12; ++cell)
          active += executor->GetCellPower(row, cell).value_or(false);
        Require(executor->GetCellPower(row, 0).value_or(false) &&
                    executor->GetCellPower(row, 11).value_or(false),
                "Indexed monitor contact and output agree with execution");
      }
    }
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    Require(active > 0 && !executor->GetCellPower(-1, 0).has_value(),
            "Monitor lookup handles active and missing cells");
    std::cout << "PASS monitor lookup benchmark: 300 rows, 20 frames, "
              << elapsed_ms << " ms\n";
  }

  static void MonitorOutputReset(Application* app, ProgrammingMode* editor) {
    editor->monitor_external_plc_ = false;
    editor->SetMonitorMode(false);
    LadderProgram ladder;
    Rung end;
    end.isEndRung = true;
    ladder.rungs = {ContactRow({"LD X0"}, "OUT Y6"), end};
    editor->SetLadderProgram(ladder);
    Require(editor->CompileLadderToOpenPLC(), "Compile old monitor output");
    editor->SetMonitorMode(true);
    editor->SetDeviceState("X0", true);
    MonitorTick(editor);
    Require(editor->GetDeviceState("Y6"), "Old fixture turns Y6 on");
    editor->plc_executor_->SetDeviceState("Y377", true);
    editor->SetMonitorMode(true);
    Require(editor->GetDeviceState("Y6"),
            "Repeated monitor selection must not reset an active session");
    editor->SetMonitorMode(false);
    Require(!editor->GetDeviceState("Y6") &&
                !editor->plc_executor_->GetDeviceState("Y6") &&
                !editor->plc_executor_->GetDeviceState("Y377"),
            "Leaving monitor clears displayed and executor outputs");
    editor->SetMonitorMode(true);
    MonitorTick(editor);
    Require(editor->GetDeviceState("Y6"),
            "Current ladder may turn its output on again from active input");
    ladder.rungs = {ContactRow({"LD X0"}, "OUT Y0"), end};
    editor->SetLadderProgram(ladder);
    Require(editor->CompileLadderToOpenPLC(), "Compile replacement monitor");
    Require(!editor->GetDeviceState("Y6") &&
                !editor->plc_executor_->GetDeviceState("Y6"),
            "New program must discard removed output before first scan");
    MonitorTick(editor);
    Require(editor->GetDeviceState("Y0") && !editor->GetDeviceState("Y6"),
            "Replacement drives only its own output");
    app->compiled_plc_executor_->SetDeviceState("Y6", true);
    editor->monitor_external_plc_ = true;
    editor->SetMonitorMode(false);
    editor->SetMonitorMode(true);
    Require(app->compiled_plc_executor_->GetDeviceState("Y6"),
            "Monitor transitions must not clear physical PLC outputs");
    editor->UpdateWithPlcState(false);
    Require(!editor->plc_executor_->GetDeviceState("Y6"),
            "Returning from physical runtime discards stale local output");
    editor->SetDeviceState("X0", false);
    editor->SetMonitorMode(false);
    std::cout << "PASS monitor output reset: mode transitions, replacement "
                 "compile, Y377 and physical runtime isolation\n";
  }

  static void MonitorRegisterPresets(Application* app, ProgrammingMode* editor) {
    LadderProgram ladder;
    ladder.rungs = {ContactRow({"LD X0"}, "MOV K3 D1"),
                    ContactRow({"LD X1"}, "MOV K7 D1"),
                    ContactRow({"LD X2"}, "OUT T200 D1"),
                    ContactRow({"LD X3"}, "OUT C1 D1"),
                    ContactRow({"LD X4"}, "MOV K9 D1"),
                    ContactRow({"LD X5"}, "MOV K0 D1")};
    Rung end;
    end.isEndRung = true;
    ladder.rungs.push_back(end);
    editor->SetLadderProgram(ladder);
    Require(editor->CompileLadderToOpenPLC(), "Compile D preset monitor fixture");
    editor->SetMonitorMode(true);
    for (bool external : {false, true}) {
      auto* executor = external ? app->compiled_plc_executor_.get()
                                : editor->plc_executor_.get();
      executor->ResetMemory();
      executor->SetContinuousExecution(true, 10);
      for (int input = 0; input < 5; ++input) executor->SetInput(input, false);
      executor->SetInput(5, true);
      Require(executor->ExecuteScanCycle().success, "Initialize retained D word");
      executor->SetInput(5, false);
      executor->SetInput(3, true);
      Require(executor->ExecuteScanCycle().success, "Execute zero D preset scan");
      Require(executor->GetCounterPreset(1) == 1 &&
                  plc_emulator::programming::FormatTimerCounterMonitorValues(
                      editor->ladder_program_.rungs[3].cells.back(), *executor)
                      .find("D1=0") != std::string::npos,
              "Blue D monitor shows zero despite effective counter threshold one");
      for (int preset : {3, 7}) {
        executor->SetInput(0, preset == 3);
        executor->SetInput(1, preset == 7);
        executor->SetInput(2, true);
        executor->SetInput(3, true);
        Require(executor->ExecuteScanCycle().success, "Execute D preset scan");
        if (external) editor->UpdateWithPlcState(true);
        else editor->SyncOpenPLCToTimersCounters();
        Require(editor->GetTimerStates().at("T200").preset == preset &&
                    editor->GetCounterStates().at("C1").preset == preset,
                "Monitor resolves changing D presets in both execution modes");
        Require(editor->GetTimerStates().at("T200").done ==
                    executor->GetDeviceState("T200"),
                "Monitor timer completion uses runtime contact state");
      }
      executor->SetInput(4, true);
      Require(executor->ExecuteScanCycle().success, "Write D preset after counter");
      Require(executor->GetCounterPreset(1) == 7 &&
                  plc_emulator::programming::FormatTimerCounterMonitorValues(
                      editor->ladder_program_.rungs[3].cells.back(), *executor)
                      .find("D1=9") != std::string::npos &&
                  plc_emulator::programming::FormatTimerCounterMonitorValues(
                      editor->ladder_program_.rungs[2].cells.back(), *executor)
                      .find("D1=9") != std::string::npos,
              "Blue monitor shows live D operand after later rung writes it");
    }
    editor->SetMonitorMode(false);
    std::cout << "PASS D preset monitor: zero clamp, later rung write, "
                 "T200/C1 D1 changes 3 -> 7; "
                 "editor and external runtime\n";
  }

  static LadderInstruction ContactCell(const char* text) {
    LadderInstruction cell;
    std::string error;
    Require(plc_emulator::programming::ParseCellInput(
                text, LadderInstructionType::XIC, &cell, &error),
            "Parse contact test instruction");
    return cell;
  }

  static Rung ContactRow(std::initializer_list<const char*> contacts,
                         const char* output) {
    Rung row;
    row.cells.resize(12);
    for (auto& cell : row.cells) cell.type = LadderInstructionType::HLINE;
    int column = 0;
    for (const char* contact : contacts) row.cells[column++] = ContactCell(contact);
    row.cells.back() = output ? ContactCell(output) : LadderInstruction{};
    return row;
  }

  static LadderProgram LoadContactCSV(const LadderProgram& ladder,
                                      const char* name) {
    const std::filesystem::path path =
        std::filesystem::path(__FILE__).parent_path().parent_path() /
        "tmp" / (std::string(name) + ".csv");
    std::string content;
    std::string error;
    Require(plc_emulator::programming::ExportGXCSV(
                ladder, name, &content, &error),
            "Export contact fixture as GXWORKS2 UTF-16LE CSV");
    {
      std::ofstream file(path, std::ios::binary);
      file.write(content.data(), static_cast<std::streamsize>(content.size()));
      Require(file.good(), "Write contact CSV fixture");
    }
    ProjectFileManager files;
    ProjectFileManager::LoadResult loaded = files.LoadProject(path.string());
    Require(loaded.success, "Load contact CSV using production file loader");
    // Compile the imported grid, rather than a cached instruction program.
    LadderProgram grid;
    grid.rungs = std::move(loaded.program.rungs);
    grid.verticalConnections = std::move(loaded.program.verticalConnections);
    return grid;
  }

  static void NewContacts(ProgrammingMode* editor, bool csv = false) {
    LadderProgram ladder;
    ladder.rungs = {
        ContactRow({"LD X2"}, "MOV K9 D0"),
        ContactRow({"LD X3"}, "MOV K10 D0"),
        ContactRow({"LD X4"}, "MOV K11 D0"),
        ContactRow({"LD X5"}, "DMOV K32768 D10"),
        ContactRow({"LD X6"}, "DMOV K32767 D10"),
        ContactRow({"LD X7"}, "DMOV K-32769 D10"),
        ContactRow({"LDP X0"}, "OUT Y0"),
        ContactRow({"LDF X0"}, "OUT Y1"),
        ContactRow({"LD X1", "LDP X0"}, "OUT Y2"),
        ContactRow({"LD X1", "LDF X0"}, "OUT Y3"),
        ContactRow({"LD M10", "LDP M11"}, "OUT Y4"),
        ContactRow({"LDP X0", "LDP X0"}, "OUT Y5"),
        ContactRow({"LD X0", "MEP"}, "OUT Y6"),
        ContactRow({"LD X0", "MEF"}, "OUT Y7"),
        ContactRow({"LD X0", "INV"}, "OUT Y10"),
        ContactRow({"LD>= D0 K10"}, "OUT Y11"),
        ContactRow({"LD= D0 K10"}, "OUT Y12"),
        ContactRow({"DLD> D10 K32767"}, "OUT Y13"),
        ContactRow({"LDP X0"}, "OUT Y14"),
        ContactRow({"LDF X0"}, nullptr),
        ContactRow({"LD X0", "LD X1", "MEP"}, "OUT Y15"),
        ContactRow({"LD X0", "LD X1", "MEF"}, "OUT Y16"),
        ContactRow({"LD M10", "LDF M11"}, "OUT Y17"),
        ContactRow({"LD< D0 K10"}, "OUT Y20"),
        ContactRow({"LD<= D0 K10"}, "OUT Y21"),
        ContactRow({"LD> D0 K10"}, "OUT Y22"),
        ContactRow({"LD<> D0 K10"}, "OUT Y23"),
        ContactRow({"DLD<= D10 K32767"}, "OUT Y24"),
        ContactRow({"DLD< D10 K0"}, "OUT Y25")};
    VerticalConnection branch;
    branch.x = 1;
    branch.rungs = {18, 19};
    ladder.verticalConnections.push_back(branch);
    Rung end;
    end.isEndRung = true;
    ladder.rungs.push_back(end);
    if (csv) ladder = LoadContactCSV(ladder, "new-contacts-gxworks2");
    editor->SetLadderProgram(ladder);
    Require(editor->CompileLadderToOpenPLC(), "Compile native contact test ladder");
    editor->SetMonitorMode(true);
    editor->monitor_external_plc_ = false;
    auto* executor = editor->plc_executor_.get();
    executor->SetContinuousExecution(true, 10);
    const std::array<std::array<bool, 2>, 8> pattern = {{
        {false, false}, {true, false}, {true, true}, {true, true},
        {false, true}, {false, true}, {true, true}, {false, false}}};
    for (int run = 0; run < 2; ++run) {
      if (run == 1)
        Require(editor->CompileLadderToOpenPLC(), "Recompile contact test ladder");
      executor->ResetMemory();
      bool previous = false;
      bool previous_combined = false;
      bool previous_memory = false;
      for (int scan = 0; scan < 400; ++scan) {
        const bool input = pattern[scan % pattern.size()][0];
        const bool enable = pattern[scan % pattern.size()][1];
        const bool memory = scan % 5 < 3;
        const bool memory_enable = scan % 4 != 0;
        executor->SetInput(0, input);
        executor->SetInput(1, enable);
        for (int index = 2; index < 8; ++index) executor->SetInput(index, false);
        executor->SetInput(2 + scan % 3, true);
        executor->SetInput(5 + scan % 3, true);
        executor->SetDeviceState("M10", memory_enable);
        executor->SetDeviceState("M11", memory);
        Require(executor->ExecuteScanCycle().success, "Execute compiled contact scan");
        const bool rising = input && !previous;
        const bool falling = !input && previous;
        const bool combined = input && enable;
        const std::array<bool, 22> expected = {
            rising, falling, enable && rising, enable && falling,
            memory_enable && memory && !previous_memory, rising,
            rising, falling, !input, scan % 3 != 0, scan % 3 == 1,
            scan % 3 == 0, rising || falling,
            combined && !previous_combined, !combined && previous_combined,
            memory_enable && !memory && previous_memory,
            scan % 3 == 0, scan % 3 != 2, scan % 3 == 2, scan % 3 != 1,
            scan % 3 != 0, scan % 3 == 2};
        for (int output = 0; output < static_cast<int>(expected.size()); ++output) {
          if (executor->GetOutput(output) != expected[output]) {
            throw std::runtime_error("Contact output mismatch at scan " +
                std::to_string(scan) + " Y index " + std::to_string(output));
          }
        }
        editor->UpdateVisualActiveStates();
        const auto& rows = editor->ladder_program_.rungs;
        Require(rows[6].cells[0].isActive == rising &&
                    rows[7].cells[0].isActive == falling &&
                    rows[6].cells.back().isActive == rising &&
                    rows[7].cells.back().isActive == falling,
                "Monitor pulse contact and coil reflect the current scan only");
        Require(rows[15].cells[0].isActive == expected[9] &&
                    rows[17].cells[0].isActive == expected[11],
                "Monitor comparison contact reflects signed word evaluation");
        previous = input;
        previous_combined = combined;
        previous_memory = memory;
      }
    }
    for (int reset = 0; reset < 2; ++reset) {
      executor->ResetMemory();
      executor->SetInput(0, true);
      executor->SetInput(1, true);
      Require(executor->ExecuteScanCycle().success && executor->GetOutput(0) &&
                  executor->GetOutput(2) && executor->GetOutput(5) &&
                  executor->GetOutput(6) && !executor->GetOutput(1),
              "First scan with input ON produces rising edges after reset");
      Require(executor->ExecuteScanCycle().success && !executor->GetOutput(0) &&
                  !executor->GetOutput(2) && !executor->GetOutput(5) &&
                  !executor->GetOutput(6) && !executor->GetOutput(1),
              "Startup pulse does not repeat on held input");
    }
    executor->ResetMemory();
    executor->SetInput(0, false);
    Require(executor->ExecuteScanCycle().success && !executor->GetOutput(1) &&
                !executor->GetOutput(7),
            "Reset with input OFF does not generate a falling pulse");
    editor->SetMonitorMode(false);
    std::cout << "PASS new contacts: 805 " << (csv ? "CSV-loaded" : "native")
              << " ladder scans; LDP/LDF, "
                 "gated/repeated/internal/parallel edges, INV/MEP/MEF, "
                 "16/32-bit signed comparisons, reset/recompile and monitor\n";
  }

  static void PulseContactWiring(Application* app, ProgrammingMode* editor,
                                 bool csv = false) {
    LadderProgram ladder;
    ladder.rungs = {
        ContactRow({"LDP X0"}, "SET M20"),
        ContactRow({"LDF X0"}, "RST M20"),
        ContactRow({"LD M20"}, "OUT Y0"),
        ContactRow({"LD X1"}, "OUT Y1")};
    Rung end;
    end.isEndRung = true;
    ladder.rungs.push_back(end);
    if (csv) ladder = LoadContactCSV(ladder, "pulse-wiring-gxworks2");
    editor->SetLadderProgram(ladder);
    Require(editor->CompileLadderToOpenPLC(), "Compile pulse-driven physical ladder");
    Require(app->DeserializeLayoutJson(kLayout), "Load pulse physical wiring fixture");
    app->SyncLadderProgramFromProgrammingMode();
    app->compiled_plc_executor_->ResetMemory();
    app->CompileAndLoadLadderProgram();
    app->is_plc_running_ = true;
    app->current_mode_ = Mode::WIRING;
    for (int tick = 0; tick < 5; ++tick) WiringTick(app, editor);
    for (int cycle = 0; cycle < 3; ++cycle) {
      component_input::SetButtonUnitPressed(&app->placed_components_[2], 2, true);
      for (int tick = 0; tick < 130; ++tick) WiringTick(app, editor);
      Require(app->GetPlcDeviceState("M20") && app->GetPlcDeviceState("Y0") &&
                  app->GetPlcDeviceState("X1") && app->GetPlcDeviceState("Y1") &&
                  app->placed_components_[7].internalStates.at(state_keys::kLampRed)
                      > 0.5f &&
                  app->placed_components_[5].internalStates.at(state_keys::kPosition)
                      > 145.0f && !editor->ladder_program_.rungs[0].cells[0].isActive,
              "Physical rising edge latches valve; held button emits no more pulses");
      component_input::SetButtonUnitPressed(&app->placed_components_[2], 2, false);
      for (int tick = 0; tick < 180; ++tick) WiringTick(app, editor);
      Require(!app->GetPlcDeviceState("M20") && !app->GetPlcDeviceState("Y0") &&
                  !app->GetPlcDeviceState("X1") && !app->GetPlcDeviceState("Y1") &&
                  app->placed_components_[7].internalStates.at(state_keys::kLampRed)
                      < 0.5f &&
                  app->placed_components_[5].internalStates.at(state_keys::kPosition)
                      < 1.0f && !editor->ladder_program_.rungs[1].cells[0].isActive,
              "Physical falling edge resets latch and returns cylinder");
    }
    std::cout << "PASS pulse wiring " << (csv ? "CSV-loaded" : "native")
              << ": real button -> LDP SET / LDF RST -> "
                 "valve/cylinder/sensor/lamp; 3 round trips\n";
  }

  static void InstructionSpans(ProgrammingMode* editor) {
    using Type = LadderInstructionType;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    editor->SetLadderProgram(TestLadder());
    editor->ladder_column_width_ = 90;
    auto& cells = editor->ladder_program_.rungs[0].cells;
    Require(editor->GetCellSpan(0, 0).last == 0,
            "Input contact uses one column with readable typography");
    editor->SelectSingleCell(0, 0, false);
    Require(editor->selected_cell_ == 0,
            "Click input column selects the owning contact");
    std::string error;
    LadderInstruction dmov;
    LadderInstruction add;
    Require(plc_emulator::programming::ParseCellInput(
                "DMOV K2147483647 D200", Type::APPLICATION, &dmov, &error) &&
            plc_emulator::programming::ParseCellInput(
                "ADD D100 K32767 D102", Type::APPLICATION, &add, &error),
            "Parse long DMOV and shorter ADD");
    Require(editor->InstructionColumns(dmov) > editor->InstructionColumns(add),
            "Long DMOV reserves more width than ADD based on rendered text");
    Require(plc_emulator::programming::ParseCellInput(
                "MOV K32767 D100", Type::APPLICATION, &cells.back(), &error),
            "Parse spanning MOV");
    auto span = editor->GetCellSpan(0, 9);
    Require(span.first == 9 && span.last == 11 && span.anchor == 11,
            "MOV owns three grid columns with runtime anchor preserved");
    editor->SelectSingleCell(0, 9, false);
    Require(editor->selected_cell_ == 11, "Click covered column selects MOV");
    ProgrammingMode::CellSelectionBounds bounds;
    Require(editor->GetCellSelectionBounds(&bounds) && bounds.minCell == 9 &&
                bounds.maxCell == 11, "Selection encloses the whole instruction");
    ImGui::GetIO().KeyShift = false;
    ImGui::GetIO().KeyCtrl = false;
    ImGui::GetIO().WantTextInput = false;
    editor->HandleKeyboardInput(ImGuiKey_LeftArrow);
    Require(editor->selected_cell_ == 8, "Left skips the entire MOV footprint");
    editor->HandleKeyboardInput(ImGuiKey_RightArrow);
    Require(editor->selected_cell_ == 11, "Right enters MOV as one instruction");
    ImGui::GetIO().KeyCtrl = true;
    editor->HandleKeyboardInput(ImGuiKey_LeftArrow);
    Require(editor->ladder_program_.rungs[0].cells[9].type == Type::HLINE &&
                editor->ladder_program_.rungs[0].cells[10].type == Type::HLINE,
            "Ctrl+arrow preserves runtime wiring inside instruction footprint");
    editor->HandleKeyboardInput(ImGuiKey_RightArrow);
    ImGui::GetIO().KeyCtrl = false;
    editor->CopySelectedCellsToClipboard();
    Require(editor->cell_clipboard_.front().size() == 3 &&
                editor->cell_clipboard_.front().back().mnemonic == "MOV",
            "Copy captures the complete multi-column instruction");
    editor->DeleteSelectedCells();
    Require(editor->ladder_program_.rungs[0].cells.back().type == Type::EMPTY,
            "Delete covered region removes its owning instruction");
    editor->UndoProgrammingState();
    Require(editor->GetCellSpan(0, 9).anchor == 11 &&
                editor->ladder_program_.rungs[0].cells.back().mnemonic == "MOV",
            "Undo restores instruction and footprint together");
    editor->SelectSingleCell(0, 9, false);
    editor->PasteCellClipboard();
    Require(editor->CompileLadderToOpenPLC(), "Pasted footprint compiles unchanged");
    VerticalConnection branch;
    branch.x = 10;
    branch.rungs = {0, 1};
    editor->ladder_program_.verticalConnections.push_back(branch);
    Require(editor->GetInstructionSpan(0, 11).first == 10,
            "Instruction cannot absorb a vertical branch");
    editor->SelectSingleCell(0, 11, false);
    editor->HandleEditAction(Type::APPLICATION);
    std::snprintf(editor->temp_address_buffer_,
                  sizeof(editor->temp_address_buffer_), "DMOV K2147483647 D201");
    editor->ConfirmInstruction();
    Require(editor->show_address_popup_ && !editor->inline_input_error_.empty() &&
                editor->ladder_program_.rungs[0].cells.back().operands.front()
                    .immediate == 32767,
            "Collision rejects insertion without overwriting the old command");
    editor->show_address_popup_ = false;
    editor->ladder_program_.verticalConnections.clear();
    editor->ladder_column_width_ = 25;
    span = editor->GetCellSpan(0, 0);
    Require(span.last > span.first, "Long contact label reserves extra columns");
    editor->ladder_column_width_ = 90;
    ImGui::EndFrame();
    std::cout << "PASS instruction spans: hit/selection, arrows, copy/paste, "
                 "delete/undo, branch collision and compile\n";
  }

  static void InstructionShortcuts(ProgrammingMode* editor) {
    using Type = LadderInstructionType;
    editor->SetLadderProgram(TestLadder());
    ImGuiIO& io = ImGui::GetIO();
    io.WantTextInput = false;
    const auto enter = [editor, &io](int key, bool shift, int row,
                                     const char* text) {
      editor->show_address_popup_ = false;
      editor->SelectSingleCell(row, 0, false);
      io.KeyShift = shift;
      editor->HandleKeyboardInput(key);
      editor->ExecutePendingAction();
      std::snprintf(editor->temp_address_buffer_,
                    sizeof(editor->temp_address_buffer_), "%s", text);
      editor->ConfirmInstruction();
      Require(!editor->show_address_popup_, "Shortcut instruction accepted");
    };
    enter(ImGuiKey_F7, true, 0, "X0");
    Require(editor->ladder_program_.rungs[0].cells[0].type ==
                Type::RISING_CONTACT && editor->selected_cell_ == 1,
            "Shift+F7 places LDP at cursor without jumping to coil");
    enter(ImGuiKey_F8, true, 1, "X0");
    Require(editor->ladder_program_.rungs[1].cells[0].type ==
                Type::FALLING_CONTACT,
            "Shift+F8 places LDF at cursor");
    enter(ImGuiKey_F8, false, 2, "MOV K10 D0");
    Require(editor->ladder_program_.rungs[2].cells.back().type ==
                Type::APPLICATION && editor->selected_cell_ == 11,
            "F8 accepts bracket instruction and selects its output cell");
    editor->show_address_popup_ = true;
    io.KeyShift = true;
    editor->HandleKeyboardInput(ImGuiKey_F7);
    Require(editor->pending_instruction_type_ == Type::RISING_CONTACT,
            "Open input supports switching to rising contact");
    editor->HandleKeyboardInput(ImGuiKey_F8);
    Require(editor->pending_instruction_type_ == Type::FALLING_CONTACT,
            "Open input supports switching to falling contact");
    io.KeyShift = false;
    editor->HandleKeyboardInput(ImGuiKey_F8);
    Require(editor->pending_instruction_type_ == Type::APPLICATION,
            "Open input supports switching to application instruction");
    editor->show_address_popup_ = false;
    const int end = static_cast<int>(editor->ladder_program_.rungs.size()) - 1;
    enter(ImGuiKey_F8, false, end, "PLS M10");
    Require(editor->selected_cell_ == 11 && !editor->rung_selection_mode_,
            "F8 on END selects inserted output instead of row header");
    enter(ImGuiKey_F5, false, editor->selected_rung_, "M8000");
    LadderInstruction cell;
    std::string error;
    Require(!plc_emulator::programming::ParseCellInput(
                "Y0", Type::APPLICATION, &cell, &error),
            "F8 requires mnemonic instead of silently creating OUT");
    Require(editor->CompileLadderToOpenPLC(), "Compile shortcut-created ladder");
    auto* executor = editor->plc_executor_.get();
    executor->ResetMemory();
    const auto scan = [executor](bool input, bool rising, bool falling) {
      executor->SetDeviceState("X0", input);
      Require(executor->ExecuteScanCycle().success, "Scan shortcut ladder");
      Require(executor->GetDeviceState("Y0") == rising &&
                  executor->GetDeviceState("Y1") == falling,
              "Pulse contacts execute only on their respective scan edges");
    };
    scan(false, false, false);
    scan(true, true, false);
    scan(true, false, false);
    scan(false, false, true);
    scan(false, false, false);
    Require(executor->GetWordValue(
                *plc_emulator::programming::ParseDeviceAddress("D0"), false) == 10,
            "F8 MOV executes in compiled ladder");
    std::cout << "PASS GX instruction shortcuts: F8, Shift+F7/F8, popup switch, "
                 "END insertion, compiled pulse edges and MOV\n";
  }

  static void RenderPartsPreview(Application* app, bool render_hmi, bool render_list,
                                 bool render_feedback, bool render_symbols,
                                 bool render_doctor) {
    (void)app;
    const int width = render_list ? 460 : render_hmi ? 1400 : 1200;
    const int height = render_list ? 900 : render_hmi ? 1000 : 1200;
    GLuint framebuffer = 0;
    GLuint texture = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "Offscreen native preview framebuffer");
    if (render_doctor) {
      app->show_rtl_toolchain_panel_ = true;
      app->rtl_doctor_profile_loaded_ = true;
      app->rtl_doctor_profile_ = {"Ubuntu", 2};
      app->rtl_toolchain_status_valid_ = true;
      app->rtl_toolchain_status_.doctor.profile = {"Ubuntu", 2};
      app->rtl_toolchain_status_.doctor.ready = true;
      app->rtl_toolchain_status_.doctor.distributions =
          {{"Ubuntu", "Running", 2, true}};
      app->rtl_toolchain_status_.doctor.checks = {
          {"WSL host", true, "Ubuntu Running 2"},
          {"WSL2 distribution", true, "Ubuntu (WSL2)"},
          {"Linux kernel", true, "WSL2 Linux kernel"},
          {"Verilator", true, "Verilator 5.020"},
          {"C++ compiler", true, "g++ 13.3"},
          {"MinGW Windows compiler", true, "MinGW-w64 GCC 13"},
          {"make", true, "GNU Make 4.3"}};
    }
    for (int frame = 0; frame < ((render_hmi || render_list || render_symbols || render_doctor) ? 3 : 1); ++frame) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::GetIO().DisplaySize = ImVec2(width, height);
    ImGui::GetIO().DisplayFramebufferScale = ImVec2(1, 1);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("Native component preview", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
    ImDrawList* list = ImGui::GetWindowDrawList();
    list->AddRectFilled(ImVec2(0, 0), ImVec2(width, height),
                         IM_COL32(250, 250, 250, 255));
    const auto part = [&](ComponentType type, ImVec2 origin, const char* label,
                          bool front = false) {
      const ComponentDefinition* definition = GetComponentDefinition(type);
      Require(definition != nullptr, "Preview uses registered native definition");
      PlacedComponent component{};
      component.type = type;
      component.size = definition->size;
      if (definition->InitDefaultState) definition->InitDefaultState(&component);
      component.internalStates["position"] = 100;
      list->AddText(ImVec2(origin.x, origin.y - 22),
                    IM_COL32(30, 30, 30, 255), label);
      if (front && plc_emulator::IsStorageSensorPart(type))
        plc_emulator::RenderStorageSensorFront(list, component, origin, 1, {});
      else if (front) plc_emulator::RenderServoFront(list, component, origin, 1);
      else definition->Render(list, component, origin, 1);
    };
    if (render_doctor) {
      // The Tool Doctor renders its own window below.
    } else if (render_symbols) {
      auto& editor = *app->programming_mode_;
      LadderProgram ladder = TestLadder();
      ladder.rungs.clear();
      const char* outputs[] = {"OUT Y0", "OUT Y1", "SET M0", "RST M0",
                               "OUT T0 K10", "OUT C0 K3", "MOV K10 D0",
                               "PLS M1", "PLF M2", "OUT T255 K32767",
                               "OUT C199 K32767", "DMOV K2147483647 D200",
                               "ADD D100 K32767 D102", "OUT Y3"};
      const char* contacts[] = {"LD X0", "LDI X1", "LDP M8000", "LDF M8001"};
      for (int row = 0; row < static_cast<int>(std::size(outputs)); ++row) {
        Rung rung;
        rung.cells.assign(12, {});
        for (int col = 1; col < 11; ++col)
          rung.cells[col].type = LadderInstructionType::HLINE;
        std::string error;
        Require(plc_emulator::programming::ParseCellInput(
                    row == 13 ? "LD>= D100 K32767" : contacts[row % 4],
                    LadderInstructionType::XIC, &rung.cells.front(), &error),
                "Parse preview pulse contact");
        Require(plc_emulator::programming::ParseCellInput(outputs[row],
                    LadderInstructionType::APPLICATION, &rung.cells.back(), &error),
                "Parse preview output instruction");
        ladder.rungs.push_back(rung);
      }
      Rung end;
      end.isEndRung = true;
      ladder.rungs.push_back(end);
      editor.SetLadderProgram(ladder);
      editor.RefreshColumnLayout();
      editor.SelectSingleCell(12, 9, false);
      editor.RenderLadderDiagram();
    } else if (render_feedback) {
      auto& editor = *app->programming_mode_;
      LadderProgram ladder;
      ladder.rungs = {ContactRow({"LD X0"}, "MOV K12 D1"),
                      ContactRow({"LD X0"}, "OUT T1 D1"),
                      ContactRow({"LD X0"}, "OUT C1 D1"),
                      ContactRow({"LD X0"}, "MUL C1 K10 D2"),
                      ContactRow({"LD X0"}, "ADD D1 K5 D3")};
      Rung end;
      end.isEndRung = true;
      ladder.rungs.push_back(end);
      editor.SetLadderProgram(ladder);
      Require(editor.CompileLadderToOpenPLC(), "Compile monitor preview");
      editor.SetMonitorMode(true);
      editor.monitor_external_plc_ = false;
      editor.plc_executor_->SetInput(0, true);
      editor.plc_executor_->SetContinuousExecution(true, 10);
      for (int scan = 0; scan < 30; ++scan)
        Require(editor.plc_executor_->ExecuteScanCycle().success,
                "Run monitor preview");
      editor.SyncOpenPLCToTimersCounters();
      editor.UpdateVisualActiveStates();
      ImGui::Columns(2);
      ImGui::SetColumnWidth(0, 950);
      editor.RefreshColumnLayout();
      editor.RenderLadderDiagram();
      ImGui::NextColumn();
      editor.RenderDeviceMonitor();
      ImGui::Columns(1);
    } else if (render_list) {
      app->component_list_filter_ = Application::ComponentListFilter::kAutomationExam;
      app->component_list_view_mode_ = Application::ComponentListViewMode::NAME;
      app->RenderComponentList();
    } else if (render_hmi) {
      app->physics_paused_ = true;
      app->hmi_edit_settings_ = {};
      auto& settings = app->hmi_edit_settings_;
      settings.widgets[3].position = {66, 125};
      settings.widgets[3].size = {150, 40};
      settings.widgets[3].text = "Position";
      settings.widgets[3].unit = "mm";
      settings.widgets[3].decimals = 1;
      HmiWidget title;
      title.type = HmiWidgetType::kText;
      title.text = "Process controls";
      title.position = {5, 5}; title.size = {170, 22};
      settings.widgets.push_back(title);
      HmiWidget lamp;
      lamp.type = HmiWidgetType::kLamp;
      lamp.text = "RUN"; lamp.address = "Y0";
      lamp.position = {180, 5}; lamp.size = {60, 25};
      settings.widgets.push_back(lamp);
      app->hmi_selected_widget_ = 5;
      ImGui::OpenPopup(TR("ui.hmi.editor", "HMI screen editor"));
      ImGui::SetNextWindowPos({200, 160}, ImGuiCond_Always);
      app->RenderHmiEditor(-1);
    } else {
    part(ComponentType::kDcMotorDriver, {40, 840}, "DC motor driver: voltage control");
    part(ComponentType::kDcMotor, {40, 45}, "DC motor");
    part(ComponentType::kServoMotor, {240, 45}, "Servo motor");
    part(ComponentType::kServoAmplifier, {430, 45}, "Servo amplifier");
    part(ComponentType::PLC, {430, 260}, "PLC: height reference");
    part(ComponentType::kServoLift, {700, 45}, "Lift: top view");
    part(ComponentType::kLiftSuction, {840, 45}, "Integrated: top view");
    part(ComponentType::kLiftSuction, {840, 345}, "Integrated: front view", true);
    part(ComponentType::kAcSupply, {40, 300}, "AC supply");
    part(ComponentType::kSensorCylinder, {40, 465}, "Sensor cylinder");
    part(ComponentType::kSuctionCylinder, {430, 465}, "Suction cylinder");
    part(ComponentType::kFiberAmplifier, {40, 650}, "Fiber amplifier: top");
    part(ComponentType::kFiberTransmitter, {40, 760}, "Fiber TX");
    part(ComponentType::kFiberReceiver, {180, 760}, "Fiber RX");
    part(ComponentType::kWarehouse, {350, 650}, "Warehouse: top");
    part(ComponentType::kWarehouse, {550, 620}, "Warehouse: front", true);
    part(ComponentType::kTouchPanel, {780, 700}, "Touch panel: 4:3 / automatic PLC");
    part(ComponentType::kDrivenWarehouse, {40, 1080},
         "Pneumatic warehouse: mounted ports");
    }
    ImGui::End();
    if (render_doctor) {
      ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(760, 700), ImGuiCond_Always);
      app->RenderRtlToolchainPanel();
    }
    ImGui::Render();
    }
    glViewport(0, 0, width, height);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    std::vector<unsigned char> pixels(width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream output(render_doctor ? "tmp/rtl-doctor-preview.ppm" : render_symbols ? "tmp/ladder-symbols-preview.ppm" : render_feedback ? "tmp/feedback-preview.ppm" : render_list ? "tmp/component-list-preview.ppm"
                                    : render_hmi ? "tmp/hmi-editor-preview.ppm"
                                    : "tmp/native-components-preview.ppm", std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row)
      output.write(reinterpret_cast<const char*>(pixels.data() + row * width * 3),
                    width * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &texture);
  }

  static void ClickHmiControl(Application* app, int component_id, int widget_id,
                              ImVec2 window_size = {450, 320}) {
    ImVec2 target{3000, 3000};
    for (int frame = 0; frame < 5; ++frame) {
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::GetIO().AddMousePosEvent(frame >= 2 ? target.x : 3000,
                                     frame >= 2 ? target.y : 3000);
      ImGui::GetIO().AddMouseButtonEvent(0, frame == 3);
      ImGui::NewFrame();
      ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
      ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
      ImGui::Begin("HMI input test", nullptr, ImGuiWindowFlags_NoSavedSettings);
      const ImVec2 start = ImGui::GetCursorScreenPos();
      const auto& widget = app->FindComponentById(component_id)->hmi_settings.widgets[widget_id];
      const float toolbar_height = ImGui::GetFrameHeightWithSpacing();
      ImVec2 available = ImGui::GetContentRegionAvail();
      available.y -= toolbar_height;
      const float scale = std::min(available.x / kHmiScreenWidth,
                                   available.y / kHmiScreenHeight);
      target = {
          start.x + (available.x - kHmiScreenWidth * scale) / 2 +
              (widget.position.x + widget.size.width / 2) * scale,
          start.y + toolbar_height +
              (available.y - kHmiScreenHeight * scale) / 2 +
              (widget.position.y + widget.size.height / 2) * scale};
      app->RenderStorageSensorControls(component_id);
      ImGui::End();
      ImGui::Render();
      if (frame >= 3)
        Require(app->GetPlcDeviceState(widget.address) == (frame == 3),
                "Rendered widget position accepts actual ImGui press/release");
    }
  }

  static void StorageSensors(Application* app) {
    Require(app->DeserializeLayoutJson(R"json({"components":[
      {"instance_id":400,"type":"SENSOR_CYLINDER","x":0,"y":0,"flip_x":true},
      {"instance_id":401,"type":"WAREHOUSE","x":10,"y":20}
    ]})json"), "Load warehouse indexing fixture");
    app->physics_paused_ = true;
    Require(app->ApplyWarehouseDrive(401, 400, 70) &&
                !app->ApplyWarehouseDrive(401, 999, 70) &&
                !app->ApplyWarehouseDrive(401, 400, -1),
            "Validate warehouse drive command");
    app->FindComponentById(400)->internalStates["position"] = 160;
    plc_emulator::UpdateWarehouseWorkpieces(&app->placed_components_, false);
    const auto indexed_saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(indexed_saved) &&
                app->FindComponentById(401)->warehouse_cylinder_id == 400 &&
                std::abs(app->FindComponentById(401)->position.x - 10) < 0.001,
            "Warehouse binding persists while runtime base movement resets on load");
    auto invalid_drive = nlohmann::json::parse(indexed_saved);
    invalid_drive["components"][1]["warehouse_drive"]["cylinder_id"] = 999;
    Require(!app->DeserializeLayoutJson(invalid_drive.dump()) &&
                app->FindComponentById(401)->warehouse_cylinder_id == 400,
            "Invalid warehouse binding rejects before replacing scene");
    Require(app->DeserializeLayoutJson(R"json({"components":[
      {"instance_id":200,"type":"PLC","x":0,"y":0},
      {"instance_id":201,"type":"POWER_SUPPLY","x":0,"y":0},
      {"instance_id":202,"type":"FIBER_AMPLIFIER","x":0,"y":0},
      {"instance_id":203,"type":"FIBER_TRANSMITTER","x":100,"y":100},
      {"instance_id":204,"type":"FIBER_RECEIVER","x":300,"y":100,"rotation_quadrants":2},
      {"instance_id":205,"type":"WORKPIECE_NONMETAL","x":230,"y":95},
      {"instance_id":206,"type":"TOUCH_PANEL","x":0,"y":0,
        "hmi_settings":{"buttons":["M100","M101","M102"],"display":"D100","momentary":true}},
      {"instance_id":207,"type":"WAREHOUSE","x":0,"y":0}
    ],"electrical_config":{"plc_input_mode":"sink","plc_output_mode":"source"}})json"),
      "New storage/optical/HMI definitions load");
    const auto connect = [&](int from, int first, int to, int second) {
      const size_t before = app->wires_.size();
      app->StartWireConnection(from, first, {0, 0});
      app->CompleteWireConnection(to, second, {0, 0});
      const bool completed = app->wires_.size() == before + 1;
      if (!completed) app->is_connecting_ = false;
      return completed;
    };
    Require(connect(201, 0, 202, 0) && connect(201, 1, 202, 1) &&
            connect(202, 2, 200, 0) && connect(202, 3, 203, 0) &&
            connect(204, 0, 202, 4), "Power, SIG and bidirectional optical tool connections");
    Require(!connect(202, 3, 204, 0), "TX and RX channels cannot cross");
    app->FindComponentById(203)->flip_x = true;
    app->FindComponentById(204)->rotation_quadrants = 0;
    app->UpdatePortPositions();
    app->SimulateElectrical();
    Require(app->FindComponentById(202)->fiber_state.blocked &&
            app->GetPlcDeviceState("X0"), "Actual through-beam SIG reaches PLC X");
    app->FindComponentById(205)->z_state.current_height = 2;
    app->SimulateElectrical();
    Require(!app->GetPlcDeviceState("X0"), "Different height does not block optical ray");
    Require(connect(201, 0, 206, 0) && connect(201, 1, 206, 1), "HMI only needs DC power; PLC connection is automatic");
    Require(!connect(200, 32, 201, 1), "Removed PLC COM port cannot be wired");
    app->physics_paused_ = false;
    app->is_plc_running_ = true;
    app->SimulateElectrical();
    namespace model = plc_emulator::programming;
    model::ExecutionProgram hmi_program;
    std::string error;
    const std::vector<model::InstructionStatement> hmi_statements = {
        {"LD", {"M100"}}, {"OUT", {"Y0"}},
        {"MOV", {"K125", "D100"}}, {"END", {}}};
    Require(model::CompileStatements(hmi_statements, &hmi_program, &error) &&
        app->compiled_plc_executor_->LoadProgram(hmi_program), "Compile HMI command/display ladder");
    app->compiled_plc_executor_->SetContinuousExecution(true, 10);
    app->UpdateStorageSensorDevices();
    Require(app->FindComponentById(206)->hmi_state.online &&
            app->SetHmiButton(206, 0, true) && app->GetPlcDeviceState("M100"),
            "Powered online HMI commands the real PLC M device");
    for (int scan = 0; scan < 12; ++scan)
      Require(app->compiled_plc_executor_->ExecuteScanCycle().success, "Scan HMI command");
    app->UpdateStorageSensorDevices();
    Require(app->GetPlcDeviceState("Y0") &&
            app->FindComponentById(206)->hmi_state.value_valid &&
            app->FindComponentById(206)->hmi_state.display_value == 125,
            "HMI M command executes ladder and display reads real D value");
    auto customizable = app->FindComponentById(206)->hmi_settings;
    Require(customizable.widgets.size() == 4 && customizable.widgets[0].address == "M100",
            "Legacy three-button HMI migrates to four editable widgets");
    HmiWidget extra;
    extra.address = "M103";
    extra.text = "Stop";
    extra.position = {5, 5}; extra.size = {50, 25};
    customizable.widgets.push_back(extra);
    extra.type = HmiWidgetType::kLamp; extra.address = "Y0";
    extra.position = {65, 5}; extra.text = "Running";
    customizable.widgets.push_back(extra);
    extra.type = HmiWidgetType::kNumber; extra.address = "D100";
    extra.position = {125, 5}; extra.unit = "mm"; extra.decimals = 1;
    customizable.widgets.push_back(extra);
    extra.type = HmiWidgetType::kText; extra.address.clear();
    extra.position = {185, 5}; extra.text = "Process";
    customizable.widgets.push_back(extra);
    extra.type = HmiWidgetType::kButton; extra.address = "M104";
    extra.position = {5, 80}; extra.momentary = false;
    customizable.widgets.push_back(extra);
    app->physics_paused_ = true;
    Require(app->ApplyHmiSettings(206, customizable) && !app->GetPlcDeviceState("M100"),
            "Applying a custom screen releases old held commands");
    auto invalid_screen = customizable;
    invalid_screen.widgets[4].address = "X0";
    Require(!app->ApplyHmiSettings(206, invalid_screen), "HMI cannot write input addresses");
    invalid_screen = customizable; invalid_screen.widgets[4].position.x = 245;
    Require(!app->ApplyHmiSettings(206, invalid_screen), "Off-screen widgets rejected");
    invalid_screen = customizable; invalid_screen.widgets[4].address = "M100";
    Require(!app->ApplyHmiSettings(206, invalid_screen), "Duplicate command addresses rejected");
    app->physics_paused_ = false;
    Require(app->SetHmiButton(206, 0, true) && app->SetHmiButton(206, 4, true) &&
            !app->SetHmiButton(206, 5, true), "Extra button operates; lamps stay read-only");
    Require(app->SetHmiButton(206, 8, true) && app->GetPlcDeviceState("M104") &&
            !app->FindComponentById(206)->hmi_state.held[8] &&
            app->SetHmiButton(206, 8, false) && !app->GetPlcDeviceState("M104"),
            "Per-widget toggle controls M independently of momentary buttons");
    app->UpdateStorageSensorDevices();
    Require(app->FindComponentById(206)->hmi_state.values[5] &&
            app->FindComponentById(206)->hmi_state.number_valid[6] &&
            app->FindComponentById(206)->hmi_state.numbers[6] == 125,
            "Custom indicator and additional D display read actual PLC state");
    auto paged = customizable;
    paged.pages = {"Manual", "Automatic"};
    HmiWidget navigation;
    navigation.target_page = 1;
    navigation.text = "Automatic";
    paged.widgets.push_back(navigation);
    HmiWidget message;
    message.type = HmiWidgetType::kMessage;
    message.address = "D100";
    message.page = 1;
    message.visibility_address = "M109";
    message.messages = {"Stopped", "Continuous", "Paused"};
    paged.widgets.push_back(message);
    app->physics_paused_ = true;
    Require(app->ApplyHmiSettings(206, paged), "Apply two HMI screens");
    app->physics_paused_ = false;
    app->UpdateStorageSensorDevices();
    Require(app->SetHmiButton(206, 0, true) &&
                app->SetHmiButton(206, 9, true) &&
                app->FindComponentById(206)->hmi_state.active_page == 1 &&
                !app->GetPlcDeviceState("M100") &&
                !app->SetHmiButton(206, 0, true),
            "Screen navigation releases held bits and blocks hidden controls");
    app->UpdateStorageSensorDevices();
    Require(app->FindComponentById(206)->hmi_state.number_valid[10] &&
                app->FindComponentById(206)->hmi_state.hidden[10] &&
                app->FindComponentById(206)->hmi_state.numbers[10] == 125,
            "Message widget reads its actual PLC word");
    app->SetPlcDeviceState("M109", true);
    app->UpdateStorageSensorDevices();
    Require(!app->FindComponentById(206)->hmi_state.hidden[10],
            "Visibility follows the actual PLC bit without changing page");
    const std::string paged_saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(paged_saved) &&
                app->FindComponentById(206)->hmi_settings.pages == paged.pages &&
                app->FindComponentById(206)->hmi_settings.widgets[9].target_page == 1 &&
                app->FindComponentById(206)->hmi_settings.widgets[10].messages == message.messages &&
                app->FindComponentById(206)->hmi_state.active_page == 0,
            "HMI pages, navigation and messages persist; runtime starts on first screen");
    auto invalid_pages = paged;
    invalid_pages.widgets[9].target_page = 2;
    app->physics_paused_ = true;
    Require(!app->ApplyHmiSettings(206, invalid_pages), "Invalid screen target rejected");
    Require(app->ApplyHmiSettings(206, customizable), "Restore original HMI test screen");
    app->physics_paused_ = false;
    Require(app->SetHmiButton(206, 4, true), "Hold extra command before roundtrip");
    const std::string custom_saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(custom_saved) && !app->GetPlcDeviceState("M103") &&
            app->FindComponentById(206)->hmi_settings.widgets.size() == 9 &&
            app->FindComponentById(206)->hmi_settings.widgets[6].unit == "mm" &&
            app->FindComponentById(206)->hmi_settings.widgets[6].decimals == 1,
            "Custom layout and formatting roundtrip; reload releases held extra button");
    auto old_layout = nlohmann::json::parse(custom_saved);
    for (auto& part : old_layout["components"]) {
      if (part["instance_id"] != 206) continue;
      auto& old_screen = part["hmi_settings"];
      old_screen.erase("screen_width"); old_screen.erase("screen_height");
      for (auto& item : old_screen["widgets"]) {
        item["x"] = item["x"].get<float>() * 246 / kHmiScreenWidth;
        item["y"] = item["y"].get<float>() * 117 / kHmiScreenHeight;
        item["width"] = item["width"].get<float>() * 246 / kHmiScreenWidth;
        item["height"] = item["height"].get<float>() * 117 / kHmiScreenHeight;
        item["font_size"] = item["font_size"].get<float>() * 246 / kHmiScreenWidth;
      }
    }
    Require(app->DeserializeLayoutJson(old_layout.dump()) &&
            std::abs(app->FindComponentById(206)->hmi_settings.widgets[4].position.x - 5) < 0.01 &&
            std::abs(app->FindComponentById(206)->hmi_settings.widgets[4].size.width - 50) < 0.01,
            "Old wide-screen layouts migrate proportionally into the 4:3 screen");
    Require(app->DeserializeLayoutJson(custom_saved), "Restore current 4:3 screen");
    const auto* panel_definition = GetComponentDefinition(ComponentType::kTouchPanel);
    Require(panel_definition->size.width == 360 && panel_definition->size.height == 270 &&
            panel_definition->port_count == 2 && kHmiScreenWidth * 3 == kHmiScreenHeight * 4,
            "HMI body and screen are 4:3 with only power pins");
    app->UpdatePortPositions(); app->SimulateElectrical(); app->UpdateStorageSensorDevices();
    Require(app->SetHmiButton(206, 0, true) && app->SetHmiButton(206, 4, true),
            "Reloaded screen retains working commands");
    app->show_pneumatic_front_ = false;
    Require(app->OpenHmiEditor(206) && app->physics_paused_ &&
            !app->show_pneumatic_front_ && !app->GetPlcDeviceState("M103") &&
            app->hmi_editor_component_id_ == 206 && app->hmi_editor_open_pending_,
            "Direct HMI editor pauses and releases buttons without opening front view");
    Require(!app->OpenHmiEditor(200), "Screen edit applies only to an HMI");
    app->CloseHmiEditor();
    Require(!app->physics_paused_ && !app->hmi_editor_open_pending_ &&
            app->hmi_editor_component_id_ == -1 && !app->physics_time_initialized_,
            "Closing the HMI editor resumes with a fresh physics clock");
    ClickHmiControl(app, 206, 4);
    ClickHmiControl(app, 206, 4, {800, 400});
    ClickHmiControl(app, 206, 4, {480, 700});
    auto duplicate_plc = *app->FindComponentById(200);
    duplicate_plc.instanceId = 209;
    app->placed_components_.push_back(duplicate_plc);
    app->UpdateStorageSensorDevices();
    Require(!app->FindComponentById(206)->hmi_state.online,
            "Automatic HMI connection never selects an ambiguous PLC");
    app->placed_components_.pop_back();
    app->UpdateStorageSensorDevices();
    const auto linked_plc = *app->FindComponentById(200);
    app->placed_components_.erase(std::remove_if(app->placed_components_.begin(),
        app->placed_components_.end(), [](const PlacedComponent& part) {
          return part.instanceId == 200;
        }), app->placed_components_.end());
    app->UpdateStorageSensorDevices();
    Require(!app->FindComponentById(206)->hmi_state.online &&
            !app->GetPlcDeviceState("M100") && !app->GetPlcDeviceState("M103") &&
            !app->SetHmiButton(206, 1, true),
            "Lost communications releases momentary input and blocks new writes");
    app->placed_components_.push_back(linked_plc);
    app->UpdatePortPositions();
    app->SimulateElectrical();
    app->UpdateStorageSensorDevices();
    Require(app->SetHmiButton(206, 1, true), "Press another momentary HMI button");
    app->wires_.erase(std::remove_if(app->wires_.begin(), app->wires_.end(),
        [](const Wire& wire) { return wire.fromComponentId == 201 &&
            wire.fromPortId == 0 && wire.toComponentId == 206; }), app->wires_.end());
    app->SimulateElectrical();
    app->UpdateStorageSensorDevices();
    Require(!app->GetPlcDeviceState("M101") &&
            !app->FindComponentById(206)->hmi_state.value_valid,
            "Power loss releases held button and invalidates display");
    app->physics_paused_ = true;
    FiberSettings fiber; fiber.dark_on = false;
    Require(app->ApplyFiberSettings(202, fiber), "Optical mode uses paused command");
    HmiSettings hmi;
    for (int i = 0; i < 3; ++i) hmi.widgets[i].address = "M" + std::to_string(110 + i);
    hmi.widgets[3].address = "D120";
    Require(app->ApplyHmiSettings(206, hmi), "Validated HMI mapping command");
    app->UndoWiringState();
    Require(app->FindComponentById(206)->hmi_settings.widgets[3].address == "D100" &&
            app->FindComponentById(206)->hmi_settings.widgets.size() == 9, "Undo panel mapping and custom layout");
    app->RedoWiringState();
    Require(app->FindComponentById(206)->hmi_settings.widgets[3].address == "D120" &&
            app->FindComponentById(206)->hmi_settings.widgets.size() == 4, "Redo panel mapping and widget removal");
    auto* stored_piece = app->FindComponentById(205);
    stored_piece->carry_state.owner_id = 207;
    stored_piece->carry_state.slot_id = 3;
    plc_emulator::UpdateWarehouseWorkpieces(&app->placed_components_);
    Require(!app->ApplyZLayerConfig(207, {2, 4, 4}), "Occupied warehouse cannot change height");
    app->ClearComponentSelection();
    app->FindComponentById(207)->selected = true;
    app->selected_component_id_ = 207;
    Require(app->BuildComponentMoveSession(207, {0, 0}).offsets.empty(),
            "Occupied warehouse cannot be dragged while holding slots");
    const std::string saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(saved) &&
            !app->FindComponentById(202)->fiber_settings.dark_on &&
            app->FindComponentById(206)->hmi_settings.widgets[3].address == "D120",
            "Optical mode and HMI mapping survive project save/load");
    Require(app->FindComponentById(205)->carry_state.slot_id == 3 &&
            app->FindComponentById(205)->carry_state.owner_id == 207 &&
            std::abs(app->FindComponentById(205)->z_state.current_height - 2) < 0.001,
            "Stored slot ID, owner and service height restored");
    Require(app->ApplyZLayerConfig(205, {3, 3, 3}) &&
            app->FindComponentById(205)->carry_state.slot_id == -1,
            "Manual stored-piece height command releases complete slot ownership");
    const std::string edited = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(edited) &&
            app->FindComponentById(205)->carry_state.owner_id == -1,
            "Released slot cannot leak an invalid owner into project save");
    Require(app->DeserializeLayoutJson(saved), "Restore storage fixture");
    std::string invalid = saved;
    invalid.replace(invalid.find("D120"), 4, "X120");
    Require(!app->DeserializeLayoutJson(invalid) &&
            app->FindComponentById(206)->hmi_settings.widgets[3].address == "D120",
            "Invalid mapping rejected before replacing scene");
    app->physics_paused_ = false;
  }

  static void DcDriverWiring(Application* app) {
    Require(app->DeserializeLayoutJson(R"json({
      "electrical_config":{"plc_input_mode":"sink","plc_output_mode":"source"},
      "components":[
        {"instance_id":9,"type":"PLC","x":0,"y":200},
        {"instance_id":10,"type":"POWER_SUPPLY","x":0,"y":0},
        {"instance_id":11,"type":"DC_MOTOR_DRIVER","x":150,"y":0},
        {"instance_id":13,"type":"DC_MOTOR","x":500,"y":0}
      ],"wires":[]})json"), "Load PLC, driver and motor PWM configuration");
    const auto connect = [&](int id, int port, int target, int target_port) {
      const size_t before = app->wires_.size();
      app->StartWireConnection(id, port, {});
      app->CompleteWireConnection(target, target_port, {});
      Require(app->wires_.size() == before + 1, "PLC PWM driver wiring");
    };
    connect(10, 0, 11, 0); connect(10, 1, 11, 1);
    connect(9, 20, 11, 2); connect(9, 21, 11, 3); connect(9, 22, 11, 4);
    connect(11, 7, 13, 0); connect(11, 8, 13, 1);
    connect(9, 16, 11, 5);
    namespace model = plc_emulator::programming;
    model::ExecutionProgram program;
    std::string error;
    const std::vector<model::InstructionStatement> pwm_statements = {
        {"LD", {"M200"}}, {"MOV", {"K10", "D200"}}, {"MOV", {"K20", "D201"}},
        {"LD", {"M201"}}, {"MOV", {"K5", "D200"}},
        {"LD", {"M202"}}, {"MOV", {"K0", "D200"}},
        {"LD", {"M203"}}, {"MOV", {"D201", "D200"}},
        {"LD", {"M204"}}, {"MOV", {"K10", "D200"}}, {"MOV", {"K40", "D201"}},
        {"LD", {"M100"}}, {"PWM", {"D200", "D201", "Y0"}},
        {"LD", {"M100"}}, {"OUT", {"Y4"}},
        {"LD", {"M101"}}, {"OUT", {"Y5"}},
        {"LD", {"M102"}}, {"OUT", {"Y6"}}, {"END", {}}};
    Require(model::CompileStatements(pwm_statements, &program, &error) &&
        app->compiled_plc_executor_->LoadProgram(program), "Compile actual PWM and DIR ladder");
    app->compiled_plc_executor_->SetContinuousExecution(true, 10);
    app->is_plc_running_ = true;
    const auto solve = [&] {
      for (int scan = 0; scan < 4; ++scan)
        Require(app->compiled_plc_executor_->ExecuteScanCycle().success, "Scan PWM control ladder");
      app->UpdatePortPositions(); app->SimulateElectrical();
      for (int tick = 0; tick < 240; ++tick)
        app->UpdateServoActuators(1.0f / 240);
    };
    app->SetPlcDeviceState("M100", true); app->SetPlcDeviceState("M200", true);
    solve();
    const double half_speed = app->FindComponentById(13)->axis_state.velocity;
    Require(app->port_voltages_.at({13, 0}) == 12 &&
            std::abs(half_speed - app->FindComponentById(13)->axis_settings.max_speed / 2) < 0.001,
            "PLC 50 percent PWM drives half speed without analog module");
    app->SetPlcDeviceState("M200", false); app->SetPlcDeviceState("M201", true);
    solve();
    Require(app->port_voltages_.at({13, 0}) == 6 &&
            std::abs(app->FindComponentById(13)->axis_state.velocity - half_speed / 2) < 0.001,
            "Running PLC D word change updates PWM duty and speed");
    app->SetPlcDeviceState("M201", false); app->SetPlcDeviceState("M204", true);
    solve();
    Require(app->port_voltages_.at({13, 0}) == 6,
            "Changing PWM frequency at fixed duty leaves speed unchanged");
    app->SetPlcDeviceState("M204", false); app->SetPlcDeviceState("M101", true);
    solve();
    Require(app->FindComponentById(13)->axis_state.velocity < 0 &&
            app->port_voltages_.at({13, 1}) == 6, "PLC DIR output reverses motor");
    app->SetPlcDeviceState("M102", true); solve();
    Require(app->FindComponentById(13)->axis_state.velocity == 0, "PLC BRK output stops motor");
    app->SetPlcDeviceState("M102", false); app->SetPlcDeviceState("M202", true);
    solve();
    Require(app->FindComponentById(13)->axis_state.velocity == 0, "Zero PWM duty stops motor");
    app->SetPlcDeviceState("M202", false); app->SetPlcDeviceState("M203", true);
    solve();
    Require(app->port_voltages_.at({13, 1}) == 24 &&
            std::abs(app->FindComponentById(13)->axis_state.velocity + half_speed * 2) < 0.001,
            "Full PWM duty reaches full reverse speed");
    app->wires_.pop_back(); solve();
    Require(app->FindComponentById(11)->axis_state.alarm != 0 &&
            app->FindComponentById(13)->axis_state.velocity == 0,
            "PWM wire loss stops motor without manual fallback");
    connect(9, 16, 11, 5); solve();
    const std::string saved = app->SerializeLayoutJson();
    Require(saved.find("dc_drive_voltage") == std::string::npos &&
            app->DeserializeLayoutJson(saved), "PWM wiring persists without manual voltage setting");
    app->is_plc_running_ = false; app->SimulateElectrical();
    for (int tick = 0; tick < 240; ++tick) app->UpdateServoActuators(1.0f / 240);
    Require(app->FindComponentById(13)->axis_state.velocity == 0, "PLC stop removes drive");
    app->is_plc_running_ = true;
  }

  static void WarehouseMouseRemoval(Application* app) {
    for (ComponentType type : {ComponentType::kWarehouse,
                               ComponentType::kDrivenWarehouse}) {
      for (int rotation = 0; rotation < 4; ++rotation) {
        app->placed_components_.clear();
        PlacedComponent warehouse{};
        warehouse.instanceId = 100;
        warehouse.type = type;
        warehouse.size = GetComponentDefinition(type)->size;
        warehouse.position = {200, 300};
        warehouse.rotation_quadrants = rotation;
        warehouse.z_config = {1, 3, 1};
        app->placed_components_.push_back(warehouse);
        for (int slot = 1; slot <= 6; ++slot) {
          PlacedComponent piece{};
          piece.instanceId = 100 + slot;
          piece.type = ComponentType::WORKPIECE_METAL;
          piece.size = {30, 30};
          piece.carry_state.owner_id = 100;
          piece.carry_state.slot_id = slot;
          app->placed_components_.push_back(piece);
        }
        plc_emulator::UpdateWarehouseWorkpieces(&app->placed_components_, false);
        for (float zoom : {0.25f, 1.0f, 3.0f}) {
          const ImVec2 origin = plc_emulator::WarehouseOverlayOrigin(warehouse, zoom);
          for (int slot = 1; slot <= 6; ++slot) {
            const ImVec2 point(
                origin.x + (45 + (slot - 1) % 2 * 70) *
                    plc_emulator::kWarehouseOverlayScale,
                origin.y + (38 + (2 - (slot - 1) / 2) * 65) *
                    plc_emulator::kWarehouseOverlayScale);
            const int index = plc_emulator::FindWarehouseOverlayWorkpiece(
                app->placed_components_, point, zoom);
            Require(index == slot, "Pick exact occupied slot at every zoom/rotation");
          }
        }
        const float height = app->placed_components_[4].z_state.current_height;
        Application::CanvasInteractionCommand command;
        command.type = Application::CanvasInteractionCommand::Type::StartComponentMove;
        command.component_id = 104;
        command.world_pos = {600, 400};
        app->ApplyCanvasInteractionCommand(command);
        command.type = Application::CanvasInteractionCommand::Type::UpdateComponentMove;
        command.world_pos = {900, 600};
        app->ApplyCanvasInteractionCommand(command);
        plc_emulator::UpdateWarehouseWorkpieces(&app->placed_components_);
        const auto& removed = app->placed_components_[4];
        Require(removed.carry_state.owner_id == -1 &&
                    removed.z_state.current_height == height &&
                    std::abs(GetComponentWorldCenter(removed).x - 900) < 0.01f &&
                    std::abs(GetComponentWorldCenter(removed).y - 600) < 0.01f,
                "Withdrawal frees slot, preserves height and follows cursor");
        command.type = Application::CanvasInteractionCommand::Type::EndComponentMove;
        app->ApplyCanvasInteractionCommand(command);
        plc_emulator::UpdateWarehouseWorkpieces(&app->placed_components_);
        Require(app->placed_components_[4].carry_state.owner_id == -1 &&
                    app->placed_components_[3].carry_state.owner_id == 100 &&
                    app->placed_components_[5].carry_state.owner_id == 100,
                "Mouse release leaves removed piece free and neighbors stored");
      }
    }
  }

  static void ServoWiring(Application* app) {
    constexpr const char* layout = R"json({
      "electrical_config":{"plc_input_mode":"source","plc_output_mode":"source"},
      "components":[
        {"instance_id":0,"type":"PLC","x":400,"y":0},
        {"instance_id":1,"type":"POWER_SUPPLY","x":0,"y":0},
        {"instance_id":2,"type":"AC_SUPPLY","x":0,"y":200},
        {"instance_id":3,"type":"SERVO_AMPLIFIER","x":200,"y":200},
        {"instance_id":4,"type":"LIFT_SUCTION","x":500,"y":200},
        {"instance_id":5,"type":"DC_MOTOR","x":0,"y":400},
        {"instance_id":6,"type":"SERVO_MOTOR","x":200,"y":400},
        {"instance_id":7,"type":"SERVO_LIFT","x":400,"y":400}
      ],"wires":[]
    })json";
    Require(app->DeserializeLayoutJson(layout), "Load six motor-system definitions");
    const auto connect = [&](int first, int port, int second, int target) {
      const size_t before = app->wires_.size();
      app->StartWireConnection(first, port, {});
      app->CompleteWireConnection(second, target, {});
      Require(app->wires_.size() == before + 1, "Real servo wire tool accepts role");
    };
    app->current_tool_ = ToolType::kWire;
    for (int phase = 0; phase < 4; ++phase) connect(2, phase, 3, phase);
    for (int phase = 0; phase < 3; ++phase) connect(3, 14 + phase, 4, 10 + phase);
    connect(3, 3, 4, 13);
    connect(3, 17, 4, 14);
    connect(1, 0, 3, 4);
    connect(1, 1, 3, 5);
    connect(1, 1, 3, 7);
    connect(1, 0, 3, 9);
    connect(1, 0, 3, 10);
    connect(0, 16, 3, 6);
    connect(0, 20, 3, 8);
    connect(3, 11, 0, 0);
    connect(3, 12, 0, 1);
    Require(!app->IsValidWireConnection(
        app->GetRuntimePortsForComponent(app->placed_components_[2])[0],
        app->GetRuntimePortsForComponent(app->placed_components_[1])[0]),
        "AC phase cannot connect to a DC output");
    namespace model = plc_emulator::programming;
    model::ExecutionProgram program;
    std::string error;
    Require(model::CompileStatements(
        {{"LD", {"M100"}}, {"DDRVI", {"K-1000", "K20000", "Y0", "Y4"}},
         {"END", {}}}, &program, &error), "Compile high-rate reverse positioning");
    Require(app->compiled_plc_executor_->LoadProgram(program), "Load servo program");
    app->compiled_plc_executor_->SetContinuousExecution(true, 10);
    app->UpdatePortPositions();
    app->SimulateElectrical();
    app->UpdateServoActuators(1.0f / 240);
    Require(app->placed_components_[3].axis_state.ready, "Actual wires enable servo");
    app->compiled_plc_executor_->SetMemory(100, true);
    for (int scan = 0; scan < 180; ++scan) {
      Require(app->compiled_plc_executor_->ExecuteScanCycle().success,
              "Run actual compiled motion scan");
      app->SimulateElectrical();
      for (int tick = 0; tick < 3; ++tick) app->UpdateServoActuators(1.0f / 240);
    }
    app->SimulateElectrical();
    Require(app->GetPlcDeviceState("X0") && app->GetPlcDeviceState("X1"),
            "READY and actual INP reach real PLC electrical inputs");
    Require(std::abs(app->placed_components_[4].z_state.current_height - 2.9f)
                < 0.001f,
            "Compiled 20kHz positioning reaches continuous lift height");
    Require(app->port_voltages_.at({3, 0}) < 0 &&
                app->port_voltages_.at({4, 10}) < 0,
            "AC and servo phases must never become fake DC logic voltages");
    app->physics_paused_ = true;
    AxisSettings settings = app->placed_components_[4].axis_settings;
    settings.lead_mm = 10;
    Require(app->ApplyAxisSettings(4, settings), "Paused axis command applies");
    app->UndoWiringState();
    Require(app->placed_components_[4].axis_settings.lead_mm == 5,
            "Axis setting Undo restores calibrated lead");
    app->RedoWiringState();
    Require(app->placed_components_[4].axis_settings.lead_mm == 10,
            "Axis setting Redo restores change");
    const std::string saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(saved) &&
                app->placed_components_[4].axis_settings.lead_mm == 10 &&
                app->placed_components_[4].axis_state.position == 10,
            "Axis settings save/load restores configured initial Z, not runtime");
    const size_t count = app->placed_components_.size();
    Require(!app->DeserializeLayoutJson(R"json({"components":[{"axis_settings":{
      "pulses_per_turn":1000,"lead_mm":0,"stroke_mm":100,
      "max_speed":10,"acceleration":20}}]})json") &&
      app->placed_components_.size() == count,
      "Invalid lead rejects file before replacing scene");
    app->physics_paused_ = false;
    std::cout << "PASS servo application: real wire roles, compiled 20kHz pulses, "
                 "Z and PLC READY/INP, DC isolation, settings Undo and save/load\n";
  }

  static void IntegratedSensors(Application* app) {
    constexpr const char* kSensors = R"json({
      "electrical_config":{"plc_input_mode":"source"},
      "components":[
        {"instance_id":0,"type":"PLC","x":400,"y":0},
        {"instance_id":1,"type":"POWER_SUPPLY","x":0,"y":0},
        {"instance_id":2,"type":"SENSOR_CYLINDER","x":0,"y":200},
        {"instance_id":3,"type":"SUCTION_CYLINDER","x":0,"y":300},
        {"instance_id":4,"type":"STOPPER_CYLINDER","x":0,"y":400},
        {"instance_id":5,"type":"VACUUM_GENERATOR","x":0,"y":500}
      ],
      "wires":[
        {"id":0,"from_component_id":1,"from_port_id":0,"to_component_id":2,"to_port_id":2,"wire_kind":"electric"},
        {"id":1,"from_component_id":1,"from_port_id":1,"to_component_id":2,"to_port_id":3,"wire_kind":"electric"},
        {"id":2,"from_component_id":2,"from_port_id":4,"to_component_id":0,"to_port_id":0,"wire_kind":"electric"},
        {"id":3,"from_component_id":2,"from_port_id":5,"to_component_id":0,"to_port_id":1,"wire_kind":"electric"}
      ]
    })json";
    Require(app->DeserializeLayoutJson(kSensors), "Load four new part types");
    Require(app->placed_components_.size() == 6 && app->wires_.size() == 4,
            "New parts and actual electrical ports must register");
    Require(app->GetRuntimePortsForComponent(app->placed_components_[2]).size()
                == 6 &&
            app->GetRuntimePortsForComponent(app->placed_components_[3]).size()
                == 8,
            "Integrated part pinouts are separate from legacy cylinder");
    const auto plc_ports = app->GetRuntimePortsForComponent(app->placed_components_[0]);
    Require(plc_ports.size() == 40 && plc_ports[16].role == "Y0" &&
                plc_ports[32].role == "X20" && plc_ports[39].role == "X27",
            "Eight extra inputs preserve original saved output terminal IDs");
    Wire extra_input{};
    extra_input.id = 100;
    extra_input.fromComponentId = 2;
    extra_input.fromPortId = 5;
    extra_input.toComponentId = 0;
    extra_input.toPortId = 40;
    app->wires_.push_back(extra_input);
    app->UpdatePortPositions();
    app->SimulateElectrical();
    Require(!app->GetPlcDeviceState("X0") && app->GetPlcDeviceState("X1") &&
                app->GetPlcDeviceState("X27"),
            "Powered retracted NPN sensor drives original and extended inputs");
    auto& cylinder = app->placed_components_[2];
    cylinder.internalStates["position"] = 80;
    app->SimulateElectrical();
    Require(!app->GetPlcDeviceState("X0") && !app->GetPlcDeviceState("X1"),
            "Middle stroke leaves both end sensors off");
    cylinder.internalStates["position"] = 160;
    app->SimulateElectrical();
    Require(app->GetPlcDeviceState("X0") && !app->GetPlcDeviceState("X1"),
            "Forward endpoint drives only FWD input");
    app->wires_.erase(app->wires_.begin() + 1);
    app->SimulateElectrical();
    Require(!app->GetPlcDeviceState("X0"),
            "Ground loss disables endpoint signal in same network solve");
    Require(app->DeserializeLayoutJson(kSensors), "Restore powered fixture");
    app->SetPlcInputMode(PlcInputMode::SINK, true);
    app->SimulateElectrical();
    Require(app->GetPlcDeviceState("X1"),
            "PNP conversion drives sink PLC through same output pin");
    const auto ports = app->GetRuntimePortsForComponent(
        app->placed_components_[3]);
    app->placed_components_[3].internalStates["position"] = 100;
    const auto moved = app->GetRuntimePortsForComponent(
        app->placed_components_[3]);
    Require(ports[6].relativePos.x - moved[6].relativePos.x == 100,
            "VAC wire anchor follows attached moving head");
    Require(!app->IsValidWireConnection(
                app->GetRuntimePortsForComponent(app->placed_components_[5])[1],
                ports[0]), "VAC cannot wire directly to a positive chamber");
    app->placed_components_[3].customLabel = "Hidden cylinder name";
    app->placed_components_[3].show_custom_label = false;
    const std::string saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(saved) &&
                app->placed_components_.size() == 6 && app->wires_.size() == 4 &&
                app->placed_components_[3].customLabel == "Hidden cylinder name" &&
                !app->placed_components_[3].show_custom_label,
            "All new part types and sensor mode round trip");
    app->physics_paused_ = true;
    Require(app->ApplyZLayerConfig(4, {4, 7, 7}),
            "Stopper Z range is editable while paused");
    Require(plc_emulator::ResolveContactHeight(app->placed_components_[4]) == 7,
            "Stopper initial height maps to retracted plate");
    app->physics_paused_ = false;
    std::cout << "PASS new parts: registry, actual NPN/PNP PLC feedback, "
                 "power loss, moving VAC anchor, role validation and save/load\n";
  }

  static void WireTool(Application* app) {
    Require(app->DeserializeLayoutJson(kLayout), "Load legacy wire fixture");
    Require(app->wires_.size() == 11, "Legacy wire types preserved");
    app->wires_.clear();
    app->current_tool_ = ToolType::kWire;
    app->StartWireConnection(3, 0, ImVec2(50, 340));
    Require(app->pending_wire_kind_ == WireKind::kPneumatic,
            "Pneumatic starting port determines connection kind");
    app->AddWayPoint(ImVec2(200, 300));
    app->CompleteWireConnection(1, 0, ImVec2(50, 50));
    Require(app->wires_.empty() && app->is_connecting_ &&
                app->current_way_points_.size() == 1,
            "Invalid destination preserves route and does not create a wire");
    app->CompleteWireConnection(4, 2, ImVec2(280, 340));
    Require(app->wires_.size() == 1 &&
                app->wires_[0].kind == WireKind::kPneumatic &&
                !app->is_connecting_, "Complete pneumatic connection");
    app->StartWireConnection(4, 2, ImVec2(280, 340));
    app->CompleteWireConnection(3, 0, ImVec2(50, 340));
    Require(app->wires_.size() == 1, "Reverse duplicate rejected");
    app->is_connecting_ = false;
    app->StartWireConnection(1, 0, ImVec2(50, 50));
    app->CompleteWireConnection(4, 0, ImVec2(280, 340));
    Require(app->wires_.size() == 2 &&
                app->wires_[1].kind == WireKind::kElectric,
            "Same tool automatically creates an electrical connection");
    app->UndoWiringState();
    Require(app->wires_.size() == 1, "Unified wire creation supports Undo");
    app->RedoWiringState();
    Require(app->wires_.size() == 2, "Unified wire creation supports Redo");
    app->placed_components_[5].z_config = {4.0f, 4.0f, 4.0f};
    const std::string saved = app->SerializeLayoutJson();
    Require(app->DeserializeLayoutJson(saved) && app->wires_.size() == 2 &&
                app->wires_[0].kind == WireKind::kPneumatic &&
                app->wires_[1].kind == WireKind::kElectric,
            "New explicit wire kinds round trip");
    Require(app->placed_components_[5].z_config.initial_height == 4.0f &&
                app->placed_components_[5].z_state.current_height == 4.0f,
            "Z configuration restores initial height");
    app->physics_paused_ = false;
    Require(!app->ApplyZLayerConfig(5, {5, 5, 5}),
            "Z edits require actual simulation pause");
    app->physics_paused_ = true;
    Require(app->ApplyZLayerConfig(5, {5, 5, 5}),
            "Paused Z command applies valid height");
    app->UndoWiringState();
    Require(app->placed_components_[5].z_state.current_height == 4,
            "Z edit Undo restores prior configuration and height");
    app->RedoWiringState();
    Require(app->placed_components_[5].z_state.current_height == 5,
            "Z edit Redo restores new height");
    app->physics_paused_ = false;
    const size_t before_invalid_load = app->placed_components_.size();
    Require(!app->DeserializeLayoutJson(
        R"({"components":[{"z_config":{"min_layer":2,"max_layer":1,"initial_height":1}}]})") &&
        app->placed_components_.size() == before_invalid_load,
        "Invalid Z range is rejected before mutating the existing scene");
    Require(saved.find("wire_kind") != std::string::npos &&
                saved.find("is_electric") == std::string::npos,
            "New schema has one authoritative wire kind");
    Port transmitter;
    transmitter.type = PortType::kOptical;
    transmitter.role = "OPTICAL_TX";
    Port head = transmitter;
    head.role = "OPTICAL_TX_HEAD";
    Require(app->IsValidWireConnection(transmitter, head) &&
                app->IsValidWireConnection(head, transmitter),
            "Optical role pairing works in both click directions");
    head.role = "OPTICAL_RX_HEAD";
    Require(!app->IsValidWireConnection(transmitter, head),
            "TX cannot connect to RX head");
    std::cout << "PASS unified wire tool: legacy/new save, auto kind, "
                 "invalid target, reverse duplicate, optical roles\n";
  }

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
    static int frame_index = 0;
    const int frame_ms = vary_frame_time_ ? (++frame_index % 2 ? 2 : 18) : 10;
    app->physics_time_initialized_ = true;
    app->last_physics_time_ = std::chrono::steady_clock::now() -
                             std::chrono::milliseconds(frame_ms);
    app->Update();
    editor->UpdateWithPlcState(true);
  }

 private:
  inline static bool vary_frame_time_ = false;
};

}  // namespace plc

int main(int argc, char** argv) {
  try {
    if (argc == 3 && (std::string(argv[1]) == "--verify-exam3" ||
                     std::string(argv[1]) == "--verify-exam3-jitter")) {
      plc::ApplicationRuntimeTest::VerifyExam3(
          argv[2], std::string(argv[1]) == "--verify-exam3-jitter");
      return 0;
    }
    plc::ApplicationRuntimeTest::Run(argc > 1 &&
        std::string(argv[1]) == "--render-parts",
        argc > 1 && std::string(argv[1]) == "--render-hmi",
        argc > 1 && std::string(argv[1]) == "--render-list",
        argc > 1 && std::string(argv[1]) == "--render-feedback",
        argc > 1 && std::string(argv[1]) == "--render-symbols",
        argc > 1 && std::string(argv[1]) == "--render-doctor");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL application runtime: " << error.what() << '\n';
    return 1;
  }
}


