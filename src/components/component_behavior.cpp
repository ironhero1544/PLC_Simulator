#include "plc_emulator/components/component_behavior.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>
#include <string>

namespace plc {
namespace {

bool HandleLimitSwitchShiftClick(PlacedComponent* comp, ImVec2 world_pos,
                                 int button) {
  (void)world_pos;
  if (!comp || button != ImGuiMouseButton_Left) {
    return false;
  }
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.KeyShift || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    return false;
  }
  float current = comp->internalStates.count(state_keys::kIsPressedManual)
                      ? comp->internalStates.at(state_keys::kIsPressedManual)
                      : 0.0f;
  comp->internalStates[state_keys::kIsPressedManual] = 1.0f - current;
  return true;
}

bool HandleButtonUnitMouseDown(PlacedComponent* comp, ImVec2 world_pos,
                               int button) {
  if (!comp || button != ImGuiMouseButton_Left) {
    return false;
  }
  bool handled = false;
  float local_y = world_pos.y - comp->position.y;
  for (int i = 0; i < 2; ++i) {
    float y_offset = 20.0f + static_cast<float>(i) * 30.0f;
    if (local_y > y_offset - 8.0f && local_y < y_offset + 8.0f) {
      std::string key = std::string(state_keys::kPressedPrefix) +
                        std::to_string(i);
      comp->internalStates[key] = 1.0f;
      handled = true;
    }
  }
  return handled;
}

bool HandleButtonUnitMouseUp(PlacedComponent* comp, ImVec2 world_pos,
                             int button) {
  (void)world_pos;
  if (!comp || button != ImGuiMouseButton_Left) {
    return false;
  }
  comp->internalStates[std::string(state_keys::kPressedPrefix) + "0"] = 0.0f;
  comp->internalStates[std::string(state_keys::kPressedPrefix) + "1"] = 0.0f;
  return true;
}

bool HandleButtonUnitDoubleClick(PlacedComponent* comp, ImVec2 world_pos,
                                 int button) {
  if (!comp || button != ImGuiMouseButton_Left) {
    return false;
  }
  float local_y = world_pos.y - comp->position.y;
  if (local_y >= 70.0f && local_y <= 90.0f) {
    std::string key = std::string(state_keys::kPressedPrefix) + "2";
    float current = comp->internalStates.count(key)
                        ? comp->internalStates.at(key)
                        : 0.0f;
    comp->internalStates[key] = 1.0f - current;
  }
  return true;
}

bool HandleFrlMouseWheel(PlacedComponent* comp, ImVec2 world_pos,
                         const ImGuiIO& io) {
  (void)world_pos;
  if (!comp || io.MouseWheel == 0.0f) {
    return false;
  }

  float current_pressure = comp->internalStates.count(state_keys::kAirPressure)
                               ? comp->internalStates.at(state_keys::kAirPressure)
                               : 6.0f;
  float step = io.KeyCtrl ? 0.1f : 0.5f;

  if (io.MouseWheel > 0) {
    current_pressure += step;
  } else {
    current_pressure -= step;
  }

  current_pressure = std::max(0.0f, std::min(10.0f, current_pressure));
  comp->internalStates[state_keys::kAirPressure] = current_pressure;
  return true;
}

bool HandleMeterValveMouseWheel(PlacedComponent* comp, ImVec2 world_pos,
                                const ImGuiIO& io) {
  (void)world_pos;
  if (!comp || io.MouseWheel == 0.0f) {
    return false;
  }

  float current = comp->internalStates.count(state_keys::kFlowSetting)
                      ? comp->internalStates.at(state_keys::kFlowSetting)
                      : 1.0f;
  float step = io.KeyCtrl ? 0.005f : 0.02f;

  if (io.MouseWheel > 0) {
    current += step;
  } else {
    current -= step;
  }

  current = std::max(0.0f, std::min(1.0f, current));
  comp->internalStates[state_keys::kFlowSetting] = current;
  return true;
}

bool HandleEmergencyStopDoubleClick(PlacedComponent* comp, ImVec2 world_pos,
                                    int button) {
  (void)world_pos;
  if (!comp || button != ImGuiMouseButton_Left) {
    return false;
  }
  float current = comp->internalStates.count(state_keys::kIsPressed)
                      ? comp->internalStates.at(state_keys::kIsPressed)
                      : 0.0f;
  comp->internalStates[state_keys::kIsPressed] = 1.0f - current;
  return true;
}

bool BuildLimitSwitchTooltip(const PlacedComponent& comp, ImVec2 world_pos) {
  (void)world_pos;
  bool is_pressed = comp.internalStates.count(state_keys::kIsPressed) &&
                    comp.internalStates.at(state_keys::kIsPressed) > 0.5f;
  ImGui::BeginTooltip();
  char tooltip_buf[256] = {0};
  FormatString(tooltip_buf, sizeof(tooltip_buf),
               "ui.wiring.tooltip_state_fmt", "State: %s",
               is_pressed ? TR("ui.wiring.state_pressed", "Pressed")
                          : TR("ui.wiring.state_released", "Released"));
  ImGui::TextUnformatted(tooltip_buf);
  ImGui::EndTooltip();
  return true;
}

bool BuildButtonUnitTooltip(const PlacedComponent& comp, ImVec2 world_pos) {
  (void)world_pos;
  bool is_pressed = false;
  for (int i = 0; i < 3; ++i) {
    std::string key = std::string(state_keys::kPressedPrefix) +
                      std::to_string(i);
    if (comp.internalStates.count(key) &&
        comp.internalStates.at(key) > 0.5f) {
      is_pressed = true;
      break;
    }
  }

  ImGui::BeginTooltip();
  char tooltip_buf[256] = {0};
  FormatString(tooltip_buf, sizeof(tooltip_buf),
               "ui.wiring.tooltip_state_fmt", "State: %s",
               is_pressed ? TR("ui.wiring.state_pressed", "Pressed")
                          : TR("ui.wiring.state_released", "Released"));
  ImGui::TextUnformatted(tooltip_buf);
  ImGui::EndTooltip();
  return true;
}

bool BuildFrlTooltip(const PlacedComponent& comp, ImVec2 world_pos) {
  (void)world_pos;
  float pressure = comp.internalStates.count(state_keys::kAirPressure)
                       ? comp.internalStates.at(state_keys::kAirPressure)
                       : 6.0f;
  ImGui::BeginTooltip();
  char tooltip_buf[256] = {0};
  FormatString(tooltip_buf, sizeof(tooltip_buf),
               "ui.wiring.tooltip_pressure_fmt",
               "Current Pressure: %.1f bar",
               pressure);
  ImGui::TextUnformatted(tooltip_buf);
  ImGui::TextDisabled("%s",
                      TR("ui.wiring.tooltip_pressure_hint",
                         "Use mouse wheel to adjust."));
  ImGui::EndTooltip();
  return true;
}

bool BuildMeterValveTooltip(const PlacedComponent& comp, ImVec2 world_pos) {
  (void)world_pos;
  bool meter_in = true;
  if (comp.internalStates.count(state_keys::kMeterMode)) {
    meter_in = comp.internalStates.at(state_keys::kMeterMode) < 0.5f;
  }

  ImGui::BeginTooltip();
  ImGui::TextUnformatted(meter_in ? "Meter: IN" : "Meter: OUT");
  ImGui::EndTooltip();
  return true;
}

}  // namespace

