// application_components.cpp
//
// Component rendering functions.

// src/Application_Components.cpp

#include "plc_emulator/core/application.h"

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/component_behavior.h"
#include "plc_emulator/components/processing_cylinder_def.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>  // For std::remove_if
#include <iostream>
#include <string>
#include <vector>

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

enum class ListFilter {
  ALL = 0,
  AUTOMATION = 1,
  SEMICONDUCTOR = 2
};

struct ComponentListEntry {
  const ComponentDefinition* def = nullptr;
  int index = -1;
};

bool IsRtlComponentListable(const RtlLibraryEntry& entry) {
  return entry.analyzeSuccess && entry.buildSuccess && entry.componentEnabled &&
         !entry.ports.empty();
}

const std::vector<ComponentType>& GetFunctionAxisOrder() {
  static const std::vector<ComponentType> kFunctionAxis = {
      ComponentType::PLC,
      ComponentType::POWER_SUPPLY,
      ComponentType::EMERGENCY_STOP,
      ComponentType::BUTTON_UNIT,
      ComponentType::LIMIT_SWITCH,
      ComponentType::SENSOR,
      ComponentType::INDUCTIVE_SENSOR,
      ComponentType::RING_SENSOR,
      ComponentType::FRL,
      ComponentType::MANIFOLD,
      ComponentType::METER_VALVE,
      ComponentType::VALVE_SINGLE,
      ComponentType::VALVE_DOUBLE,
      ComponentType::CYLINDER,
      ComponentType::PROCESSING_CYLINDER,
      ComponentType::CONVEYOR,
      ComponentType::TOWER_LAMP,
      ComponentType::BOX,
      ComponentType::WORKPIECE_METAL,
      ComponentType::WORKPIECE_NONMETAL,
      ComponentType::RTL_MODULE,
  };
  return kFunctionAxis;
}

int GetFunctionAxisOrderIndex(ComponentType type) {
  const auto& order = GetFunctionAxisOrder();
  auto it = std::find(order.begin(), order.end(), type);
  if (it == order.end()) {
    return static_cast<int>(order.size());
  }
  return static_cast<int>(std::distance(order.begin(), it));
}

