// app_project.cpp
//
// Project/ladder load/save helpers.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/ld_to_ladder_converter.h"
#include "plc_emulator/project/openplc_compiler_integration.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace plc {
namespace {

const char* ComponentTypeToString(ComponentType type) {
  switch (type) {
    case ComponentType::PLC:
      return "PLC";
    case ComponentType::FRL:
      return "FRL";
    case ComponentType::MANIFOLD:
      return "MANIFOLD";
    case ComponentType::LIMIT_SWITCH:
      return "LIMIT_SWITCH";
    case ComponentType::SENSOR:
      return "SENSOR";
    case ComponentType::CYLINDER:
      return "CYLINDER";
    case ComponentType::VALVE_SINGLE:
      return "VALVE_SINGLE";
    case ComponentType::VALVE_DOUBLE:
      return "VALVE_DOUBLE";
    case ComponentType::BUTTON_UNIT:
      return "BUTTON_UNIT";
    case ComponentType::POWER_SUPPLY:
      return "POWER_SUPPLY";
    case ComponentType::WORKPIECE_METAL:
      return "WORKPIECE_METAL";
    case ComponentType::WORKPIECE_NONMETAL:
      return "WORKPIECE_NONMETAL";
    case ComponentType::RING_SENSOR:
      return "RING_SENSOR";
    case ComponentType::METER_VALVE:
      return "METER_VALVE";
    case ComponentType::INDUCTIVE_SENSOR:
      return "INDUCTIVE_SENSOR";
    case ComponentType::CONVEYOR:
      return "CONVEYOR";
    case ComponentType::PROCESSING_CYLINDER:
      return "PROCESSING_CYLINDER";
    case ComponentType::BOX:
      return "BOX";
    case ComponentType::TOWER_LAMP:
      return "TOWER_LAMP";
    case ComponentType::EMERGENCY_STOP:
      return "EMERGENCY_STOP";
    default:
      return "UNKNOWN";
  }
}

bool ComponentTypeFromString(const std::string& value, ComponentType* out) {
  if (!out) {
    return false;
  }
  if (value == "PLC") {
    *out = ComponentType::PLC;
  } else if (value == "FRL") {
    *out = ComponentType::FRL;
  } else if (value == "MANIFOLD") {
    *out = ComponentType::MANIFOLD;
  } else if (value == "LIMIT_SWITCH") {
    *out = ComponentType::LIMIT_SWITCH;
  } else if (value == "SENSOR") {
    *out = ComponentType::SENSOR;
  } else if (value == "CYLINDER") {
    *out = ComponentType::CYLINDER;
  } else if (value == "VALVE_SINGLE") {
    *out = ComponentType::VALVE_SINGLE;
  } else if (value == "VALVE_DOUBLE") {
    *out = ComponentType::VALVE_DOUBLE;
  } else if (value == "BUTTON_UNIT") {
    *out = ComponentType::BUTTON_UNIT;
  } else if (value == "POWER_SUPPLY") {
    *out = ComponentType::POWER_SUPPLY;
  } else if (value == "WORKPIECE_METAL") {
    *out = ComponentType::WORKPIECE_METAL;
  } else if (value == "WORKPIECE_NONMETAL") {
    *out = ComponentType::WORKPIECE_NONMETAL;
  } else if (value == "RING_SENSOR") {
    *out = ComponentType::RING_SENSOR;
  } else if (value == "METER_VALVE") {
    *out = ComponentType::METER_VALVE;
  } else if (value == "INDUCTIVE_SENSOR") {
    *out = ComponentType::INDUCTIVE_SENSOR;
  } else if (value == "CONVEYOR") {
    *out = ComponentType::CONVEYOR;
  } else if (value == "PROCESSING_CYLINDER") {
    *out = ComponentType::PROCESSING_CYLINDER;
  } else if (value == "BOX") {
    *out = ComponentType::BOX;
  } else if (value == "TOWER_LAMP") {
    *out = ComponentType::TOWER_LAMP;
  } else if (value == "EMERGENCY_STOP") {
    *out = ComponentType::EMERGENCY_STOP;
  } else {
    return false;
  }
  return true;
}

}  // namespace