const ComponentBehavior* GetComponentBehavior(ComponentType type) {
  constexpr int kComponentTypeCount = 20;
  static_assert(static_cast<int>(ComponentType::EMERGENCY_STOP) ==
                    kComponentTypeCount - 1,
                "Update component behavior table for new component types.");

  static const ComponentBehavior kBehaviors[kComponentTypeCount] = {
      // PLC
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // FRL
      {nullptr, nullptr, nullptr, HandleFrlMouseWheel, BuildFrlTooltip},
      // MANIFOLD
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // LIMIT_SWITCH
      {HandleLimitSwitchShiftClick, nullptr, nullptr, nullptr,
       BuildLimitSwitchTooltip},
      // SENSOR
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // CYLINDER
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // VALVE_SINGLE
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // VALVE_DOUBLE
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // BUTTON_UNIT
      {HandleButtonUnitMouseDown, HandleButtonUnitMouseUp,
       HandleButtonUnitDoubleClick, nullptr, BuildButtonUnitTooltip},
      // POWER_SUPPLY
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // WORKPIECE_METAL
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // WORKPIECE_NONMETAL
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // RING_SENSOR
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // METER_VALVE
      {nullptr, nullptr, nullptr, HandleMeterValveMouseWheel,
       BuildMeterValveTooltip},
      // INDUCTIVE_SENSOR
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // CONVEYOR
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // PROCESSING_CYLINDER
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // BOX
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // TOWER_LAMP
      {nullptr, nullptr, nullptr, nullptr, nullptr},
      // EMERGENCY_STOP
      {nullptr, nullptr, HandleEmergencyStopDoubleClick, nullptr, nullptr}};

  int index = static_cast<int>(type);
  if (index < 0 || index >= kComponentTypeCount) {
    return nullptr;
  }
  return &kBehaviors[index];
}

}  // namespace plc
