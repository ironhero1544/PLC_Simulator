// application_wiring.cpp
//
// Wire drawing and editing.

// src/Application_Wiring.cpp

#include "imgui.h"
#include "plc_emulator/components/component_behavior.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <map>
#include <utility>
#include <vector>
#include <string>

#include <GLFW/glfw3.h>

namespace plc {

namespace {

using PortRef = std::pair<int, int>;

int FindTopmostComponentIndexAtPosition(
    const std::vector<PlacedComponent>& components,
    ImVec2 world_pos) {
  int best_index = -1;
  int best_z = std::numeric_limits<int>::min();
  int best_instance = -1;
  for (size_t i = 0; i < components.size(); ++i) {
    const auto& comp = components[i];
    if (!IsPointInsideComponent(comp, world_pos)) {
      continue;
    }
    if (comp.z_order > best_z ||
        (comp.z_order == best_z && comp.instanceId > best_instance)) {
      best_z = comp.z_order;
      best_instance = comp.instanceId;
      best_index = static_cast<int>(i);
    }
  }
  return best_index;
}

ImVec2 GetBehaviorWorldPos(const PlacedComponent& comp, ImVec2 world_pos) {
  ImVec2 local = WorldToLocal(comp, world_pos);
  return ImVec2(comp.position.x + local.x, comp.position.y + local.y);
}

constexpr ImU32 kTagColors[] = {
    IM_COL32(0, 0, 0, 255),      // black (default)
    IM_COL32(200, 40, 40, 255),   // red
    IM_COL32(220, 110, 20, 255),  // orange
    IM_COL32(190, 140, 0, 255),   // yellow
    IM_COL32(0, 130, 70, 255),    // green
    IM_COL32(0, 90, 200, 255),    // blue
    IM_COL32(0, 60, 120, 255),    // navy
    IM_COL32(130, 50, 170, 255)   // purple
};

constexpr ImU32 kPneumaticTagBackground = IM_COL32(210, 235, 250, 255);
constexpr ImU32 kElectricTagBackground = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kTagTextColor = IM_COL32(20, 20, 20, 255);
constexpr float kTagOutwardDotThreshold = 0.35f;

ImU32 GetTagColor(int index) {
  const int count = static_cast<int>(sizeof(kTagColors) / sizeof(kTagColors[0]));
  if (count <= 0) {
    return IM_COL32(0, 0, 0, 255);
  }
  if (index < 0) {
    index = 0;
  }
  index %= count;
  return kTagColors[index];
}

enum class TagPointerSide { Left, Right, Top, Bottom };

struct TagDirectionInfo {
  ImVec2 dir = {1.0f, 0.0f};
  ImVec2 perp = {0.0f, 1.0f};
  TagPointerSide side = TagPointerSide::Right;
};

TagDirectionInfo ResolveTagDirection(ImVec2 port_pos,
                                     ImVec2 neighbor_pos,
                                     ImVec2 component_center,
                                     bool has_component_center) {
  TagDirectionInfo info;
  ImVec2 dir = {neighbor_pos.x - port_pos.x, neighbor_pos.y - port_pos.y};
  float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
  if (len > 0.001f) {
    dir.x /= len;
    dir.y /= len;
  } else {
    dir = {1.0f, 0.0f};
  }
  if (has_component_center) {
    ImVec2 outward = {port_pos.x - component_center.x,
                      port_pos.y - component_center.y};
    float outward_len =
        std::sqrt(outward.x * outward.x + outward.y * outward.y);
    if (outward_len > 0.001f) {
      outward.x /= outward_len;
      outward.y /= outward_len;
      float dot = dir.x * outward.x + dir.y * outward.y;
      if (dot < kTagOutwardDotThreshold) {
        dir = outward;
      }
    }
  }

  info.dir = dir;
  info.perp = {-dir.y, dir.x};

  if (std::abs(dir.x) >= std::abs(dir.y)) {
    info.side = (dir.x >= 0.0f) ? TagPointerSide::Right : TagPointerSide::Left;
  } else {
    info.side = (dir.y >= 0.0f) ? TagPointerSide::Bottom : TagPointerSide::Top;
  }

  return info;
}

struct TagLabelLayout {
  ImVec2 rect_min = {};
  ImVec2 rect_max = {};
  TagPointerSide side = TagPointerSide::Right;
  float tip_length = 0.0f;
  float tail_half_width = 0.0f;
  float padding = 0.0f;
};

TagLabelLayout BuildTagLabelLayout(ImVec2 port_pos,
                                   ImVec2 neighbor_pos,
                                   ImVec2 component_center,
                                   bool has_component_center,
                                   const std::string& text,
                                   float layout_scale,
                                   int stack_index) {
  TagLabelLayout layout;
  if (text.empty()) {
    layout.rect_min = port_pos;
    layout.rect_max = port_pos;
    return layout;
  }
  TagDirectionInfo direction = ResolveTagDirection(
      port_pos, neighbor_pos, component_center, has_component_center);
  ImVec2 dir = direction.dir;
  ImVec2 perp = direction.perp;
  layout.side = direction.side;

  layout.padding = 4.0f * layout_scale;
  layout.tip_length = 10.0f * layout_scale;
  layout.tail_half_width = 2.0f * layout_scale;
  ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
  float half_w = text_size.x * 0.5f + layout.padding;
  float half_h = text_size.y * 0.5f + layout.padding;

  float base_offset = 12.0f * layout_scale + layout.tip_length;
  if (layout.side == TagPointerSide::Left ||
      layout.side == TagPointerSide::Right) {
    base_offset += half_w;
  } else {
    base_offset += half_h;
  }

  float stack_offset = 0.0f;
  if (stack_index > 0) {
    int step = (stack_index + 1) / 2;
    float sign = (stack_index % 2 == 1) ? 1.0f : -1.0f;
    stack_offset = sign * static_cast<float>(step) * 12.0f * layout_scale;
  }

  ImVec2 anchor = {port_pos.x + dir.x * base_offset + perp.x * stack_offset,
                   port_pos.y + dir.y * base_offset + perp.y * stack_offset};
  layout.rect_min = {anchor.x - half_w, anchor.y - half_h};
  layout.rect_max = {anchor.x + half_w, anchor.y + half_h};
  return layout;
}

struct TagLayoutEntry {
  Wire* wire = nullptr;
  int component_id = 0;
  ImVec2 port_pos = {};
  ImVec2 neighbor_pos = {};
  ImVec2 component_center = {};
  bool has_component_center = false;
  TagPointerSide side = TagPointerSide::Right;
  TagLabelLayout layout;
};

template <typename WorldToScreenFn>
std::vector<TagLayoutEntry> BuildTagLayouts(
    std::vector<Wire>* wires,
    const std::map<PortRef, Position>& port_positions,
    const std::vector<PlacedComponent>& placed_components,
    WorldToScreenFn world_to_screen,
    float layout_scale) {
  std::vector<TagLayoutEntry> entries;
  std::map<int, ImVec2> component_centers;
  for (const auto& comp : placed_components) {
    const Size display = GetComponentDisplaySize(comp);
    Position center_world(comp.position.x + display.width * 0.5f,
                          comp.position.y + display.height * 0.5f);
    component_centers[comp.instanceId] = world_to_screen(center_world);
  }

  for (auto& wire : *wires) {
    if (!wire.isTagged || wire.tagText.empty()) {
      continue;
    }
    auto startIt =
        port_positions.find({wire.fromComponentId, wire.fromPortId});
    auto endIt = port_positions.find({wire.toComponentId, wire.toPortId});
    if (startIt == port_positions.end() || endIt == port_positions.end()) {
      continue;
    }

    ImVec2 start_screen_pos = world_to_screen(startIt->second);
    ImVec2 end_screen_pos = world_to_screen(endIt->second);
    ImVec2 start_neighbor = end_screen_pos;
    ImVec2 end_neighbor = start_screen_pos;
    if (!wire.wayPoints.empty()) {
      start_neighbor = world_to_screen(wire.wayPoints.front());
      end_neighbor = world_to_screen(wire.wayPoints.back());
    }

    auto append_entry = [&](int component_id,
                            ImVec2 port_pos,
                            ImVec2 neighbor_pos) {
      TagLayoutEntry entry;
      entry.wire = &wire;
      entry.component_id = component_id;
      entry.port_pos = port_pos;
      entry.neighbor_pos = neighbor_pos;
      auto center_it = component_centers.find(component_id);
      if (center_it != component_centers.end()) {
        entry.component_center = center_it->second;
        entry.has_component_center = true;
      }
      TagDirectionInfo direction = ResolveTagDirection(
          port_pos, neighbor_pos, entry.component_center,
          entry.has_component_center);
      entry.side = direction.side;
      entry.layout = BuildTagLabelLayout(
          port_pos, neighbor_pos, entry.component_center,
          entry.has_component_center, wire.tagText, layout_scale, 0);
      entries.push_back(entry);
    };

    append_entry(wire.fromComponentId, start_screen_pos, start_neighbor);
    append_entry(wire.toComponentId, end_screen_pos, end_neighbor);
  }

  using TagGroupKey = std::pair<int, TagPointerSide>;
  std::map<TagGroupKey, std::vector<TagLayoutEntry*>> groups;
  for (auto& entry : entries) {
    groups[{entry.component_id, entry.side}].push_back(&entry);
  }

  const float spacing = 8.0f * layout_scale;
  for (auto& [key, group] : groups) {
    if (group.empty()) {
      continue;
    }
    TagPointerSide side = key.second;
    const bool vertical = (side == TagPointerSide::Left ||
                           side == TagPointerSide::Right);
    std::sort(group.begin(), group.end(),
              [&](const TagLayoutEntry* a, const TagLayoutEntry* b) {
                return vertical ? (a->layout.rect_min.y <
                                   b->layout.rect_min.y)
                                : (a->layout.rect_min.x <
                                   b->layout.rect_min.x);
              });

    float last_max = -FLT_MAX;
    for (auto* entry : group) {
      float min_axis =
          vertical ? entry->layout.rect_min.y : entry->layout.rect_min.x;
      float max_axis =
          vertical ? entry->layout.rect_max.y : entry->layout.rect_max.x;
      if (min_axis < last_max + spacing) {
        float delta = (last_max + spacing) - min_axis;
        if (vertical) {
          entry->layout.rect_min.y += delta;
          entry->layout.rect_max.y += delta;
        } else {
          entry->layout.rect_min.x += delta;
          entry->layout.rect_max.x += delta;
        }
        min_axis += delta;
        max_axis += delta;
      }
      last_max = max_axis;
    }
  }

  return entries;
}

}  // namespace

// ?????FIX: ??????????? ??? ??????????????. ?????
// ??????? Application.h????? ??????, ???????? ?????????? ??????.
// Position GetPortRelativePositionById(const PlacedComponent& comp, int
// portId);

void Application::StartWireConnection(int componentId, int portId,
                                      ImVec2 portWorldPos) {
  if (!is_connecting_) {
    is_connecting_ = true;
    wire_start_component_id_ = componentId;
    wire_start_port_id_ = portId;
    wire_start_pos_ = portWorldPos;
    wire_current_pos_ = portWorldPos;
    current_way_points_.clear();
    last_pointer_world_pos_ = portWorldPos;
    last_pointer_move_time_ = ImGui::GetTime();
    last_auto_waypoint_time_ = last_pointer_move_time_;
  }
}

void Application::UpdateWireDrawing(ImVec2 mousePos) {
  if (is_connecting_) {
    wire_current_pos_ = mousePos;
  }
}

void Application::CompleteWireConnection(int componentId, int portId,
                                         ImVec2 portWorldPos) {
  if (is_connecting_ &&
      !(componentId == wire_start_component_id_ &&
        portId == wire_start_port_id_)) {
    int targetComponentId = -1;
    Port* targetPort = FindPortAtPosition(portWorldPos, targetComponentId);
    if (targetPort && targetComponentId == componentId) {
      int startComponentId = -1;
      Port* startPort = FindPortAtPosition(wire_start_pos_, startComponentId);
      if (startPort && startComponentId == wire_start_component_id_) {
        if (IsValidWireConnection(*startPort, *targetPort)) {
          PushWiringUndoState();
          Wire newWire;
          newWire.id = next_wire_id_++;
          newWire.fromComponentId = wire_start_component_id_;
          newWire.fromPortId = wire_start_port_id_;
          newWire.toComponentId = componentId;
          newWire.toPortId = portId;
          newWire.wayPoints = current_way_points_;
          newWire.isElectric = (current_tool_ == ToolType::ELECTRIC);

          if (newWire.isElectric) {
            newWire.color = {1.0f, 0.27f, 0.0f, 1.0f};
            newWire.thickness = 2.0f;
          } else {
            newWire.color = {0.13f, 0.59f, 0.95f, 1.0f};
            newWire.thickness = 3.0f;
          }
          wires_.push_back(newWire);
        }
      }
    }
    is_connecting_ = false;
    current_way_points_.clear();
  }
}

void Application::RenderWires(ImDrawList* draw_list) {
  const float layout_scale = GetLayoutScale();
  const ImGuiIO& io = ImGui::GetIO();
  const bool reveal_tagged = io.KeyAlt;
  Wire* hovered_tagged_wire =
      reveal_tagged ? FindTaggedWireAtScreenPos(io.MousePos) : nullptr;
  const float wire_cull_margin = 80.0f * layout_scale;
  ImVec2 canvas_min = canvas_top_left_;
  ImVec2 canvas_max =
      ImVec2(canvas_top_left_.x + canvas_size_.x,
             canvas_top_left_.y + canvas_size_.y);

  auto draw_tag_label = [&](const TagLabelLayout& layout,
                            ImVec2 port_pos,
                            bool is_electric,
                            int color_index,
                            const std::string& text) {
    if (text.empty()) {
      return;
    }
    ImU32 border = GetTagColor(color_index);
    ImU32 fill = is_electric ? kElectricTagBackground : kPneumaticTagBackground;
    float mid_x = (layout.rect_min.x + layout.rect_max.x) * 0.5f;
    float mid_y = (layout.rect_min.y + layout.rect_max.y) * 0.5f;

    draw_list->AddRectFilled(layout.rect_min, layout.rect_max, fill,
                             4.0f * layout_scale);
    draw_list->AddRect(layout.rect_min, layout.rect_max, border,
                       4.0f * layout_scale, 0, 1.5f * layout_scale);

    ImVec2 to_port = {port_pos.x - mid_x, port_pos.y - mid_y};
    ImVec2 tail_p1 = {};
    ImVec2 tail_p2 = {};
    ImVec2 tail_tip = port_pos;
    if (std::abs(to_port.x) >= std::abs(to_port.y)) {
      if (to_port.x >= 0.0f) {
        tail_p1 = {layout.rect_max.x, mid_y - layout.tail_half_width};
        tail_p2 = {layout.rect_max.x, mid_y + layout.tail_half_width};
      } else {
        tail_p1 = {layout.rect_min.x, mid_y - layout.tail_half_width};
        tail_p2 = {layout.rect_min.x, mid_y + layout.tail_half_width};
      }
    } else {
      if (to_port.y >= 0.0f) {
        tail_p1 = {mid_x - layout.tail_half_width, layout.rect_max.y};
        tail_p2 = {mid_x + layout.tail_half_width, layout.rect_max.y};
      } else {
        tail_p1 = {mid_x - layout.tail_half_width, layout.rect_min.y};
        tail_p2 = {mid_x + layout.tail_half_width, layout.rect_min.y};
      }
    }

    draw_list->AddTriangleFilled(tail_p1, tail_p2, tail_tip, fill);
    draw_list->AddLine(tail_p1, tail_tip, border, 1.5f * layout_scale);
    draw_list->AddLine(tail_p2, tail_tip, border, 1.5f * layout_scale);
    draw_list->AddText(ImVec2(layout.rect_min.x + layout.padding,
                              layout.rect_min.y + layout.padding),
                       kTagTextColor, text.c_str());
  };

  std::vector<ImVec2> waypoints_screen;
  for (auto& wire : wires_) {
    auto startIt =
        port_positions_.find({wire.fromComponentId, wire.fromPortId});
    auto endIt = port_positions_.find({wire.toComponentId, wire.toPortId});

    if (startIt == port_positions_.end() || endIt == port_positions_.end()) {
      continue;
    }
    if (wire.isTagged && !reveal_tagged) {
      continue;
    }
    if (wire.isTagged && reveal_tagged && hovered_tagged_wire &&
        wire.id != hovered_tagged_wire->id) {
      continue;
    }

    ImVec2 start_screen_pos = WorldToScreen(startIt->second);
    ImVec2 end_screen_pos = WorldToScreen(endIt->second);

    ImU32 wire_color;
    float wire_thickness = (wire.isElectric ? 2.0f : 3.0f);

    if (wire.isElectric) {
      auto port1_ref = std::make_pair(wire.fromComponentId, wire.fromPortId);
      auto port2_ref = std::make_pair(wire.toComponentId, wire.toPortId);
      float v1 = port_voltages_.count(port1_ref) ? port_voltages_.at(port1_ref)
                                                 : -1.0f;
      float v2 = port_voltages_.count(port2_ref) ? port_voltages_.at(port2_ref)
                                                 : -1.0f;

      if (v1 > 23.0f && v2 > 23.0f) {
        wire_color = IM_COL32(255, 87, 34, 255);
        wire_thickness = 2.5f;
      } else if (v1 < 1.0f && v2 < 1.0f && v1 >= 0.0f && v2 >= 0.0f) {
        wire_color = IM_COL32(21, 101, 192, 255);
        wire_thickness = 2.5f;
      } else {
        wire_color = IM_COL32(120, 120, 120, 200);
      }
    } else {
      wire_color =
          IM_COL32(static_cast<int>(wire.color.r * 255), static_cast<int>(wire.color.g * 255),
                   static_cast<int>(wire.color.b * 255), static_cast<int>(wire.color.a * 255));
    }

    if (wire.isTagged && reveal_tagged) {
      wire_color = GetTagColor(wire.tagColorIndex);
    }

    float min_x = std::min(start_screen_pos.x, end_screen_pos.x);
    float max_x = std::max(start_screen_pos.x, end_screen_pos.x);
    float min_y = std::min(start_screen_pos.y, end_screen_pos.y);
    float max_y = std::max(start_screen_pos.y, end_screen_pos.y);
    waypoints_screen.clear();
    waypoints_screen.reserve(wire.wayPoints.size());
    for (const auto& wp : wire.wayPoints) {
      ImVec2 next_pos = WorldToScreen(wp);
      waypoints_screen.push_back(next_pos);
      min_x = std::min(min_x, next_pos.x);
      max_x = std::max(max_x, next_pos.x);
      min_y = std::min(min_y, next_pos.y);
      max_y = std::max(max_y, next_pos.y);
    }
    if (max_x < canvas_min.x - wire_cull_margin ||
        min_x > canvas_max.x + wire_cull_margin ||
        max_y < canvas_min.y - wire_cull_margin ||
        min_y > canvas_max.y + wire_cull_margin) {
      continue;
    }

    ImVec2 current_pos = start_screen_pos;
    for (const auto& next_pos : waypoints_screen) {
      draw_list->AddLine(current_pos, next_pos, wire_color, wire_thickness);
      current_pos = next_pos;
    }
    draw_list->AddLine(current_pos, end_screen_pos, wire_color, wire_thickness);

    if (wire.id == selected_wire_id_) {
      ImU32 highlight_color = IM_COL32(255, 255, 0, 150);
      ImVec2 p1 = start_screen_pos;
      for (const auto& p2 : waypoints_screen) {
        draw_list->AddLine(p1, p2, highlight_color, wire_thickness + 3.0f);
        p1 = p2;
      }
      draw_list->AddLine(p1, end_screen_pos, highlight_color,
                         wire_thickness + 3.0f);
    }

    if (wire.id == selected_wire_id_ ||
        (editing_wire_id_ == wire.id &&
         wire_edit_mode_ == WireEditMode::MOVING_POINT)) {
      for (size_t i = 0; i < waypoints_screen.size(); ++i) {
        ImVec2 wp_pos = waypoints_screen[i];
        float radius = (editing_wire_id_ == wire.id && editing_point_index_ == static_cast<int>(i))
                           ? 8.0f
                           : 5.0f;
        draw_list->AddCircleFilled(wp_pos, radius, IM_COL32(255, 255, 0, 255));
        draw_list->AddCircle(wp_pos, radius, IM_COL32(0, 0, 0, 255), 12, 1.5f);
      }
    }
  }

  if (!reveal_tagged) {
    auto world_to_screen = [&](const Position& pos) {
      return WorldToScreen(pos);
    };
    std::vector<TagLayoutEntry> tag_layouts = BuildTagLayouts(
        &wires_, port_positions_, placed_components_, world_to_screen,
        layout_scale);
    for (const auto& entry : tag_layouts) {
      if (!entry.wire) {
        continue;
      }
      draw_tag_label(entry.layout, entry.port_pos, entry.wire->isElectric,
                     entry.wire->tagColorIndex, entry.wire->tagText);
    }
  }
}

Application::WiringUndoState Application::CaptureWiringState() const {
  WiringUndoState state;
  state.components = placed_components_;
  state.wires = wires_;
  state.selected_component_id = selected_component_id_;
  state.selected_wire_id = selected_wire_id_;
  state.next_instance_id = next_instance_id_;
  state.next_wire_id = next_wire_id_;
  state.next_z_order = next_z_order_;
  return state;
}

void Application::ApplyWiringState(const WiringUndoState& state) {
  placed_components_ = state.components;
  wires_ = state.wires;
  selected_component_id_ = state.selected_component_id;
  selected_wire_id_ = state.selected_wire_id;
  next_instance_id_ = state.next_instance_id;
  next_wire_id_ = state.next_wire_id;
  next_z_order_ = state.next_z_order;

  is_dragging_ = false;
  dragged_component_index_ = -1;
  is_moving_component_ = false;
  moving_component_id_ = -1;
  is_connecting_ = false;
  current_way_points_.clear();
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;
  show_tag_popup_ = false;
  tag_edit_wire_id_ = -1;
}

void Application::PushWiringUndoState() {
  wiring_undo_stack_.push_back(CaptureWiringState());
  if (wiring_undo_stack_.size() > kWiringUndoLimit) {
    wiring_undo_stack_.erase(wiring_undo_stack_.begin());
  }
  wiring_redo_stack_.clear();
}

void Application::UndoWiringState() {
  if (wiring_undo_stack_.empty()) {
    return;
  }
  wiring_redo_stack_.push_back(CaptureWiringState());
  ApplyWiringState(wiring_undo_stack_.back());
  wiring_undo_stack_.pop_back();
}

void Application::RedoWiringState() {
  if (wiring_redo_stack_.empty()) {
    return;
  }
  wiring_undo_stack_.push_back(CaptureWiringState());
  ApplyWiringState(wiring_redo_stack_.back());
  wiring_redo_stack_.pop_back();
}

void Application::DeleteWire(int wireId) {
  PushWiringUndoState();
  wires_.erase(
      std::remove_if(wires_.begin(), wires_.end(),
                     [wireId](const Wire& wire) { return wire.id == wireId; }),
      wires_.end());
  selected_wire_id_ = -1;
}

void Application::RenderWiringCanvas() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);

  ImGuiWindowFlags canvas_flags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  if (ImGui::BeginChild("Canvas", ImVec2(0, 0), true, canvas_flags)) {
    canvas_top_left_ = ImGui::GetCursorScreenPos();
    canvas_size_ = ImGui::GetContentRegionAvail();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    const bool popup_blocking =
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

    ImGui::SetCursorScreenPos(canvas_top_left_);
    ImGui::InvisibleButton("canvas_interaction_area", canvas_size_);
    ImVec2 mouse_screen_pos =
        last_pointer_is_pan_input_ ? touch_anchor_screen_pos_ : io.MousePos;
    ImVec2 mouse_world_pos = ScreenToWorld(mouse_screen_pos);

    const bool is_canvas_hovered =
        !popup_blocking &&
        (ImGui::IsItemHovered() ||
         (mouse_screen_pos.x >= canvas_top_left_.x &&
          mouse_screen_pos.x <= canvas_top_left_.x + canvas_size_.x &&
          mouse_screen_pos.y >= canvas_top_left_.y &&
          mouse_screen_pos.y <= canvas_top_left_.y + canvas_size_.y));
    const bool allow_pan_zoom =
        !popup_blocking && (is_canvas_hovered || touch_gesture_active_);
    const bool is_pan_input = last_pointer_is_pan_input_;
    const double now = ImGui::GetTime();
    const float move_epsilon = 1.0f;
    const float move_dx = mouse_world_pos.x - last_pointer_world_pos_.x;
    const float move_dy = mouse_world_pos.y - last_pointer_world_pos_.y;
    if ((move_dx * move_dx + move_dy * move_dy) >=
        (move_epsilon * move_epsilon)) {
      last_pointer_world_pos_ = mouse_world_pos;
      last_pointer_move_time_ = now;
    }

    bool frl_interaction_handled = false;
    bool wheel_handled = false;
    if (!popup_blocking) {
      // Handle FRL pressure adjustment
      frl_interaction_handled =
          HandleFRLPressureAdjustment(mouse_world_pos, io);

      // Handle zoom and pan
      HandleZoomAndPan(allow_pan_zoom, io, frl_interaction_handled,
                       wheel_handled);

      if ((wheel_handled || frl_interaction_handled) && is_canvas_hovered) {
        ImGui::SetItemDefaultFocus();
      }
    }

    // Render grid
    RenderGrid(draw_list);

    if (!popup_blocking) {
      // Handle component drop and wire deletion
      HandleComponentDropAndWireDelete(is_canvas_hovered, mouse_world_pos, io);

      auto clear_component_selection = [&]() {
        for (auto& comp : placed_components_) {
          comp.selected = false;
        }
        selected_component_id_ = -1;
      };

      auto try_begin_wire_edit = [&](ImVec2 world_pos) {
        constexpr float kWaypointRadius = 8.0f;
        for (auto& wire : wires_) {
          int idx = FindWayPointAtPosition(wire, world_pos, kWaypointRadius);
          if (idx >= 0) {
            PushWiringUndoState();
            clear_component_selection();
            selected_wire_id_ = wire.id;
            wire_edit_mode_ = WireEditMode::MOVING_POINT;
            editing_wire_id_ = wire.id;
            editing_point_index_ = idx;
            return true;
          }
        }
        return false;
      };

      bool wire_edit_active = (wire_edit_mode_ == WireEditMode::MOVING_POINT);
      if (wire_edit_active) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          for (auto& wire : wires_) {
            if (wire.id == editing_wire_id_ && editing_point_index_ >= 0 &&
                editing_point_index_ <
                    static_cast<int>(wire.wayPoints.size())) {
              ImVec2 reference_point = mouse_world_pos;
              if (editing_point_index_ > 0) {
                const Position& prev =
                    wire.wayPoints[static_cast<size_t>(editing_point_index_ -
                                                      1)];
                reference_point = ImVec2(prev.x, prev.y);
              } else {
                auto it = port_positions_.find(
                    {wire.fromComponentId, wire.fromPortId});
                if (it != port_positions_.end()) {
                  reference_point = ImVec2(it->second.x, it->second.y);
                }
              }

              ImVec2 snapped_pos = ApplyPortSnap(mouse_world_pos);
              if (snapped_pos.x == mouse_world_pos.x &&
                  snapped_pos.y == mouse_world_pos.y) {
                snapped_pos = ApplyAngleSnap(mouse_world_pos, reference_point);
              }
              wire.wayPoints[static_cast<size_t>(editing_point_index_)] =
                  snapped_pos;
            }
          }
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          wire_edit_mode_ = WireEditMode::NONE;
          editing_wire_id_ = -1;
          editing_point_index_ = -1;
        }
      }

      if (!wire_edit_active &&
          (current_tool_ == ToolType::SELECT ||
           current_tool_ == ToolType::TAG)) {
        if (is_moving_component_) {
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            for (auto& comp : placed_components_) {
              if (comp.instanceId == moving_component_id_) {
                comp.position.x = mouse_world_pos.x - drag_start_offset_.x;
                comp.position.y = mouse_world_pos.y - drag_start_offset_.y;
                break;
              }
            }
          }
          if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            HandleComponentMoveEnd();
        } else {
          if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
              is_canvas_hovered) {
            bool interaction_handled = false;
            if (io.KeyShift) {
              int top_index =
                  FindTopmostComponentIndexAtPosition(placed_components_,
                                                      mouse_world_pos);
              if (top_index != -1) {
                auto& comp =
                    placed_components_[static_cast<size_t>(top_index)];
                if (comp.type == ComponentType::LIMIT_SWITCH) {
                  float current =
                      comp.internalStates.count(state_keys::kIsPressedManual)
                          ? comp.internalStates.at(
                                state_keys::kIsPressedManual)
                          : 0.0f;
                  PushWiringUndoState();
                  comp.internalStates[state_keys::kIsPressedManual] =
                      1.0f - current;
                  HandleComponentSelection(comp.instanceId);
                  interaction_handled = true;
                }
              }
            }
            if (current_tool_ == ToolType::TAG) {
              Wire* tagged_wire =
                  FindTaggedWireAtScreenPos(mouse_screen_pos);
              if (tagged_wire && tagged_wire->isTagged) {
                selected_wire_id_ = tagged_wire->id;
                interaction_handled = true;
              } else {
                Wire* wire = FindWireAtPosition(mouse_world_pos);
                if (wire && !wire->isTagged) {
                  OpenTagPopupForWire(wire->id);
                  interaction_handled = true;
                }
              }
            }
            if (!interaction_handled) {
              int top_index =
                  FindTopmostComponentIndexAtPosition(placed_components_,
                                                      mouse_world_pos);
              if (top_index != -1) {
                HandleComponentSelection(
                    placed_components_[static_cast<size_t>(top_index)]
                        .instanceId);
                interaction_handled = true;
              }
            }
            if (!interaction_handled) {
              if (!try_begin_wire_edit(mouse_world_pos)) {
                HandleWireSelection(mouse_world_pos);
              }
            }
          }

          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
              is_canvas_hovered) {
            bool handled = false;
            if (current_tool_ == ToolType::TAG) {
              Wire* tagged_wire =
                  FindTaggedWireAtScreenPos(mouse_screen_pos);
              if (!tagged_wire) {
                Wire* wire = FindWireAtPosition(mouse_world_pos);
                if (wire && wire->isTagged) {
                  tagged_wire = wire;
                }
              }
              if (tagged_wire) {
                OpenTagPopupForWire(tagged_wire->id);
                handled = true;
              }
            }
            if (!handled) {
              int top_index =
                  FindTopmostComponentIndexAtPosition(placed_components_,
                                                      mouse_world_pos);
              if (top_index != -1) {
                auto& comp =
                    placed_components_[static_cast<size_t>(top_index)];
                const ComponentBehavior* behavior =
                    GetComponentBehavior(comp.type);
                bool handled_by_behavior = false;
                if (behavior && behavior->OnDoubleClick) {
                  ImVec2 adjusted_pos =
                      GetBehaviorWorldPos(comp, mouse_world_pos);
                  handled_by_behavior = behavior->OnDoubleClick(
                      &comp, adjusted_pos, ImGuiMouseButton_Left);
                }
                if (!handled_by_behavior) {
                  HandleComponentSelection(comp.instanceId);
                  OpenComponentContextMenu(comp.instanceId, mouse_screen_pos);
                }
              }
            }
          }

          if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
              is_canvas_hovered &&
              !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            int top_index =
                FindTopmostComponentIndexAtPosition(placed_components_,
                                                    mouse_world_pos);
            if (top_index != -1) {
              auto& comp =
                  placed_components_[static_cast<size_t>(top_index)];
              const ComponentBehavior* behavior =
                  GetComponentBehavior(comp.type);
              if (behavior && behavior->OnMouseDown) {
                ImVec2 adjusted_pos =
                    GetBehaviorWorldPos(comp, mouse_world_pos);
                behavior->OnMouseDown(&comp, adjusted_pos,
                                      ImGuiMouseButton_Left);
              }
            }
          } else {
            for (auto& comp : placed_components_) {
              const ComponentBehavior* behavior =
                  GetComponentBehavior(comp.type);
              if (behavior && behavior->OnMouseUp) {
                ImVec2 adjusted_pos =
                    GetBehaviorWorldPos(comp, mouse_world_pos);
                behavior->OnMouseUp(&comp, adjusted_pos,
                                    ImGuiMouseButton_Left);
              }
            }
          }

          if (is_canvas_hovered && !is_dragging_ &&
              ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
              selected_component_id_ != -1 && !is_moving_component_) {
            for (const auto& comp : placed_components_) {
              if (comp.instanceId == selected_component_id_) {
                if (IsPointInsideComponent(comp, mouse_world_pos)) {
                  HandleComponentMoveStart(selected_component_id_,
                                           mouse_world_pos);
                }
                break;
              }
            }
          }
        }
      } else if (!wire_edit_active) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
          int clickedComponentId = -1;
          Port* clickedPort =
              FindPortAtPosition(mouse_world_pos, clickedComponentId);
          if (clickedPort && clickedComponentId != -1) {
            ImVec2 port_world_pos = mouse_world_pos;
            auto it =
                port_positions_.find({clickedComponentId, clickedPort->id});
            if (it != port_positions_.end()) {
              port_world_pos = ImVec2(it->second.x, it->second.y);
            }
            if (!is_connecting_) {
              StartWireConnection(clickedComponentId, clickedPort->id,
                                  port_world_pos);
            } else {
              CompleteWireConnection(clickedComponentId, clickedPort->id,
                                     port_world_pos);
            }
          } else if (is_connecting_) {
            HandleWayPointClick(mouse_world_pos, true);
          } else {
            if (!try_begin_wire_edit(mouse_world_pos)) {
              HandleWireSelection(mouse_world_pos);
            }
          }
        }
        if (is_connecting_ && is_pan_input && is_canvas_hovered) {
          const double idle_threshold = 0.25;
          const double cooldown = 0.35;
          const float min_waypoint_dist = 6.0f;
          if ((now - last_pointer_move_time_) >= idle_threshold &&
              (now - last_auto_waypoint_time_) >= cooldown) {
            ImVec2 candidate = ApplySnap(mouse_world_pos, true);
            ImVec2 last_point = current_way_points_.empty()
                                    ? wire_start_pos_
                                    : ImVec2(current_way_points_.back().x,
                                             current_way_points_.back().y);
            float dx = candidate.x - last_point.x;
            float dy = candidate.y - last_point.y;
            if ((dx * dx + dy * dy) >=
                (min_waypoint_dist * min_waypoint_dist)) {
              AddWayPoint(candidate);
              last_auto_waypoint_time_ = now;
            }
          }
        }
      }
    }

    RenderPlacedComponents(draw_list);
    RenderWires(draw_list);
    if (is_connecting_) {
      ImVec2 snapped_mouse_pos = ApplySnap(mouse_world_pos, true);
      UpdateWireDrawing(snapped_mouse_pos);
      if (snapped_mouse_pos.x != mouse_world_pos.x ||
          snapped_mouse_pos.y != mouse_world_pos.y) {
        RenderSnapGuides(draw_list, snapped_mouse_pos);
      }
      RenderTemporaryWire(draw_list);
    }

    if (is_canvas_hovered && !is_dragging_ && !is_moving_component_ &&
        !is_connecting_) {
      bool tooltip_shown = false;
      int hoveredComponentId = -1;
      Port* hoveredPort =
          FindPortAtPosition(mouse_world_pos, hoveredComponentId);
      if (hoveredPort) {
        ImGui::BeginTooltip();
        char tooltip_buf[256] = {0};
        FormatString(tooltip_buf, sizeof(tooltip_buf),
                     "ui.wiring.tooltip_port_ids",
                     "Component ID: %d, Port ID: %d",
                     hoveredComponentId, hoveredPort->id);
        ImGui::TextUnformatted(tooltip_buf);
        ImGui::Separator();
        FormatString(tooltip_buf, sizeof(tooltip_buf),
                     "ui.wiring.tooltip_type_fmt", "Type: %s",
                     (hoveredPort->type == PortType::ELECTRIC)
                         ? TR("ui.wiring.port_type_electric", "Electric")
                         : TR("ui.wiring.port_type_pneumatic", "Pneumatic"));
        ImGui::TextUnformatted(tooltip_buf);
        FormatString(tooltip_buf, sizeof(tooltip_buf),
                     "ui.wiring.tooltip_role_fmt", "Role: %s",
                     hoveredPort->role.c_str());
        ImGui::TextUnformatted(tooltip_buf);
        FormatString(tooltip_buf, sizeof(tooltip_buf),
                     "ui.wiring.tooltip_direction_fmt", "Direction: %s",
                     hoveredPort->isInput
                         ? TR("ui.wiring.direction_input", "Input")
                         : TR("ui.wiring.direction_output", "Output"));
        ImGui::TextUnformatted(tooltip_buf);
        ImGui::EndTooltip();
        tooltip_shown = true;
      }

      if (!tooltip_shown) {
        int hover_index = is_canvas_hovered
                              ? FindTopmostComponentIndexAtPosition(
                                    placed_components_, mouse_world_pos)
                              : -1;
        if (hover_index != -1) {
          const auto& comp = placed_components_[static_cast<size_t>(hover_index)];
          const ComponentBehavior* behavior =
              GetComponentBehavior(comp.type);
          if (behavior && behavior->BuildTooltip &&
              behavior->BuildTooltip(
                  comp, GetBehaviorWorldPos(comp, mouse_world_pos))) {
            tooltip_shown = true;
          }
        }
        if (!tooltip_shown) {
          for (size_t i = 0; i < placed_components_.size(); ++i) {
            if (static_cast<int>(i) == hover_index) {
              continue;
            }
            const auto& comp = placed_components_[i];
            if (IsPointInsideComponent(comp, mouse_world_pos)) {
              const ComponentBehavior* behavior =
                  GetComponentBehavior(comp.type);
              if (behavior && behavior->BuildTooltip &&
                  behavior->BuildTooltip(
                      comp, GetBehaviorWorldPos(comp, mouse_world_pos))) {
                tooltip_shown = true;
                break;
              }
            }
          }
        }
      }
    }

    if (is_canvas_hovered) {
      const float layout_scale = GetLayoutScale();
      ImVec2 help_pos =
          ImVec2(canvas_top_left_.x + canvas_size_.x - 200 * layout_scale,
                 canvas_top_left_.y + canvas_size_.y - 100 * layout_scale);
      ImGui::SetCursorScreenPos(help_pos);

      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
      ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f * layout_scale);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                          ImVec2(8 * layout_scale, 6 * layout_scale));

      if (ImGui::BeginChild("CameraHelp",
                            ImVec2(190 * layout_scale, 90 * layout_scale),
                            false,
                            ImGuiWindowFlags_NoScrollbar)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        ImGui::Text("%s", TR("ui.wiring.camera_help.title", "Camera"));
        ImGui::Separator();
        ImGui::Text("%s", TR("ui.wiring.camera_help.zoom", "Zoom: Mouse wheel (no Ctrl)"));
        ImGui::Text("%s", TR("ui.wiring.camera_help.pan_middle",
                             "Pan: Middle drag / Alt + Right drag"));
        ImGui::Text("%s", TR("ui.wiring.camera_help.trackpad",
                             "Trackpad: Ctrl + Scroll (Pan)"));
        ImGui::Text("%s", TR("ui.wiring.camera_help.touch",
                             "Touch: Pinch to zoom, drag to pan"));
        ImGui::PopStyleColor();
      }
      ImGui::EndChild();
      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor();
    }

    RenderTagPopup();
    RenderComponentContextMenu();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

