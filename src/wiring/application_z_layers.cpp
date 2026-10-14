#include "plc_emulator/core/application.h"

#include "plc_emulator/components/servo_def.h"
#include "plc_emulator/components/storage_sensor_def.h"
#include "plc_emulator/physics/storage_sensor_simulation.h"

#include "plc_emulator/components/sensor_cylinder_def.h"

#include <cfloat>
#include <string>

#include "imgui.h"

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/physics/z_layer.h"

namespace plc {

bool Application::ApplyZLayerConfig(int component_id,
                                   const ZLayerConfig& config) {
  if (!physics_paused_ || !plc_emulator::IsValidZLayerConfig(config)) {
    return false;
  }
  PlacedComponent* component = FindComponentById(component_id);
  if (!component) {
    return false;
  }
  if (component->type != ComponentType::PROCESSING_CYLINDER &&
      component->type != ComponentType::kStopperCylinder &&
      !plc_emulator::IsServoLift(component->type) &&
      !plc::IsWarehouseType(component->type) &&
      config.min_layer != config.max_layer) {
    return false;
  }
  if (plc::IsWarehouseType(component->type)) {
    if (config.min_layer >= config.max_layer) return false;
    for (const auto& placed : placed_components_) {
      if (placed.carry_state.owner_id == component_id) return false;
    }
  }
  PushWiringUndoState();
  component->z_config = config;
  plc_emulator::InitializeZLayerState(component);
  for (auto& placed : placed_components_) {
    if (placed.instanceId == component_id ||
        placed.carry_state.owner_id == component_id) {
      placed.carry_state = {};
    }
    if (placed.type == ComponentType::kServoAmplifier &&
        placed.axis_state.load_id == component_id) {
      placed.axis_state = {};
    }
    for (auto& [key, value] : placed.internalStates) {
      if (key.starts_with("snap_lock_") || key.starts_with("proc_ready_") ||
          key == "is_contacted") {
        value = 0.0f;
      }
    }
  }
  return true;
}

bool Application::ApplyAxisSettings(int component_id,
                                     const AxisSettings& settings) {
  PlacedComponent* component = FindComponentById(component_id);
  if (!physics_paused_ || !component ||
      (!plc_emulator::IsServoLoad(component->type) &&
       component->type != ComponentType::kDcMotor) ||
      !plc_emulator::IsValidAxisSettings(settings)) return false;
  PushWiringUndoState();
  component->axis_settings = settings;
  component->axis_state = {};
  plc_emulator::InitializeZLayerState(component);
  for (auto& placed : placed_components_) {
    if (placed.type == ComponentType::kServoAmplifier &&
        placed.axis_state.load_id == component_id) placed.axis_state = {};
    if (placed.carry_state.owner_id == component_id) {
      placed.carry_state = {};
    }
  }
  return true;
}

void Application::RenderZLayerPanel() {
  if (!show_z_layer_panel_) {
    return;
  }
  ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(TR("ui.z_layers.title", "Physical Z layers"),
                    &show_z_layer_panel_)) {
    ImGui::End();
    return;
  }
  RegisterOverlayInputCaptureRect(ImGui::GetWindowPos(),
      ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x,
             ImGui::GetWindowPos().y + ImGui::GetWindowSize().y),
      ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
          ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
  ImGui::Checkbox(TR("ui.z_layers.pause", "Pause simulation to edit"),
                  &physics_paused_);
  ImGui::TextWrapped("%s", TR("ui.z_layers.description",
      "Physical height is independent of drawing order."));
  ImGui::BeginChild("ZComponentList", ImVec2(0, 200), true);
  for (const auto& component : placed_components_) {
    const ComponentDefinition* definition =
        GetComponentDefinition(component.type);
    const std::string label = std::to_string(component.instanceId) + "  " +
        (definition ? TR(definition->display_name, definition->display_name)
                    : "Component");
    ImGui::PushID(component.instanceId);
    if (ImGui::Selectable(label.c_str(),
                          component.instanceId == z_edit_component_id_)) {
      z_edit_component_id_ = component.instanceId;
      selected_component_id_ = component.instanceId;
      for (auto& placed : placed_components_) {
        placed.selected = placed.instanceId == component.instanceId;
      }
      z_edit_config_ = component.z_config;
      axis_edit_settings_ = component.axis_settings;
      fiber_edit_settings_ = component.fiber_settings;
      hmi_edit_settings_ = component.hmi_settings;
    }
    ImGui::SameLine();
    ImGui::Text("Z %.2f", plc_emulator::ResolveContactHeight(component));
    ImGui::PopID();
  }
  ImGui::EndChild();
  const PlacedComponent* component = FindComponentById(z_edit_component_id_);
  if (component) {
    ImGui::Text("Z %.2f", plc_emulator::ResolveContactHeight(*component));
    ImGui::BeginDisabled(!physics_paused_);
    if (component->type == ComponentType::PROCESSING_CYLINDER ||
        component->type == ComponentType::kStopperCylinder ||
        plc_emulator::IsServoLift(component->type) ||
        plc::IsWarehouseType(component->type)) {
      ImGui::InputFloat(TR("ui.z_layers.minimum", "Minimum"),
                        &z_edit_config_.min_layer);
      ImGui::InputFloat(TR("ui.z_layers.maximum", "Maximum"),
                        &z_edit_config_.max_layer);
      ImGui::InputFloat(TR("ui.z_layers.initial", "Initial height"),
                        &z_edit_config_.initial_height);
    } else {
      ImGui::InputFloat(TR("ui.z_layers.height", "Installation height"),
                        &z_edit_config_.initial_height);
      z_edit_config_.min_layer = z_edit_config_.initial_height;
      z_edit_config_.max_layer = z_edit_config_.initial_height;
    }
    const bool valid = plc_emulator::IsValidZLayerConfig(z_edit_config_);
    ImGui::BeginDisabled(!valid);
    if (ImGui::Button(TR("ui.z_layers.apply", "Apply height"))) {
      ApplyZLayerConfig(z_edit_component_id_, z_edit_config_);
    }
    ImGui::EndDisabled();
    if (!valid) {
      ImGui::TextUnformatted(TR("ui.z_layers.invalid", "Invalid height range"));
    }
    if (plc_emulator::IsServoLoad(component->type) ||
        component->type == ComponentType::kDcMotor) {
      ImGui::Separator();
      ImGui::TextUnformatted(TR("ui.axis.settings", "Axis settings"));
      ImGui::InputDouble(TR("ui.axis.pulses", "Pulses per revolution"),
                          &axis_edit_settings_.pulses_per_turn);
      if (plc_emulator::IsServoLift(component->type)) {
        ImGui::InputDouble(TR("ui.axis.lead", "Lead (mm/revolution)"),
                            &axis_edit_settings_.lead_mm);
        ImGui::InputDouble(TR("ui.axis.stroke", "Stroke (mm)"),
                            &axis_edit_settings_.stroke_mm);
      }
      ImGui::InputDouble(TR("ui.axis.speed", "Maximum speed (rev/s)"),
                          &axis_edit_settings_.max_speed);
      ImGui::InputDouble(TR("ui.axis.acceleration", "Acceleration (rev/s2)"),
                          &axis_edit_settings_.acceleration);
      ImGui::BeginDisabled(!plc_emulator::IsValidAxisSettings(axis_edit_settings_));
      if (ImGui::Button(TR("ui.axis.apply", "Apply axis settings"))) {
        ApplyAxisSettings(component->instanceId, axis_edit_settings_);
      }
      ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
  }
  ImGui::End();
}

void Application::RenderPneumaticFrontPreview() {
  for (auto& part : placed_components_) {
    if (part.type != ComponentType::kTouchPanel ||
        (show_pneumatic_front_ && part.instanceId == pneumatic_front_component_id_)) continue;
    for (int i = 0; i < static_cast<int>(part.hmi_settings.widgets.size()); ++i)
      if (part.hmi_state.held[i]) SetHmiButton(part.instanceId, i, false);
  }
  if (!show_pneumatic_front_) return;
  const PlacedComponent* preview =
      FindComponentById(pneumatic_front_component_id_);
  const bool is_hmi = preview && preview->type == ComponentType::kTouchPanel;
  ImGui::SetNextWindowSize(is_hmi ? ImVec2(640, 560) : ImVec2(410, 310),
                           ImGuiCond_FirstUseEver);
  if (is_hmi) ImGui::SetNextWindowSizeConstraints({480, 440}, {FLT_MAX, FLT_MAX});
  if (ImGui::Begin(is_hmi ? TR("ui.hmi.operate", "Operate HMI")
                          : TR("ui.wiring.context.front_preview", "Front view"),
                   &show_pneumatic_front_)) {
    const PlacedComponent* component =
        FindComponentById(pneumatic_front_component_id_);
    if (component) {
      ImGui::Text("Z %.2f", plc_emulator::ResolveContactHeight(*component));
      if (component->type != ComponentType::kTouchPanel) {
        if (plc_emulator::IsStorageSensorPart(component->type)) {
          plc_emulator::RenderStorageSensorFront(ImGui::GetWindowDrawList(),
              *component, ImGui::GetCursorScreenPos(), 1.0f, placed_components_);
        } else if (plc_emulator::IsServoLift(component->type)) {
          plc_emulator::RenderServoFront(ImGui::GetWindowDrawList(), *component,
                                         ImGui::GetCursorScreenPos(), 1.0f);
        } else {
          plc_emulator::RenderPneumaticFront(ImGui::GetWindowDrawList(),
              *component, ImGui::GetCursorScreenPos(), 1.0f);
        }
        ImGui::Dummy({340, component->type == ComponentType::kFiberAmplifier
                              ? 80.0f : 280.0f});
      }
      RenderStorageSensorControls(component->instanceId);
    }
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    RegisterOverlayInputCaptureRect(position,
        ImVec2(position.x + size.x, position.y + size.y),
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
  } else {
    auto* panel = FindComponentById(pneumatic_front_component_id_);
    if (panel && panel->type == ComponentType::kTouchPanel) {
      for (int i = 0; i < static_cast<int>(panel->hmi_settings.widgets.size()); ++i)
        if (panel->hmi_state.held[i]) SetHmiButton(panel->instanceId, i, false);
    }
  }
  ImGui::End();
}

}  // namespace plc