std::vector<ComponentListEntry> BuildComponentList(ListFilter filter) {
  std::vector<ComponentListEntry> entries;
  int count = GetComponentDefinitionCount();
  entries.reserve(count);
  for (int i = 0; i < count; ++i) {
    const ComponentDefinition* def = GetComponentDefinitionByIndex(i);
    if (!def || !IsComponentVisibleByFilter(def->category,
                                            static_cast<int>(filter))) {
      continue;
    }
    if (def->type == ComponentType::RTL_MODULE) {
      continue;
    }
    entries.push_back({def, i});
  }

  std::sort(entries.begin(), entries.end(),
            [](const ComponentListEntry& a, const ComponentListEntry& b) {
              int rank_a = GetFunctionAxisOrderIndex(a.def->type);
              int rank_b = GetFunctionAxisOrderIndex(b.def->type);
              if (rank_a != rank_b) {
                return rank_a < rank_b;
              }
              return a.index < b.index;
            });
  return entries;
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
                            ImVec2 preview_size,
                            float scale) {
  if (!def || !def->Render) {
    return;
  }
  float zoom_x = preview_size.x / def->size.width;
  float zoom_y = preview_size.y / def->size.height;
  float zoom = std::min(zoom_x, zoom_y) * scale;
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

ImVec2 GetComponentRenderOrigin(const PlacedComponent& comp,
                                ImVec2 screen_top_left,
                                float zoom) {
  const Size display = GetComponentDisplaySize(comp);
  ImVec2 display_center(screen_top_left.x + display.width * zoom * 0.5f,
                        screen_top_left.y + display.height * zoom * 0.5f);
  ImVec2 base_center(comp.size.width * zoom * 0.5f,
                     comp.size.height * zoom * 0.5f);
  return ImVec2(display_center.x - base_center.x,
                display_center.y - base_center.y);
}

void TransformDrawListVertices(ImDrawList* draw_list,
                               int vtx_start,
                               int vtx_end,
                               ImVec2 center,
                               int rotation_quadrants,
                               bool flip_x,
                               bool flip_y) {
  const int quadrants = NormalizeRotationQuadrants(rotation_quadrants);
  if (quadrants == 0 && !flip_x && !flip_y) {
    return;
  }
  for (int i = vtx_start; i < vtx_end; ++i) {
    ImDrawVert& v = draw_list->VtxBuffer[i];
    ImVec2 offset(v.pos.x - center.x, v.pos.y - center.y);
    if (flip_x) {
      offset.x = -offset.x;
    }
    if (flip_y) {
      offset.y = -offset.y;
    }
    ImVec2 rotated = RotatePoint(offset, quadrants);
    v.pos = ImVec2(center.x + rotated.x, center.y + rotated.y);
  }
}

void ApplyAlphaToDrawList(ImDrawList* draw_list,
                          int vtx_start,
                          int vtx_end,
                          float alpha) {
  const float clamped = std::max(0.0f, std::min(1.0f, alpha));
  if (clamped >= 0.999f) {
    return;
  }
  for (int i = vtx_start; i < vtx_end; ++i) {
    ImDrawVert& v = draw_list->VtxBuffer[i];
    ImU32 col = v.col;
    unsigned int a =
        (col >> IM_COL32_A_SHIFT) & static_cast<unsigned int>(0xFF);
    a = static_cast<unsigned int>(static_cast<float>(a) * clamped);
    v.col = (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
  }
}

}  // namespace

void Application::HandleComponentDragStart(int componentIndex) {
  int count = GetComponentDefinitionCount();
  if (componentIndex >= 0 && componentIndex < count) {
    is_dragging_ = true;
    dragged_component_index_ = componentIndex;
    dragged_rtl_module_id_.clear();
    std::cout << "Drag started successfully! isDragging=" << is_dragging_
              << ", draggedIndex=" << dragged_component_index_ << std::endl;
  }
}

void Application::HandleRtlComponentDragStart(const std::string& moduleId) {
  if (moduleId.empty()) {
    return;
  }
  is_dragging_ = true;
  dragged_component_index_ = -1;
  dragged_rtl_module_id_ = moduleId;
}

void Application::HandleComponentDrag() {
  if (is_dragging_) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    std::cout << "Component " << dragged_component_index_ << " is being dragged!"
              << std::endl;
  }
}

// Drop a palette component onto the canvas. Placement is stored as a top-left
// anchor even though the cursor initially points at the component center.
void Application::HandleComponentDrop(Position position) {
  std::cout << "DROP: isDrag=" << is_dragging_
            << " index=" << dragged_component_index_ << std::endl;

  if (is_dragging_ && !dragged_rtl_module_id_.empty()) {
    PlaceRtlLibraryComponent(dragged_rtl_module_id_, position);
    is_dragging_ = false;
    dragged_component_index_ = -1;
    dragged_rtl_module_id_.clear();
  } else if (is_dragging_ && dragged_component_index_ >= 0) {
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

    PushWiringUndoState();

    PlacedComponent newComponent;
    newComponent.instanceId = next_instance_id_++;
    newComponent.type = def->type;
    newComponent.position = position;

    newComponent.size = def->size;
    newComponent.z_order = next_z_order_++;
    if (def->InitDefaultState) {
      def->InitDefaultState(&newComponent);
    }
    ApplyElectricalDefaults(&newComponent);

    newComponent.position.x -= newComponent.size.width / 2.0f;
    newComponent.position.y -= newComponent.size.height / 2.0f;

    placed_components_.push_back(newComponent);

    std::cout << "SUCCESS: Total=" << placed_components_.size() << std::endl;

    is_dragging_ = false;
    dragged_component_index_ = -1;
    dragged_rtl_module_id_.clear();
  } else {
    std::cout << "FAIL: Conditions not met" << std::endl;
  }
}