void Application::OpenTagPopupForWire(int wire_id) {
  tag_edit_wire_id_ = wire_id;
  tag_color_index_ = 0;
  tag_text_buffer_[0] = '\0';
  for (const auto& wire : wires_) {
    if (wire.id != wire_id) {
      continue;
    }
    if (wire.isTagged) {
      std::snprintf(tag_text_buffer_, sizeof(tag_text_buffer_), "%s",
                    wire.tagText.c_str());
      tag_color_index_ = wire.tagColorIndex;
    }
    break;
  }
  show_tag_popup_ = true;
}

void Application::RenderTagPopup() {
  if (show_tag_popup_ && !ImGui::IsPopupOpen("Wire Tag")) {
    ImGui::OpenPopup("Wire Tag");
  }

  bool popup_open = show_tag_popup_;
  if (ImGui::BeginPopupModal("Wire Tag", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    const float layout_scale = GetLayoutScale();
    ImGui::TextUnformatted(TR("ui.wiring.tag_title", "Wire Tag"));
    ImGui::Spacing();

    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }

    bool confirm = false;
    if (ImGui::InputText("Tag", tag_text_buffer_, sizeof(tag_text_buffer_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      confirm = true;
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(TR("ui.wiring.tag_color", "Color"));
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const int color_count =
        static_cast<int>(sizeof(kTagColors) / sizeof(kTagColors[0]));
    ImVec2 button_size(22.0f * layout_scale, 22.0f * layout_scale);
    for (int i = 0; i < color_count; ++i) {
      ImGui::PushID(i);
      ImVec4 color = ImGui::ColorConvertU32ToFloat4(kTagColors[i]);
      if (ImGui::ColorButton("##tagcolor", color,
                             ImGuiColorEditFlags_NoTooltip |
                                 ImGuiColorEditFlags_NoDragDrop |
                                 ImGuiColorEditFlags_NoPicker,
                             button_size)) {
        tag_color_index_ = i;
      }
      if (tag_color_index_ == i) {
        ImVec2 rect_min = ImGui::GetItemRectMin();
        ImVec2 rect_max = ImGui::GetItemRectMax();
        draw_list->AddRect(rect_min, rect_max, GetTagColor(i), 3.0f, 0,
                           2.0f * layout_scale);
      }
      ImGui::PopID();
      if (i < color_count - 1) {
        ImGui::SameLine();
      }
    }

    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120 * layout_scale, 0))) {
      confirm = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120 * layout_scale, 0))) {
      popup_open = false;
    }

    if (confirm) {
      Wire* target = nullptr;
      for (auto& wire : wires_) {
        if (wire.id == tag_edit_wire_id_) {
          target = &wire;
          break;
        }
      }
      if (target) {
        PushWiringUndoState();
        std::string tag_text(tag_text_buffer_);
        if (tag_text.empty()) {
          target->isTagged = false;
          target->tagText.clear();
          target->tagColorIndex = 0;
        } else {
          target->isTagged = true;
          target->tagText = tag_text;
          target->tagColorIndex = tag_color_index_;
        }
      }
      popup_open = false;
    }

    ImGui::EndPopup();
  }

  show_tag_popup_ = popup_open;
  if (!show_tag_popup_) {
    tag_edit_wire_id_ = -1;
  }
}
void Application::HandleWayPointClick(ImVec2 worldPos, bool use_port_snap_only) {
  (void)use_port_snap_only;
  if (is_connecting_) {
    ImVec2 snappedPos = ApplySnap(worldPos, true);
    AddWayPoint(snappedPos);
  }
}

