#include "plc_emulator/components/pneumatic_component.h"
#include "imgui.h"
#include "plc_emulator/components/storage_sensor_def.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/physics/storage_sensor_simulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace plc {
bool Application::ApplyWarehouseDrive(int component_id, int cylinder_id,
                                      float travel) {
  auto* warehouse = FindComponentById(component_id);
  const auto* cylinder = FindComponentById(cylinder_id);
  if (!physics_paused_ || !warehouse ||
      warehouse->type != ComponentType::kWarehouse || !std::isfinite(travel) ||
      travel <= 0 || travel > 10000 ||
      (cylinder_id != -1 &&
       (!cylinder || (cylinder->type != ComponentType::CYLINDER &&
                      cylinder->type != ComponentType::kSensorCylinder))))
    return false;
  PushWiringUndoState();
  warehouse->warehouse_cylinder_id = cylinder_id;
  warehouse->warehouse_travel = travel;
  plc_emulator::UpdateWarehouseWorkpieces(&placed_components_, false);
  return true;
}
bool Application::ApplyFiberSettings(int component_id,
                                     const FiberSettings& settings) {
  auto* component = FindComponentById(component_id);
  if (!physics_paused_ || !component ||
      component->type != ComponentType::kFiberAmplifier ||
      !plc_emulator::IsValidFiberSettings(settings))
    return false;
  PushWiringUndoState();
  component->fiber_settings = settings;
  OnElectricalConfigChanged();
  return true;
}
bool Application::ApplyHmiSettings(int component_id,
                                   const HmiSettings& settings) {
  auto* component = FindComponentById(component_id);
  if (!physics_paused_ || !component ||
      component->type != ComponentType::kTouchPanel ||
      !plc_emulator::IsValidHmiSettings(settings))
    return false;
  PushWiringUndoState();
  ReleaseHmiButtons(component);
  component->hmi_settings = settings;
  component->hmi_state = {};
  return true;
}
void Application::ReleaseHmiButtons(PlacedComponent* component) {
  if (!component || component->type != ComponentType::kTouchPanel)
    return;
  for (size_t i = 0; i < component->hmi_settings.widgets.size(); ++i) {
    if (component->hmi_state.held[i])
      SetPlcDeviceState(component->hmi_settings.widgets[i].address, false);
    component->hmi_state.held[i] = false;
  }
}
bool Application::SetHmiButton(int component_id, int button, bool pressed) {
  auto* component = FindComponentById(component_id);
  if (!component || component->type != ComponentType::kTouchPanel ||
      button < 0 ||
      static_cast<size_t>(button) >= component->hmi_settings.widgets.size() ||
      !plc_emulator::IsValidHmiSettings(component->hmi_settings))
    return false;
  const auto& widget = component->hmi_settings.widgets[button];
  if (widget.type != HmiWidgetType::kButton ||
      ((widget.page != component->hmi_state.active_page ||
        component->hmi_state.hidden[button]) &&
       (pressed || !component->hmi_state.held[button])))
    return false;
  const bool online = is_plc_running_ && compiled_plc_executor_ &&
                      plc_emulator::FindHmiPlc(*component, placed_components_,
                                               port_voltages_) >= 0;
  if ((!online || physics_paused_) && pressed)
    return false;
  if (!online && !component->hmi_state.held[button])
    return false;
  if (widget.target_page >= 0) {
    if (pressed) {
      ReleaseHmiButtons(component);
      component->hmi_state.active_page = widget.target_page;
    }
    return true;
  }
  SetPlcDeviceState(widget.address, pressed);
  component->hmi_state.held[button] = widget.momentary && pressed;
  component->hmi_state.values[button] = pressed;
  return true;
}
void Application::UpdateStorageSensorDevices() {
  static thread_local std::vector<std::pair<size_t, bool>> previous;
  previous.clear();
  previous.reserve(placed_components_.size());
  for (size_t i = 0; i < placed_components_.size(); ++i) {
    if (placed_components_[i].type == ComponentType::kFiberAmplifier)
      previous.emplace_back(i, placed_components_[i].fiber_state.output);
  }
  plc_emulator::UpdateFiberSensors(&placed_components_, wires_, port_voltages_);
  bool changed = false;
  for (const auto& [index, output] : previous)
    changed |= placed_components_[index].fiber_state.output != output;
  if (changed)
    SimulateElectrical();
  for (auto& part : placed_components_) {
    if (part.type != ComponentType::kTouchPanel)
      continue;
    const int linked =
        plc_emulator::FindHmiPlc(part, placed_components_, port_voltages_);
    const bool online = linked >= 0 && is_plc_running_ &&
                        compiled_plc_executor_ &&
                        plc_emulator::IsValidHmiSettings(part.hmi_settings);
    if (!online || (part.hmi_state.online && part.hmi_state.plc_id != linked)) {
      ReleaseHmiButtons(&part);
      part.hmi_state = {};
    }
    part.hmi_state.online = online;
    part.hmi_state.plc_id = online ? linked : -1;
    if (!online)
      continue;
    if (physics_paused_)
      ReleaseHmiButtons(&part);
    part.hmi_state.value_valid = false;
    for (size_t i = 0; i < part.hmi_settings.widgets.size(); ++i) {
      const auto& widget = part.hmi_settings.widgets[i];
      part.hmi_state.hidden[i] = !widget.visibility_address.empty() &&
                                !GetPlcDeviceState(widget.visibility_address);
      if (part.hmi_state.hidden[i] && part.hmi_state.held[i]) {
        SetPlcDeviceState(widget.address, false);
        part.hmi_state.held[i] = false;
      }
      if (widget.type == HmiWidgetType::kButton ||
          widget.type == HmiWidgetType::kLamp)
        part.hmi_state.values[i] = GetPlcDeviceState(widget.address);
      else if (widget.type == HmiWidgetType::kNumber ||
               widget.type == HmiWidgetType::kMessage) {
        const auto address =
            compiled_plc_executor_->ResolveDeviceAddress(widget.address);
        part.hmi_state.number_valid[i] = address.has_value();
        if (address) {
          part.hmi_state.numbers[i] =
              compiled_plc_executor_->GetWordValue(*address);
          if (!part.hmi_state.value_valid) {
            part.hmi_state.value_valid = true;
            part.hmi_state.display_value = part.hmi_state.numbers[i];
          }
        }
      }
    }
  }
}
void Application::RenderStorageSensorControls(int component_id) {
  const auto* component = FindComponentById(component_id);
  if (!component)
    return;
  if (component->type == ComponentType::kDrivenWarehouse) {
    ImGui::TextUnformatted(TR("component.driven_warehouse.desc",
        "Integrated 70 mm double-acting cylinder and linear guide"));
    ImGui::Text("%.1f / 70 mm", plc_emulator::ReadPneumaticState(*component,
                                                              "position"));
    for (int slot = 1; slot <= 6; ++slot) {
      int occupant = -1;
      for (const auto& part : placed_components_) {
        if (part.carry_state.owner_id == component_id &&
            part.carry_state.slot_id == slot) occupant = part.instanceId;
      }
      ImGui::Text("%d  Z %.2f  ID %d", slot,
                  plc_emulator::WarehouseSlotHeight(*component, slot), occupant);
    }
    return;
  }
  if (component->type == ComponentType::kWarehouse) {
    ImGui::Checkbox(TR("ui.z_layers.pause", "Pause simulation to edit"),
                    &physics_paused_);
    ImGui::BeginDisabled(!physics_paused_);
    int cylinder_id = component->warehouse_cylinder_id;
    float travel = component->warehouse_travel;
    char preview[48];
    std::snprintf(preview, sizeof(preview), cylinder_id < 0 ? "---" : "ID %d",
                  cylinder_id);
    if (ImGui::BeginCombo(TR("ui.warehouse.cylinder", "Warehouse cylinder"),
                          preview)) {
      if (ImGui::Selectable("---", cylinder_id == -1))
        ApplyWarehouseDrive(component_id, -1, travel);
      for (const auto& part : placed_components_) {
        if (part.type != ComponentType::CYLINDER &&
            part.type != ComponentType::kSensorCylinder)
          continue;
        ImGui::PushID(part.instanceId);
        std::snprintf(preview, sizeof(preview), "ID %d", part.instanceId);
        if (ImGui::Selectable(preview, cylinder_id == part.instanceId))
          ApplyWarehouseDrive(component_id, part.instanceId, travel);
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    if (ImGui::InputFloat(TR("ui.warehouse.travel", "Base travel"), &travel) &&
        std::isfinite(travel) && travel > 0 && travel <= 10000)
      ApplyWarehouseDrive(component_id, cylinder_id, travel);
    ImGui::EndDisabled();
    for (int slot = 1; slot <= 6; ++slot) {
      int occupant = -1;
      for (const auto& part : placed_components_) {
        if (part.carry_state.owner_id == component_id &&
            part.carry_state.slot_id == slot)
          occupant = part.instanceId;
      }
      ImGui::Text("%d  Z %.2f  ID %d", slot,
                  plc_emulator::WarehouseSlotHeight(*component, slot),
                  occupant);
    }
    return;
  }
  if (component->type == ComponentType::kFiberAmplifier)
    ImGui::Checkbox(TR("ui.z_layers.pause", "Pause simulation to edit"),
                    &physics_paused_);
  if (component->type == ComponentType::kFiberAmplifier) {
    const char* states[] = {TR("ui.fiber.ok", "Optical link OK"),
                            TR("ui.fiber.power", "Check 24V / 0V power"),
                            TR("ui.fiber.link", "Check TX / RX links"),
                            TR("ui.fiber.align_error",
                               "Check head direction, distance and height")};
    ImGui::TextUnformatted(
        states[std::clamp(component->fiber_state.fault, 0, 3)]);
    ImGui::BeginDisabled(!physics_paused_);
    ImGui::InputDouble(TR("ui.fiber.distance", "Maximum distance"),
                       &fiber_edit_settings_.max_distance);
    ImGui::InputDouble(TR("ui.fiber.alignment", "Alignment tolerance"),
                       &fiber_edit_settings_.alignment_tolerance);
    ImGui::InputDouble(TR("ui.fiber.width", "Beam width"),
                       &fiber_edit_settings_.beam_width);
    ImGui::Checkbox(TR("ui.fiber.dark_on", "Dark-ON"),
                    &fiber_edit_settings_.dark_on);
    ImGui::BeginDisabled(
        !plc_emulator::IsValidFiberSettings(fiber_edit_settings_));
    if (ImGui::Button(TR("ui.fiber.apply", "SET / MODE")))
      ApplyFiberSettings(component_id, fiber_edit_settings_);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    return;
  }
  if (component->type != ComponentType::kTouchPanel)
    return;
  if (ImGui::Button(TR("ui.hmi.edit_screen", "Edit screen")))
    OpenHmiEditor(component_id);
  ImGui::SameLine();
  ImGui::TextUnformatted(component->hmi_state.online ? "PLC ONLINE"
                                                     : "OFFLINE");
  const ImVec2 available = ImGui::GetContentRegionAvail();
  if (available.x <= 0 || available.y <= 0)
    return;
  const float scale = std::min(available.x / kHmiScreenWidth,
                             available.y / kHmiScreenHeight);
  const ImVec2 region_origin = ImGui::GetCursorScreenPos();
  const ImVec2 origin{
      region_origin.x + (available.x - kHmiScreenWidth * scale) * 0.5f,
      region_origin.y + (available.y - kHmiScreenHeight * scale) * 0.5f};
  plc_emulator::RenderHmiScreen(ImGui::GetWindowDrawList(),
                                component->hmi_settings, component->hmi_state,
                                origin, scale);
  ImGui::BeginDisabled(!component->hmi_state.online || physics_paused_);
  for (int i = static_cast<int>(component->hmi_settings.widgets.size()) - 1;
       i >= 0; --i) {
    const auto& widget = component->hmi_settings.widgets[i];
    if (widget.type != HmiWidgetType::kButton ||
        widget.page != component->hmi_state.active_page ||
        component->hmi_state.hidden[i])
      continue;
    ImGui::PushID(static_cast<int>(i));
    ImGui::SetCursorScreenPos({origin.x + widget.position.x * scale,
                               origin.y + widget.position.y * scale});
    const bool clicked = ImGui::InvisibleButton(
        "button", {widget.size.width * scale, widget.size.height * scale});
    if (widget.target_page >= 0) {
      if (clicked) {
        SetHmiButton(component_id, i, true);
        ImGui::PopID();
        break;
      }
    } else if (widget.momentary) {
      const bool pressed = ImGui::IsItemActive();
      if (pressed != component->hmi_state.held[i])
        SetHmiButton(component_id, static_cast<int>(i), pressed);
    } else if (clicked)
      SetHmiButton(component_id, static_cast<int>(i),
                   !component->hmi_state.values[i]);
    ImGui::PopID();
  }
  ImGui::EndDisabled();
  ImGui::SetCursorScreenPos(region_origin);
  ImGui::Dummy(available);
}
}  // namespace plc