void Application::HandleComponentSelection(int instanceId) {
  // Exclusive component selection clears any wire selection/edit state first.
  ClearWireSelection();
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;

  ClearComponentSelection();
  for (auto& comp : placed_components_) {
    if (comp.instanceId == instanceId) {
      comp.selected = true;
      selected_component_id_ = instanceId;
      break;
    }
  }
}

void Application::ToggleComponentSelection(int instanceId) {
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;

  for (auto& comp : placed_components_) {
    if (comp.instanceId != instanceId) {
      continue;
    }
    comp.selected = !comp.selected;
    if (comp.selected) {
      selected_component_id_ = instanceId;
    } else if (selected_component_id_ == instanceId) {
      selected_component_id_ = -1;
    }
    break;
  }
  RepairPrimarySelection();
}

void Application::ClearComponentSelection() {
  for (auto& comp : placed_components_) {
    comp.selected = false;
  }
  selected_component_id_ = -1;
}

void Application::RepairPrimarySelection() {
  if (selected_component_id_ != -1) {
    for (const auto& comp : placed_components_) {
      if (comp.instanceId == selected_component_id_ && comp.selected) {
        return;
      }
    }
  }

  selected_component_id_ = -1;
  for (const auto& comp : placed_components_) {
    if (comp.selected) {
      selected_component_id_ = comp.instanceId;
      return;
    }
  }
}

bool Application::HasSelectedComponents() const {
  for (const auto& comp : placed_components_) {
    if (comp.selected) {
      return true;
    }
  }
  return false;
}

void Application::DeleteSelectedComponent() {
  std::vector<int> selected_ids;
  for (const auto& comp : placed_components_) {
    if (comp.selected) {
      selected_ids.push_back(comp.instanceId);
    }
  }
  if (selected_ids.empty() && selected_component_id_ >= 0) {
    selected_ids.push_back(selected_component_id_);
  }
  if (selected_ids.empty()) {
    return;
  }

  PushWiringUndoState();
  auto is_selected_id = [&](int instance_id) {
    return std::find(selected_ids.begin(), selected_ids.end(), instance_id) !=
           selected_ids.end();
  };

  wires_.erase(
      std::remove_if(wires_.begin(), wires_.end(),
                     [&](const Wire& wire) {
                       return is_selected_id(wire.fromComponentId) ||
                              is_selected_id(wire.toComponentId);
                     }),
      wires_.end());

  placed_components_.erase(
      std::remove_if(placed_components_.begin(), placed_components_.end(),
                     [&](const PlacedComponent& comp) {
                       return is_selected_id(comp.instanceId);
                     }),
      placed_components_.end());

  ClearComponentSelection();
  RepairPrimaryWireSelection();
}

Application::ComponentMoveSession Application::BuildComponentMoveSession(
    int instanceId, ImVec2 mousePos) const {
  ComponentMoveSession session;
  session.anchor_component_id = instanceId;
  const bool move_selected_group = HasSelectedComponents();
  for (const auto& comp : placed_components_) {
    const bool should_move =
        move_selected_group ? comp.selected : (comp.instanceId == instanceId);
    if (!should_move) {
      continue;
    }

    if (comp.instanceId == instanceId) {
      session.drag_start_offset.x = mousePos.x - comp.position.x;
      session.drag_start_offset.y = mousePos.y - comp.position.y;
    }
    session.offsets.push_back(
        {comp.instanceId,
         ImVec2(mousePos.x - comp.position.x, mousePos.y - comp.position.y)});
  }
  return session;
}

