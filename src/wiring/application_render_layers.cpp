#include "plc_emulator/core/application.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/core/render_layer.h"
#include "plc_emulator/lang/lang_manager.h"

namespace plc {

void Application::ApplyRenderLayerEdit(RenderLayerEdit edit, float value,
                                       const std::string& group,
                                       bool record_undo) {
  if (!HasSelectedComponents() || !std::isfinite(value)) return;
  if (record_undo) PushWiringUndoState();
  for (auto& component : placed_components_) {
    if (!component.selected) continue;
    switch (edit) {
      case RenderLayerEdit::kOpacity:
        component.render_opacity = std::clamp(value, 0.0f, 1.0f);
        break;
      case RenderLayerEdit::kVisibility:
        component.render_hidden = value != 0.0f;
        break;
      case RenderLayerEdit::kGroup:
        component.render_group = group;
        break;
    }
  }
}

void Application::MoveSelectedRenderLayers(int direction, int target_id) {
  if (!HasSelectedComponents()) return;
  std::vector<size_t> order;
  plc_emulator::SortRenderLayerIndices(placed_components_, &order);
  const auto selected = [&](size_t index) {
    return placed_components_[index].selected;
  };
  if (direction == 0) {
    const auto target =
        std::find_if(order.begin(), order.end(), [&](size_t index) {
          return placed_components_[index].instanceId == target_id;
        });
    if (target == order.end() || selected(*target)) return;
    std::vector<size_t> block;
    for (size_t index : order)
      if (selected(index)) block.push_back(index);
    std::erase_if(order, selected);
    const auto destination =
        std::find_if(order.begin(), order.end(), [&](size_t index) {
          return placed_components_[index].instanceId == target_id;
        });
    // The panel lists foreground first: drop immediately above the target.
    order.insert(destination + 1, block.begin(), block.end());
  } else if (direction == 2) {
    std::stable_partition(order.begin(), order.end(),
                          [&](size_t index) { return !selected(index); });
  } else if (direction == -2) {
    std::stable_partition(order.begin(), order.end(), selected);
  } else if (direction > 0) {
    for (size_t i = order.size(); i > 1; --i) {
      if (selected(order[i - 2]) && !selected(order[i - 1]))
        std::swap(order[i - 2], order[i - 1]);
    }
  } else {
    for (size_t i = 1; i < order.size(); ++i) {
      if (selected(order[i]) && !selected(order[i - 1]))
        std::swap(order[i], order[i - 1]);
    }
  }
  PushWiringUndoState();
  for (size_t i = 0; i < order.size(); ++i)
    placed_components_[order[i]].z_order = static_cast<int>(i);
  next_z_order_ = static_cast<int>(order.size());
}

void Application::SelectRenderLayerRow(int component_id, bool additive,
                                       bool range) {
  if (range && render_layer_anchor_id_ >= 0) {
    const auto find_index = [&](int id) {
      return std::find_if(render_layer_order_.begin(),
                          render_layer_order_.end(), [&](size_t index) {
                            return placed_components_[index].instanceId == id;
                          });
    };
    const auto anchor = find_index(render_layer_anchor_id_);
    const auto target = find_index(component_id);
    if (anchor != render_layer_order_.end() &&
        target != render_layer_order_.end()) {
      if (!additive) ClearComponentSelection();
      for (auto item = std::min(anchor, target);
           item <= std::max(anchor, target); ++item)
        placed_components_[*item].selected = true;
      selected_component_id_ = component_id;
      return;
    }
  }
  if (additive)
    ToggleComponentSelection(component_id);
  else
    HandleComponentSelection(component_id);
  render_layer_anchor_id_ = component_id;
}

void Application::RenderLayerPanel() {
  if (!show_render_layer_panel_) return;
  ImGui::SetNextWindowSize({500, 640}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints({390, 350}, {FLT_MAX, FLT_MAX});
  if (!ImGui::Begin(TR("ui.render_layers.title", "Display layers"),
                    &show_render_layer_panel_)) {
    ImGui::End();
    return;
  }
  const ImVec2 origin = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  RegisterOverlayInputCaptureRect(
      origin, {origin.x + size.x, origin.y + size.y},
      ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
          ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
  plc_emulator::SortRenderLayerIndices(placed_components_,
                                       &render_layer_order_);
  std::reverse(render_layer_order_.begin(), render_layer_order_.end());
  ImGui::TextWrapped(
      "%s", TR("ui.render_layers.help",
               "Top rows appear in front. Ctrl: add selection; Shift: range."));
  ImGui::TextWrapped(
      "%s", TR("ui.render_layers.select_help",
               "Drag the left selection column to select multiple rows."));
  ImGui::TextWrapped(
      "%s", TR("ui.render_layers.drag_help",
               "Drag rows to reorder; drop onto a group title to join it."));
  ImGui::TextWrapped(
      "%s", TR("ui.render_layers.canvas_help",
               "Canvas rectangle selections can also be edited here."));

  const auto first_selected =
      std::find_if(placed_components_.begin(), placed_components_.end(),
                   [](const auto& part) { return part.selected; });
  const bool has_selection = first_selected != placed_components_.end();
  ImGui::BeginDisabled(!has_selection);
  if (ImGui::Button(TR("ui.render_layers.up", "Up")))
    MoveSelectedRenderLayers(1);
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.render_layers.down", "Down")))
    MoveSelectedRenderLayers(-1);
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.render_layers.front", "Front")))
    MoveSelectedRenderLayers(2);
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.render_layers.back", "Back")))
    MoveSelectedRenderLayers(-2);
  float opacity = has_selection ? first_selected->render_opacity * 100 : 100;
  const bool changed = ImGui::SliderFloat(
      TR("ui.render_layers.opacity", "Opacity"), &opacity, 0, 100, "%.0f%%");
  if (ImGui::IsItemActivated() && has_selection) PushWiringUndoState();
  if (changed)
    ApplyRenderLayerEdit(RenderLayerEdit::kOpacity, opacity / 100, "", false);
  if (ImGui::Button(TR("ui.render_layers.hide", "Hide")))
    ApplyRenderLayerEdit(RenderLayerEdit::kVisibility, 1);
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.render_layers.show", "Show")))
    ApplyRenderLayerEdit(RenderLayerEdit::kVisibility, 0);
  ImGui::InputText(TR("ui.render_layers.group_name", "Group name"),
                   render_group_buffer_, sizeof(render_group_buffer_));
  if (ImGui::Button(TR("ui.render_layers.group", "Group / rename selection")) &&
      render_group_buffer_[0] != '\0')
    ApplyRenderLayerEdit(RenderLayerEdit::kGroup, 0, render_group_buffer_);
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.render_layers.ungroup", "Ungroup")))
    ApplyRenderLayerEdit(RenderLayerEdit::kGroup, 0);
  ImGui::EndDisabled();

  const ImGuiIO& io = ImGui::GetIO();
  if (!io.MouseDown[0]) render_layer_brush_selecting_ = false;
  ImGui::BeginChild("RenderLayerList", {0, 0}, true);
  // Groups occupy a selectable drop row; their members keep actual draw order.
  for (size_t row = 0; row < render_layer_order_.size(); ++row) {
    auto& component = placed_components_[render_layer_order_[row]];
    if (!component.render_group.empty()) {
      bool first_in_group = true;
      for (size_t before = 0; before < row; ++before) {
        if (placed_components_[render_layer_order_[before]].render_group ==
            component.render_group)
          first_in_group = false;
      }
      if (first_in_group) {
        ImGui::PushID(component.instanceId);
        const std::string group = component.render_group;
        const std::string title = "[" + group + "]##Group";
        if (ImGui::Selectable(title.c_str(), false)) {
          if (!io.KeyCtrl) ClearComponentSelection();
          for (auto& part : placed_components_)
            if (part.render_group == group) part.selected = true;
          selected_component_id_ = component.instanceId;
          std::snprintf(render_group_buffer_, sizeof(render_group_buffer_),
                        "%s", group.c_str());
        }
        if (ImGui::BeginDragDropSource()) {
          ClearComponentSelection();
          for (auto& part : placed_components_)
            if (part.render_group == group) part.selected = true;
          selected_component_id_ = component.instanceId;
          ImGui::SetDragDropPayload("PLC_RENDER_LAYERS", &component.instanceId,
                                    sizeof(component.instanceId));
          ImGui::TextUnformatted(group.c_str());
          ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
          if (ImGui::AcceptDragDropPayload("PLC_RENDER_LAYERS"))
            ApplyRenderLayerEdit(RenderLayerEdit::kGroup, 0, group);
          ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
      }
    }
    ImGui::PushID(component.instanceId);
    // Separate a brush-selection gutter from the reorder drag source.
    ImGui::Selectable(component.selected ? "+##Select" : " ##Select",
                      component.selected, 0, {22, 0});
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
      if (!io.KeyCtrl) ClearComponentSelection();
      render_layer_brush_selecting_ = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        render_layer_brush_selecting_) {
      component.selected = true;
      selected_component_id_ = component.instanceId;
      render_layer_anchor_id_ = component.instanceId;
    }
    ImGui::SameLine();
    const auto* definition = GetComponentDefinition(component.type);
    const char* name = component.customLabel.empty()
                           ? (definition ? TR(definition->display_name,
                                              definition->display_name)
                                         : "Component")
                           : component.customLabel.c_str();
    char label[512];
    std::snprintf(
        label, sizeof(label), "%d  %s  [%d]  %.0f%% %s%s", component.instanceId,
        name, component.z_order, component.render_opacity * 100,
        component.render_hidden ? TR("ui.render_layers.hidden", "Hidden ") : "",
        component.render_group.c_str());
    if (ImGui::Selectable(label, component.selected,
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      // Keep a multi-selection intact while beginning a drag of one member.
      if (!component.selected || io.KeyCtrl || io.KeyShift)
        SelectRenderLayerRow(component.instanceId, io.KeyCtrl, io.KeyShift);
    }
    if (ImGui::BeginDragDropSource()) {
      if (!component.selected)
        SelectRenderLayerRow(component.instanceId, false, false);
      ImGui::SetDragDropPayload("PLC_RENDER_LAYERS", &component.instanceId,
                                sizeof(component.instanceId));
      ImGui::TextUnformatted(TR("ui.render_layers.drag", "Move selection"));
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (ImGui::AcceptDragDropPayload("PLC_RENDER_LAYERS"))
        MoveSelectedRenderLayers(0, component.instanceId);
      ImGui::EndDragDropTarget();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::End();
}

}  // namespace plc