void Application::AddWayPoint(ImVec2 position) {
  current_way_points_.push_back(position);
}

void Application::RenderTemporaryWire(ImDrawList* draw_list) {
  if (!is_connecting_)
    return;
  ImU32 temp_color = (current_tool_ == ToolType::ELECTRIC)
                         ? IM_COL32(255, 69, 0, 150)
                         : IM_COL32(33, 150, 243, 150);

  ImVec2 current_pos_on_screen = WorldToScreen(wire_start_pos_);

  for (const auto& wayPoint : current_way_points_) {
    ImVec2 next_pos_on_screen = WorldToScreen(wayPoint);
    draw_list->AddLine(current_pos_on_screen, next_pos_on_screen, temp_color,
                       2.0f);
    current_pos_on_screen = next_pos_on_screen;
  }

  ImVec2 end_pos_on_screen = WorldToScreen(wire_current_pos_);
  draw_list->AddLine(current_pos_on_screen, end_pos_on_screen, temp_color,
                     2.0f);
}

void Application::HandleWireSelection(ImVec2 worldPos) {
  Wire* selectedWire = FindWireAtPosition(worldPos);
  if (selectedWire) {
    selected_wire_id_ = selectedWire->id;
    for (auto& comp : placed_components_) {
      comp.selected = false;
    }
    selected_component_id_ = -1;
    wire_edit_mode_ = WireEditMode::NONE;
    editing_wire_id_ = -1;
    editing_point_index_ = -1;
  } else {
    selected_wire_id_ = -1;
    wire_edit_mode_ = WireEditMode::NONE;
    editing_wire_id_ = -1;
    editing_point_index_ = -1;
  }
}