void Application::ApplyComponentMoveManualDragState(
    const ComponentMoveSession& session,
    bool active) {
  for (const auto& moving_entry : session.offsets) {
    PlacedComponent* comp = FindComponentById(moving_entry.first);
    if (!comp || !IsWorkpieceType(comp->type)) {
      continue;
    }
    if (active) {
      comp->internalStates[state_keys::kIsManualDrag] = 1.0f;
      comp->internalStates[state_keys::kVelX] = 0.0f;
      comp->internalStates[state_keys::kVelY] = 0.0f;
      comp->internalStates[state_keys::kIsStuckBox] = 0.0f;
    } else {
      comp->internalStates[state_keys::kIsManualDrag] = 0.0f;
    }
  }
}

void Application::BeginComponentMoveSession(
    const ComponentMoveSession& session) {
  moving_component_offsets_.clear();
  if (session.offsets.empty()) {
    is_moving_component_ = false;
    moving_component_id_ = -1;
    drag_start_offset_ = {0.0f, 0.0f};
    return;
  }

  PushWiringUndoState();
  is_moving_component_ = true;
  moving_component_id_ = session.anchor_component_id;
  drag_start_offset_ = session.drag_start_offset;
  moving_component_offsets_ = session.offsets;
  ApplyComponentMoveManualDragState(session, true);
}

void Application::EndComponentMoveSession() {
  ComponentMoveSession session;
  session.anchor_component_id = moving_component_id_;
  session.drag_start_offset = drag_start_offset_;
  session.offsets = moving_component_offsets_;
  ApplyComponentMoveManualDragState(session, false);
  is_moving_component_ = false;
  moving_component_id_ = -1;
  drag_start_offset_ = {0.0f, 0.0f};
  moving_component_offsets_.clear();
}

