#include "plc_emulator/components/component_behavior.h"

#include "imgui.h"
#include "plc_emulator/components/component_input_resolver.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>
#include <string>

namespace plc {
namespace {

ComponentInteractionResult UnhandledInteraction() {
  return {};
}

ComponentInteractionResult MakeHandledInteraction(
    ComponentInteractionCommand command) {
  ComponentInteractionResult result;
  result.handled = true;
  result.commands.push_back(command);
  return result;
}

bool IsPointInsideCircle(ImVec2 point, ImVec2 center, float radius) {
  float dx = point.x - center.x;
  float dy = point.y - center.y;
  return (dx * dx + dy * dy) <= (radius * radius);
}

constexpr float kButtonUnitCenterX = 25.0f;
constexpr float kButtonUnitBaseY = 20.0f;
constexpr float kButtonUnitSpacingY = 30.0f;
constexpr float kButtonUnitRadius = 9.0f;
constexpr float kEmergencyStopRadius = 26.0f;


ComponentInteractionResult HandleButtonUnitMouseDown(
    const PlacedComponent& comp, ImVec2 world_pos, int button) {
  if (button != ImGuiMouseButton_Left) {
    return UnhandledInteraction();
  }
  ImVec2 local(world_pos.x - comp.position.x,
               world_pos.y - comp.position.y);
  for (int i = 0; i < 2; ++i) {
    float y_offset = kButtonUnitBaseY + static_cast<float>(i) *
                                          kButtonUnitSpacingY;
    if (IsPointInsideCircle(local, ImVec2(kButtonUnitCenterX, y_offset),
                            kButtonUnitRadius)) {
      ComponentInteractionCommand command;
      command.type = ComponentInteractionCommandType::SetButtonPressed;
      command.button_index = i;
      command.pressed = true;
      return MakeHandledInteraction(command);
    }
  }
  return UnhandledInteraction();
}

ComponentInteractionResult HandleButtonUnitMouseUp(
    const PlacedComponent& comp, ImVec2 world_pos, int button) {
  (void)comp;
  (void)world_pos;
  if (button != ImGuiMouseButton_Left) {
    return UnhandledInteraction();
  }
  ComponentInteractionCommand command;
  command.type = ComponentInteractionCommandType::ReleaseButtonMomentary;
  return MakeHandledInteraction(command);
}

ComponentInteractionResult HandleButtonUnitDoubleClick(
    const PlacedComponent& comp, ImVec2 world_pos, int button) {
  if (button != ImGuiMouseButton_Left) {
    return UnhandledInteraction();
  }
  ImVec2 local(world_pos.x - comp.position.x,
               world_pos.y - comp.position.y);
  float y_offset = kButtonUnitBaseY + 2.0f * kButtonUnitSpacingY;
  if (!IsPointInsideCircle(local, ImVec2(kButtonUnitCenterX, y_offset),
                           kButtonUnitRadius)) {
    return UnhandledInteraction();
  }
  ComponentInteractionCommand command;
  command.type = ComponentInteractionCommandType::ToggleButtonLatch;
  command.button_index = 2;
  return MakeHandledInteraction(command);
}

ComponentInteractionResult HandleFrlMouseWheel(const PlacedComponent& comp,
                                               ImVec2 world_pos,
                                               const ImGuiIO& io) {
  (void)world_pos;
  if (io.MouseWheel == 0.0f) {
    return UnhandledInteraction();
  }

  float current_pressure = comp.internalStates.count(state_keys::kAirPressure)
                               ? comp.internalStates.at(state_keys::kAirPressure)
                               : 6.0f;
  float step = io.KeyCtrl ? 0.1f : 0.5f;
  if (io.KeyShift) {
    step *= 0.5f;
  }

  if (io.MouseWheel > 0) {
    current_pressure += step;
  } else {
    current_pressure -= step;
  }

  current_pressure = std::max(0.0f, std::min(10.0f, current_pressure));
  ComponentInteractionCommand command;
  command.type = ComponentInteractionCommandType::SetAirPressure;
  command.scalar = current_pressure;
  return MakeHandledInteraction(command);
}

ComponentInteractionResult HandleMeterValveMouseWheel(
    const PlacedComponent& comp, ImVec2 world_pos, const ImGuiIO& io) {
  (void)world_pos;
  if (io.MouseWheel == 0.0f) {
    return UnhandledInteraction();
  }

  float current = comp.internalStates.count(state_keys::kFlowSetting)
                      ? comp.internalStates.at(state_keys::kFlowSetting)
                      : 1.0f;
  float step = io.KeyCtrl ? 0.005f : 0.02f;
  if (io.KeyShift) {
    step *= 0.5f;
  }

  if (io.MouseWheel > 0) {
    current += step;
  } else {
    current -= step;
  }

  current = std::max(0.0f, std::min(1.0f, current));
  ComponentInteractionCommand command;
  command.type = ComponentInteractionCommandType::SetFlowSetting;
  command.scalar = current;
  return MakeHandledInteraction(command);
}

ComponentInteractionResult HandleEmergencyStopDoubleClick(
    const PlacedComponent& comp, ImVec2 world_pos, int button) {
  if (button != ImGuiMouseButton_Left) {
    return UnhandledInteraction();
  }
  ImVec2 local(world_pos.x - comp.position.x,
               world_pos.y - comp.position.y);
  if (!IsPointInsideCircle(local, ImVec2(40.0f, 35.0f),
                           kEmergencyStopRadius)) {
    return UnhandledInteraction();
  }
  ComponentInteractionCommand command;
  command.type = ComponentInteractionCommandType::ToggleEmergencyStop;
  return MakeHandledInteraction(command);
}

bool BuildLimitSwitchTooltip(const PlacedComponent& comp, ImVec2 world_pos) {
  (void)world_pos;
  bool is_pressed = component_input::IsLimitSwitchPressed(comp);
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
    if (component_input::IsButtonUnitPressed(comp, i)) {
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

void ApplyComponentInteractionResult(PlacedComponent* comp,
                                     const ComponentInteractionResult& result) {
  if (!comp || !result.handled) {
    return;
  }

  for (const auto& command : result.commands) {
    switch (command.type) {
      case ComponentInteractionCommandType::SetButtonPressed:
        component_input::SetButtonUnitPressed(comp, command.button_index,
                                              command.pressed);
        break;
      case ComponentInteractionCommandType::ReleaseButtonMomentary:
        component_input::ReleaseButtonUnitMomentaryButtons(comp);
        break;
      case ComponentInteractionCommandType::ToggleButtonLatch:
        component_input::ToggleButtonUnitLatch(comp, command.button_index);
        break;
      case ComponentInteractionCommandType::ToggleEmergencyStop:
        component_input::ToggleEmergencyStopPressed(comp);
        break;
      case ComponentInteractionCommandType::ToggleLimitSwitchManualOverride:
        component_input::ToggleLimitSwitchManualOverride(comp);
        break;
      case ComponentInteractionCommandType::SetAirPressure:
        comp->internalStates[state_keys::kAirPressure] = command.scalar;
        break;
      case ComponentInteractionCommandType::SetFlowSetting:
        comp->internalStates[state_keys::kFlowSetting] = command.scalar;
        break;
      case ComponentInteractionCommandType::None:
      default:
        break;
    }
  }
}

const ComponentBehavior* GetComponentBehavior(ComponentType type) {
  constexpr int kComponentTypeCount = 21;
  static_assert(static_cast<int>(ComponentType::RTL_MODULE) ==
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
      {nullptr, nullptr, nullptr, nullptr, BuildLimitSwitchTooltip},
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
      {nullptr, nullptr, HandleEmergencyStopDoubleClick, nullptr, nullptr},
      // RTL_MODULE
      {nullptr, nullptr, nullptr, nullptr, nullptr}};

  int index = static_cast<int>(type);
  if (index < 0 || index >= kComponentTypeCount) {
    return nullptr;
  }
  return &kBehaviors[index];
}

}  // namespace plc