LadderInstructionType Application::StringToInstructionType(

    const std::string& str) {

  if (str == "XIC")

    return LadderInstructionType::XIC;

  if (str == "XIO")

    return LadderInstructionType::XIO;

  if (str == "OTE")

    return LadderInstructionType::OTE;

  if (str == "SET")

    return LadderInstructionType::SET;

  if (str == "RST")

    return LadderInstructionType::RST;

  if (str == "TON")

    return LadderInstructionType::TON;

  if (str == "CTU")

    return LadderInstructionType::CTU;

  if (str == "RST_TMR_CTR")

    return LadderInstructionType::RST_TMR_CTR;

  if (str == "HLINE")

    return LadderInstructionType::HLINE;

  return LadderInstructionType::EMPTY;

}

void Application::SyncLadderProgramFromProgrammingMode() {

  if (!programming_mode_) {

    printf(

        "Error: ProgrammingMode is not initialized. Cannot sync ladder "

        "program.\n");

    return;
  
  }
  
  


  loaded_ladder_program_ = programming_mode_->GetLadderProgram();

  plc_device_states_ = programming_mode_->GetDeviceStates();

  plc_timer_states_ = programming_mode_->GetTimerStates();

  plc_counter_states_ = programming_mode_->GetCounterStates();



  if (simulator_core_) {

    simulator_core_->SyncFromProgrammingMode(programming_mode_.get());



    if (simulator_core_->UpdateIOMapping()) {
      printf("[INFO] I/O mapping updated from wiring connections.\n");
    }



    printf("[INFO] Data synchronized to PLCSimulatorCore as well.\n");

  }



  if (compiled_plc_executor_) {

    CompileAndLoadLadderProgram();

  }

}

bool Application::SaveLayout(const std::string& file_path) {
  json root;
  root["version"] = 1;
  root["next_instance_id"] = next_instance_id_;
  root["next_wire_id"] = next_wire_id_;
  root["next_z_order"] = next_z_order_;

  json components = json::array();
  for (const auto& comp : placed_components_) {
    json item;
    item["instance_id"] = comp.instanceId;
    item["type"] = ComponentTypeToString(comp.type);
    item["x"] = comp.position.x;
    item["y"] = comp.position.y;
    item["z_order"] = comp.z_order;
    item["rotation_quadrants"] = comp.rotation_quadrants;
    item["flip_x"] = comp.flip_x;
    item["flip_y"] = comp.flip_y;
    components.push_back(item);
  }
  root["components"] = components;

  json wires = json::array();
  for (const auto& wire : wires_) {
    json item;
    item["id"] = wire.id;
    item["from_component_id"] = wire.fromComponentId;
    item["from_port_id"] = wire.fromPortId;
    item["to_component_id"] = wire.toComponentId;
    item["to_port_id"] = wire.toPortId;
    item["is_electric"] = wire.isElectric;
    item["thickness"] = wire.thickness;
    item["is_tagged"] = wire.isTagged;
    item["tag_text"] = wire.tagText;
    item["tag_color_index"] = wire.tagColorIndex;

    json color;
    color["r"] = wire.color.r;
    color["g"] = wire.color.g;
    color["b"] = wire.color.b;
    color["a"] = wire.color.a;
    item["color"] = color;

    json waypoints = json::array();
    for (const auto& wp : wire.wayPoints) {
      json point;
      point["x"] = wp.x;
      point["y"] = wp.y;
      waypoints.push_back(point);
    }
    item["waypoints"] = waypoints;
    wires.push_back(item);
  }
  root["wires"] = wires;

  std::ofstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file << root.dump(2);
  return true;
}

