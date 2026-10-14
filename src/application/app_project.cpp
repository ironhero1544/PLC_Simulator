#include "plc_emulator/physics/storage_sensor_simulation.h"
#include "plc_emulator/programming/execution_program.h"

#include "plc_emulator/components/pneumatic_component.h"
// app_project.cpp
//
// Project/ladder load/save helpers.

#include "plc_emulator/core/application.h"

#include "plc_emulator/components/wiring_port_rules.h"
#include "plc_emulator/physics/z_layer.h"
#include "plc_emulator/physics/ring_feed_simulation.h"
#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/programming/ladder_program_utils.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/ld_to_ladder_converter.h"
#include "plc_emulator/project/openplc_compiler_integration.h"
#include "plc_emulator/rtl/rtl_project_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace plc {
namespace {

std::vector<std::string> GetProjectRtlModuleIds(const json& layout) {
  std::vector<std::string> module_ids;
  const auto components = layout.find("components");
  if (components == layout.end() || !components->is_array()) return module_ids;
  for (const auto& component : *components) {
    if (!component.is_object() ||
        component.value("type", "") != "RTL_MODULE") continue;
    const auto rtl = component.find("rtl");
    if (rtl == component.end() || !rtl->is_object()) continue;
    for (const char* key : {"module_id", "override_id"}) {
      const std::string id = rtl->value(key, "");
      if (!id.empty() &&
          std::find(module_ids.begin(), module_ids.end(), id) == module_ids.end())
        module_ids.push_back(id);
    }
  }
  return module_ids;
}

FiberSettings ReadFiberSettings(const json& value) {
  return {value.at("max_distance").get<double>(),
          value.at("alignment_tolerance").get<double>(),
          value.at("beam_width").get<double>(), value.at("dark_on").get<bool>()};
}
HmiSettings ReadHmiSettings(const json& value) {
  HmiSettings settings;
  settings.pages = value.value("pages", settings.pages);
  if (!value.contains("widgets")) {
    const auto buttons = value.at("buttons").get<std::array<std::string, 3>>();
    for (size_t i = 0; i < buttons.size(); ++i) {
      settings.widgets[i].address = buttons[i];
      settings.widgets[i].momentary = value.at("momentary").get<bool>();
    }
    settings.widgets[3].address = value.at("display").get<std::string>();
  } else {
    const auto& widgets = value.at("widgets");
    if (!widgets.is_array() || widgets.size() > kMaxHmiWidgets)
      throw std::runtime_error("Invalid HMI widget list");
    const float source_width = value.value("screen_width", 246.0f);
    const float source_height = value.value("screen_height", 117.0f);
    if (!std::isfinite(source_width) || !std::isfinite(source_height) ||
        source_width <= 0 || source_height <= 0)
      throw std::runtime_error("Invalid HMI screen dimensions");
    const float scale_x = kHmiScreenWidth / source_width;
    const float scale_y = kHmiScreenHeight / source_height;
    settings.widgets.clear();
    settings.widgets.reserve(widgets.size());
    for (const auto& item : widgets) {
      HmiWidget widget;
      const std::string type = item.at("type").get<std::string>();
      if (type == "button") widget.type = HmiWidgetType::kButton;
      else if (type == "lamp") widget.type = HmiWidgetType::kLamp;
      else if (type == "number") widget.type = HmiWidgetType::kNumber;
      else if (type == "text") widget.type = HmiWidgetType::kText;
      else if (type == "message") widget.type = HmiWidgetType::kMessage;
      else throw std::runtime_error("Invalid HMI widget type");
      widget.text = item.at("text").get<std::string>();
      widget.address = item.at("address").get<std::string>();
      widget.position = {item.at("x").get<float>(), item.at("y").get<float>()};
      widget.size = {item.at("width").get<float>(), item.at("height").get<float>()};
      widget.momentary = item.at("momentary").get<bool>();
      widget.unit = item.at("unit").get<std::string>();
      widget.decimals = item.at("decimals").get<int>();
      widget.font_size = item.at("font_size").get<float>();
      widget.on_color = item.at("on_color").get<uint32_t>();
      widget.page = item.value("page", 0);
      widget.target_page = item.value("target_page", -1);
      widget.visibility_address = item.value("visibility_address", std::string{});
      widget.messages = item.value("messages", widget.messages);
      if (!std::isfinite(widget.font_size) || widget.font_size < 6 || widget.font_size > 32)
        throw std::runtime_error("Invalid HMI font size");
      widget.position.x *= scale_x;
      widget.position.y *= scale_y;
      widget.size.width *= scale_x;
      widget.size.height *= scale_y;
      widget.font_size = std::clamp(widget.font_size * std::min(scale_x, scale_y), 6.0f, 32.0f);
      settings.widgets.push_back(std::move(widget));
    }
  }
  return settings;
}

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
    case ComponentType::kSensorCylinder: return "SENSOR_CYLINDER";
    case ComponentType::kSuctionCylinder: return "SUCTION_CYLINDER";
    case ComponentType::kStopperCylinder: return "STOPPER_CYLINDER";
    case ComponentType::kVacuumGenerator: return "VACUUM_GENERATOR";
    case ComponentType::kDcMotor: return "DC_MOTOR";
    case ComponentType::kDcMotorDriver: return "DC_MOTOR_DRIVER";
    case ComponentType::kServoMotor: return "SERVO_MOTOR";
    case ComponentType::kServoAmplifier: return "SERVO_AMPLIFIER";
    case ComponentType::kServoLift: return "SERVO_LIFT";
    case ComponentType::kLiftSuction: return "LIFT_SUCTION";
    case ComponentType::kAcSupply: return "AC_SUPPLY";
    case ComponentType::kWarehouse: return "WAREHOUSE";
    case ComponentType::kDrivenWarehouse: return "DRIVEN_WAREHOUSE";
    case ComponentType::kMagazine: return "MAGAZINE";
    case ComponentType::kFiberAmplifier: return "FIBER_AMPLIFIER";
    case ComponentType::kFiberTransmitter: return "FIBER_TRANSMITTER";
    case ComponentType::kFiberReceiver: return "FIBER_RECEIVER";
    case ComponentType::kTouchPanel: return "TOUCH_PANEL";

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
    case ComponentType::RTL_MODULE:
      return "RTL_MODULE";
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
  } else if (value == "SENSOR_CYLINDER") {
    *out = ComponentType::kSensorCylinder;
  } else if (value == "SUCTION_CYLINDER") {
    *out = ComponentType::kSuctionCylinder;
  } else if (value == "STOPPER_CYLINDER") {
    *out = ComponentType::kStopperCylinder;
  } else if (value == "DC_MOTOR_DRIVER") {
    *out = ComponentType::kDcMotorDriver;

  } else if (value == "DC_MOTOR") {
    *out = ComponentType::kDcMotor;
  } else if (value == "SERVO_MOTOR") {
    *out = ComponentType::kServoMotor;
  } else if (value == "SERVO_AMPLIFIER") {
    *out = ComponentType::kServoAmplifier;
  } else if (value == "SERVO_LIFT") {
    *out = ComponentType::kServoLift;
  } else if (value == "LIFT_SUCTION") {
    *out = ComponentType::kLiftSuction;
  } else if (value == "MAGAZINE") {
    *out = ComponentType::kMagazine;
  } else if (value == "DRIVEN_WAREHOUSE") {
    *out = ComponentType::kDrivenWarehouse;
  } else if (value == "WAREHOUSE") {
    *out = ComponentType::kWarehouse;
  } else if (value == "FIBER_AMPLIFIER") {
    *out = ComponentType::kFiberAmplifier;
  } else if (value == "FIBER_TRANSMITTER") {
    *out = ComponentType::kFiberTransmitter;
  } else if (value == "FIBER_RECEIVER") {
    *out = ComponentType::kFiberReceiver;
  } else if (value == "TOUCH_PANEL") {
    *out = ComponentType::kTouchPanel;
  } else if (value == "AC_SUPPLY") {
    *out = ComponentType::kAcSupply;
  } else if (value == "VACUUM_GENERATOR") {
    *out = ComponentType::kVacuumGenerator;
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
  } else if (value == "RTL_MODULE") {
    *out = ComponentType::RTL_MODULE;
  } else {
    return false;
  }
  return true;
}