void Application::RenderPlacedComponents(ImDrawList* draw_list) {
  // [PPT: ??? ???] for??? ?????? ???????? ?????????????????.
  std::vector<size_t> draw_order(placed_components_.size());
  for (size_t i = 0; i < placed_components_.size(); ++i) {
    draw_order[i] = i;
  }
  std::sort(draw_order.begin(), draw_order.end(),
            [&](size_t a, size_t b) {
              const auto& comp_a = placed_components_[a];
              const auto& comp_b = placed_components_[b];
              if (comp_a.z_order != comp_b.z_order) {
                return comp_a.z_order < comp_b.z_order;
              }
              return comp_a.instanceId < comp_b.instanceId;
            });

  const ImGuiIO& io = ImGui::GetIO();
  const bool ghost_mode = io.KeyShift && HasSelectedComponents();
  const float ghost_alpha = 0.35f;
  const float cull_margin = 120.0f;
  ImVec2 view_top_left = ScreenToWorld(canvas_top_left_);
  ImVec2 view_bottom_right = ScreenToWorld(
      ImVec2(canvas_top_left_.x + canvas_size_.x,
             canvas_top_left_.y + canvas_size_.y));
  float view_min_x = std::min(view_top_left.x, view_bottom_right.x) - cull_margin;
  float view_max_x = std::max(view_top_left.x, view_bottom_right.x) + cull_margin;
  float view_min_y = std::min(view_top_left.y, view_bottom_right.y) - cull_margin;
  float view_max_y = std::max(view_top_left.y, view_bottom_right.y) + cull_margin;

  for (size_t index : draw_order) {
    auto& comp = placed_components_[index];
    const Size display = GetComponentDisplaySize(comp);
    float comp_min_x = comp.position.x;
    float comp_min_y = comp.position.y;
    float comp_max_x = comp.position.x + display.width;
    float comp_max_y = comp.position.y + display.height;
    if (comp_max_x < view_min_x || comp_min_x > view_max_x ||
        comp_max_y < view_min_y || comp_min_y > view_max_y) {
      continue;
    }
    if (comp.type == ComponentType::PLC) {
      comp.internalStates[state_keys::kPlcRunning] =
          is_plc_running_ ? 1.0f : 0.0f;
      comp.internalStates[state_keys::kPlcInputMode] =
          plc_input_mode_ == PlcInputMode::SOURCE ? 1.0f : 0.0f;
      comp.internalStates[state_keys::kPlcOutputMode] =
          plc_output_mode_ == PlcOutputMode::SOURCE ? 1.0f : 0.0f;
    }
    ImVec2 screen_top_left = WorldToScreen({comp.position.x, comp.position.y});
    ImVec2 render_origin =
        GetComponentRenderOrigin(comp, screen_top_left, camera_zoom_);
    ImVec2 display_center(
        screen_top_left.x + display.width * camera_zoom_ * 0.5f,
        screen_top_left.y + display.height * camera_zoom_ * 0.5f);

    // [PPT: ??? ???] switch??? ?????? ?????? ?????????? ??????
    // ????????
    const ComponentDefinition* def = GetComponentDefinition(comp.type);
    const int vtx_start = draw_list->VtxBuffer.Size;
    if (def && def->Render) {
      def->Render(draw_list, comp, render_origin, camera_zoom_);
    } else {
      ImVec2 end_pos = {render_origin.x + comp.size.width * camera_zoom_,
                        render_origin.y + comp.size.height * camera_zoom_};
      draw_list->AddRectFilled(render_origin, end_pos,
                               IM_COL32(128, 128, 128, 255));
    }

    const int vtx_end = draw_list->VtxBuffer.Size;
    TransformDrawListVertices(draw_list, vtx_start, vtx_end, display_center,
                              comp.rotation_quadrants, comp.flip_x,
                              comp.flip_y);
    if (ghost_mode && !comp.selected) {
      ApplyAlphaToDrawList(draw_list, vtx_start, vtx_end, ghost_alpha);
    }

    // ??????????? ????????
    if (comp.selected) {
      const int sel_start = draw_list->VtxBuffer.Size;
      ImVec2 rect_min = render_origin;
      ImVec2 rect_max = {render_origin.x + comp.size.width * camera_zoom_,
                         render_origin.y + comp.size.height * camera_zoom_};
      draw_list->AddRect(rect_min, rect_max, IM_COL32(0, 123, 255, 255), 0.0f,
                         0, 3.0f);
      const int sel_end = draw_list->VtxBuffer.Size;
      TransformDrawListVertices(draw_list, sel_start, sel_end, display_center,
                                comp.rotation_quadrants, comp.flip_x,
                                comp.flip_y);
    }
  }

  for (size_t index : draw_order) {
    const auto& comp = placed_components_[index];
    if (comp.type != ComponentType::PROCESSING_CYLINDER) {
      continue;
    }
    const Size display = GetComponentDisplaySize(comp);
    float comp_min_x = comp.position.x;
    float comp_min_y = comp.position.y;
    float comp_max_x = comp.position.x + display.width;
    float comp_max_y = comp.position.y + display.height;
    if (comp_max_x < view_min_x || comp_min_x > view_max_x ||
        comp_max_y < view_min_y || comp_min_y > view_max_y) {
      continue;
    }
    ImVec2 screen_top_left = WorldToScreen({comp.position.x, comp.position.y});
    ImVec2 render_origin =
        GetComponentRenderOrigin(comp, screen_top_left, camera_zoom_);
    ImVec2 display_center(
        screen_top_left.x + display.width * camera_zoom_ * 0.5f,
        screen_top_left.y + display.height * camera_zoom_ * 0.5f);
    const int vtx_start = draw_list->VtxBuffer.Size;
    RenderProcessingCylinderHead(draw_list, comp, render_origin, camera_zoom_);
    const int vtx_end = draw_list->VtxBuffer.Size;
    TransformDrawListVertices(draw_list, vtx_start, vtx_end, display_center,
                              comp.rotation_quadrants, comp.flip_x,
                              comp.flip_y);
    if (ghost_mode && !comp.selected) {
      ApplyAlphaToDrawList(draw_list, vtx_start, vtx_end, ghost_alpha);
    }
  }
}