bool Application::LoadLayout(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  json root = json::parse(buffer.str(), nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return false;
  }

  placed_components_.clear();
  wires_.clear();
  selected_component_id_ = -1;
  selected_wire_id_ = -1;
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;
  is_connecting_ = false;
  current_way_points_.clear();
  is_dragging_ = false;
  is_moving_component_ = false;

  int max_instance_id = -1;
  int max_wire_id = -1;
  int max_z_order = std::numeric_limits<int>::min();

  auto components_it = root.find("components");
  if (components_it != root.end() && components_it->is_array()) {
    for (const auto& item : *components_it) {
      if (!item.is_object()) {
        continue;
      }
      ComponentType type;
      std::string type_text = item.value("type", "");
      if (!ComponentTypeFromString(type_text, &type)) {
        continue;
      }
      const ComponentDefinition* def = GetComponentDefinition(type);
      if (!def) {
        continue;
      }

      PlacedComponent comp;
      comp.instanceId = item.value("instance_id", 0);
      comp.type = type;
      comp.position.x = item.value("x", 0.0f);
      comp.position.y = item.value("y", 0.0f);
      comp.size = def->size;
      comp.z_order = item.value("z_order",
                                static_cast<int>(placed_components_.size()));
      comp.rotation_quadrants = item.value("rotation_quadrants", 0);
      comp.flip_x = item.value("flip_x", false);
      comp.flip_y = item.value("flip_y", false);
      if (def->InitDefaultState) {
        def->InitDefaultState(&comp);
      }
      placed_components_.push_back(comp);

      max_instance_id = std::max(max_instance_id, comp.instanceId);
      max_z_order = std::max(max_z_order, comp.z_order);
    }
  }

  auto wires_it = root.find("wires");
  if (wires_it != root.end() && wires_it->is_array()) {
    for (const auto& item : *wires_it) {
      if (!item.is_object()) {
        continue;
      }
      Wire wire;
      wire.id = item.value("id", 0);
      wire.fromComponentId = item.value("from_component_id", 0);
      wire.fromPortId = item.value("from_port_id", 0);
      wire.toComponentId = item.value("to_component_id", 0);
      wire.toPortId = item.value("to_port_id", 0);
      wire.isElectric = item.value("is_electric", true);
      wire.thickness = item.value("thickness", wire.isElectric ? 2.0f : 3.0f);
      wire.tagText = item.value("tag_text", std::string());
      wire.isTagged = item.value("is_tagged", !wire.tagText.empty());
      wire.tagColorIndex = item.value("tag_color_index", 0);

      if (item.contains("color") && item["color"].is_object()) {
        const json& color = item["color"];
        wire.color.r = color.value("r", wire.isElectric ? 1.0f : 0.13f);
        wire.color.g = color.value("g", wire.isElectric ? 0.27f : 0.59f);
        wire.color.b = color.value("b", wire.isElectric ? 0.0f : 0.95f);
        wire.color.a = color.value("a", 1.0f);
      } else {
        wire.color = wire.isElectric ? Color{1.0f, 0.27f, 0.0f, 1.0f}
                                     : Color{0.13f, 0.59f, 0.95f, 1.0f};
      }

      wire.wayPoints.clear();
      if (item.contains("waypoints") && item["waypoints"].is_array()) {
        for (const auto& wp : item["waypoints"]) {
          if (!wp.is_object()) {
            continue;
          }
          Position pos;
          pos.x = wp.value("x", 0.0f);
          pos.y = wp.value("y", 0.0f);
          wire.wayPoints.push_back(pos);
        }
      }

      wires_.push_back(wire);
      max_wire_id = std::max(max_wire_id, wire.id);
    }
  }

  next_instance_id_ = root.value("next_instance_id", max_instance_id + 1);
  if (next_instance_id_ <= max_instance_id) {
    next_instance_id_ = max_instance_id + 1;
  }
  next_wire_id_ = root.value("next_wire_id", max_wire_id + 1);
  if (next_wire_id_ <= max_wire_id) {
    next_wire_id_ = max_wire_id + 1;
  }
  if (max_z_order == std::numeric_limits<int>::min()) {
    max_z_order = -1;
  }
  next_z_order_ = root.value("next_z_order", max_z_order + 1);
  if (next_z_order_ <= max_z_order) {
    next_z_order_ = max_z_order + 1;
  }

  return true;
}