int Application::FindWayPointAtPosition(Wire& wire, ImVec2 worldPos,
                                        float radius) {
  for (size_t i = 0; i < wire.wayPoints.size(); i++) {
    float dx = worldPos.x - wire.wayPoints[i].x;
    float dy = worldPos.y - wire.wayPoints[i].y;
    float dist_sq = dx * dx + dy * dy;
    if (dist_sq < (radius / camera_zoom_) * (radius / camera_zoom_)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

Wire* Application::FindWireAtPosition(ImVec2 worldPos, float tolerance) {
  Wire* closestWire = nullptr;
  float effective_tolerance = std::max(tolerance, 10.0f);
  float min_dist_sq = (effective_tolerance / camera_zoom_) *
                      (effective_tolerance / camera_zoom_);

  for (auto& wire : wires_) {
    auto startIt =
        port_positions_.find({wire.fromComponentId, wire.fromPortId});
    auto endIt = port_positions_.find({wire.toComponentId, wire.toPortId});
    if (startIt == port_positions_.end() || endIt == port_positions_.end())
      continue;

    ImVec2 p1 = startIt->second;

    std::vector<ImVec2> points_for_check;
    for (const auto& wp : wire.wayPoints) {
      points_for_check.push_back(wp);
    }
    points_for_check.push_back(endIt->second);

    for (const auto& p2 : points_for_check) {
      float dx = p2.x - p1.x;
      float dy = p2.y - p1.y;

      if (dx == 0 && dy == 0) {
        p1 = p2;
        continue;
      }

      float t = ((worldPos.x - p1.x) * dx + (worldPos.y - p1.y) * dy) /
                (dx * dx + dy * dy);
      t = std::max(0.0f, std::min(1.0f, t));

      float closestX = p1.x + t * dx;
      float closestY = p1.y + t * dy;

      float dist_sq = (worldPos.x - closestX) * (worldPos.x - closestX) +
                      (worldPos.y - closestY) * (worldPos.y - closestY);

      if (dist_sq < min_dist_sq) {
        min_dist_sq = dist_sq;
        closestWire = &wire;
      }
      p1 = p2;
    }
  }
  return closestWire;
}

Wire* Application::FindTaggedWireAtScreenPos(ImVec2 screen_pos) {
  const float layout_scale = GetLayoutScale();
  auto world_to_screen = [&](const Position& pos) {
    return WorldToScreen(pos);
  };
  std::vector<TagLayoutEntry> tag_layouts = BuildTagLayouts(
      &wires_, port_positions_, placed_components_, world_to_screen,
      layout_scale);
  for (const auto& entry : tag_layouts) {
    ImVec2 min = entry.layout.rect_min;
    ImVec2 max = entry.layout.rect_max;
    if (screen_pos.x >= min.x && screen_pos.x <= max.x &&
        screen_pos.y >= min.y && screen_pos.y <= max.y) {
      return entry.wire;
    }
  }
  return nullptr;
}

// ============================================================================
// RenderWiringCanvas Helper Functions (Phase 6 Refactoring)
// ============================================================================

bool Application::HandleFRLPressureAdjustment(ImVec2 mouse_world_pos, 
                                               const ImGuiIO& io) {
  if (!io.MouseWheel)
    return false;

  for (auto& comp : placed_components_) {
    if (IsPointInsideComponent(comp, mouse_world_pos)) {
      const ComponentBehavior* behavior =
          GetComponentBehavior(comp.type);
      if (behavior && behavior->OnMouseWheel &&
          behavior->OnMouseWheel(
              &comp, GetBehaviorWorldPos(comp, mouse_world_pos), io)) {
        return true;
      }
    }
  }
  return false;
}

void Application::HandleZoomAndPan(bool is_canvas_hovered, const ImGuiIO& io,
                                    bool frl_handled, bool& wheel_handled) {
  if (touch_gesture_active_ && is_canvas_hovered) {
    ImVec2 anchor = touch_anchor_screen_pos_;
    ImVec2 mouse_pos_before_zoom = ScreenToWorld(anchor);
    float zoom_factor = 1.0f + touch_zoom_delta_;
    if (zoom_factor > 0.01f) {
      camera_zoom_ *= zoom_factor;
      camera_zoom_ = std::max(0.05f, std::min(camera_zoom_, 10.0f));
      ImVec2 mouse_pos_after_zoom = ScreenToWorld(anchor);
      camera_offset_.x += (mouse_pos_before_zoom.x - mouse_pos_after_zoom.x);
      camera_offset_.y += (mouse_pos_before_zoom.y - mouse_pos_after_zoom.y);
    }

    camera_offset_.x += touch_pan_delta_.x / camera_zoom_;
    camera_offset_.y += touch_pan_delta_.y / camera_zoom_;
    touch_gesture_active_ = false;
    touch_zoom_delta_ = 0.0f;
    touch_pan_delta_ = ImVec2(0.0f, 0.0f);
    wheel_handled = true;
  }

  // Zoom with mouse wheel
  if (is_canvas_hovered && io.MouseWheel != 0 && !io.KeyCtrl && !frl_handled) {
    ImVec2 mouse_pos_before_zoom = ScreenToWorld(io.MousePos);
    float zoom_speed = 0.15f;
    if (io.MouseWheel > 0)
      camera_zoom_ *= (1.0f + zoom_speed);
    else
      camera_zoom_ *= (1.0f - zoom_speed);
    camera_zoom_ = std::max(0.05f, std::min(camera_zoom_, 10.0f));
    ImVec2 mouse_pos_after_zoom = ScreenToWorld(io.MousePos);
    camera_offset_.x += (mouse_pos_before_zoom.x - mouse_pos_after_zoom.x);
    camera_offset_.y += (mouse_pos_before_zoom.y - mouse_pos_after_zoom.y);
    wheel_handled = true;
  }

  // Trackpad pan with Ctrl
  if (is_canvas_hovered && io.KeyCtrl &&
      (io.MouseWheelH != 0 || io.MouseWheel != 0) && !frl_handled) {
    float trackpad_sensitivity = 0.8f;
    camera_offset_.x += io.MouseWheelH * 20.0f * trackpad_sensitivity;
    camera_offset_.y += io.MouseWheel * 20.0f * trackpad_sensitivity;
    wheel_handled = true;
  }
  
  // Pan with middle mouse
  float pan_sensitivity = 1.0f;
  if (is_canvas_hovered &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    camera_offset_.x += io.MouseDelta.x / camera_zoom_ * pan_sensitivity;
    camera_offset_.y += io.MouseDelta.y / camera_zoom_ * pan_sensitivity;
  }
  
  // Pan with Alt+Right
  if (is_canvas_hovered && io.KeyAlt &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
    camera_offset_.x += io.MouseDelta.x / camera_zoom_ * pan_sensitivity;
    camera_offset_.y += io.MouseDelta.y / camera_zoom_ * pan_sensitivity;
  }
}

void Application::RenderGrid(ImDrawList* draw_list) {
  float grid_step = snap_settings_.gridSize;
  ImVec2 world_top_left = ScreenToWorld(canvas_top_left_);
  ImVec2 world_bottom_right =
      ScreenToWorld(ImVec2(canvas_top_left_.x + canvas_size_.x,
                           canvas_top_left_.y + canvas_size_.y));
  
  // Vertical lines
  for (float x = floorf(world_top_left.x / grid_step) * grid_step;
       x < world_bottom_right.x; x += grid_step) {
    draw_list->AddLine(WorldToScreen({x, world_top_left.y}),
                       WorldToScreen({x, world_bottom_right.y}),
                       IM_COL32(220, 220, 220, 80));
  }
  
  // Horizontal lines
  for (float y = floorf(world_top_left.y / grid_step) * grid_step;
       y < world_bottom_right.y; y += grid_step) {
    draw_list->AddLine(WorldToScreen({world_top_left.x, y}),
                       WorldToScreen({world_bottom_right.x, y}),
                       IM_COL32(220, 220, 220, 80));
  }
}

void Application::UpdateTouchGesture(float zoom_delta, ImVec2 pan_delta,
                                     bool active) {
  touch_gesture_active_ = active;
  if (!active) {
    touch_zoom_delta_ = 0.0f;
    touch_pan_delta_ = ImVec2(0.0f, 0.0f);
    return;
  }
  touch_zoom_delta_ = zoom_delta;
  touch_pan_delta_ = pan_delta;
}

void Application::SetPanInputActive(bool active) {
  last_pointer_is_pan_input_ = active;
}

void Application::SetTouchAnchor(ImVec2 screen_pos) {
  touch_anchor_screen_pos_ = screen_pos;
}

void Application::HandleComponentDropAndWireDelete(bool is_canvas_hovered,
                                                    ImVec2 mouse_world_pos,
                                                    const ImGuiIO& io) {
  auto can_delete_wire = [&](const Wire* wire) -> bool {
    if (!wire) {
      return false;
    }
    if (!wire->isTagged) {
      return true;
    }
    if (io.KeyAlt) {
      return true;
    }
    Wire* tagged_wire = FindTaggedWireAtScreenPos(io.MousePos);
    return tagged_wire && tagged_wire->id == wire->id;
  };

  // Handle component drop
  if (is_dragging_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (is_canvas_hovered)
      HandleComponentDrop(mouse_world_pos);
    else {
      is_dragging_ = false;
      dragged_component_index_ = -1;
    }
  }

  bool right_click = win32_right_click_ ||
                     ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  bool right_double_click =
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right);
  bool right_down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) ==
                    GLFW_PRESS;
  if (!right_click && right_down && !prev_right_button_down_) {
    right_click = true;
  }
  win32_right_click_ = false;
  prev_right_button_down_ = right_down;

  if (right_click && is_canvas_hovered && io.KeyAlt) {
    Wire* tagged_wire = FindTaggedWireAtScreenPos(io.MousePos);
    if (!tagged_wire) {
      Wire* wire = FindWireAtPosition(mouse_world_pos);
      if (wire && wire->isTagged) {
        tagged_wire = wire;
      }
    }
    if (tagged_wire && tagged_wire->isTagged) {
      PushWiringUndoState();
      tagged_wire->isTagged = false;
      tagged_wire->tagText.clear();
      tagged_wire->tagColorIndex = 0;
    }
    return;
  }

  if (right_click && is_canvas_hovered && io.KeyCtrl) {
    int top_index =
        FindTopmostComponentIndexAtPosition(placed_components_,
                                            mouse_world_pos);
    if (top_index != -1) {
      const auto& comp = placed_components_[static_cast<size_t>(top_index)];
      HandleComponentSelection(comp.instanceId);
      OpenComponentContextMenu(comp.instanceId, io.MousePos);
    }
    return;
  }

  // Right click: cancel wire if connecting. Right double click: delete under cursor.
  if (right_click && !is_canvas_hovered) {
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] Right click: canvas not hovered");
    }
  }
  if (right_click && is_canvas_hovered && !io.KeyAlt) {
    if (is_connecting_) {
      is_connecting_ = false;
      current_way_points_.clear();
      if (IsDebugEnabled()) {
        DebugLog("[INPUT] Right click: cancel wire");
      }
    } else {
      bool deleted = false;
      if (right_double_click) {
        int top_index =
            FindTopmostComponentIndexAtPosition(placed_components_,
                                                mouse_world_pos);
        if (top_index != -1) {
          const auto& comp =
              placed_components_[static_cast<size_t>(top_index)];
          HandleComponentSelection(comp.instanceId);
          DeleteSelectedComponent();
          deleted = true;
          if (IsDebugEnabled()) {
            DebugLog("[INPUT] Right double click: delete component " +
                     std::to_string(comp.instanceId));
          }
        }
      }
      if (!deleted) {
        Wire* wire_to_delete = FindWireAtPosition(mouse_world_pos);
        if (wire_to_delete && !can_delete_wire(wire_to_delete)) {
          wire_to_delete = nullptr;
        }
        if (wire_to_delete) {
          int wire_id = wire_to_delete->id;
          DeleteWire(wire_id);
          deleted = true;
          if (IsDebugEnabled()) {
            DebugLog("[INPUT] Right click: delete wire " +
                     std::to_string(wire_id));
          }
        }
      }
      if (!deleted && IsDebugEnabled()) {
        DebugLog("[INPUT] Right click: nothing to delete at (" +
                 std::to_string(static_cast<int>(mouse_world_pos.x)) + "," +
                 std::to_string(static_cast<int>(mouse_world_pos.y)) + ")");
      }
    }
  }

  // Delete component or wire with side button (XButton1)
  bool side_click = win32_side_click_ || ImGui::IsMouseClicked(3);
  bool side_down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_4) ==
                   GLFW_PRESS;
  bool side_hold = win32_side_down_ || ImGui::IsMouseDown(3) || side_down;
  if (!side_click && side_down && !prev_side_button_down_) {
    side_click = true;
  }
  win32_side_click_ = false;
  win32_side_down_ = side_hold;
  prev_side_button_down_ = side_down;

  if (side_click && !is_canvas_hovered) {
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] Side click: canvas not hovered");
    }
  }
  if ((side_click || side_hold) && is_canvas_hovered) {
    if (side_click && is_connecting_) {
      is_connecting_ = false;
      current_way_points_.clear();
      if (IsDebugEnabled()) {
        DebugLog("[INPUT] Side click: cancel wire");
      }
    }
    bool deleted = false;
    if (side_click) {
      int top_index =
          FindTopmostComponentIndexAtPosition(placed_components_,
                                              mouse_world_pos);
      if (top_index != -1) {
        const auto& comp = placed_components_[static_cast<size_t>(top_index)];
        HandleComponentSelection(comp.instanceId);
        DeleteSelectedComponent();
        deleted = true;
        if (IsDebugEnabled()) {
          DebugLog("[INPUT] Side click: delete component " +
                   std::to_string(comp.instanceId));
        }
      }
    }
    if (!deleted) {
      const float delete_tolerance = side_hold ? 20.0f : 10.0f;
      Wire* wire_to_delete = FindWireAtPosition(mouse_world_pos,
                                                delete_tolerance);
      if (wire_to_delete && !can_delete_wire(wire_to_delete)) {
        wire_to_delete = nullptr;
      }
      if (wire_to_delete) {
        int wire_id = wire_to_delete->id;
        DeleteWire(wire_id);
        deleted = true;
        if (IsDebugEnabled()) {
          DebugLog("[INPUT] Side click: delete wire " +
                   std::to_string(wire_id));
        }
      }
    }
    if (!deleted && side_click && IsDebugEnabled()) {
      DebugLog("[INPUT] Side click: nothing to delete at (" +
               std::to_string(static_cast<int>(mouse_world_pos.x)) + "," +
               std::to_string(static_cast<int>(mouse_world_pos.y)) + ")");
    }
  }
}