bool IsSensorComponentType(ComponentType type) {
  return type == ComponentType::kFiberAmplifier ||
         plc_emulator::IsSensorCylinder(type) ||
         type == ComponentType::SENSOR ||
         type == ComponentType::INDUCTIVE_SENSOR ||
         type == ComponentType::RING_SENSOR;
}

const char* RtlLogicFamilyToString(RtlLogicFamily family) {
  switch (family) {
    case RtlLogicFamily::INDUSTRIAL_24V:
      return "INDUSTRIAL_24V";
    case RtlLogicFamily::CMOS_5V:
      return "CMOS_5V";
    case RtlLogicFamily::TTL_5V:
      return "TTL_5V";
    default:
      return "INDUSTRIAL_24V";
  }
}

bool RtlLogicFamilyFromString(const std::string& value,
                              RtlLogicFamily* out) {
  if (!out) {
    return false;
  }
  if (value == "INDUSTRIAL_24V") {
    *out = RtlLogicFamily::INDUSTRIAL_24V;
  } else if (value == "CMOS_5V") {
    *out = RtlLogicFamily::CMOS_5V;
  } else if (value == "TTL_5V") {
    *out = RtlLogicFamily::TTL_5V;
  } else {
    return false;
  }
  return true;
}

const char* PlcInputModeToString(PlcInputMode mode) {
  return mode == PlcInputMode::SOURCE ? "source" : "sink";
}

bool PlcInputModeFromString(const std::string& value, PlcInputMode* mode) {
  if (!mode) {
    return false;
  }
  if (value == "source") {
    *mode = PlcInputMode::SOURCE;
    return true;
  }
  if (value == "sink") {
    *mode = PlcInputMode::SINK;
    return true;
  }
  return false;
}

const char* PlcOutputModeToString(PlcOutputMode mode) {
  return mode == PlcOutputMode::SOURCE ? "source" : "sink";
}

bool PlcOutputModeFromString(const std::string& value, PlcOutputMode* mode) {
  if (!mode) {
    return false;
  }
  if (value == "source") {
    *mode = PlcOutputMode::SOURCE;
    return true;
  }
  if (value == "sink") {
    *mode = PlcOutputMode::SINK;
    return true;
  }
  return false;
}

SensorOutputMode DefaultSensorOutputModeForInput(PlcInputMode mode) {
  return mode == PlcInputMode::SOURCE ? SensorOutputMode::NPN
                                      : SensorOutputMode::PNP;
}

const char* SensorOutputModeToString(SensorOutputMode mode) {
  return mode == SensorOutputMode::NPN ? "npn" : "pnp";
}

bool SensorOutputModeFromString(const std::string& value,
                                SensorOutputMode* mode) {
  if (!mode) {
    return false;
  }
  if (value == "npn") {
    *mode = SensorOutputMode::NPN;
    return true;
  }
  if (value == "pnp") {
    *mode = SensorOutputMode::PNP;
    return true;
  }
  return false;
}