void Application::LoadLadderProgramFromLD(const std::string& filepath) {

  std::cout << "[INFO] Loading ladder program from .ld file: " << filepath

            << std::endl;




  LDToLadderConverter converter;




  LadderProgram loadedProgram;

  bool success = converter.ConvertFromLDFile(filepath, loadedProgram);



  if (!success) {

    std::cout << "[ERROR] Failed to load .ld file: " << converter.GetLastError()

              << std::endl;

    std::cout << "[ERROR] Invalid .ld format. Check file contents and try again."
              << std::endl;

    return;

  }




  auto stats = converter.GetConversionStats();

  std::cout << "[INFO] Successfully loaded .ld file!" << std::endl;

  std::cout << "[INFO] Conversion stats:" << std::endl;

  std::cout << "   - Networks: " << stats.networksCount << std::endl;

  std::cout << "   - Contacts: " << stats.contactsCount << std::endl;

  std::cout << "   - Coils: " << stats.coilsCount << std::endl;

  std::cout << "   - Blocks: " << stats.blocksCount << std::endl;




  loaded_ladder_program_ = loadedProgram;




  if (programming_mode_) {



    std::cout << "[INFO] Syncing to Programming Mode for UI editing..."

              << std::endl;





  }




  plc_device_states_.clear();

  plc_timer_states_.clear();

  plc_counter_states_.clear();




  for (int i = 0; i < 16; ++i) {

    std::string i_str = std::to_string(i);

    plc_device_states_["X" + i_str] = false;

    plc_device_states_["Y" + i_str] = false;

    plc_device_states_["M" + i_str] = false;

  }




  for (const auto& rung : loadedProgram.rungs) {

    if (rung.isEndRung)

      continue;



    for (const auto& cell : rung.cells) {

      if (!cell.address.empty()) {

        if (cell.type == LadderInstructionType::TON) {

          TimerState& timer = plc_timer_states_[cell.address];

          timer.preset = 10;




          if (!cell.preset.empty() && cell.preset[0] == 'K') {

            try {

              timer.preset = std::stoi(cell.preset.substr(1));

            } catch (const std::exception&) {

              timer.preset = 10;

            }

          }

        } else if (cell.type == LadderInstructionType::CTU) {

          CounterState& counter = plc_counter_states_[cell.address];

          counter.preset = 10;




          if (!cell.preset.empty() && cell.preset[0] == 'K') {

            try {

              counter.preset = std::stoi(cell.preset.substr(1));

            } catch (const std::exception&) {

              counter.preset = 10;

            }

          }

        }

      }

    }

  }



  std::cout << "[INFO] Initialized " << plc_timer_states_.size() << " timers and "

            << plc_counter_states_.size() << " counters" << std::endl;




  if (compiled_plc_executor_) {

    std::cout << "[INFO] Auto-compiling loaded ladder program..." << std::endl;

    CompileAndLoadLadderProgram();

  }



  std::cout << "[INFO] Ladder program loaded and ready for execution!" << std::endl;

}