bool Application::IsPointInCanvas(ImVec2 screen_pos) const {
  if (canvas_size_.x <= 0.0f || canvas_size_.y <= 0.0f) {
    return false;
  }
  return screen_pos.x >= canvas_top_left_.x &&
         screen_pos.x <= canvas_top_left_.x + canvas_size_.x &&
         screen_pos.y >= canvas_top_left_.y &&
         screen_pos.y <= canvas_top_left_.y + canvas_size_.y;
}

PlacedComponent* Application::FindComponentById(int instance_id) {
  for (auto& comp : placed_components_) {
    if (comp.instanceId == instance_id) {
      return &comp;
    }
  }
  return nullptr;
}

void Application::ApplyRotationToComponent(PlacedComponent* comp,
                                           int delta_quadrants) {
  if (!comp) {
    return;
  }
  const Size old_display = GetComponentDisplaySize(*comp);
  ImVec2 center(comp->position.x + old_display.width * 0.5f,
                comp->position.y + old_display.height * 0.5f);

  comp->rotation_quadrants =
      NormalizeRotationQuadrants(comp->rotation_quadrants + delta_quadrants);
  const Size new_display = GetComponentDisplaySize(*comp);
  comp->position.x = center.x - new_display.width * 0.5f;
  comp->position.y = center.y - new_display.height * 0.5f;
}