float SensorOutputModeToStoredValue(SensorOutputMode mode) {
  return mode == SensorOutputMode::NPN ? 1.0f : 0.0f;
}

SensorOutputMode SensorOutputModeFromStoredValue(float value) {
  return value >= 0.5f ? SensorOutputMode::NPN : SensorOutputMode::PNP;
}

LadderProgram BuildProgramForSave(ProgrammingMode* programming_mode) {
  LadderProgram current_program;
  if (!programming_mode) {
    return current_program;
  }

  current_program = programming_mode->GetLadderProgram();
  const auto& timer_states = programming_mode->GetTimerStates();
  const auto& counter_states = programming_mode->GetCounterStates();

  auto try_parse_preset = [](const std::string& address, char prefix,
                             int* device_num, int* preset_num) -> bool {
    if (!device_num || !preset_num) {
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
      size_t device_start = j;
      while (j < address.size() &&
             std::isdigit(static_cast<unsigned char>(address[j]))) {
        ++j;
      }
      std::string device_digits = address.substr(device_start, j - device_start);
      if (device_digits.empty()) {
        continue;
      }
      size_t k_pos = address.find_first_of("Kk", j);
      if (k_pos == std::string::npos) {
        continue;
      }
      size_t p = k_pos + 1;
      while (p < address.size() &&
             !std::isdigit(static_cast<unsigned char>(address[p]))) {
        ++p;
      }
      if (p >= address.size()) {
        continue;
      }
      size_t preset_start = p;
      while (p < address.size() &&
             std::isdigit(static_cast<unsigned char>(address[p]))) {
        ++p;
      }
      std::string preset_digits =
          address.substr(preset_start, p - preset_start);
      if (preset_digits.empty()) {
        continue;
      }
      try {
        *device_num = std::stoi(device_digits);
        *preset_num = std::stoi(preset_digits);
        return true;
      } catch (...) {
        return false;
      }
    }
    return false;
  };

  for (auto& rung : current_program.rungs) {
    if (rung.isEndRung) {
      continue;
    }
    for (auto& cell : rung.cells) {
      if (!cell.preset.empty() || cell.address.empty()) {
        continue;
      }

      bool is_timer_output =
          (cell.type == LadderInstructionType::TON ||
           (cell.type == LadderInstructionType::OTE &&
            cell.address[0] == 'T'));
      bool is_counter_output =
          (cell.type == LadderInstructionType::CTU ||
           (cell.type == LadderInstructionType::OTE &&
            cell.address[0] == 'C'));

      if (is_timer_output) {
        auto it = timer_states.find(cell.address);
        if (it != timer_states.end() && it->second.preset > 0) {
          cell.preset = "K" + std::to_string(it->second.preset);
          continue;
        }
        int device_num = 0;
        int preset_num = 0;
        if (try_parse_preset(cell.address, 'T', &device_num, &preset_num) &&
            preset_num > 0) {
          cell.address = "T" + std::to_string(device_num);
          cell.preset = "K" + std::to_string(preset_num);
        }
      } else if (is_counter_output) {
        auto it = counter_states.find(cell.address);
        if (it != counter_states.end() && it->second.preset > 0) {
          cell.preset = "K" + std::to_string(it->second.preset);
          continue;
        }
        int device_num = 0;
        int preset_num = 0;
        if (try_parse_preset(cell.address, 'C', &device_num, &preset_num) &&
            preset_num > 0) {
          cell.address = "C" + std::to_string(device_num);
          cell.preset = "K" + std::to_string(preset_num);
        }
      }
    }
  }

  CanonicalizeLadderProgram(&current_program);
  return current_program;
}

}  // namespace

LadderInstructionType Application::StringToInstructionType(

    const std::string& str) {
  if (str == "WRAPPING_SOURCE") return LadderInstructionType::kWrappingSource;
  if (str == "WRAPPING_DESTINATION") return LadderInstructionType::kWrappingDestination;

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

  if (str == "BKRST")

    return LadderInstructionType::BKRST;

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
  CanonicalizeLadderProgram(&loaded_ladder_program_);

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

void Application::SetPlcInputMode(PlcInputMode mode, bool auto_convert_sensors) {
  plc_input_mode_ = mode;
  if (auto_convert_sensors) {
    AutoConvertSensorsForInputMode();
  }
  OnElectricalConfigChanged();
}

void Application::SetPlcOutputMode(PlcOutputMode mode) {
  plc_output_mode_ = mode;
  OnElectricalConfigChanged();
}

bool Application::IsPlcInputVoltageActive(float voltage) const {
  if (voltage < -0.5f) {
    return false;
  }
  if (plc_input_mode_ == PlcInputMode::SOURCE) {
    return voltage >= -0.1f && voltage < 6.0f;
  }
  return voltage > 12.0f;
}

float Application::GetPlcOutputOnVoltage() const {
  return plc_output_mode_ == PlcOutputMode::SOURCE ? 24.0f : 0.0f;
}

SensorOutputMode Application::GetSensorOutputMode(
    const PlacedComponent& comp) const {
  auto it = comp.internalStates.find(state_keys::kSensorOutputMode);
  if (it == comp.internalStates.end()) {
    return DefaultSensorOutputModeForInput(plc_input_mode_);
  }
  return SensorOutputModeFromStoredValue(it->second);
}

void Application::SetSensorOutputMode(PlacedComponent* comp,
                                      SensorOutputMode mode) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kSensorOutputMode] =
      SensorOutputModeToStoredValue(mode);
}

void Application::ApplyElectricalDefaults(PlacedComponent* comp) const {
  if (!comp || !IsSensorComponentType(comp->type)) {
    return;
  }
  comp->internalStates[state_keys::kSensorOutputMode] =
      SensorOutputModeToStoredValue(
          DefaultSensorOutputModeForInput(plc_input_mode_));
}

