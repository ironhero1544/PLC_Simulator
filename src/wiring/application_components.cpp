// application_components.cpp
//
// Component rendering functions.

// src/Application_Components.cpp

#include "plc_emulator/core/application.h"

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>  // For std::remove_if
#include <iostream>
#include <string>

namespace plc {

namespace {

bool IsWorkpieceType(ComponentType type) {
  return type == ComponentType::WORKPIECE_METAL ||
         type == ComponentType::WORKPIECE_NONMETAL;
}

bool IsComponentVisibleByFilter(ComponentCategory category, int filter) {
  if (filter == 0) {
    return true;
  }
  if (filter == 1) {
    return category == ComponentCategory::AUTOMATION ||
           category == ComponentCategory::BOTH ||
           category == ComponentCategory::ALL;
  }
  return category == ComponentCategory::SEMICONDUCTOR ||
         category == ComponentCategory::BOTH ||
         category == ComponentCategory::ALL;
}

void DrawGridToggleIcon(ImDrawList* draw_list, ImVec2 pos, float size,
                        bool active) {
  ImU32 stroke = active ? IM_COL32(60, 60, 60, 255) : IM_COL32(90, 90, 90, 255);
  ImU32 fill = active ? IM_COL32(0, 0, 0, 12) : IM_COL32(0, 0, 0, 0);
  float rounding = 6.0f;

  ImVec2 max = ImVec2(pos.x + size, pos.y + size);
  draw_list->AddRectFilled(pos, max, fill, rounding);

  float inset = size * 0.22f;
  ImVec2 inner_min = ImVec2(pos.x + inset, pos.y + inset);
  ImVec2 inner_max = ImVec2(max.x - inset, max.y - inset);
  draw_list->AddRect(inner_min, inner_max, stroke, 2.0f);

  float mid_x = (inner_min.x + inner_max.x) * 0.5f;
  float mid_y = (inner_min.y + inner_max.y) * 0.5f;
  draw_list->AddLine(ImVec2(mid_x, inner_min.y), ImVec2(mid_x, inner_max.y),
                     stroke, 1.5f);
  draw_list->AddLine(ImVec2(inner_min.x, mid_y), ImVec2(inner_max.x, mid_y),
                     stroke, 1.5f);
}

void DrawListToggleIcon(ImDrawList* draw_list, ImVec2 pos, float size,
                        bool active) {
  ImU32 stroke = active ? IM_COL32(60, 60, 60, 255) : IM_COL32(90, 90, 90, 255);
  ImU32 fill = active ? IM_COL32(0, 0, 0, 12) : IM_COL32(0, 0, 0, 0);
  float rounding = 6.0f;

  ImVec2 max = ImVec2(pos.x + size, pos.y + size);
  draw_list->AddRectFilled(pos, max, fill, rounding);

  float inset = size * 0.22f;
  float square = size * 0.18f;
  ImVec2 icon_min =
      ImVec2(pos.x + inset, pos.y + (size - square) * 0.5f);
  ImVec2 icon_max = ImVec2(icon_min.x + square, icon_min.y + square);
  draw_list->AddRectFilled(icon_min, icon_max, stroke, 2.0f);

  float line_start_x = icon_max.x + size * 0.12f;
  float line_end_x = max.x - inset;
  float line_y = icon_min.y + square * 0.2f;
  float line_gap = square * 0.5f;
  draw_list->AddLine(ImVec2(line_start_x, line_y),
                     ImVec2(line_end_x, line_y), stroke, 1.5f);
  draw_list->AddLine(ImVec2(line_start_x, line_y + line_gap),
                     ImVec2(line_end_x, line_y + line_gap), stroke, 1.5f);
}

void RenderComponentPreview(ImDrawList* draw_list,
                            const ComponentDefinition* def,
                            ImVec2 preview_pos,
                            ImVec2 preview_size) {
  if (!def || !def->Render) {
    return;
  }
  float zoom_x = preview_size.x / def->size.width;
  float zoom_y = preview_size.y / def->size.height;
  float zoom = std::min(zoom_x, zoom_y);
  if (zoom <= 0.01f) {
    return;
  }

  ImVec2 draw_pos = ImVec2(preview_pos.x + (preview_size.x - def->size.width * zoom) * 0.5f,
                           preview_pos.y + (preview_size.y - def->size.height * zoom) * 0.5f);
  PlacedComponent temp;
  temp.type = def->type;
  temp.size = def->size;
  temp.position = {0.0f, 0.0f};
  temp.rotation = 0.0f;
  if (def->InitDefaultState) {
    def->InitDefaultState(&temp);
  }
  def->Render(draw_list, temp, draw_pos, zoom);
}

}  // namespace

void Application::HandleComponentDragStart(int componentIndex) {
  int count = GetComponentDefinitionCount();
  if (componentIndex >= 0 && componentIndex < count) {
    is_dragging_ = true;
    dragged_component_index_ = componentIndex;
    std::cout << "Drag started successfully! isDragging=" << is_dragging_
              << ", draggedIndex=" << dragged_component_index_ << std::endl;
  }
}

void Application::HandleComponentDrag() {
  if (is_dragging_) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    std::cout << "Component " << dragged_component_index_ << " is being dragged!"
              << std::endl;
  }
}