void Application::RotateSelectedComponent(int delta_quadrants) {
  if (selected_component_id_ == -1) {
    return;
  }
  PushWiringUndoState();
  PlacedComponent* comp = FindComponentById(selected_component_id_);
  ApplyRotationToComponent(comp, delta_quadrants);
}

void Application::BringComponentToFront(int component_id) {
  PlacedComponent* comp = FindComponentById(component_id);
  if (!comp) {
    return;
  }
  PushWiringUndoState();
  int max_z = std::numeric_limits<int>::min();
  for (const auto& entry : placed_components_) {
    max_z = std::max(max_z, entry.z_order);
  }
  comp->z_order = max_z + 1;
}

void Application::SendComponentToBack(int component_id) {
  PlacedComponent* comp = FindComponentById(component_id);
  if (!comp) {
    return;
  }
  PushWiringUndoState();
  int min_z = std::numeric_limits<int>::max();
  for (const auto& entry : placed_components_) {
    min_z = std::min(min_z, entry.z_order);
  }
  comp->z_order = min_z - 1;
}

void Application::BringComponentForward(int component_id) {
  PushWiringUndoState();
  std::vector<size_t> order(placed_components_.size());
  for (size_t i = 0; i < placed_components_.size(); ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(),
            [&](size_t a, size_t b) {
              const auto& comp_a = placed_components_[a];
              const auto& comp_b = placed_components_[b];
              if (comp_a.z_order != comp_b.z_order) {
                return comp_a.z_order < comp_b.z_order;
              }
              return comp_a.instanceId < comp_b.instanceId;
            });

  for (size_t i = 0; i < order.size(); ++i) {
    const auto& comp = placed_components_[order[i]];
    if (comp.instanceId == component_id) {
      if (i + 1 < order.size()) {
        auto& current = placed_components_[order[i]];
        auto& next = placed_components_[order[i + 1]];
        std::swap(current.z_order, next.z_order);
      }
      break;
    }
  }
}