void Application::CompileAndLoadLadderProgram() {

  if (!compiled_plc_executor_) {

    std::cout << "[WARN] CompiledPLCExecutor not initialized" << std::endl;

    return;

  }



  if (loaded_ladder_program_.rungs.empty()) {

    std::cout << "[WARN] No ladder program to compile" << std::endl;

    return;

  }



  try {

    // Step 1: Convert ladder program to .ld file

    std::cout << "[INFO] Converting ladder program to OpenPLC .ld format..."

              << std::endl;



    LadderToLDConverter converter;

    converter.SetDebugMode(enable_debug_logging_);




    std::string tempLdPath = "temp_ladder_program.ld";

    bool convertSuccess =

        converter.ConvertToLDFile(loaded_ladder_program_, tempLdPath);



    if (!convertSuccess) {

      std::cout << "[ERROR] Failed to convert ladder program to .ld format: "

                << converter.GetLastError() << std::endl;

      return;

    }



    std::cout << "[INFO] Ladder program converted to .ld format successfully"

              << std::endl;



    // Step 2: Compile .ld file to C++ code

    std::cout << "[INFO] Compiling .ld file to C++ code..." << std::endl;



    OpenPLCCompilerIntegration compiler;

    compiler.SetDebugMode(enable_debug_logging_);

    compiler.SetIOConfiguration(16, 16);  // FX3U-32M: 16 inputs, 16 outputs



    auto compilationResult = compiler.CompileLDFile(tempLdPath);



    if (!compilationResult.success) {

      std::cout << "[ERROR] Failed to compile .ld file: "

                << compilationResult.errorMessage << std::endl;

      return;

    }



    std::cout << "[INFO] .ld file compiled successfully" << std::endl;

    std::cout << "[INFO] Compilation statistics:" << std::endl;

    std::cout << "   - Generated code size: "

              << compilationResult.generatedCode.length() << " characters"

              << std::endl;

    std::cout << "   - Input count: " << compilationResult.inputCount

              << std::endl;

    std::cout << "   - Output count: " << compilationResult.outputCount

              << std::endl;

    std::cout << "   - Memory count: " << compilationResult.memoryCount

              << std::endl;



    // Step 3: Load compiled code into CompiledPLCExecutor

    std::cout << "[INFO] Loading compiled code into PLC executor..." << std::endl;



    bool loadSuccess =

        compiled_plc_executor_->LoadFromCompilationResult(compilationResult);



    if (!loadSuccess) {

      std::cout << "[ERROR] Failed to load compiled code into PLC executor"

                << std::endl;

      return;

    }



    std::cout << "[INFO] Compiled ladder program loaded successfully!" << std::endl;

    std::cout << "[INFO] CompiledPLCExecutor is ready for execution" << std::endl;



    // Step 4: Cleanup temporary files

    std::remove(tempLdPath.c_str());



  } catch (const std::exception& e) {

    std::cout << "[ERROR] Exception during compilation: " << e.what() << std::endl;

  }

}

void Application::CreateDefaultTestLadderProgram() {

  std::cout << "[INFO] Creating default test ladder program..." << std::endl;




  loaded_ladder_program_.rungs.clear();

  loaded_ladder_program_.verticalConnections.clear();




  Rung rung0;

  rung0.number = 0;

  rung0.cells.resize(12);



  // X0 (XIC)

  rung0.cells[0].type = LadderInstructionType::XIC;

  rung0.cells[0].address = "X0";




  for (int i = 1; i < 11; i++) {

    rung0.cells[static_cast<std::size_t>(i)].type = LadderInstructionType::HLINE;

  }



  // Y0 (OTE)

  rung0.cells[11].type = LadderInstructionType::OTE;

  rung0.cells[11].address = "Y0";



  loaded_ladder_program_.rungs.push_back(rung0);




  Rung rung1;

  rung1.number = 1;

  rung1.cells.resize(12);



  // X1 (XIC)

  rung1.cells[0].type = LadderInstructionType::XIC;

  rung1.cells[0].address = "X1";




  for (int i = 1; i < 11; i++) {

    rung1.cells[static_cast<std::size_t>(i)].type = LadderInstructionType::HLINE;

  }



  // Y1 (OTE)

  rung1.cells[11].type = LadderInstructionType::OTE;

  rung1.cells[11].address = "Y1";



  loaded_ladder_program_.rungs.push_back(rung1);



  // End Rung

  Rung endRung;

  endRung.number = 2;

  endRung.isEndRung = true;

  loaded_ladder_program_.rungs.push_back(endRung);




  plc_device_states_.clear();

  for (int i = 0; i < 16; i++) {

    plc_device_states_["X" + std::to_string(i)] = false;

    plc_device_states_["Y" + std::to_string(i)] = false;

  }




  plc_timer_states_.clear();

  plc_counter_states_.clear();



  std::cout << "[INFO] Default test ladder program created: X0->Y0, X1->Y1"

            << std::endl;

}