// src/Application_Components.cpp

// ????? ???????? ?????? ??????????
void Application::HandleComponentDrop(Position position) {
  std::cout << "DROP: isDrag=" << is_dragging_
            << " index=" << dragged_component_index_ << std::endl;

  if (is_dragging_ && dragged_component_index_ >= 0) {
    const ComponentDefinition* def =
        GetComponentDefinitionByIndex(dragged_component_index_);
    if (!def) {
      std::cout << "FAIL: Definition missing" << std::endl;
      is_dragging_ = false;
      dragged_component_index_ = -1;
      return;
    }
    std::cout << "CREATE: type=" << dragged_component_index_ << " at ("
              << position.x << "," << position.y << ")" << std::endl;

    PlacedComponent newComponent;
    newComponent.instanceId = next_instance_id_++;
    newComponent.type = def->type;
    newComponent.position = position;

    newComponent.size = def->size;
    if (def->InitDefaultState) {
      def->InitDefaultState(&newComponent);
    }

    newComponent.position.x -= newComponent.size.width / 2.0f;
    newComponent.position.y -= newComponent.size.height / 2.0f;

    placed_components_.push_back(newComponent);

    std::cout << "SUCCESS: Total=" << placed_components_.size() << std::endl;

    is_dragging_ = false;
    dragged_component_index_ = -1;
  } else {
    std::cout << "FAIL: Conditions not met" << std::endl;
  }
}

void Application::HandleComponentSelection(int instanceId) {
  // ????? ??? ???
  selected_wire_id_ = -1;
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;

  // [PPT: ??? ???] for??? ?????? ??? ??????????? ?????
  // ???????????
  for (auto& comp : placed_components_) {
    comp.selected = (comp.instanceId == instanceId);
  }
  selected_component_id_ = instanceId;
}

void Application::DeleteSelectedComponent() {
  if (selected_component_id_ >= 0) {
    // ????????????? ???????? ????? ???
    wires_.erase(
        std::remove_if(wires_.begin(), wires_.end(),
                       [this](const Wire& wire) {
                         return wire.fromComponentId == selected_component_id_ ||
                                wire.toComponentId == selected_component_id_;
                       }),
        wires_.end());

    // ?????? ???
    for (auto it = placed_components_.begin(); it != placed_components_.end();
         ++it) {
      if (it->instanceId == selected_component_id_) {
        placed_components_.erase(it);
        selected_component_id_ = -1;
        break;
      }
    }
  }
}

void Application::HandleComponentMoveStart(int instanceId, ImVec2 mousePos) {
  // [PPT: ??? ???] for??? ?????? ???????????????????.
  for (auto& comp : placed_components_) {
    if (comp.instanceId == instanceId) {
      is_moving_component_ = true;
      moving_component_id_ = instanceId;
      drag_start_offset_.x = mousePos.x - comp.position.x;
      drag_start_offset_.y = mousePos.y - comp.position.y;
      if (IsWorkpieceType(comp.type)) {
        comp.internalStates[state_keys::kIsManualDrag] = 1.0f;
        comp.internalStates[state_keys::kVelX] = 0.0f;
        comp.internalStates[state_keys::kVelY] = 0.0f;
        comp.internalStates[state_keys::kIsStuckBox] = 0.0f;
      }
      break;
    }
  }
}

void Application::HandleComponentMoveEnd() {
  is_moving_component_ = false;
  moving_component_id_ = -1;
  for (auto& comp : placed_components_) {
    if (IsWorkpieceType(comp.type)) {
      comp.internalStates[state_keys::kIsManualDrag] = 0.0f;
    }
  }
}

void Application::RenderPlacedComponents(ImDrawList* draw_list) {
  // [PPT: ??? ???] for??? ?????? ???????? ?????????????????.
  for (const auto& comp : placed_components_) {
    ImVec2 screen_pos = WorldToScreen({comp.position.x, comp.position.y});

    // [PPT: ??? ???] switch??? ?????? ?????? ?????????? ??????
    // ????????
    const ComponentDefinition* def = GetComponentDefinition(comp.type);
    if (def && def->Render) {
      def->Render(draw_list, comp, screen_pos, camera_zoom_);
    } else {
      ImVec2 end_pos = {screen_pos.x + comp.size.width * camera_zoom_,
                        screen_pos.y + comp.size.height * camera_zoom_};
      draw_list->AddRectFilled(screen_pos, end_pos,
                               IM_COL32(128, 128, 128, 255));
    }

    // ??????????? ????????
    if (comp.selected) {
      ImVec2 end_pos = {screen_pos.x + comp.size.width * camera_zoom_,
                        screen_pos.y + comp.size.height * camera_zoom_};
      draw_list->AddRect(screen_pos, end_pos, IM_COL32(0, 123, 255, 255), 0.0f,
                         0, 3.0f);
    }
  }
}

