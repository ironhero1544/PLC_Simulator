#include "plc_emulator/components/storage_sensor_def.h"
// application_components.cpp
//
// Component rendering functions.

// src/Application_Components.cpp

#include "plc_emulator/core/application.h"

#include "plc_emulator/core/render_layer.h"

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/component_behavior.h"
#include "plc_emulator/components/processing_cylinder_def.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>  // For std::remove_if
#include <cstdio>
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
  SEMICONDUCTOR = 2,
  kAutomationExam = 3
};

// Functional filtering is independent of the qualification filter.
int GetComponentFunctionGroup(ComponentType type) {
  switch (type) {
    case ComponentType::BUTTON_UNIT:
    case ComponentType::EMERGENCY_STOP:
    case ComponentType::LIMIT_SWITCH:
    case ComponentType::SENSOR:
    case ComponentType::INDUCTIVE_SENSOR:
    case ComponentType::RING_SENSOR:
    case ComponentType::kFiberAmplifier:
    case ComponentType::kFiberTransmitter:
    case ComponentType::kFiberReceiver:
      return 1;
    case ComponentType::TOWER_LAMP:
    case ComponentType::CONVEYOR:
    case ComponentType::kDcMotor:
    case ComponentType::kServoMotor:
    case ComponentType::kServoLift:
      return 2;
    case ComponentType::FRL:
    case ComponentType::MANIFOLD:
    case ComponentType::METER_VALVE:
    case ComponentType::VALVE_SINGLE:
    case ComponentType::VALVE_DOUBLE:
    case ComponentType::CYLINDER:
    case ComponentType::PROCESSING_CYLINDER:
    case ComponentType::kSensorCylinder:
    case ComponentType::kSuctionCylinder:
    case ComponentType::kStopperCylinder:
    case ComponentType::kVacuumGenerator:
    case ComponentType::kLiftSuction:
      return 3;
    case ComponentType::POWER_SUPPLY:
    case ComponentType::kAcSupply:
      return 4;
    case ComponentType::PLC:
    case ComponentType::kTouchPanel:
    case ComponentType::kDcMotorDriver:
    case ComponentType::kServoAmplifier:
    case ComponentType::RTL_MODULE:
      return 5;
    default:
      return 6;
  }
}

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
      ComponentType::kSensorCylinder,
      ComponentType::kSuctionCylinder,
      ComponentType::kStopperCylinder,
      ComponentType::kVacuumGenerator,
      ComponentType::kDcMotor,
      ComponentType::kDcMotorDriver,
      ComponentType::kServoMotor,
      ComponentType::kServoAmplifier,
      ComponentType::kServoLift,
      ComponentType::kLiftSuction,
      ComponentType::kAcSupply,
      ComponentType::kWarehouse,
      ComponentType::kDrivenWarehouse,
      ComponentType::kFiberAmplifier,
      ComponentType::kFiberTransmitter,
      ComponentType::kFiberReceiver,
      ComponentType::kTouchPanel,

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
                                            filter == ListFilter::kAutomationExam ? 1 : static_cast<int>(filter))) {
      continue;
    }
    if (def->type == ComponentType::RTL_MODULE ||
        def->type == ComponentType::kMagazine) {
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

  for (auto& component : placed_components_) {
    if (is_selected_id(component.instanceId)) ReleaseHmiButtons(&component);
  }

  placed_components_.erase(
      std::remove_if(placed_components_.begin(), placed_components_.end(),
                     [&](const PlacedComponent& comp) {
                       return is_selected_id(comp.instanceId);
                     }),
      placed_components_.end());

  for (auto& piece : placed_components_) {
    if (is_selected_id(piece.carry_state.owner_id)) piece.carry_state = {};
  }
  ClearComponentSelection();
  RepairPrimaryWireSelection();
}