void Application::RenderComponentList() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
  if (ImGui::BeginChild("ComponentList", ImVec2(0, 0), true)) {
    {
      ImVec2 window_min = ImGui::GetWindowPos();
      ImVec2 window_size = ImGui::GetWindowSize();
      ImVec2 window_max(window_min.x + window_size.x, window_min.y + window_size.y);
      RegisterOverlayInputCaptureRect(
          window_min, window_max,
          ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
              ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    }
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

    ListFilter list_filter = ListFilter::ALL;
    if (component_list_filter_ == ComponentListFilter::AUTOMATION) {
      list_filter = ListFilter::AUTOMATION;
    } else if (component_list_filter_ == ComponentListFilter::SEMICONDUCTOR) {
      list_filter = ListFilter::SEMICONDUCTOR;
    }
    std::vector<ComponentListEntry> entries = BuildComponentList(list_filter);
    int visible_count = 0;
    auto open_rtl_library = [this]() {
      show_rtl_library_panel_ = true;
      if (selected_rtl_module_id_.empty() && rtl_project_manager_ &&
          !rtl_project_manager_->GetLibrary().empty()) {
        selected_rtl_module_id_ = rtl_project_manager_->GetLibrary().front().moduleId;
      }
    };
    auto truncate_label = [](const std::string& text, size_t max_chars) {
      if (text.size() <= max_chars) {
        return text;
      }
      return text.substr(0, max_chars - 3) + "...";
    };
    if (component_list_view_mode_ == ComponentListViewMode::ICON) {
      float spacing = 6.0f * layout_scale;
      float content_width = ImGui::GetContentRegionAvail().x;
      float card_size = (content_width - spacing) * 0.5f;
      float min_card = 90.0f * layout_scale;
      card_size = std::max(min_card, card_size);
      int columns = 2;
      int col = 0;

      for (const auto& entry : entries) {
        const ComponentDefinition* def = entry.def;
        const int i = entry.index;
        if (!def) {
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
          ImVec2 preview_area = ImVec2(card_size - preview_padding * 2.0f,
                                       card_size - 22 * layout_scale -
                                           preview_padding * 2.0f);
          float preview_scale = 0.6f;
          ImVec2 preview_size = preview_area;
          preview_pos.x += preview_padding;
          preview_pos.y += preview_padding;
          draw_list->PushClipRect(preview_pos,
                                  ImVec2(preview_pos.x + preview_size.x,
                                         preview_pos.y + preview_size.y),
                                  true);
          RenderComponentPreview(draw_list, def, preview_pos, preview_size,
                                 preview_scale);
          draw_list->PopClipRect();

          ImGui::SetCursorPosY(card_size - 20 * layout_scale);
          ImGui::SetCursorPosX(6 * layout_scale);
          const char* label = TR(def->display_name, "Component");
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
      if (rtl_project_manager_ &&
          (component_list_filter_ == ComponentListFilter::ALL ||
           component_list_filter_ == ComponentListFilter::SEMICONDUCTOR)) {
        for (const auto& rtl_entry : rtl_project_manager_->GetLibrary()) {
          if (!IsRtlComponentListable(rtl_entry)) {
            continue;
          }
          ++visible_count;
          ImGui::PushID(rtl_entry.moduleId.c_str());
          ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
          ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                             6.0f * layout_scale);

          if (ImGui::BeginChild("RtlComponentIcon", ImVec2(card_size, card_size),
                                true, ImGuiWindowFlags_NoScrollbar)) {
            float preview_padding = 10.0f * layout_scale;
            ImVec2 preview_pos = ImGui::GetCursorScreenPos();
            ImVec2 preview_area = ImVec2(card_size - preview_padding * 2.0f,
                                         card_size - 22 * layout_scale -
                                             preview_padding * 2.0f);
            preview_pos.x += preview_padding;
            preview_pos.y += preview_padding;

            const ComponentDefinition* rtlDef =
                GetComponentDefinition(ComponentType::RTL_MODULE);
            PlacedComponent previewComp;
            previewComp.type = ComponentType::RTL_MODULE;
            previewComp.customLabel = rtl_entry.displayName;
            previewComp.rtlModuleId = rtl_entry.topModule;
            previewComp.runtimePorts =
                RtlProjectManager::BuildRuntimePorts(rtl_entry.ports);
            const size_t inputCount = std::count_if(
                rtl_entry.ports.begin(), rtl_entry.ports.end(),
                [](const RtlPortDescriptor& port) { return port.isInput; });
            const size_t outputCount = rtl_entry.ports.size() - inputCount;
            const size_t maxSideCount = std::max(inputCount, outputCount);
            previewComp.size = {260.0f,
                                std::max(140.0f, 92.0f +
                                                      18.0f * static_cast<float>(
                                                                  maxSideCount))};
            if (rtlDef && rtlDef->Render) {
              float zoom_x = preview_area.x / previewComp.size.width;
              float zoom_y = preview_area.y / previewComp.size.height;
              float zoom = std::min(zoom_x, zoom_y) * 0.75f;
              ImVec2 draw_pos = ImVec2(
                  preview_pos.x +
                      (preview_area.x - previewComp.size.width * zoom) * 0.5f,
                  preview_pos.y +
                      (preview_area.y - previewComp.size.height * zoom) * 0.5f);
              draw_list->PushClipRect(
                  preview_pos,
                  ImVec2(preview_pos.x + preview_area.x,
                         preview_pos.y + preview_area.y),
                  true);
              rtlDef->Render(draw_list, previewComp, draw_pos, zoom);
              draw_list->PopClipRect();
            }

            std::string label = truncate_label(rtl_entry.displayName, 16);
            ImGui::SetCursorPosY(card_size - 19 * layout_scale);
            ImGui::SetCursorPosX(6 * layout_scale);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::SetWindowFontScale(0.88f);
            ImGui::TextUnformatted(label.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::InvisibleButton("RtlDragButtonIcon", ImVec2(-1, -1));
            bool is_hovered = ImGui::IsItemHovered();
            bool is_dragging_this =
                ImGui::IsItemActive() &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left);

            if (is_dragging_this && !is_dragging_) {
              HandleRtlComponentDragStart(rtl_entry.moduleId);
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
              ImGui::SetTooltip("%s", TR("ui.component_list.tooltip_drag",
                                         "Drag to the canvas."));
            }
          }
          ImGui::EndChild();
          ImGui::PopStyleVar();
          ImGui::PopStyleColor();
          ImGui::PopID();

          col++;
          if (col < columns) {
            ImGui::SameLine(0.0f, spacing);
          } else {
            col = 0;
            ImGui::Dummy(ImVec2(0.0f, spacing));
            ImGui::SetCursorPosX(10 * layout_scale);
          }
        }

        ++visible_count;
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * layout_scale);
        if (ImGui::BeginChild("RtlCreateIcon", ImVec2(card_size, card_size), true,
                              ImGuiWindowFlags_NoScrollbar)) {
          ImVec2 min = ImGui::GetWindowPos();
          ImVec2 max = ImVec2(min.x + card_size, min.y + card_size);
          ImVec2 center = ImVec2((min.x + max.x) * 0.5f, min.y + card_size * 0.38f);
          draw_list->AddLine(ImVec2(center.x - 10.0f * layout_scale, center.y),
                             ImVec2(center.x + 10.0f * layout_scale, center.y),
                             IM_COL32(90, 90, 90, 255), 2.0f);
          draw_list->AddLine(ImVec2(center.x, center.y - 10.0f * layout_scale),
                             ImVec2(center.x, center.y + 10.0f * layout_scale),
                             IM_COL32(90, 90, 90, 255), 2.0f);
          ImGui::SetCursorPosY(card_size - 30 * layout_scale);
          ImGui::SetCursorPosX(8 * layout_scale);
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
          ImGui::SetWindowFontScale(0.86f);
          ImGui::TextUnformatted("Add RTL Module...");
          ImGui::SetWindowFontScale(1.0f);
          ImGui::PopStyleColor();
          ImGui::SetCursorPos(ImVec2(0, 0));
          ImGui::InvisibleButton("RtlCreateOpenIcon", ImVec2(-1, -1));
          bool is_hovered = ImGui::IsItemHovered();
          if (ImGui::IsItemClicked()) {
            open_rtl_library();
          }
          if (is_hovered) {
            draw_list->AddRect(ImGui::GetItemRectMin(),
                               ImGui::GetItemRectMax(),
                               IM_COL32(0, 123, 255, 150), 6.0f, 0, 2.0f);
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
      for (const auto& entry : entries) {
        const ComponentDefinition* def = entry.def;
        const int i = entry.index;
        if (!def) {
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
      if (rtl_project_manager_ &&
          (component_list_filter_ == ComponentListFilter::ALL ||
           component_list_filter_ == ComponentListFilter::SEMICONDUCTOR)) {
        for (const auto& rtl_entry : rtl_project_manager_->GetLibrary()) {
          if (!IsRtlComponentListable(rtl_entry)) {
            continue;
          }
          ++visible_count;
          ImGui::PushID(rtl_entry.moduleId.c_str());
          ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
          ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * layout_scale);
          if (ImGui::BeginChild("RtlLibraryEntry", ImVec2(-10, 85 * layout_scale),
                                true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::SetCursorPos(ImVec2(10 * layout_scale, 10 * layout_scale));
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
            ImGui::TextWrapped("%s", rtl_entry.displayName.c_str());
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(10 * layout_scale);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", rtl_entry.topModule.c_str());
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::InvisibleButton("RtlDragButton", ImVec2(-1, -1));
            bool is_hovered = ImGui::IsItemHovered();
            bool is_dragging_this =
                ImGui::IsItemActive() &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left);
            if (is_dragging_this && !is_dragging_) {
              HandleRtlComponentDragStart(rtl_entry.moduleId);
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
              ImGui::SetTooltip("%s", TR("ui.component_list.tooltip_drag",
                                         "Drag to the canvas."));
            }
          }
          ImGui::EndChild();
          ImGui::PopStyleVar();
          ImGui::PopStyleColor();
          ImGui::PopID();

          ImGui::Spacing();
          ImGui::SetCursorPosX(10 * layout_scale);
        }

        ++visible_count;
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * layout_scale);
        if (ImGui::BeginChild("RtlLibraryCreateEntry",
                              ImVec2(-10, 85 * layout_scale), true,
                              ImGuiWindowFlags_NoScrollbar)) {
          ImGui::SetCursorPos(ImVec2(10 * layout_scale, 10 * layout_scale));
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
          ImGui::TextWrapped("%s", "Add RTL Module...");
          ImGui::PopStyleColor();

          ImGui::SetCursorPosX(10 * layout_scale);
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
          ImGui::TextWrapped("%s", "Create and manage project RTL modules.");
          ImGui::PopStyleColor();

          ImGui::SetCursorPos(ImVec2(0, 0));
          ImGui::InvisibleButton("RtlCreateOpenName", ImVec2(-1, -1));
          bool is_hovered = ImGui::IsItemHovered();
          if (ImGui::IsItemClicked()) {
            open_rtl_library();
          }
          if (is_hovered) {
            draw_list->AddRect(ImGui::GetItemRectMin(),
                               ImGui::GetItemRectMax(),
                               IM_COL32(0, 123, 255, 150), 6.0f, 0, 2.0f);
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