void Application::RenderComponentList() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
  if (ImGui::BeginChild("ComponentList", ImVec2(0, 0), true)) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGui::SetCursorPos(ImVec2(15 * layout_scale, 12 * layout_scale));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::Text("%s", TR("ui.component_list.title", "Components"));
    ImGui::PopStyleColor();

    float toggle_size = 26.0f * layout_scale;
    float right_padding = 36.0f * layout_scale;
    float filter_width = 120.0f * layout_scale;
    float total_width = ImGui::GetWindowWidth();
    float filter_start_x =
        (total_width - filter_width) * 0.5f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(filter_start_x);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * layout_scale);
    ImGui::PushItemWidth(filter_width);
    const char* filter_label = TR("ui.component_list.filter_all", "All");
    if (component_list_filter_ == ComponentListFilter::AUTOMATION) {
      filter_label = TR("ui.component_list.filter_auto", "Automation");
    } else if (component_list_filter_ == ComponentListFilter::SEMICONDUCTOR) {
      filter_label = TR("ui.component_list.filter_semi", "Semiconductor");
    }

    if (ImGui::BeginCombo("##ComponentFilter", filter_label,
                          ImGuiComboFlags_HeightSmall)) {
      if (ImGui::Selectable(TR("ui.component_list.filter_all", "All"),
                            component_list_filter_ ==
                                ComponentListFilter::ALL)) {
        component_list_filter_ = ComponentListFilter::ALL;
      }
      if (ImGui::Selectable(TR("ui.component_list.filter_auto", "Automation"),
                            component_list_filter_ ==
                                ComponentListFilter::AUTOMATION)) {
        component_list_filter_ = ComponentListFilter::AUTOMATION;
      }
      if (ImGui::Selectable(
              TR("ui.component_list.filter_semi", "Semiconductor"),
              component_list_filter_ ==
                  ComponentListFilter::SEMICONDUCTOR)) {
        component_list_filter_ = ComponentListFilter::SEMICONDUCTOR;
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::SameLine();
    ImGui::SetCursorPosX(total_width - right_padding - toggle_size);
    ImVec2 toggle_pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("ViewToggle", ImVec2(toggle_size, toggle_size));
    if (ImGui::IsItemClicked()) {
      component_list_view_mode_ =
          (component_list_view_mode_ == ComponentListViewMode::ICON)
              ? ComponentListViewMode::NAME
              : ComponentListViewMode::ICON;
    }
    bool grid_active = component_list_view_mode_ == ComponentListViewMode::ICON;
    if (grid_active) {
      DrawGridToggleIcon(draw_list, toggle_pos, toggle_size, false);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("ui.component_list.view_icon", "Icon view"));
      }
    } else {
      DrawListToggleIcon(draw_list, toggle_pos, toggle_size, false);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("ui.component_list.view_name", "Name view"));
      }
    }

    ImGui::SetCursorPosX(15 * layout_scale);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetCursorPosX(10 * layout_scale);

    int count = GetComponentDefinitionCount();
    int visible_count = 0;
    if (component_list_view_mode_ == ComponentListViewMode::ICON) {
      float spacing = 6.0f * layout_scale;
      float content_width = ImGui::GetContentRegionAvail().x;
      float card_size = (content_width - spacing) * 0.5f;
      float min_card = 90.0f * layout_scale;
      card_size = std::max(min_card, card_size);
      int columns = 2;
      int col = 0;

      for (int i = 0; i < count; i++) {
        const ComponentDefinition* def = GetComponentDefinitionByIndex(i);
        if (!def || !IsComponentVisibleByFilter(
                        def->category,
                        static_cast<int>(component_list_filter_))) {
          continue;
        }
        visible_count++;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * layout_scale);

        if (ImGui::BeginChild(("ComponentIcon" + std::to_string(i)).c_str(),
                              ImVec2(card_size, card_size), true,
                              ImGuiWindowFlags_NoScrollbar)) {
          float preview_padding = 10.0f * layout_scale;
          ImVec2 preview_pos = ImGui::GetCursorScreenPos();
          ImVec2 preview_size = ImVec2(card_size - preview_padding * 2.0f,
                                       card_size - 22 * layout_scale -
                                           preview_padding * 2.0f);
          preview_pos.x += preview_padding;
          preview_pos.y += preview_padding;
          draw_list->PushClipRect(preview_pos,
                                  ImVec2(preview_pos.x + preview_size.x,
                                         preview_pos.y + preview_size.y),
                                  true);
          RenderComponentPreview(draw_list, def, preview_pos, preview_size);
          draw_list->PopClipRect();

          ImGui::SetCursorPosY(card_size - 20 * layout_scale);
          ImGui::SetCursorPosX(6 * layout_scale);
          const char* label =
              def->short_name ? def->short_name
                              : TR(def->display_name, "Component");
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
          ImGui::TextUnformatted(label);
          ImGui::PopStyleColor();

          ImGui::SetCursorPos(ImVec2(0, 0));
          ImGui::InvisibleButton(("DragButtonIcon" + std::to_string(i)).c_str(),
                                 ImVec2(-1, -1));
          bool is_hovered = ImGui::IsItemHovered();
          bool is_dragging_this =
              ImGui::IsItemActive() &&
              ImGui::IsMouseDragging(ImGuiMouseButton_Left);

          if (is_dragging_this && !is_dragging_) {
            HandleComponentDragStart(i);
          }
          if (is_dragging_this) {
            draw_list->AddRectFilled(ImGui::GetItemRectMin(),
                                     ImGui::GetItemRectMax(),
                                     IM_COL32(0, 0, 0, 50), 6.0f);
            ImGui::SetTooltip("%s", TR("ui.component_list.tooltip_drag",
                                       "Drag to the canvas."));
          } else if (is_hovered) {
            draw_list->AddRect(ImGui::GetItemRectMin(),
                               ImGui::GetItemRectMax(),
                               IM_COL32(0, 123, 255, 150), 6.0f, 0, 2.0f);
            ImGui::SetTooltip("%s\n%s",
                              TR("ui.component_list.tooltip_place",
                                 "Drag to place."),
                              TR(def->description, "No description"));
          }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        col++;
        if (col < columns) {
          ImGui::SameLine(0.0f, spacing);
        } else {
          col = 0;
          ImGui::Dummy(ImVec2(0.0f, spacing));
          ImGui::SetCursorPosX(10 * layout_scale);
        }
      }
    } else {
      for (int i = 0; i < count; i++) {
        const ComponentDefinition* def = GetComponentDefinitionByIndex(i);
        if (!def || !IsComponentVisibleByFilter(
                        def->category,
                        static_cast<int>(component_list_filter_))) {
          continue;
        }
        visible_count++;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * layout_scale);

        if (ImGui::BeginChild(("Component" + std::to_string(i)).c_str(),
                              ImVec2(-10, 85 * layout_scale), true,
                              ImGuiWindowFlags_NoScrollbar)) {
          ImGui::SetCursorPos(ImVec2(10 * layout_scale, 10 * layout_scale));
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
          ImGui::TextWrapped("%s", TR(def->display_name, "Component"));
          ImGui::PopStyleColor();

          ImGui::SetCursorPosX(10 * layout_scale);
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
          ImGui::TextWrapped("%s", TR(def->description, "No description"));
          ImGui::PopStyleColor();

          ImGui::SetCursorPos(ImVec2(0, 0));
          ImGui::InvisibleButton(("DragButton" + std::to_string(i)).c_str(),
                                 ImVec2(-1, -1));

          bool is_hovered = ImGui::IsItemHovered();
          bool is_dragging_this = ImGui::IsItemActive() &&
                                  ImGui::IsMouseDragging(ImGuiMouseButton_Left);

          if (is_dragging_this && !is_dragging_) {
            HandleComponentDragStart(i);
          }

          if (is_dragging_this) {
            draw_list->AddRectFilled(ImGui::GetItemRectMin(),
                                     ImGui::GetItemRectMax(),
                                     IM_COL32(0, 0, 0, 50), 6.0f);
            ImGui::SetTooltip("%s", TR("ui.component_list.tooltip_drag",
                                       "Drag to the canvas."));
          } else if (is_hovered) {
            draw_list->AddRect(ImGui::GetItemRectMin(),
                               ImGui::GetItemRectMax(),
                               IM_COL32(0, 123, 255, 150), 6.0f, 0, 2.0f);
            ImGui::SetTooltip("%s\n%s",
                              TR("ui.component_list.tooltip_place",
                                 "Drag to place."),
                              TR(def->description, "No description"));
          }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::SetCursorPosX(10 * layout_scale);
      }
    }
    if (visible_count == 0) {
      ImGui::Spacing();
      ImGui::SetCursorPosX(20 * layout_scale);
      ImGui::Text("%s",
                  TR("ui.component_list.empty", "No components in this category."));
    }

    if (is_dragging_) {
      HandleComponentDrag();
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

}  // namespace plc