Application::ComponentMoveSession Application::BuildComponentMoveSession(
    int instanceId, ImVec2 mousePos) const {
  ComponentMoveSession session;
  session.anchor_component_id = instanceId;
  const PlacedComponent* anchor = nullptr;
  const PlacedComponent* owner = nullptr;
  for (const auto& component : placed_components_) {
    if (component.instanceId == instanceId) anchor = &component;
  }
  if (anchor) {
    for (const auto& component : placed_components_) {
      if (component.instanceId == anchor->carry_state.owner_id) owner = &component;
    }
  }
  if (anchor && IsWorkpieceType(anchor->type) && owner &&
      IsWarehouseType(owner->type) && anchor->carry_state.slot_id >= 1) {
    const Size display = GetComponentDisplaySize(*anchor);
    session.drag_start_offset = {display.width / 2, display.height / 2};
    session.offsets.push_back({instanceId, session.drag_start_offset});
    return session;
  }
  const bool move_selected_group = HasSelectedComponents();
  for (const auto& component : placed_components_) {
    if (!plc::IsWarehouseType(component.type) ||
        !(move_selected_group ? component.selected : component.instanceId == instanceId)) continue;
    if (std::any_of(placed_components_.begin(), placed_components_.end(),
        [&](const auto& part) { return part.carry_state.owner_id == component.instanceId; }))
      return session;
  }
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
      comp->carry_state = {};
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
  static thread_local std::vector<size_t> draw_order;
  plc_emulator::SortRenderLayerIndices(placed_components_, &draw_order);

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

  // Show the configured cylinder linkage and the warehouse travel axis.
  for (const auto& warehouse : placed_components_) {
    if (!plc::IsWarehouseType(warehouse.type) ||
        !plc_emulator::IsRenderVisible(warehouse)) continue;
    const auto* drive = FindComponentById(warehouse.warehouse_cylinder_id);
    if (!drive) continue;
    const Size size = GetComponentDisplaySize(warehouse);
    const ImVec2 center(warehouse.position.x + size.width * 0.5f,
                        warehouse.position.y + size.height * 0.5f);
    const ImVec2 base(center.x - warehouse.warehouse_drive_offset.x,
                      center.y - warehouse.warehouse_drive_offset.y);
    const ImVec2 direction = LocalDirToWorld(*drive, {-1.0f, 0.0f});
    const ImVec2 end(base.x + direction.x * warehouse.warehouse_travel,
                     base.y + direction.y * warehouse.warehouse_travel);
    const ImVec2 start_screen = WorldToScreen(base);
    const ImVec2 end_screen = WorldToScreen(end);
    const ImU32 guide_color = IM_COL32(25, 110, 130,
        static_cast<int>(180 * warehouse.render_opacity));
    draw_list->AddLine(start_screen, end_screen, guide_color,
                       3.0f * camera_zoom_);
    draw_list->AddCircle(start_screen, 5.0f * camera_zoom_, guide_color);
    draw_list->AddCircle(end_screen, 5.0f * camera_zoom_, guide_color);
    const ImVec2 drive_point = WorldToScreen(LocalToWorld(*drive, {240, 43}));
    const ImVec2 anchor = WorldToScreen(center);
    for (int segment = 0; segment < 20; segment += 2) {
      const float first = segment / 20.0f;
      const float last = (segment + 1) / 20.0f;
      draw_list->AddLine(
          {drive_point.x + (anchor.x - drive_point.x) * first,
           drive_point.y + (anchor.y - drive_point.y) * first},
          {drive_point.x + (anchor.x - drive_point.x) * last,
           drive_point.y + (anchor.y - drive_point.y) * last},
          guide_color, 2.0f * camera_zoom_);
    }
    const ImVec2 label = WorldToScreen({base.x + size.width * 0.5f + 12,
                                        end.y});
    char travel_text[64];
    std::snprintf(travel_text, sizeof(travel_text), "CYL -> WH %.0f mm",
                  warehouse.warehouse_travel);
    draw_list->AddText(ImGui::GetFont(), 22.0f * camera_zoom_, label,
                       guide_color, travel_text);
  }

  for (size_t index : draw_order) {
    auto& comp = placed_components_[index];
    if (!plc_emulator::IsRenderVisible(comp)) continue;
    if (comp.carry_state.owner_id >= 0 && comp.carry_state.slot_id >= 1) {
      const PlacedComponent* owner =
          FindComponentById(comp.carry_state.owner_id);
      if (owner && IsWarehouseType(owner->type)) continue;
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
      if (comp.type == ComponentType::PROCESSING_CYLINDER)
        RenderProcessingCylinderHead(draw_list, comp, render_origin, camera_zoom_);
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
    const float opacity = comp.render_opacity *
        ((ghost_mode && !comp.selected) ? ghost_alpha : 1.0f);
    ApplyAlphaToDrawList(draw_list, vtx_start, vtx_end, opacity);

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

  // Keep user labels upright even when the physical component is rotated.
  for (const auto& comp : placed_components_) {
    const Size display = GetComponentDisplaySize(comp);
    if (!plc_emulator::IsRenderVisible(comp) ||
        !comp.show_custom_label || comp.customLabel.empty() ||
        comp.carry_state.owner_id >= 0) {
      continue;
    }
    const ImVec2 origin = WorldToScreen(comp.position);
    const float font_size = std::max(18.0f, 22.0f * camera_zoom_);
    ImFont* font = ImGui::GetFont();
    const ImVec2 text_size = font->CalcTextSizeA(
        font_size, FLT_MAX, 0.0f, comp.customLabel.c_str());
    const ImVec2 label_pos(origin.x, origin.y - text_size.y - 5.0f);
    const int label_start = draw_list->VtxBuffer.Size;
    draw_list->AddRectFilled(
        {label_pos.x - 3.0f, label_pos.y - 2.0f},
        {label_pos.x + text_size.x + 3.0f, label_pos.y + text_size.y + 2.0f},
        IM_COL32(255, 255, 255, 235), 2.0f);
    draw_list->AddText(font, font_size, label_pos,
                       IM_COL32(45, 50, 58, 255), comp.customLabel.c_str());
    ApplyAlphaToDrawList(draw_list, label_start, draw_list->VtxBuffer.Size,
                         comp.render_opacity);
  }
  for (const auto& warehouse : placed_components_) {
    if (!plc::IsWarehouseType(warehouse.type) ||
        !plc_emulator::IsRenderVisible(warehouse)) continue;
    const bool occupied = std::any_of(placed_components_.begin(),
        placed_components_.end(), [&](const auto& part) {
          return part.carry_state.owner_id == warehouse.instanceId &&
                 part.carry_state.slot_id >= 1;
        });
    if (!occupied && !IsPointInsideComponent(warehouse,
        ScreenToWorld(ImGui::GetMousePos()))) continue;
    const ImVec2 origin = WorldToScreen(
        plc_emulator::WarehouseOverlayOrigin(warehouse, camera_zoom_));
    const int overlay_start = draw_list->VtxBuffer.Size;
    plc_emulator::RenderStorageSensorFront(
        draw_list, warehouse, origin,
        plc_emulator::kWarehouseOverlayScale * camera_zoom_, placed_components_);
    ApplyAlphaToDrawList(draw_list, overlay_start, draw_list->VtxBuffer.Size,
                         warehouse.render_opacity);
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

    const float toggle_size = std::max(
        {26.0f * layout_scale, ImGui::GetFrameHeight(),
         ImGui::CalcTextSize("Z").x + 2 * ImGui::GetStyle().FramePadding.x});
    ImGui::Spacing();
    ImGui::SetCursorPosX(15 * layout_scale);
    const float filter_width = std::max(
        60.0f, ImGui::GetContentRegionAvail().x - 3 * toggle_size -
                   3 * ImGui::GetStyle().ItemSpacing.x);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * layout_scale);
    ImGui::PushItemWidth(filter_width);
    const char* filter_label = TR("ui.component_list.filter_all", "All");
    if (component_list_filter_ == ComponentListFilter::AUTOMATION) {
      filter_label = TR("ui.component_list.filter_auto", "Automation");
    } else if (component_list_filter_ == ComponentListFilter::SEMICONDUCTOR) {
      filter_label = TR("ui.component_list.filter_semi", "Semiconductor");
    } else if (component_list_filter_ == ComponentListFilter::kAutomationExam) {
      filter_label = TR("ui.component_list.filter_exam", "Automation equipment exam");
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
      if (ImGui::Selectable(
              TR("ui.component_list.filter_exam", "Automation equipment exam"),
              component_list_filter_ == ComponentListFilter::kAutomationExam)) {
        component_list_filter_ = ComponentListFilter::kAutomationExam;
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::SameLine();
    if (ImGui::Button("Z##ZLayersToggle", {toggle_size, toggle_size})) {
      show_z_layer_panel_ = !show_z_layer_panel_;
    }
    ImGui::SameLine();
    if (ImGui::Button("L##RenderLayersToggle", {toggle_size, toggle_size})) {
      show_render_layer_panel_ = !show_render_layer_panel_;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", TR("ui.render_layers.title", "Display layers"));
    }
    ImGui::SameLine();
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
    } else if (component_list_filter_ == ComponentListFilter::kAutomationExam) {
      list_filter = ListFilter::kAutomationExam;
    }
    const char* function_labels[] = {
        TR("ui.component_list.function_all", "All functions"),
        TR("ui.component_list.function_input", "Input"),
        TR("ui.component_list.function_output", "Output"),
        TR("ui.component_list.function_pneumatic", "Pneumatic"),
        TR("ui.component_list.function_power", "Power"),
        TR("ui.component_list.function_control", "Control"),
        TR("ui.component_list.function_other", "Other")};
    ImGui::SetCursorPosX(15 * layout_scale);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("##ComponentFunctionFilter", &component_function_filter_,
                 function_labels, IM_ARRAYSIZE(function_labels));
    ImGui::Spacing();
    std::vector<ComponentListEntry> entries = BuildComponentList(list_filter);
    if (component_function_filter_ != 0) {
      std::erase_if(entries, [this](const ComponentListEntry& entry) {
        return GetComponentFunctionGroup(entry.def->type) !=
               component_function_filter_;
      });
    }
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
    ImGui::SetCursorPosX(10 * layout_scale);
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
          (component_function_filter_ == 0 || component_function_filter_ == 5) &&
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
          (component_function_filter_ == 0 || component_function_filter_ == 5) &&
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