void Application::SendComponentBackward(int component_id) {
  PushWiringUndoState();
  std::vector<size_t> order(placed_components_.size());
  for (size_t i = 0; i < placed_components_.size(); ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(),
            [&](size_t a, size_t b) {
              const auto& comp_a = placed_components_[a];
              const auto& comp_b = placed_components_[b];
              if (comp_a.z_order != comp_b.z_order) {
                return comp_a.z_order < comp_b.z_order;
              }
              return comp_a.instanceId < comp_b.instanceId;
            });

  for (size_t i = 0; i < order.size(); ++i) {
    const auto& comp = placed_components_[order[i]];
    if (comp.instanceId == component_id) {
      if (i > 0) {
        auto& current = placed_components_[order[i]];
        auto& prev = placed_components_[order[i - 1]];
        std::swap(current.z_order, prev.z_order);
      }
      break;
    }
  }
}

void Application::OpenComponentContextMenu(int component_id,
                                           ImVec2 screen_pos) {
  context_menu_component_id_ = component_id;
  context_menu_pos_ = screen_pos;
  show_component_context_menu_ = true;
}

void Application::RenderComponentContextMenu() {
  if (show_component_context_menu_) {
    ImGui::SetNextWindowPos(context_menu_pos_);
    ImGui::OpenPopup("Component Context");
    show_component_context_menu_ = false;
  }

  if (ImGui::BeginPopup("Component Context")) {
    int component_id = context_menu_component_id_;
    if (component_id != -1) {
      if (ImGui::MenuItem(
              TR("ui.wiring.context.bring_front", "Bring to Front"))) {
        BringComponentToFront(component_id);
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.send_back", "Send to Back"))) {
        SendComponentToBack(component_id);
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.bring_forward", "Bring Forward"))) {
        BringComponentForward(component_id);
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.send_backward", "Send Backward"))) {
        SendComponentBackward(component_id);
      }
      ImGui::Separator();
      if (ImGui::MenuItem(
              TR("ui.wiring.context.rotate_cw", "Rotate 90deg"))) {
        PlacedComponent* comp = FindComponentById(component_id);
        PushWiringUndoState();
        ApplyRotationToComponent(comp, 1);
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.rotate_ccw", "Rotate -90deg"))) {
        PlacedComponent* comp = FindComponentById(component_id);
        PushWiringUndoState();
        ApplyRotationToComponent(comp, -1);
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.flip_horizontal", "Flip Horizontal"))) {
        PlacedComponent* comp = FindComponentById(component_id);
        if (comp) {
          PushWiringUndoState();
          comp->flip_x = !comp->flip_x;
        }
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.flip_vertical", "Flip Vertical"))) {
        PlacedComponent* comp = FindComponentById(component_id);
        if (comp) {
          PushWiringUndoState();
          comp->flip_y = !comp->flip_y;
        }
      }
    }
    ImGui::EndPopup();
  } else if (!show_component_context_menu_) {
    context_menu_component_id_ = -1;
  }
}

}  // namespace plc