bool Application::SaveProject(const std::string& filePath,

                              const std::string& projectName) {

  std::cout << "[INFO] Saving project to: " << filePath << std::endl;



  if (!programming_mode_ || !project_file_manager_) {

    std::cerr << "[ERROR] Project save failed: components not initialized"

              << std::endl;

    return false;

  }



  try {


    LadderProgram currentProgram = programming_mode_->GetLadderProgram();
    const auto& timer_states = programming_mode_->GetTimerStates();
    const auto& counter_states = programming_mode_->GetCounterStates();
    auto try_parse_preset = [](const std::string& address,
                               char prefix,
                               int* deviceNum,
                               int* presetNum) -> bool {
      if (!deviceNum || !presetNum) {
        return false;
      }
      for (size_t i = 0; i < address.size(); ++i) {
        if (address[i] != prefix) {
          continue;
        }
        size_t j = i + 1;
        while (j < address.size() &&
               !std::isdigit(static_cast<unsigned char>(address[j]))) {
          ++j;
        }
        if (j >= address.size()) {
          continue;
        }
        size_t deviceStart = j;
        while (j < address.size() &&
               std::isdigit(static_cast<unsigned char>(address[j]))) {
          ++j;
        }
        std::string deviceDigits = address.substr(deviceStart, j - deviceStart);
        if (deviceDigits.empty()) {
          continue;
        }
        size_t kPos = address.find_first_of("Kk", j);
        if (kPos == std::string::npos) {
          continue;
        }
        size_t p = kPos + 1;
        while (p < address.size() &&
               !std::isdigit(static_cast<unsigned char>(address[p]))) {
          ++p;
        }
        if (p >= address.size()) {
          continue;
        }
        size_t presetStart = p;
        while (p < address.size() &&
               std::isdigit(static_cast<unsigned char>(address[p]))) {
          ++p;
        }
        std::string presetDigits =
            address.substr(presetStart, p - presetStart);
        if (presetDigits.empty()) {
          continue;
        }
        try {
          *deviceNum = std::stoi(deviceDigits);
          *presetNum = std::stoi(presetDigits);
          return true;
        } catch (...) {
          return false;
        }
      }
      return false;
    };
    for (auto& rung : currentProgram.rungs) {
      if (rung.isEndRung) {
        continue;
      }
      for (auto& cell : rung.cells) {
        if (!cell.preset.empty()) {
          continue;
        }
        if (cell.address.empty()) {
          continue;
        }
        bool isTimerOutput =
            (cell.type == LadderInstructionType::TON ||
             (cell.type == LadderInstructionType::OTE &&
              cell.address[0] == 'T'));
        bool isCounterOutput =
            (cell.type == LadderInstructionType::CTU ||
             (cell.type == LadderInstructionType::OTE &&
              cell.address[0] == 'C'));
        if (isTimerOutput) {
          auto it = timer_states.find(cell.address);
          if (it != timer_states.end() && it->second.preset > 0) {
            cell.preset = "K" + std::to_string(it->second.preset);
            continue;
          }
          int deviceNum = 0;
          int presetNum = 0;
          if (try_parse_preset(cell.address, 'T', &deviceNum, &presetNum) &&
              presetNum > 0) {
            cell.address = "T" + std::to_string(deviceNum);
            cell.preset = "K" + std::to_string(presetNum);
          }
        } else if (isCounterOutput) {
          auto it = counter_states.find(cell.address);
          if (it != counter_states.end() && it->second.preset > 0) {
            cell.preset = "K" + std::to_string(it->second.preset);
            continue;
          }
          int deviceNum = 0;
          int presetNum = 0;
          if (try_parse_preset(cell.address, 'C', &deviceNum, &presetNum) &&
              presetNum > 0) {
            cell.address = "C" + std::to_string(deviceNum);
            cell.preset = "K" + std::to_string(presetNum);
          }
        }
      }
    }

    project_file_manager_->SetDebugMode(enable_debug_logging_);

    auto result = project_file_manager_->SaveProject(currentProgram, filePath,

                                                    projectName);



    if (result.success) {

      std::cout << "[INFO] Project saved successfully!" << std::endl;

      std::cout << "[INFO] Project info:" << std::endl;

      std::cout << "   - Name: " << result.info.projectName << std::endl;

      std::cout << "   - Size: " << result.compressedSize

                << " bytes (compressed)" << std::endl;

      std::cout << "   - Rungs: " << currentProgram.rungs.size() << std::endl;

      std::cout << "   - Vertical connections: "

                << currentProgram.verticalConnections.size() << std::endl;

      std::cout << "   - Layout checksum: " << result.info.layoutChecksum

                << std::endl;

      return true;

    } else {

      std::cerr << "[ERROR] Project save failed: " << result.errorMessage

                << std::endl;

      return false;

    }



  } catch (const std::exception& e) {

    std::cerr << "[ERROR] Project save exception: " << e.what() << std::endl;

    return false;

  }

}