void Application::AutoConvertSensorsForInputMode() {
  SensorOutputMode target_mode = DefaultSensorOutputModeForInput(plc_input_mode_);
  for (auto& comp : placed_components_) {
    if (!IsSensorComponentType(comp.type)) {
      continue;
    }
    SetSensorOutputMode(&comp, target_mode);
  }
}

void Application::OnElectricalConfigChanged() {
  advanced_last_input_hash_ = 0;
  advanced_inputs_dirty_ = true;
  RequestCacheWarmup();
}

std::string Application::SerializeLayoutJson() const {
  json root;
  root["version"] = 3;
  root["next_instance_id"] = next_instance_id_;
  root["next_wire_id"] = next_wire_id_;
  root["next_z_order"] = next_z_order_;

  json electrical_config;
  electrical_config["plc_input_mode"] = PlcInputModeToString(plc_input_mode_);
  electrical_config["plc_output_mode"] = PlcOutputModeToString(plc_output_mode_);
  root["electrical_config"] = electrical_config;

  json components = json::array();
  for (const auto& comp : placed_components_) {
    json item;
    item["instance_id"] = comp.instanceId;
    item["type"] = ComponentTypeToString(comp.type);
    item["x"] = comp.position.x - comp.warehouse_drive_offset.x;
    item["y"] = comp.position.y - comp.warehouse_drive_offset.y;
    item["z_order"] = comp.z_order;
    item["render_layer"] = {{"opacity", comp.render_opacity},
                            {"hidden", comp.render_hidden},
                            {"group", comp.render_group}};
    if (comp.type == ComponentType::FRL) {
      const auto pressure = comp.internalStates.find("air_pressure");
      item["frl_pressure"] = pressure == comp.internalStates.end() ? 6.0f : pressure->second;
    }
    if (comp.type == ComponentType::kWarehouse) {
      item["warehouse_drive"] = {{"cylinder_id", comp.warehouse_cylinder_id},
                                 {"travel", comp.warehouse_travel}};
    }
    if (plc_emulator::IsServoSystem(comp.type)) {
      const auto& axis = comp.axis_settings;
      item["axis_settings"] = {{"pulses_per_turn", axis.pulses_per_turn},
          {"lead_mm", axis.lead_mm}, {"stroke_mm", axis.stroke_mm},
          {"max_speed", axis.max_speed}, {"acceleration", axis.acceleration}};
    }
    if (comp.type == ComponentType::kFiberAmplifier) {
      const auto& fiber = comp.fiber_settings;
      item["fiber_settings"] = {{"max_distance", fiber.max_distance},
          {"alignment_tolerance", fiber.alignment_tolerance},
          {"beam_width", fiber.beam_width}, {"dark_on", fiber.dark_on}};
    }
    if (comp.type == ComponentType::kTouchPanel) {
      json widgets = json::array();
      for (const auto& widget : comp.hmi_settings.widgets) {
        const char* types[] = {"button", "lamp", "number", "text", "message"};
        widgets.push_back({{"type", types[static_cast<int>(widget.type)]},
            {"text", widget.text}, {"address", widget.address},
            {"x", widget.position.x}, {"y", widget.position.y},
            {"width", widget.size.width}, {"height", widget.size.height},
            {"momentary", widget.momentary}, {"unit", widget.unit},
            {"decimals", widget.decimals}, {"font_size", widget.font_size},
            {"on_color", widget.on_color}, {"page", widget.page},
            {"target_page", widget.target_page}, {"messages", widget.messages},
            {"visibility_address", widget.visibility_address}});
      }
      item["hmi_settings"] = {{"screen_width", kHmiScreenWidth},
                              {"screen_height", kHmiScreenHeight}, {"pages", comp.hmi_settings.pages}, {"widgets", widgets}};
    }
    if (IsWorkpiece(comp.type) && comp.carry_state.slot_id >= 1 &&
        comp.carry_state.owner_id >= 0 &&
        std::any_of(placed_components_.begin(), placed_components_.end(),
                    [&](const auto& owner) {
                      return owner.instanceId == comp.carry_state.owner_id &&
                             plc::IsWarehouseType(owner.type);
                    })) {
      item["stored_slot"] = {{"warehouse_id", comp.carry_state.owner_id},
                               {"slot_id", comp.carry_state.slot_id}};
    }
    item["z_config"] = {{"min_layer", comp.z_config.min_layer},
        {"max_layer", comp.z_config.max_layer},
        {"initial_height", comp.z_config.initial_height}};
    item["rotation_quadrants"] = comp.rotation_quadrants;
    item["flip_x"] = comp.flip_x;
    item["flip_y"] = comp.flip_y;
    if (!comp.customLabel.empty()) {
      item["custom_label"] = comp.customLabel;
    }
    item["show_custom_label"] = comp.show_custom_label;
    if (comp.type == ComponentType::RTL_MODULE) {
      item["rtl"] = {
          {"module_id", comp.rtlModuleId},
          {"source_mode", comp.rtlSourceMode},
          {"override_id", comp.rtlOverrideId},
          {"clock_pin_name", comp.rtlClockPinName},
          {"reset_pin_name", comp.rtlResetPinName},
          {"power_pin_name", comp.rtlPowerPinName},
          {"ground_pin_name", comp.rtlGroundPinName},
          {"logic_family", RtlLogicFamilyToString(comp.rtlLogicFamily)},
          {"use_internal_clock", comp.rtlUseInternalClock},
          {"clock_frequency_hz", comp.rtlClockFrequencyHz},
          {"use_startup_reset", comp.rtlUseStartupReset},
          {"reset_pulse_ms", comp.rtlResetPulseMs},
          {"reset_active_low", comp.rtlResetActiveLow},
      };
    }
    if (IsSensorComponentType(comp.type)) {
      item["sensor_output_mode"] = SensorOutputModeToString(
          GetSensorOutputMode(comp));
    }
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
    item["wire_kind"] = wire.kind == WireKind::kElectric ? "electric"
        : wire.kind == WireKind::kPneumatic ? "pneumatic" : "optical";
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
  return root.dump(2);
}

bool Application::DeserializeLayoutJson(const std::string& layout_json) {
  json root = json::parse(layout_json, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return false;
  }

  // Validate all axis settings before changing the existing scene.
  if (root.contains("components") && root["components"].is_array()) {
    for (const auto& item : root["components"]) {
      if (!item.is_object() || !item.contains("axis_settings")) continue;
      const auto& axis = item["axis_settings"];
      if (!axis.is_object()) return false;
      for (const char* key : {"pulses_per_turn", "lead_mm", "stroke_mm",
                              "max_speed", "acceleration"}) {
        if (!axis.contains(key) || !axis[key].is_number()) return false;
      }
      const AxisSettings settings{axis["pulses_per_turn"].get<double>(),
          axis["lead_mm"].get<double>(), axis["stroke_mm"].get<double>(),
          axis["max_speed"].get<double>(), axis["acceleration"].get<double>()};
      if (!plc_emulator::IsValidAxisSettings(settings)) return false;
    }
  }
  // Validate before changing the existing scene.
  if (root.contains("components") && root["components"].is_array()) {
    for (const auto& item : root["components"]) {
      if (!item.is_object() || !item.contains("z_config")) continue;
      const auto& z = item["z_config"];
      if (!z.is_object()) return false;
      for (const char* key : {"min_layer", "max_layer", "initial_height"}) {
        if (!z.contains(key) || !z[key].is_number()) return false;
      }
      const ZLayerConfig config{z["min_layer"].get<float>(),
          z["max_layer"].get<float>(), z["initial_height"].get<float>()};
      if (!plc_emulator::IsValidZLayerConfig(config)) return false;
    }
  }

  if (root.contains("components") && root["components"].is_array()) {
    std::map<std::pair<int, int>, bool> occupied;
    for (const auto& item : root["components"]) {
      if (!item.is_object()) continue;
      if (item.contains("frl_pressure")) {
        if (item.value("type", "") != "FRL" || !item["frl_pressure"].is_number()) return false;
        const float pressure = item["frl_pressure"].get<float>();
        if (!std::isfinite(pressure) || pressure < 0 || pressure > 10) return false;
      }
      if (item.contains("warehouse_drive")) {
        const auto& drive = item["warehouse_drive"];
        if (item.value("type", "") != "WAREHOUSE" || !drive.is_object() ||
            !drive.contains("cylinder_id") || !drive["cylinder_id"].is_number_integer() ||
            !drive.contains("travel") || !drive["travel"].is_number()) return false;
        const int cylinder_id = drive["cylinder_id"].get<int>();
        const float travel = drive["travel"].get<float>();
        if (!std::isfinite(travel) || travel <= 0 || travel > 10000 || cylinder_id < -1)
          return false;
        if (cylinder_id >= 0 && std::none_of(root["components"].begin(), root["components"].end(),
            [&](const auto& candidate) {
              const auto type = candidate.value("type", "");
              return candidate.value("instance_id", -1) == cylinder_id &&
                     (type == "CYLINDER" || type == "SENSOR_CYLINDER");
            })) return false;
      }
      if (item.contains("fiber_settings")) {
        const auto& settings = item["fiber_settings"];
        if (!settings.is_object()) return false;
        for (const char* key : {"max_distance", "alignment_tolerance", "beam_width"})
          if (!settings.contains(key) || !settings[key].is_number()) return false;
        if (!settings.contains("dark_on") || !settings["dark_on"].is_boolean() ||
            !plc_emulator::IsValidFiberSettings(ReadFiberSettings(settings))) return false;
      }
      if (item.contains("hmi_settings")) {
        const auto& settings = item["hmi_settings"];
        try {
          if (!settings.is_object() ||
              !plc_emulator::IsValidHmiSettings(ReadHmiSettings(settings))) return false;
        } catch (const std::exception&) {
          return false;
        }
      }
      if ((item.value("type", "") == "WAREHOUSE" || item.value("type", "") == "DRIVEN_WAREHOUSE") && item.contains("z_config") &&
          item["z_config"]["min_layer"].get<float>() >=
          item["z_config"]["max_layer"].get<float>()) return false;
      if (!item.contains("stored_slot")) continue;
      const auto& stored = item["stored_slot"];
      if (!stored.is_object() || !stored.contains("warehouse_id") ||
          !stored["warehouse_id"].is_number_integer() || !stored.contains("slot_id") ||
          !stored["slot_id"].is_number_integer()) return false;
      const int owner = stored["warehouse_id"].get<int>();
      const int slot = stored["slot_id"].get<int>();
      const std::string type = item.value("type", "");
      if ((type != "WORKPIECE_METAL" && type != "WORKPIECE_NONMETAL") ||
          slot < 1 || slot > 6 || occupied.contains({owner, slot})) return false;
      const bool exists = std::any_of(root["components"].begin(),
          root["components"].end(), [&](const json& candidate) {
            return candidate.is_object() && (candidate.value("type", "") == "WAREHOUSE" || candidate.value("type", "") == "DRIVEN_WAREHOUSE") &&
                   candidate.value("instance_id", -1) == owner;
          });
      if (!exists) return false;
      occupied[{owner, slot}] = true;
    }
  }
  // Release momentary writes before replacing the scene that owns them.
  for (auto& part : placed_components_) ReleaseHmiButtons(&part);

  plc_input_mode_ = PlcInputMode::SOURCE;
  plc_output_mode_ = PlcOutputMode::SINK;
  auto config_it = root.find("electrical_config");
  if (config_it != root.end() && config_it->is_object()) {
    PlcInputMode parsed_input = plc_input_mode_;
    PlcOutputMode parsed_output = plc_output_mode_;
    PlcInputModeFromString(config_it->value("plc_input_mode", "source"),
                           &parsed_input);
    PlcOutputModeFromString(config_it->value("plc_output_mode", "sink"),
                            &parsed_output);
    plc_input_mode_ = parsed_input;
    plc_output_mode_ = parsed_output;
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
      if (item.contains("render_layer")) {
        const auto& layer = item["render_layer"];
        const float opacity = layer.value("opacity", 1.0f);
        comp.render_opacity = std::isfinite(opacity)
                                  ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
        comp.render_hidden = layer.value("hidden", false);
        comp.render_group = layer.value("group", std::string());
      }
      comp.flip_x = item.value("flip_x", false);
      comp.flip_y = item.value("flip_y", false);
      comp.customLabel = item.value("custom_label", std::string());
      comp.show_custom_label = item.value("show_custom_label", true);
      if (def->InitDefaultState) {
        def->InitDefaultState(&comp);
      }
      if (item.contains("z_config")) {
        const auto& z = item["z_config"];
        comp.z_config = {z["min_layer"].get<float>(),
            z["max_layer"].get<float>(), z["initial_height"].get<float>()};
      }
      if (item.contains("axis_settings")) {
        const auto& axis = item["axis_settings"];
        comp.axis_settings = {axis["pulses_per_turn"].get<double>(),
            axis["lead_mm"].get<double>(), axis["stroke_mm"].get<double>(),
            axis["max_speed"].get<double>(), axis["acceleration"].get<double>()};
      }
      plc_emulator::InitializeZLayerState(&comp);
      ApplyElectricalDefaults(&comp);
      if (comp.type == ComponentType::FRL && item.contains("frl_pressure"))
        comp.internalStates["air_pressure"] = item["frl_pressure"].get<float>();
      if (comp.type == ComponentType::RTL_MODULE &&
          item.contains("rtl") && item["rtl"].is_object()) {
        const json& rtl = item["rtl"];
        comp.rtlModuleId = rtl.value("module_id", std::string());
        comp.rtlSourceMode = rtl.value("source_mode", std::string("library"));
        comp.rtlOverrideId = rtl.value("override_id", std::string());
        comp.rtlClockPinName = rtl.value("clock_pin_name", std::string());
        comp.rtlResetPinName = rtl.value("reset_pin_name", std::string());
        comp.rtlPowerPinName = rtl.value("power_pin_name", std::string());
        comp.rtlGroundPinName = rtl.value("ground_pin_name", std::string());
        RtlLogicFamilyFromString(
            rtl.value("logic_family", std::string("INDUSTRIAL_24V")),
            &comp.rtlLogicFamily);
        comp.rtlUseInternalClock = rtl.value("use_internal_clock", false);
        comp.rtlClockFrequencyHz = rtl.value("clock_frequency_hz", 1.0f);
        comp.rtlUseStartupReset = rtl.value("use_startup_reset", false);
        comp.rtlResetPulseMs = rtl.value("reset_pulse_ms", 20.0f);
        comp.rtlResetActiveLow = rtl.value("reset_active_low", false);
        RefreshRtlComponentFromLibrary(&comp);
      }
      if (IsSensorComponentType(comp.type)) {
        SensorOutputMode sensor_mode = GetSensorOutputMode(comp);
        SensorOutputModeFromString(item.value("sensor_output_mode", ""),
                                   &sensor_mode);
        SetSensorOutputMode(&comp, sensor_mode);
      }
      if (comp.type == ComponentType::kFiberAmplifier && item.contains("fiber_settings"))
        comp.fiber_settings = ReadFiberSettings(item["fiber_settings"]);
      if (comp.type == ComponentType::kTouchPanel && item.contains("hmi_settings"))
        comp.hmi_settings = ReadHmiSettings(item["hmi_settings"]);
      if (plc::IsWarehouseType(comp.type) && item.contains("warehouse_drive")) {
        comp.warehouse_cylinder_id = item["warehouse_drive"]["cylinder_id"].get<int>();
        comp.warehouse_travel = item["warehouse_drive"]["travel"].get<float>();
      }
      if (item.contains("stored_slot")) {
        comp.carry_state.owner_id = item["stored_slot"]["warehouse_id"].get<int>();
        comp.carry_state.slot_id = item["stored_slot"]["slot_id"].get<int>();
      }
      placed_components_.push_back(comp);

      max_instance_id = std::max(max_instance_id, comp.instanceId);
      max_z_order = std::max(max_z_order, comp.z_order);
    }
  }

  plc_emulator::UpdateWarehouseWorkpieces(&placed_components_, false);
  plc_emulator::UpdateRingFeedWorkpieces(&placed_components_);

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
      const std::string kind = item.value("wire_kind",
          item.value("is_electric", true) ? std::string("electric")
                                          : std::string("pneumatic"));
      if (kind != "electric" && kind != "pneumatic" && kind != "optical") {
        continue;
      }
      wire.kind = kind == "electric" ? WireKind::kElectric
          : kind == "pneumatic" ? WireKind::kPneumatic : WireKind::kOptical;
      wire.thickness = item.value("thickness", wire.kind == WireKind::kPneumatic ? 3.0f : 2.0f);
      wire.tagText = item.value("tag_text", std::string());
      wire.isTagged = item.value("is_tagged", !wire.tagText.empty());
      wire.tagColorIndex = item.value("tag_color_index", 0);

      const Color default_color = GetDefaultWireColor(wire.kind);
      if (item.contains("color") && item["color"].is_object()) {
        const json& color = item["color"];
        wire.color.r = color.value("r", default_color.r);
        wire.color.g = color.value("g", default_color.g);
        wire.color.b = color.value("b", default_color.b);
        wire.color.a = color.value("a", 1.0f);
      } else {
        wire.color = default_color;
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

      Port from_port;
      Port to_port;
      bool valid_from = false;
      bool valid_to = false;
      for (const auto& component : placed_components_) {
        if (component.instanceId != wire.fromComponentId &&
            component.instanceId != wire.toComponentId) {
          continue;
        }
        for (const Port& port : GetRuntimePortsForComponent(component)) {
          if (component.instanceId == wire.fromComponentId &&
              port.id == wire.fromPortId && GetWireKind(port.type) == wire.kind) {
            valid_from = true;
            from_port = port;
          }
          if (component.instanceId == wire.toComponentId &&
              port.id == wire.toPortId && GetWireKind(port.type) == wire.kind) {
            valid_to = true;
            to_port = port;
          }
        }
      }
      if (!valid_from || !valid_to ||
          !plc_emulator::CanConnectPorts(from_port, to_port)) {
        continue;
      }
      bool occupied = false;
      for (const auto& existing : wires_) {
        const auto uses = [&](int id, int port) {
          return (existing.fromComponentId == id && existing.fromPortId == port) ||
                 (existing.toComponentId == id && existing.toPortId == port);
        };
        occupied |= plc_emulator::HasSingleConnection(from_port) &&
                    uses(wire.fromComponentId, wire.fromPortId);
        occupied |= plc_emulator::HasSingleConnection(to_port) &&
                    uses(wire.toComponentId, wire.toPortId);
      }
      if (occupied) continue;
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

  OnElectricalConfigChanged();
  return true;
}

bool Application::SaveLayout(const std::string& file_path) {
  std::ofstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file << SerializeLayoutJson();
  return true;
}

bool Application::LoadLayout(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return DeserializeLayoutJson(buffer.str());
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
  CanonicalizeLadderProgram(&loaded_ladder_program_);




  if (programming_mode_) {



    std::cout << "[INFO] Syncing to Programming Mode for UI editing..."

              << std::endl;


    programming_mode_->SetLadderProgram(loadedProgram);
  }




  plc_device_states_.clear();

  plc_timer_states_.clear();

  plc_counter_states_.clear();




  for (int i = 0; i < 16; ++i) {

    std::string i_str = std::to_string(i);

    plc_device_states_[plc_emulator::programming::FormatIOAddress('X', i)] = false;

    plc_device_states_[plc_emulator::programming::FormatIOAddress('Y', i)] = false;

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

bool Application::LoadProgrammingProgram(
    const plc_emulator::programming::ExecutionProgram& program,
    const LadderProgram& ladder) {
  if (!compiled_plc_executor_) return false;
  LadderProgram candidate = ladder;
  if (!compiled_plc_executor_->LoadProgram(program)) return false;
  loaded_ladder_program_ = std::move(candidate);
  return true;
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

    OpenPLCCompilerIntegration compiler;
    compiler.SetDebugMode(enable_debug_logging_);
    compiler.SetIOConfiguration(16, 16);
    const auto compilationResult =
        compiler.CompileLadderProgramWithIR(loaded_ladder_program_);

    if (!compilationResult.success) {

      std::cout << "[ERROR] Failed to compile ladder: "

                << compilationResult.errorMessage << std::endl;

      return;

    }



    std::cout << "[INFO] .ld file compiled successfully" << std::endl;

    std::cout << "[INFO] Compilation statistics:" << std::endl;

    std::cout << "   - Structured program size: "

              << compilationResult.program.instructions.size() << " instructions"

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
  CanonicalizeLadderProgram(&loaded_ladder_program_);




  plc_device_states_.clear();

  for (int i = 0; i < 16; i++) {

    plc_device_states_[plc_emulator::programming::FormatIOAddress('X', i)] = false;

    plc_device_states_[plc_emulator::programming::FormatIOAddress('Y', i)] = false;

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


    LadderProgram currentProgram = BuildProgramForSave(programming_mode_.get());

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

bool Application::SaveProjectPackage(const std::string& filePath,
                                     const std::string& projectName) {
  std::cout << "[INFO] Saving project package to: " << filePath << std::endl;

  if (!programming_mode_ || !project_file_manager_) {
    std::cerr << "[ERROR] Project package save failed: components not initialized"
              << std::endl;
    return false;
  }

  try {
    LadderProgram currentProgram = BuildProgramForSave(programming_mode_.get());
    std::string layoutJson = SerializeLayoutJson();
    const auto rtl_module_ids = GetProjectRtlModuleIds(json::parse(layoutJson));
    std::string rtlLibraryJson = rtl_project_manager_
        ? rtl_project_manager_->SerializeProjectRtlJson(rtl_module_ids) : "";

    std::map<std::string, std::map<std::string, std::string>> rtlArtifacts;
    if (rtl_project_manager_) {
      for (const auto& entry : rtl_project_manager_->GetLibrary()) {
        if (entry.buildSuccess &&
            std::find(rtl_module_ids.begin(), rtl_module_ids.end(), entry.moduleId)
                != rtl_module_ids.end()) {
          auto files = rtl_project_manager_->GetBuildArtifactBundle(entry.moduleId);
          if (!files.empty()) {
            rtlArtifacts[entry.moduleId] = std::move(files);
          }
        }
      }
    }

    project_file_manager_->SetDebugMode(enable_debug_logging_);
    auto result = project_file_manager_->SaveProjectPackage(
        currentProgram, layoutJson, rtlLibraryJson, rtlArtifacts, filePath, projectName);

    if (!result.success) {
      std::cerr << "[ERROR] Project package save failed: "
                << result.errorMessage << std::endl;
      return false;
    }

    std::cout << "[INFO] Project package saved successfully!" << std::endl;
    std::cout << "   - Name: " << result.info.projectName << std::endl;
    std::cout << "   - Size: " << result.compressedSize << " bytes"
              << std::endl;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] Project package save exception: " << e.what()
              << std::endl;
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
    CanonicalizeLadderProgram(&loaded_ladder_program_);




    std::cout << "[DEBUG] Updating ProgrammingMode with loaded program..."

              << std::endl;

    programming_mode_->SetLadderProgram(result.program);




    plc_device_states_.clear();

    plc_timer_states_.clear();

    plc_counter_states_.clear();




    for (int i = 0; i <= 15; ++i) {

      plc_device_states_[plc_emulator::programming::FormatIOAddress('X', i)] = false;

      plc_device_states_[plc_emulator::programming::FormatIOAddress('Y', i)] = false;

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

bool Application::LoadProjectPackage(const std::string& filePath) {
  std::cout << "[INFO] Loading project package from: " << filePath
            << std::endl;

  if (!programming_mode_ || !project_file_manager_) {
    std::cerr << "[ERROR] Project package load failed: components not initialized"
              << std::endl;
    return false;
  }

  try {
    project_file_manager_->SetDebugMode(enable_debug_logging_);
    auto result = project_file_manager_->LoadProjectPackage(filePath);
    if (!result.success) {
      std::cerr << "[ERROR] Project package load failed: "
                << result.errorMessage << std::endl;
      return false;
    }
    if (rtl_project_manager_) {
      if (!result.rtlLibraryJson.empty()) {
        const auto rtl_module_ids =
            GetProjectRtlModuleIds(json::parse(result.layoutJson));
        json project_library = json::parse(result.rtlLibraryJson);
        auto entries = project_library.find("entries");
        if (entries != project_library.end() && entries->is_array()) {
          entries->erase(std::remove_if(entries->begin(), entries->end(),
              [&](const json& entry) {
                return !entry.is_object() ||
                    std::find(rtl_module_ids.begin(), rtl_module_ids.end(),
                              entry.value("module_id", "")) == rtl_module_ids.end();
              }), entries->end());
        }
        rtl_project_manager_->DeserializeLibraryJson(project_library.dump(), true);
        
        // Restore artifacts first so HasBuildArtifact returns true
        for (const auto& [moduleId, files] : result.rtlArtifacts) {
          if (std::find(rtl_module_ids.begin(), rtl_module_ids.end(), moduleId)
              == rtl_module_ids.end()) continue;
          const auto* entry = rtl_project_manager_->FindEntryById(moduleId);
          if (entry && !rtl_project_manager_->HasBuildArtifact(*entry))
            rtl_project_manager_->RestoreBuildArtifactBundle(moduleId, files);
        }

        for (auto& entry : rtl_project_manager_->GetLibrary()) {
          if (std::find(rtl_module_ids.begin(), rtl_module_ids.end(), entry.moduleId)
              == rtl_module_ids.end()) continue;
          if (entry.buildSuccess &&
              !rtl_project_manager_->HasBuildArtifact(entry)) {
            entry.buildSuccess = false;
            if (entry.buildDiagnostics.empty()) {
              entry.buildDiagnostics =
                  "RTL worker artifact was not found on this machine. Rebuild the module.";
            } else {
              entry.buildDiagnostics +=
                  "\nRTL worker artifact was not found on this machine. Rebuild the module.";
            }
          }
        }
      }
    }
    if (result.layoutJson.empty() ||
        !DeserializeLayoutJson(result.layoutJson)) {
      std::cerr << "[ERROR] Project package load failed: invalid layout JSON"
                << std::endl;
      return false;
    }

    loaded_ladder_program_ = result.program;
    CanonicalizeLadderProgram(&loaded_ladder_program_);
    if (rtl_project_manager_) {
      for (auto& comp : placed_components_) {
        if (comp.type == ComponentType::RTL_MODULE) {
          RefreshRtlComponentFromLibrary(&comp);
        }
      }
    }
    programming_mode_->SetLadderProgram(result.program);

    plc_device_states_.clear();
    plc_timer_states_.clear();
    plc_counter_states_.clear();

    for (int i = 0; i <= 15; ++i) {
      plc_device_states_[plc_emulator::programming::FormatIOAddress('X', i)] = false;
      plc_device_states_[plc_emulator::programming::FormatIOAddress('Y', i)] = false;
    }
    for (int i = 0; i <= 999; ++i) {
      std::string i_str = std::to_string(i);
      plc_device_states_["M" + i_str] = false;
    }

    if (compiled_plc_executor_) {
      CompileAndLoadLadderProgram();
    }

    std::cout << "[INFO] Project package loaded successfully!" << std::endl;
    std::cout << "   - Name: " << result.info.projectName << std::endl;
    std::cout << "   - Version: " << result.info.version << std::endl;
    std::cout << "   - Layout checksum: " << result.info.layoutChecksum
              << std::endl;
    std::cout << "   - Program checksum: " << result.info.programChecksum
              << std::endl;
    RequestCacheWarmup();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] Project package load exception: " << e.what()
              << std::endl;
    return false;
  }
}



}  // namespace plc