bool Application::LoadProject(const std::string& filePath) {

  std::cout << "[INFO] Loading project from: " << filePath << std::endl;



  if (!programming_mode_ || !project_file_manager_) {

    std::cerr << "[ERROR] Project load failed: components not initialized"

              << std::endl;

    return false;

  }



  try {


    project_file_manager_->SetDebugMode(enable_debug_logging_);

    auto result = project_file_manager_->LoadProject(filePath);



    if (!result.success) {

      std::cerr << "[ERROR] Project load failed: " << result.errorMessage

                << std::endl;

      return false;

    }



    std::cout << "[DEBUG] Project load successful!" << std::endl;

    std::cout << "[DEBUG] Loaded program info:" << std::endl;

    std::cout << "   - Rungs: " << result.program.rungs.size() << std::endl;

    std::cout << "   - Vertical connections: "

              << result.program.verticalConnections.size() << std::endl;




    loaded_ladder_program_ = result.program;




    std::cout << "[DEBUG] Updating ProgrammingMode with loaded program..."

              << std::endl;

    programming_mode_->SetLadderProgram(result.program);




    plc_device_states_.clear();

    plc_timer_states_.clear();

    plc_counter_states_.clear();




    for (int i = 0; i <= 15; ++i) {

      plc_device_states_["X" + std::to_string(i)] = false;

      plc_device_states_["Y" + std::to_string(i)] = false;

    }

    for (int i = 0; i <= 999; ++i) {

      std::string i_str = std::to_string(i);

      plc_device_states_["M" + i_str] = false;

    }




    if (compiled_plc_executor_) {

      CompileAndLoadLadderProgram();

    }




    std::cout << "[INFO] Project loaded successfully!" << std::endl;

    std::cout << "[INFO] Project info:" << std::endl;

    std::cout << "   - Name: " << result.info.projectName << std::endl;

    std::cout << "   - Version: " << result.info.version << std::endl;

    std::cout << "   - Created: " << result.info.createdDate << std::endl;

    std::cout << "   - Tool version: " << result.info.toolVersion << std::endl;

    std::cout << "   - Rungs: " << result.program.rungs.size() << std::endl;

    std::cout << "   - Vertical connections: "

              << result.program.verticalConnections.size() << std::endl;



    if (result.info.layoutChecksum != 0) {

      std::cout << "   - Layout checksum: " << result.info.layoutChecksum

                << " bytes" << std::endl;

    }



    if (!result.ldProgram.empty()) {

      std::cout << "   - LD program size: " << result.ldProgram.size()

                << " bytes" << std::endl;

    }

    RequestCacheWarmup();

    return true;



  } catch (const std::exception& e) {

    std::cerr << "[ERROR] Project load exception: " << e.what() << std::endl;

    return false;

  }

}



}  // namespace plc
