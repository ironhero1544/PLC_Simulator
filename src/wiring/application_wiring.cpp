// application_wiring.cpp
//
// Wire drawing and editing.

// src/Application_Wiring.cpp

#include "imgui.h"
#include "plc_emulator/components/component_behavior.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/application.h"
#ifdef _WIN32
#include "plc_emulator/core/win32_haptics.h"
#endif
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

const char* RtlLogicFamilyLabel(RtlLogicFamily family) {
  switch (family) {
    case RtlLogicFamily::INDUSTRIAL_24V:
      return TR("ui.wiring.rtl.industrial_24v", "24V Industrial");
    case RtlLogicFamily::CMOS_5V:
      return TR("ui.wiring.rtl.cmos_5v", "CMOS 5V");
    case RtlLogicFamily::TTL_5V:
      return TR("ui.wiring.rtl.ttl_5v", "TTL 5V");
    default:
      return TR("ui.wiring.rtl.industrial_24v", "24V Industrial");
  }
}

const char* DebugComponentTypeLabel(ComponentType type) {
  switch (type) {
    case ComponentType::PLC:
      return "PLC";
    case ComponentType::FRL:
      return "FRL";
    case ComponentType::MANIFOLD:
      return "MANIFOLD";
    case ComponentType::METER_VALVE:
      return "METER_VALVE";
    case ComponentType::LIMIT_SWITCH:
      return "LIMIT_SWITCH";
    case ComponentType::BUTTON_UNIT:
      return "BUTTON_UNIT";
    case ComponentType::SENSOR:
      return "SENSOR";
    case ComponentType::INDUCTIVE_SENSOR:
      return "INDUCTIVE_SENSOR";
    case ComponentType::RING_SENSOR:
      return "RING_SENSOR";
    case ComponentType::CYLINDER:
      return "CYLINDER";
    case ComponentType::VALVE_SINGLE:
      return "VALVE_SINGLE";
    case ComponentType::VALVE_DOUBLE:
      return "VALVE_DOUBLE";
    case ComponentType::POWER_SUPPLY:
      return "POWER_SUPPLY";
    case ComponentType::WORKPIECE_METAL:
      return "WORKPIECE_METAL";
    case ComponentType::WORKPIECE_NONMETAL:
      return "WORKPIECE_NONMETAL";
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

bool IsPointInsideSelectionRect(ImVec2 point,
                                float min_x,
                                float min_y,
                                float max_x,
                                float max_y) {
  return point.x >= min_x && point.x <= max_x && point.y >= min_y &&
         point.y <= max_y;
}

float Cross2D(ImVec2 a, ImVec2 b, ImVec2 c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool IsPointOnSegment(ImVec2 a, ImVec2 b, ImVec2 point) {
  constexpr float kEpsilon = 0.0001f;
  if (std::fabs(Cross2D(a, b, point)) > kEpsilon) {
    return false;
  }
  return point.x >= std::min(a.x, b.x) - kEpsilon &&
         point.x <= std::max(a.x, b.x) + kEpsilon &&
         point.y >= std::min(a.y, b.y) - kEpsilon &&
         point.y <= std::max(a.y, b.y) + kEpsilon;
}

bool DoSegmentsIntersect(ImVec2 a1, ImVec2 a2, ImVec2 b1, ImVec2 b2) {
  const float cross1 = Cross2D(a1, a2, b1);
  const float cross2 = Cross2D(a1, a2, b2);
  const float cross3 = Cross2D(b1, b2, a1);
  const float cross4 = Cross2D(b1, b2, a2);

  const bool a_straddles =
      (cross1 > 0.0f && cross2 < 0.0f) || (cross1 < 0.0f && cross2 > 0.0f);
  const bool b_straddles =
      (cross3 > 0.0f && cross4 < 0.0f) || (cross3 < 0.0f && cross4 > 0.0f);
  if (a_straddles && b_straddles) {
    return true;
  }

  return IsPointOnSegment(a1, a2, b1) || IsPointOnSegment(a1, a2, b2) ||
         IsPointOnSegment(b1, b2, a1) || IsPointOnSegment(b1, b2, a2);
}

bool DoesSegmentIntersectSelectionRect(ImVec2 p1,
                                       ImVec2 p2,
                                       float min_x,
                                       float min_y,
                                       float max_x,
                                       float max_y) {
  if (IsPointInsideSelectionRect(p1, min_x, min_y, max_x, max_y) ||
      IsPointInsideSelectionRect(p2, min_x, min_y, max_x, max_y)) {
    return true;
  }

  if (std::max(p1.x, p2.x) < min_x || std::min(p1.x, p2.x) > max_x ||
      std::max(p1.y, p2.y) < min_y || std::min(p1.y, p2.y) > max_y) {
    return false;
  }

  const ImVec2 top_left(min_x, min_y);
  const ImVec2 top_right(max_x, min_y);
  const ImVec2 bottom_right(max_x, max_y);
  const ImVec2 bottom_left(min_x, max_y);
  return DoSegmentsIntersect(p1, p2, top_left, top_right) ||
         DoSegmentsIntersect(p1, p2, top_right, bottom_right) ||
         DoSegmentsIntersect(p1, p2, bottom_right, bottom_left) ||
         DoSegmentsIntersect(p1, p2, bottom_left, top_left);
}

bool DoesWireIntersectSelectionRect(const Wire& wire,
                                    const std::map<PortRef, Position>& ports,
                                    float min_x,
                                    float min_y,
                                    float max_x,
                                    float max_y) {
  auto start_it = ports.find({wire.fromComponentId, wire.fromPortId});
  auto end_it = ports.find({wire.toComponentId, wire.toPortId});
  if (start_it == ports.end() || end_it == ports.end()) {
    return false;
  }

  ImVec2 previous(start_it->second.x, start_it->second.y);
  for (const auto& waypoint : wire.wayPoints) {
    ImVec2 current(waypoint.x, waypoint.y);
    if (DoesSegmentIntersectSelectionRect(previous, current, min_x, min_y,
                                          max_x, max_y)) {
      return true;
    }
    previous = current;
  }

  ImVec2 end_point(end_it->second.x, end_it->second.y);
  return DoesSegmentIntersectSelectionRect(previous, end_point, min_x, min_y,
                                           max_x, max_y);
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

    if (wire.selected || wire.id == selected_wire_id_) {
      ImU32 highlight_color = IM_COL32(255, 255, 0, 150);
      ImVec2 p1 = start_screen_pos;
      for (const auto& p2 : waypoints_screen) {
        draw_list->AddLine(p1, p2, highlight_color, wire_thickness + 3.0f);
        p1 = p2;
      }
      draw_list->AddLine(p1, end_screen_pos, highlight_color,
                         wire_thickness + 3.0f);
    }

    if (wire.selected || wire.id == selected_wire_id_ ||
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
  state.plc_input_mode = plc_input_mode_;
  state.plc_output_mode = plc_output_mode_;
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
  plc_input_mode_ = state.plc_input_mode;
  plc_output_mode_ = state.plc_output_mode;

  is_dragging_ = false;
  dragged_component_index_ = -1;
  is_moving_component_ = false;
  moving_component_id_ = -1;
  moving_component_offsets_.clear();
  box_select_pending_ = false;
  is_box_selecting_ = false;
  box_select_additive_ = false;
  box_select_base_selection_ids_.clear();
  box_select_base_wire_ids_.clear();
  is_connecting_ = false;
  current_way_points_.clear();
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;
  show_tag_popup_ = false;
  tag_edit_wire_id_ = -1;
  OnElectricalConfigChanged();
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

void Application::ToggleWireSelection(int wireId) {
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;

  for (auto& wire : wires_) {
    if (wire.id != wireId) {
      continue;
    }
    wire.selected = !wire.selected;
    if (wire.selected) {
      selected_wire_id_ = wireId;
    } else if (selected_wire_id_ == wireId) {
      selected_wire_id_ = -1;
    }
    break;
  }
  RepairPrimaryWireSelection();
}

void Application::ClearWireSelection() {
  for (auto& wire : wires_) {
    wire.selected = false;
  }
  selected_wire_id_ = -1;
}

void Application::RepairPrimaryWireSelection() {
  if (selected_wire_id_ != -1) {
    for (const auto& wire : wires_) {
      if (wire.id == selected_wire_id_ && wire.selected) {
        return;
      }
    }
  }

  selected_wire_id_ = -1;
  for (const auto& wire : wires_) {
    if (wire.selected) {
      selected_wire_id_ = wire.id;
      return;
    }
  }
}

bool Application::HasSelectedWires() const {
  for (const auto& wire : wires_) {
    if (wire.selected) {
      return true;
    }
  }
  return false;
}

void Application::DeleteWire(int wireId) {
  PushWiringUndoState();
  wires_.erase(
      std::remove_if(wires_.begin(), wires_.end(),
                     [wireId](const Wire& wire) { return wire.id == wireId; }),
      wires_.end());
  if (selected_wire_id_ == wireId) {
    selected_wire_id_ = -1;
  }
  if (editing_wire_id_ == wireId) {
    wire_edit_mode_ = WireEditMode::NONE;
    editing_wire_id_ = -1;
    editing_point_index_ = -1;
  }
  RepairPrimaryWireSelection();
}

void Application::DeleteSelectedWires() {
  std::vector<int> selected_ids;
  for (const auto& wire : wires_) {
    if (wire.selected) {
      selected_ids.push_back(wire.id);
    }
  }
  if (selected_ids.empty() && selected_wire_id_ >= 0) {
    selected_ids.push_back(selected_wire_id_);
  }
  if (selected_ids.empty()) {
    return;
  }

  PushWiringUndoState();
  wires_.erase(
      std::remove_if(wires_.begin(), wires_.end(),
                     [&](const Wire& wire) {
                       return std::find(selected_ids.begin(), selected_ids.end(),
                                        wire.id) != selected_ids.end();
                     }),
      wires_.end());
  ClearWireSelection();
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;
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
    canvas_directly_hovered_ = ImGui::IsItemHovered() && ImGui::IsWindowHovered();
    ImVec2 mouse_screen_pos =
        last_pointer_is_pan_input_ ? touch_anchor_screen_pos_ : io.MousePos;
    ImVec2 mouse_world_pos = ScreenToWorld(mouse_screen_pos);
    const bool mouse_within_canvas_bounds =
        (mouse_screen_pos.x >= canvas_top_left_.x &&
         mouse_screen_pos.x <= canvas_top_left_.x + canvas_size_.x &&
         mouse_screen_pos.y >= canvas_top_left_.y &&
         mouse_screen_pos.y <= canvas_top_left_.y + canvas_size_.y);
    const bool overlay_point_blocking =
        IsPointInOverlayCaptureRect(mouse_screen_pos);

    const bool is_canvas_hovered =
        !popup_blocking &&
        !overlay_point_blocking &&
        (ImGui::IsItemHovered() || mouse_within_canvas_bounds);
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

    // Render grid
    RenderGrid(draw_list);

    if (!popup_blocking) {
      // Handle component drop and wire deletion
      HandleComponentDropAndWireDelete(is_canvas_hovered, mouse_world_pos, io);
      const bool wire_edit_active = HandleWireEditMode(mouse_world_pos);
      if (!wire_edit_active &&
          (current_tool_ == ToolType::SELECT ||
           current_tool_ == ToolType::TAG)) {
        HandleSelectMode(is_canvas_hovered, mouse_screen_pos, mouse_world_pos,
                         io);
      } else if (!wire_edit_active) {
        HandleWireConnectionMode(is_canvas_hovered, is_pan_input,
                                 mouse_world_pos, now);
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

    if (current_tool_ == ToolType::SELECT && is_box_selecting_) {
      ImVec2 box_start_screen = WorldToScreen(box_select_start_world_);
      ImVec2 box_end_screen = WorldToScreen(box_select_current_world_);
      ImVec2 rect_min(std::min(box_start_screen.x, box_end_screen.x),
                      std::min(box_start_screen.y, box_end_screen.y));
      ImVec2 rect_max(std::max(box_start_screen.x, box_end_screen.x),
                      std::max(box_start_screen.y, box_end_screen.y));
      draw_list->AddRectFilled(rect_min, rect_max, IM_COL32(0, 123, 255, 35));
      draw_list->AddRect(rect_min, rect_max, IM_COL32(0, 123, 255, 200), 0.0f,
                         0, 1.5f);
    }

    if (is_canvas_hovered && !is_dragging_ && !is_moving_component_ &&
        !is_connecting_ && !is_box_selecting_ && !box_select_pending_) {
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
                 canvas_top_left_.y + canvas_size_.y - 118 * layout_scale);
      ImGui::SetCursorScreenPos(help_pos);

      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
      ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f * layout_scale);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                          ImVec2(8 * layout_scale, 6 * layout_scale));

      if (ImGui::BeginChild("CameraHelp",
                            ImVec2(190 * layout_scale, 108 * layout_scale),
                            false,
                            ImGuiWindowFlags_NoScrollbar)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
        ImGui::Text("%s", TR("ui.wiring.camera_help.title", "Camera"));
        ImGui::Separator();
        ImGui::Text("%s", TR("ui.wiring.camera_help.zoom", "Zoom: Mouse wheel / Trackpad pinch"));
        ImGui::Text("%s", TR("ui.wiring.camera_help.pan_alt",
                             "Trackpad: Pinch to zoom"));
        ImGui::Text("%s", TR("ui.wiring.camera_help.pan_middle",
                             "Pan: Middle drag / Alt + Right drag"));
        ImGui::Text("%s", TR("ui.wiring.camera_help.trackpad",
                             "Trackpad: Two-finger scroll to pan"));
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

void Application::ResetWireEditState() {
  wire_edit_mode_ = WireEditMode::NONE;
  editing_wire_id_ = -1;
  editing_point_index_ = -1;
}

bool Application::TryBuildWireEditCommand(
    ImVec2 mouse_world_pos,
    CanvasInteractionCommand* out_command) {
  if (!out_command) {
    return false;
  }
  constexpr float kWaypointRadius = 8.0f;
  for (auto& wire : wires_) {
    int idx = FindWayPointAtPosition(wire, mouse_world_pos, kWaypointRadius);
    if (idx < 0) {
      continue;
    }

    out_command->type = CanvasInteractionCommand::Type::BeginWirePointEdit;
    out_command->wire_id = wire.id;
    out_command->point_index = idx;
    return true;
  }
  return false;
}

void Application::CancelBoxSelection() {
  box_select_pending_ = false;
  is_box_selecting_ = false;
  box_select_additive_ = false;
  box_select_base_selection_ids_.clear();
  box_select_base_wire_ids_.clear();
}

void Application::SnapshotBoxSelection() {
  box_select_base_selection_ids_.clear();
  box_select_base_wire_ids_.clear();
  for (const auto& comp : placed_components_) {
    if (comp.selected) {
      box_select_base_selection_ids_.push_back(comp.instanceId);
    }
  }
  for (const auto& wire : wires_) {
    if (wire.selected || wire.id == selected_wire_id_) {
      box_select_base_wire_ids_.push_back(wire.id);
    }
  }
}

Application::BoxSelectionResult Application::BuildBoxSelectionResult() const {
  BoxSelectionResult result;
  const float min_x =
      std::min(box_select_start_world_.x, box_select_current_world_.x);
  const float max_x =
      std::max(box_select_start_world_.x, box_select_current_world_.x);
  const float min_y =
      std::min(box_select_start_world_.y, box_select_current_world_.y);
  const float max_y =
      std::max(box_select_start_world_.y, box_select_current_world_.y);

  int topmost_hit_id = -1;
  int topmost_hit_z = std::numeric_limits<int>::min();
  int topmost_hit_instance = -1;
  int topmost_added_id = -1;
  int topmost_added_z = std::numeric_limits<int>::min();
  int topmost_added_instance = -1;
  int topmost_hit_wire_id = -1;
  int topmost_added_wire_id = -1;

  for (auto& comp : placed_components_) {
    const Size display = GetComponentDisplaySize(comp);
    const bool hit =
        !(comp.position.x + display.width < min_x ||
          comp.position.x > max_x ||
          comp.position.y + display.height < min_y ||
          comp.position.y > max_y);
    const bool base_selected =
        std::find(box_select_base_selection_ids_.begin(),
                  box_select_base_selection_ids_.end(),
                  comp.instanceId) != box_select_base_selection_ids_.end();
    const bool new_selected = box_select_additive_ ? (base_selected != hit) : hit;
    if (new_selected) {
      result.selected_component_ids.push_back(comp.instanceId);
    }

    if (hit &&
        (comp.z_order > topmost_hit_z ||
         (comp.z_order == topmost_hit_z &&
          comp.instanceId > topmost_hit_instance))) {
      topmost_hit_id = comp.instanceId;
      topmost_hit_z = comp.z_order;
      topmost_hit_instance = comp.instanceId;
    }

    if (box_select_additive_ && hit && !base_selected && new_selected &&
        (comp.z_order > topmost_added_z ||
         (comp.z_order == topmost_added_z &&
          comp.instanceId > topmost_added_instance))) {
      topmost_added_id = comp.instanceId;
      topmost_added_z = comp.z_order;
      topmost_added_instance = comp.instanceId;
    }
  }

  for (auto& wire : wires_) {
    const bool hit = DoesWireIntersectSelectionRect(wire, port_positions_, min_x,
                                                    min_y, max_x, max_y);
    const bool base_selected =
        std::find(box_select_base_wire_ids_.begin(),
                  box_select_base_wire_ids_.end(),
                  wire.id) != box_select_base_wire_ids_.end();
    const bool new_selected = box_select_additive_ ? (base_selected != hit) : hit;
    if (new_selected) {
      result.selected_wire_ids.push_back(wire.id);
    }

    if (hit && wire.id > topmost_hit_wire_id) {
      topmost_hit_wire_id = wire.id;
    }

    if (box_select_additive_ && hit && !base_selected && new_selected &&
        wire.id > topmost_added_wire_id) {
      topmost_added_wire_id = wire.id;
    }
  }

  if (box_select_additive_) {
    result.primary_component_id = selected_component_id_;
    result.primary_wire_id = selected_wire_id_;
    if (topmost_added_id != -1) {
      result.primary_component_id = topmost_added_id;
    }
    if (topmost_added_wire_id != -1) {
      result.primary_wire_id = topmost_added_wire_id;
    }
  } else {
    result.primary_component_id = topmost_hit_id;
    result.primary_wire_id = topmost_hit_wire_id;
  }
  return result;
}

void Application::ApplyBoxSelectionResult(const BoxSelectionResult& result) {
  ResetWireEditState();

  for (auto& comp : placed_components_) {
    comp.selected =
        std::find(result.selected_component_ids.begin(),
                  result.selected_component_ids.end(),
                  comp.instanceId) != result.selected_component_ids.end();
  }

  for (auto& wire : wires_) {
    wire.selected =
        std::find(result.selected_wire_ids.begin(),
                  result.selected_wire_ids.end(),
                  wire.id) != result.selected_wire_ids.end();
  }

  selected_component_id_ = result.primary_component_id;
  selected_wire_id_ = result.primary_wire_id;
  RepairPrimarySelection();
  RepairPrimaryWireSelection();
}

void Application::UpdateEditedWirePoint(ImVec2 mouse_world_pos) {
  for (auto& wire : wires_) {
    if (wire.id != editing_wire_id_ || editing_point_index_ < 0 ||
        editing_point_index_ >= static_cast<int>(wire.wayPoints.size())) {
      continue;
    }

    ImVec2 reference_point = mouse_world_pos;
    if (editing_point_index_ > 0) {
      const Position& prev =
          wire.wayPoints[static_cast<size_t>(editing_point_index_ - 1)];
      reference_point = ImVec2(prev.x, prev.y);
    } else {
      auto it = port_positions_.find({wire.fromComponentId, wire.fromPortId});
      if (it != port_positions_.end()) {
        reference_point = ImVec2(it->second.x, it->second.y);
      }
    }

    ImVec2 snapped_pos = ApplyPortSnap(mouse_world_pos);
    if (snapped_pos.x == mouse_world_pos.x &&
        snapped_pos.y == mouse_world_pos.y) {
      snapped_pos = ApplyAngleSnap(mouse_world_pos, reference_point);
    }
    wire.wayPoints[static_cast<size_t>(editing_point_index_)] = snapped_pos;
    break;
  }
}

bool Application::HandleWireEditMode(ImVec2 mouse_world_pos) {
  const bool wire_edit_active = (wire_edit_mode_ == WireEditMode::MOVING_POINT);
  if (!wire_edit_active) {
    return false;
  }

  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    CanvasInteractionCommand command;
    command.type = CanvasInteractionCommand::Type::UpdateWirePointEdit;
    command.world_pos = mouse_world_pos;
    ApplyCanvasInteractionCommand(command);
  } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    CanvasInteractionCommand command;
    command.type = CanvasInteractionCommand::Type::EndWirePointEdit;
    ApplyCanvasInteractionCommand(command);
  }

  return true;
}

void Application::UpdateMovingComponentPositions(ImVec2 mouse_world_pos) {
  for (const auto& moving_entry : moving_component_offsets_) {
    PlacedComponent* comp = FindComponentById(moving_entry.first);
    if (!comp) {
      continue;
    }
    comp->position.x = mouse_world_pos.x - moving_entry.second.x;
    comp->position.y = mouse_world_pos.y - moving_entry.second.y;
  }
}

void Application::HandleSelectMode(bool is_canvas_hovered,
                                   ImVec2 mouse_screen_pos,
                                   ImVec2 mouse_world_pos,
                                   const ImGuiIO& io) {
  if (current_tool_ == ToolType::SELECT) {
    if (box_select_pending_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      box_select_current_world_ = mouse_world_pos;
      const ImVec2 box_drag_delta(mouse_screen_pos.x - box_select_press_screen_.x,
                                  mouse_screen_pos.y - box_select_press_screen_.y);
      const float box_drag_threshold = 6.0f;
      if (box_drag_delta.x * box_drag_delta.x +
              box_drag_delta.y * box_drag_delta.y >=
          box_drag_threshold * box_drag_threshold) {
        box_select_pending_ = false;
        is_box_selecting_ = true;
        ApplyBoxSelectionResult(BuildBoxSelectionResult());
      }
    }

    if (is_box_selecting_) {
      box_select_current_world_ = mouse_world_pos;
      ApplyBoxSelectionResult(BuildBoxSelectionResult());
      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        CancelBoxSelection();
      }
    } else if (box_select_pending_ &&
               ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      if (!box_select_additive_) {
        ApplyBoxSelectionResult(BoxSelectionResult{});
      }
      CancelBoxSelection();
    }
  }

  if (is_moving_component_) {
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::UpdateComponentMove;
      command.world_pos = mouse_world_pos;
      ApplyCanvasInteractionCommand(command);
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::EndComponentMove;
      ApplyCanvasInteractionCommand(command);
    }
    return;
  }

  std::vector<CanvasInteractionCommand> commands;
  auto queue_command = [&](const CanvasInteractionCommand& command) {
    commands.push_back(command);
  };
  auto flush_commands = [&]() {
    if (commands.empty()) {
      return;
    }
    ApplyCanvasInteractionCommands(commands);
    commands.clear();
  };

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
    bool interaction_handled = false;
    if (io.KeyShift) {
      int top_index =
          FindTopmostComponentIndexAtPosition(placed_components_, mouse_world_pos);
      if (top_index != -1) {
        auto& comp = placed_components_[static_cast<size_t>(top_index)];
        if (comp.type == ComponentType::LIMIT_SWITCH) {
          PushWiringUndoState();
          ComponentInteractionResult interaction;
          interaction.handled = true;
          ComponentInteractionCommand interaction_command;
          interaction_command.type =
              ComponentInteractionCommandType::ToggleLimitSwitchManualOverride;
          interaction.commands.push_back(interaction_command);

          CanvasInteractionCommand canvas_command;
          canvas_command.type =
              CanvasInteractionCommand::Type::ApplyComponentInteraction;
          canvas_command.component_id = comp.instanceId;
          canvas_command.component_interaction = interaction;
          queue_command(canvas_command);

          CanvasInteractionCommand select_command;
          select_command.type =
              CanvasInteractionCommand::Type::SelectComponentExclusive;
          select_command.component_id = comp.instanceId;
          queue_command(select_command);
          interaction_handled = true;
        }
      }
    }

    if (current_tool_ == ToolType::TAG) {
      Wire* tagged_wire = FindTaggedWireAtScreenPos(mouse_screen_pos);
      if (tagged_wire && tagged_wire->isTagged) {
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::SelectWireExclusive;
        command.wire_id = tagged_wire->id;
        queue_command(command);
        interaction_handled = true;
      } else {
        Wire* wire = FindWireAtPosition(mouse_world_pos);
        if (wire && !wire->isTagged) {
          CanvasInteractionCommand command;
          command.type = CanvasInteractionCommand::Type::OpenWireTagPopup;
          command.wire_id = wire->id;
          queue_command(command);
          interaction_handled = true;
        }
      }
    }

    if (!interaction_handled) {
      int top_index =
          FindTopmostComponentIndexAtPosition(placed_components_, mouse_world_pos);
      if (top_index != -1) {
        const bool already_selected =
            placed_components_[static_cast<size_t>(top_index)].selected;
        const int clicked_id =
            placed_components_[static_cast<size_t>(top_index)].instanceId;
        CanvasInteractionCommand command;
        command.component_id = clicked_id;
        if (current_tool_ == ToolType::SELECT && io.KeyCtrl) {
          command.type = CanvasInteractionCommand::Type::ToggleComponentSelection;
        } else if (current_tool_ == ToolType::SELECT && already_selected) {
          command.type =
              CanvasInteractionCommand::Type::SetPrimarySelectedComponent;
        } else {
          command.type = CanvasInteractionCommand::Type::SelectComponentExclusive;
        }
        queue_command(command);
        interaction_handled = true;
      }
    }

    if (!interaction_handled) {
      CanvasInteractionCommand wire_edit_command;
      if (!TryBuildWireEditCommand(mouse_world_pos, &wire_edit_command)) {
        if (current_tool_ == ToolType::SELECT) {
          Wire* wire = FindWireAtPosition(mouse_world_pos);
          if (wire) {
            CanvasInteractionCommand command;
            command.wire_id = wire->id;
            if (io.KeyCtrl) {
              command.type = CanvasInteractionCommand::Type::ToggleWireSelection;
            } else if (wire->selected) {
              command.type = CanvasInteractionCommand::Type::SetPrimarySelectedWire;
            } else {
              command.type = CanvasInteractionCommand::Type::SelectWireExclusive;
            }
            queue_command(command);
          } else {
            box_select_pending_ = true;
            is_box_selecting_ = false;
            box_select_additive_ = io.KeyCtrl;
            box_select_start_world_ = mouse_world_pos;
            box_select_current_world_ = mouse_world_pos;
            box_select_press_screen_ = mouse_screen_pos;
            SnapshotBoxSelection();
          }
          interaction_handled = true;
        } else {
          Wire* wire = FindWireAtPosition(mouse_world_pos);
          CanvasInteractionCommand command;
          if (wire) {
            command.type = CanvasInteractionCommand::Type::SelectWireExclusive;
            command.wire_id = wire->id;
          } else {
            command.type = CanvasInteractionCommand::Type::ClearWireSelection;
          }
          queue_command(command);
          interaction_handled = true;
        }
      } else {
        queue_command(wire_edit_command);
        interaction_handled = true;
      }
    }

    flush_commands();
  }

  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
    std::vector<CanvasInteractionCommand> double_click_commands;
    auto queue_double_click_command = [&](const CanvasInteractionCommand& command) {
      double_click_commands.push_back(command);
    };
    auto flush_double_click_commands = [&]() {
      if (double_click_commands.empty()) {
        return;
      }
      ApplyCanvasInteractionCommands(double_click_commands);
      double_click_commands.clear();
    };
    bool handled = false;
    if (current_tool_ == ToolType::TAG) {
      Wire* tagged_wire = FindTaggedWireAtScreenPos(mouse_screen_pos);
      if (!tagged_wire) {
        Wire* wire = FindWireAtPosition(mouse_world_pos);
        if (wire && wire->isTagged) {
          tagged_wire = wire;
        }
      }
      if (tagged_wire) {
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::OpenWireTagPopup;
        command.wire_id = tagged_wire->id;
        queue_double_click_command(command);
        handled = true;
      }
    }

    if (!handled) {
      int top_index =
          FindTopmostComponentIndexAtPosition(placed_components_, mouse_world_pos);
      if (top_index != -1) {
        auto& comp = placed_components_[static_cast<size_t>(top_index)];
        const ComponentBehavior* behavior = GetComponentBehavior(comp.type);
        bool handled_by_behavior = false;
        if (behavior && behavior->OnDoubleClick) {
          ImVec2 adjusted_pos = GetBehaviorWorldPos(comp, mouse_world_pos);
          ComponentInteractionResult interaction =
              behavior->OnDoubleClick(comp, adjusted_pos, ImGuiMouseButton_Left);
          if (interaction.handled) {
            CanvasInteractionCommand interaction_command;
            interaction_command.type =
                CanvasInteractionCommand::Type::ApplyComponentInteraction;
            interaction_command.component_id = comp.instanceId;
            interaction_command.component_interaction = interaction;
            queue_double_click_command(interaction_command);
            handled_by_behavior = true;
          }
        }
        if (!handled_by_behavior) {
          CanvasInteractionCommand command;
          command.type = CanvasInteractionCommand::Type::OpenComponentContextMenu;
          command.component_id = comp.instanceId;
          command.screen_pos = mouse_screen_pos;
          queue_double_click_command(command);
        }
      }
    }

    flush_double_click_commands();
  }

  if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && is_canvas_hovered &&
      !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
      !box_select_pending_ && !is_box_selecting_ &&
      !(current_tool_ == ToolType::SELECT && io.KeyCtrl)) {
    int top_index =
        FindTopmostComponentIndexAtPosition(placed_components_, mouse_world_pos);
    if (top_index != -1) {
      auto& comp = placed_components_[static_cast<size_t>(top_index)];
      const ComponentBehavior* behavior = GetComponentBehavior(comp.type);
      if (behavior && behavior->OnMouseDown) {
        ImVec2 adjusted_pos = GetBehaviorWorldPos(comp, mouse_world_pos);
        ComponentInteractionResult interaction =
            behavior->OnMouseDown(comp, adjusted_pos, ImGuiMouseButton_Left);
        if (interaction.handled) {
          CanvasInteractionCommand command;
          command.type =
              CanvasInteractionCommand::Type::ApplyComponentInteraction;
          command.component_id = comp.instanceId;
          command.component_interaction = interaction;
          ApplyCanvasInteractionCommand(command);
        }
      }
    }
  } else if (!box_select_pending_ && !is_box_selecting_ &&
             !(current_tool_ == ToolType::SELECT && io.KeyCtrl)) {
    std::vector<CanvasInteractionCommand> mouse_up_commands;
    for (auto& comp : placed_components_) {
      const ComponentBehavior* behavior = GetComponentBehavior(comp.type);
      if (behavior && behavior->OnMouseUp) {
        ImVec2 adjusted_pos = GetBehaviorWorldPos(comp, mouse_world_pos);
        ComponentInteractionResult interaction =
            behavior->OnMouseUp(comp, adjusted_pos, ImGuiMouseButton_Left);
        if (!interaction.handled) {
          continue;
        }
        CanvasInteractionCommand command;
        command.type =
            CanvasInteractionCommand::Type::ApplyComponentInteraction;
        command.component_id = comp.instanceId;
        command.component_interaction = interaction;
        mouse_up_commands.push_back(command);
      }
    }
    ApplyCanvasInteractionCommands(mouse_up_commands);
  }

  if (is_canvas_hovered && !is_dragging_ &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
      HasSelectedComponents() && !is_moving_component_ &&
      !box_select_pending_ && !is_box_selecting_ && !io.KeyCtrl) {
    int top_index =
        FindTopmostComponentIndexAtPosition(placed_components_, mouse_world_pos);
    if (top_index != -1) {
      const auto& comp = placed_components_[static_cast<size_t>(top_index)];
      if (comp.selected) {
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::StartComponentMove;
        command.component_id = comp.instanceId;
        command.world_pos = mouse_world_pos;
        ApplyCanvasInteractionCommand(command);
      }
    }
  }
}

void Application::HandleWireConnectionMode(bool is_canvas_hovered,
                                           bool is_pan_input,
                                           ImVec2 mouse_world_pos,
                                           double now) {
  std::vector<CanvasInteractionCommand> commands;
  auto queue_command = [&](const CanvasInteractionCommand& command) {
    commands.push_back(command);
  };
  auto flush_commands = [&]() {
    if (commands.empty()) {
      return;
    }
    ApplyCanvasInteractionCommands(commands);
    commands.clear();
  };

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
    int clicked_component_id = -1;
    Port* clicked_port =
        FindPortAtPosition(mouse_world_pos, clicked_component_id);
    if (clicked_port && clicked_component_id != -1) {
      ImVec2 port_world_pos = mouse_world_pos;
      auto it = port_positions_.find({clicked_component_id, clicked_port->id});
      if (it != port_positions_.end()) {
        port_world_pos = ImVec2(it->second.x, it->second.y);
      }
      if (!is_connecting_) {
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::StartWireConnection;
        command.component_id = clicked_component_id;
        command.port_id = clicked_port->id;
        command.world_pos = port_world_pos;
        queue_command(command);
      } else {
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::CompleteWireConnection;
        command.component_id = clicked_component_id;
        command.port_id = clicked_port->id;
        command.world_pos = port_world_pos;
        queue_command(command);
      }
    } else if (is_connecting_) {
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::AddWireWaypoint;
      command.world_pos = mouse_world_pos;
      queue_command(command);
    } else {
      CanvasInteractionCommand wire_edit_command;
      if (TryBuildWireEditCommand(mouse_world_pos, &wire_edit_command)) {
        queue_command(wire_edit_command);
      } else {
        Wire* wire = FindWireAtPosition(mouse_world_pos);
        CanvasInteractionCommand command;
        if (wire) {
          command.type = CanvasInteractionCommand::Type::SelectWireExclusive;
          command.wire_id = wire->id;
        } else {
          command.type = CanvasInteractionCommand::Type::ClearWireSelection;
        }
        queue_command(command);
      }
    }
    flush_commands();
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
      if ((dx * dx + dy * dy) >= (min_waypoint_dist * min_waypoint_dist)) {
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::AddWireWaypoint;
        command.world_pos = candidate;
        ApplyCanvasInteractionCommand(command);
        last_auto_waypoint_time_ = now;
      }
    }
  }
}

void Application::ApplyCanvasComponentCommand(
    const CanvasInteractionCommand& command) {
  switch (command.type) {
    case CanvasInteractionCommand::Type::ApplyComponentInteraction: {
      PlacedComponent* comp = FindComponentById(command.component_id);
      if (comp) {
        ApplyComponentInteractionResult(comp, command.component_interaction);
      }
      break;
    }
    case CanvasInteractionCommand::Type::StartComponentMove:
      if (command.component_id >= 0) {
        BeginComponentMoveSession(
            BuildComponentMoveSession(command.component_id, command.world_pos));
      }
      break;
    case CanvasInteractionCommand::Type::UpdateComponentMove:
      UpdateMovingComponentPositions(command.world_pos);
      break;
    case CanvasInteractionCommand::Type::EndComponentMove:
      EndComponentMoveSession();
      break;
    case CanvasInteractionCommand::Type::DeleteComponentById:
      if (command.component_id >= 0) {
        CanvasInteractionCommand select_command;
        select_command.type =
            CanvasInteractionCommand::Type::SelectComponentExclusive;
        select_command.component_id = command.component_id;
        ApplyCanvasSelectionCommand(select_command);
        DeleteSelectedComponent();
      }
      break;
    default:
      break;
  }
}

void Application::BeginWirePointEditSession(int wire_id, int point_index) {
  if (wire_id < 0 || point_index < 0) {
    return;
  }

  PushWiringUndoState();
  ClearComponentSelection();
  ClearWireSelection();
  for (auto& wire : wires_) {
    if (wire.id != wire_id) {
      continue;
    }
    wire.selected = true;
    selected_wire_id_ = wire_id;
    wire_edit_mode_ = WireEditMode::MOVING_POINT;
    editing_wire_id_ = wire_id;
    editing_point_index_ = point_index;
    break;
  }
}

void Application::SetPrimarySelectedComponentById(int component_id) {
  if (component_id < 0) {
    return;
  }

  ClearWireSelection();
  ResetWireEditState();
  for (auto& comp : placed_components_) {
    if (comp.instanceId != component_id) {
      continue;
    }
    comp.selected = true;
    selected_component_id_ = component_id;
    break;
  }
  RepairPrimarySelection();
}

void Application::SelectWireExclusiveById(int wire_id) {
  ClearComponentSelection();
  ClearWireSelection();
  ResetWireEditState();
  if (wire_id < 0) {
    return;
  }

  for (auto& wire : wires_) {
    if (wire.id != wire_id) {
      continue;
    }
    wire.selected = true;
    selected_wire_id_ = wire_id;
    break;
  }
  RepairPrimaryWireSelection();
}

void Application::SetPrimarySelectedWireById(int wire_id) {
  if (wire_id < 0) {
    return;
  }

  ClearComponentSelection();
  ResetWireEditState();
  for (auto& wire : wires_) {
    if (wire.id != wire_id) {
      continue;
    }
    wire.selected = true;
    selected_wire_id_ = wire_id;
    break;
  }
  RepairPrimaryWireSelection();
}

void Application::ApplyCanvasWireCommand(
    const CanvasInteractionCommand& command) {
  switch (command.type) {
    case CanvasInteractionCommand::Type::CancelWireConnection:
      is_connecting_ = false;
      current_way_points_.clear();
      break;
    case CanvasInteractionCommand::Type::BeginWirePointEdit:
      BeginWirePointEditSession(command.wire_id, command.point_index);
      break;
    case CanvasInteractionCommand::Type::UpdateWirePointEdit:
      UpdateEditedWirePoint(command.world_pos);
      break;
    case CanvasInteractionCommand::Type::EndWirePointEdit:
      ResetWireEditState();
      break;
    case CanvasInteractionCommand::Type::StartWireConnection:
      if (command.component_id >= 0 && command.port_id >= 0) {
        StartWireConnection(command.component_id, command.port_id,
                            command.world_pos);
      }
      break;
    case CanvasInteractionCommand::Type::CompleteWireConnection:
      if (command.component_id >= 0 && command.port_id >= 0) {
        CompleteWireConnection(command.component_id, command.port_id,
                               command.world_pos);
      }
      break;
    case CanvasInteractionCommand::Type::AddWireWaypoint:
      HandleWayPointClick(command.world_pos, true);
      break;
    case CanvasInteractionCommand::Type::OpenWireTagPopup:
      if (command.wire_id >= 0) {
        OpenTagPopupForWire(command.wire_id);
      }
      break;
    case CanvasInteractionCommand::Type::DeleteWireById:
      if (command.wire_id >= 0) {
        DeleteWire(command.wire_id);
      }
      break;
    case CanvasInteractionCommand::Type::UntagWireById:
      if (command.wire_id >= 0) {
        for (auto& wire : wires_) {
          if (wire.id != command.wire_id) {
            continue;
          }
          PushWiringUndoState();
          wire.isTagged = false;
          wire.tagText.clear();
          wire.tagColorIndex = 0;
          break;
        }
      }
      break;
    default:
      break;
  }
}

void Application::ApplyCanvasSelectionCommand(
    const CanvasInteractionCommand& command) {
  switch (command.type) {
    case CanvasInteractionCommand::Type::SelectComponentExclusive:
      if (command.component_id >= 0) {
        HandleComponentSelection(command.component_id);
      }
      break;
    case CanvasInteractionCommand::Type::ToggleComponentSelection:
      if (command.component_id >= 0) {
        ToggleComponentSelection(command.component_id);
      }
      break;
    case CanvasInteractionCommand::Type::SetPrimarySelectedComponent:
      SetPrimarySelectedComponentById(command.component_id);
      break;
    case CanvasInteractionCommand::Type::SelectWireExclusive:
      SelectWireExclusiveById(command.wire_id);
      break;
    case CanvasInteractionCommand::Type::ToggleWireSelection:
      if (command.wire_id >= 0) {
        ToggleWireSelection(command.wire_id);
      }
      break;
    case CanvasInteractionCommand::Type::SetPrimarySelectedWire:
      SetPrimarySelectedWireById(command.wire_id);
      break;
    case CanvasInteractionCommand::Type::ClearWireSelection:
      ClearWireSelection();
      ResetWireEditState();
      break;
    default:
      break;
  }
}

void Application::ApplyCanvasContextCommand(
    const CanvasInteractionCommand& command) {
  switch (command.type) {
    case CanvasInteractionCommand::Type::OpenComponentContextMenu:
      if (command.component_id >= 0) {
        CanvasInteractionCommand select_command;
        select_command.type =
            CanvasInteractionCommand::Type::SelectComponentExclusive;
        select_command.component_id = command.component_id;
        ApplyCanvasSelectionCommand(select_command);
        OpenComponentContextMenu(command.component_id, command.screen_pos);
      }
      break;
    default:
      break;
  }
}

void Application::ApplyCanvasInteractionCommand(
    const CanvasInteractionCommand& command) {
  switch (command.type) {
    case CanvasInteractionCommand::Type::ApplyComponentInteraction:
    case CanvasInteractionCommand::Type::StartComponentMove:
    case CanvasInteractionCommand::Type::UpdateComponentMove:
    case CanvasInteractionCommand::Type::EndComponentMove:
    case CanvasInteractionCommand::Type::DeleteComponentById:
      ApplyCanvasComponentCommand(command);
      break;
    case CanvasInteractionCommand::Type::CancelWireConnection:
    case CanvasInteractionCommand::Type::BeginWirePointEdit:
    case CanvasInteractionCommand::Type::UpdateWirePointEdit:
    case CanvasInteractionCommand::Type::EndWirePointEdit:
    case CanvasInteractionCommand::Type::StartWireConnection:
    case CanvasInteractionCommand::Type::CompleteWireConnection:
    case CanvasInteractionCommand::Type::AddWireWaypoint:
    case CanvasInteractionCommand::Type::OpenWireTagPopup:
    case CanvasInteractionCommand::Type::DeleteWireById:
    case CanvasInteractionCommand::Type::UntagWireById:
      ApplyCanvasWireCommand(command);
      break;
    case CanvasInteractionCommand::Type::SelectComponentExclusive:
    case CanvasInteractionCommand::Type::ToggleComponentSelection:
    case CanvasInteractionCommand::Type::SetPrimarySelectedComponent:
    case CanvasInteractionCommand::Type::SelectWireExclusive:
    case CanvasInteractionCommand::Type::ToggleWireSelection:
    case CanvasInteractionCommand::Type::SetPrimarySelectedWire:
    case CanvasInteractionCommand::Type::ClearWireSelection:
      ApplyCanvasSelectionCommand(command);
      break;
    case CanvasInteractionCommand::Type::OpenComponentContextMenu:
      ApplyCanvasContextCommand(command);
      break;
    case CanvasInteractionCommand::Type::None:
    default:
      break;
  }
}

void Application::ApplyCanvasInteractionCommands(
    const std::vector<CanvasInteractionCommand>& commands) {
  for (const auto& command : commands) {
    ApplyCanvasInteractionCommand(command);
  }
}

bool Application::HandleFRLPressureAdjustment(ImVec2 mouse_world_pos,
                                              ImVec2 mouse_screen_pos,
                                              const ImGuiIO& io,
                                              float resolved_wheel,
                                              bool wheel_from_touchpad) {
  float effective_wheel = resolved_wheel;
  bool from_touchpad = wheel_from_touchpad;
  if (effective_wheel == 0.0f) {
    effective_wheel = io.MouseWheel;
  }
  if (effective_wheel == 0.0f && touchpad_wheel_pending_) {
    effective_wheel = touchpad_wheel_delta_;
    from_touchpad = true;
  }

  if (effective_wheel == 0.0f) {
    return false;
  }

  if (IsDebugEnabled()) {
    DebugLog("[INPUT] ComponentWheel begin wheel=" +
             std::to_string(effective_wheel) + " touchpad=" +
             std::to_string(from_touchpad ? 1 : 0) + " ctrl=" +
             std::to_string(io.KeyCtrl ? 1 : 0) + " " +
             DescribeInputTargetAtScreenPos(mouse_screen_pos));
  }

  // Build a synthetic IO with the resolved wheel value and physical modifiers.
  ImGuiIO synth_io = io;
  synth_io.MouseWheel = effective_wheel;
  synth_io.KeyCtrl = io.KeyCtrl || (resolved_wheel != 0.0f && deferred_canvas_wheel_input_.physical_ctrl_down);
  synth_io.KeyShift = io.KeyShift || (resolved_wheel != 0.0f && deferred_canvas_wheel_input_.physical_shift_down);
  
  auto try_apply_wheel_interaction =
      [&](PlacedComponent* comp,
          const ComponentBehavior* behavior) -> bool {
    if (!comp || !behavior || !behavior->OnMouseWheel) {
      if (IsDebugEnabled() && comp) {
        DebugLog("[INPUT] ComponentWheel skip id=" +
                 std::to_string(comp->instanceId) + " type=" +
                 DebugComponentTypeLabel(comp->type) + " reason=no_handler");
      }
      return false;
    }
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] ComponentWheel try id=" +
               std::to_string(comp->instanceId) + " type=" +
               DebugComponentTypeLabel(comp->type) + " selected=" +
               std::to_string(comp->selected ? 1 : 0));
    }
    ComponentInteractionResult interaction =
        behavior->OnMouseWheel(*comp, GetBehaviorWorldPos(*comp, mouse_world_pos),
                               synth_io);
    if (!interaction.handled) {
      if (IsDebugEnabled()) {
        DebugLog("[INPUT] ComponentWheel miss id=" +
                 std::to_string(comp->instanceId) + " type=" +
                 DebugComponentTypeLabel(comp->type) +
                 " reason=behavior_unhandled");
      }
      return false;
    }
    CanvasInteractionCommand command;
    command.type = CanvasInteractionCommand::Type::ApplyComponentInteraction;
    command.component_id = comp->instanceId;
    command.component_interaction = interaction;
    ApplyCanvasInteractionCommand(command);
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] ComponentWheel handled id=" +
               std::to_string(comp->instanceId) + " type=" +
               DebugComponentTypeLabel(comp->type) + " commands=" +
               std::to_string(interaction.commands.size()));
    }
    return true;
  };

  // 1. Prioritize selected components
  for (auto it = placed_components_.rbegin(); it != placed_components_.rend();
       ++it) {
    auto& comp = *it;
    if (!comp.selected) {
      continue;
    }
    if (IsPointInsideComponent(comp, mouse_world_pos)) {
      const ComponentBehavior* behavior = GetComponentBehavior(comp.type);
      if (try_apply_wheel_interaction(&comp, behavior)) {
        if (from_touchpad) {
          // Consume the touchpad wheel/pan so the camera does NOT also move.
          touchpad_wheel_pending_ = false;
          touchpad_wheel_delta_ = 0.0f;
          touchpad_pan_pending_ = false;
          touchpad_pan_delta_ = ImVec2(0.0f, 0.0f);
        }
#ifdef _WIN32
        TriggerTouchpadHapticClick();
#endif
        return true;
      }
    }
  }

  // 2. Check all components (not just selected ones) if no selected component handled it
  for (auto it = placed_components_.rbegin(); it != placed_components_.rend();
       ++it) {
    auto& comp = *it;
    if (comp.selected) {
      continue; // Already checked above
    }
    if (IsPointInsideComponent(comp, mouse_world_pos)) {
      const ComponentBehavior* behavior = GetComponentBehavior(comp.type);
      if (try_apply_wheel_interaction(&comp, behavior)) {
        if (from_touchpad) {
          touchpad_wheel_pending_ = false;
          touchpad_wheel_delta_ = 0.0f;
          touchpad_pan_pending_ = false;
          touchpad_pan_delta_ = ImVec2(0.0f, 0.0f);
        }
#ifdef _WIN32
        TriggerTouchpadHapticClick();
#endif
        return true;
      }
    }
  }

  if (IsDebugEnabled()) {
    DebugLog("[INPUT] ComponentWheel no_match " +
             DescribeInputTargetAtScreenPos(mouse_screen_pos));
  }
  return false;
}

void Application::ProcessDeferredCanvasWheelInput() {
  ImGuiIO& io = ImGui::GetIO();
  if (canvas_size_.x <= 0.0f || canvas_size_.y <= 0.0f) {
    deferred_canvas_wheel_input_ = Application::DeferredCanvasWheelInput{};
    return;
  }

  Application::DeferredCanvasWheelInput wheel_input =
      deferred_canvas_wheel_input_;
#ifndef _WIN32
  if (!wheel_input.pending &&
      (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)) {
    wheel_input.pending = true;
    wheel_input.vertical_delta = io.MouseWheel;
    wheel_input.horizontal_delta = io.MouseWheelH;
    wheel_input.screen_pos = io.MousePos;
    wheel_input.physical_ctrl_down = io.KeyCtrl;
    wheel_input.synthetic_pinch = false;
    wheel_input.touchpad_like = false;
  }
#endif

  const bool popup_blocking =
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
  ImVec2 mouse_screen_pos =
      wheel_input.pending
          ? wheel_input.screen_pos
          : (last_pointer_is_pan_input_ ? touch_anchor_screen_pos_
                                        : io.MousePos);
  ImVec2 mouse_world_pos = ScreenToWorld(mouse_screen_pos);
  const bool mouse_within_canvas_bounds =
      (mouse_screen_pos.x >= canvas_top_left_.x &&
       mouse_screen_pos.x <= canvas_top_left_.x + canvas_size_.x &&
       mouse_screen_pos.y >= canvas_top_left_.y &&
       mouse_screen_pos.y <= canvas_top_left_.y + canvas_size_.y);
  const bool overlay_rect_blocking =
      IsPointInOverlayCaptureRect(mouse_screen_pos);
  const bool overlay_capture_blocking = false;
  const bool component_navigation_blocking =
      wheel_input.touchpad_like &&
      IsPointOverComponentAtScreenPos(mouse_screen_pos);
  const bool overlay_point_blocking =
      overlay_rect_blocking || overlay_capture_blocking;
  const bool navigation_blocking =
      overlay_point_blocking || component_navigation_blocking;

  const bool is_canvas_hovered =
      !popup_blocking && !navigation_blocking &&
      mouse_within_canvas_bounds;
  const bool middle_drag_active =
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
  const bool alt_right_drag_active =
      io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Right);
  const bool allow_wheel_pan_zoom =
      !popup_blocking && !navigation_blocking &&
      (is_canvas_hovered || touch_gesture_active_ ||
       touchpad_zoom_pending_ || touchpad_pan_pending_ ||
       wheel_input.pending);
  const bool allow_mouse_pan =
      !popup_blocking && !overlay_point_blocking &&
      (is_canvas_hovered || middle_drag_active || alt_right_drag_active);

  if (wheel_input.pending && IsDebugEnabled()) {
    DebugLog("[INPUT] DeferredCanvasWheel pending v=" +
             std::to_string(wheel_input.vertical_delta) + " h=" +
             std::to_string(wheel_input.horizontal_delta) + " pos=(" +
             std::to_string(mouse_screen_pos.x) + "," +
             std::to_string(mouse_screen_pos.y) + ") overlay=" +
             std::to_string(overlay_point_blocking ? 1 : 0) +
             " overlay_rect=" +
             std::to_string(overlay_rect_blocking ? 1 : 0) +
             " overlay_capture_src=disabled value=" +
             std::to_string(overlay_capture_blocking ? 1 : 0) +
             " component_nav_block=" +
             std::to_string(component_navigation_blocking ? 1 : 0) +
             " want_capture=" +
             std::to_string(io.WantCaptureMouse ? 1 : 0) +
             " canvas_hovered=" +
             std::to_string(is_canvas_hovered ? 1 : 0) +
             " allow_wheel_pan_zoom=" +
             std::to_string(allow_wheel_pan_zoom ? 1 : 0) +
             " allow_mouse_pan=" +
             std::to_string(allow_mouse_pan ? 1 : 0) + " " +
             DescribeInputTargetAtScreenPos(mouse_screen_pos));
  }

  // Win32 raw queue에 입력이 없지만 ImGui IO에 휠 값이 있다면,
  // overlay가 메시지를 소비했거나 wheel-responsive component 경로로
  // ImGui 쪽으로 넘겨진 케이스다.
  if (!wheel_input.pending && (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f) &&
      !overlay_point_blocking) {
    wheel_input.pending = true;
    wheel_input.vertical_delta = io.MouseWheel;
    wheel_input.horizontal_delta = io.MouseWheelH;
    wheel_input.screen_pos = io.MousePos;
    wheel_input.physical_ctrl_down = io.KeyCtrl;
    wheel_input.synthetic_pinch = false;
    wheel_input.touchpad_like = false;
  }

  canvas_directly_hovered_ = is_canvas_hovered;

  bool frl_interaction_handled = false;
  bool wheel_handled = false;

  const bool ui_blocking = popup_blocking || overlay_point_blocking;

  if (!ui_blocking) {
    const bool allow_component_wheel = !wheel_input.synthetic_pinch;
    frl_interaction_handled = HandleFRLPressureAdjustment(
        mouse_world_pos, mouse_screen_pos, io,
        allow_component_wheel ? wheel_input.vertical_delta : 0.0f,
        allow_component_wheel && wheel_input.touchpad_like);
  }

  if (!ui_blocking && !navigation_blocking) {
    HandleZoomAndPan(allow_wheel_pan_zoom, allow_mouse_pan, io, wheel_input,
                     frl_interaction_handled, wheel_handled);
  } else {
    if ((wheel_input.pending || touchpad_zoom_pending_ || touchpad_pan_pending_ ||
         touch_gesture_active_) &&
        IsDebugEnabled()) {
      DebugLog("[INPUT] DeferredCanvasWheel blocked popup=" +
               std::to_string(popup_blocking ? 1 : 0) + " overlay=" +
               std::to_string(overlay_point_blocking ? 1 : 0) +
               " component_nav=" +
               std::to_string(component_navigation_blocking ? 1 : 0) + " " +
               DescribeInputTargetAtScreenPos(mouse_screen_pos));
    }
    touchpad_zoom_pending_ = false;
    touchpad_zoom_delta_ = 0.0f;
    touchpad_pan_pending_ = false;
    touchpad_pan_delta_ = ImVec2(0.0f, 0.0f);
    touchpad_wheel_pending_ = false;
    touchpad_wheel_delta_ = 0.0f;
    touch_gesture_active_ = false;
    touch_zoom_delta_ = 0.0f;
    touch_pan_delta_ = ImVec2(0.0f, 0.0f);
    wheel_input.pending = false;
  }
  deferred_canvas_wheel_input_ = Application::DeferredCanvasWheelInput{};
}

void Application::HandleZoomAndPan(bool allow_wheel_pan_zoom,
                                   bool allow_mouse_pan, const ImGuiIO& io,
                                   const Application::DeferredCanvasWheelInput&
                                       wheel_input,
                                   bool frl_handled, bool& wheel_handled) {
  auto apply_zoom_at_anchor = [&](float zoom_factor, ImVec2 screen_anchor)
      -> bool {
    if (zoom_factor <= 0.01f) {
      return false;
    }

    ImVec2 world_before_zoom = ScreenToWorld(screen_anchor);
    camera_zoom_ *= zoom_factor;
    camera_zoom_ = std::max(0.05f, std::min(camera_zoom_, 10.0f));
    ImVec2 world_after_zoom = ScreenToWorld(screen_anchor);
    camera_offset_.x += (world_after_zoom.x - world_before_zoom.x);
    camera_offset_.y += (world_after_zoom.y - world_before_zoom.y);
    return true;
  };

  if (touchpad_zoom_pending_ && allow_wheel_pan_zoom) {
    float zoom_factor = 1.0f + touchpad_zoom_delta_;
    if (apply_zoom_at_anchor(zoom_factor, touchpad_zoom_anchor_screen_pos_)) {
      wheel_handled = true;
    }
    touchpad_zoom_pending_ = false;
    touchpad_zoom_delta_ = 0.0f;
  } else if (touchpad_zoom_pending_) {
    touchpad_zoom_pending_ = false;
    touchpad_zoom_delta_ = 0.0f;
  }

  if (touchpad_pan_pending_ && allow_wheel_pan_zoom) {
    camera_offset_.x += touchpad_pan_delta_.x / camera_zoom_;
    camera_offset_.y += touchpad_pan_delta_.y / camera_zoom_;
    touchpad_pan_pending_ = false;
    touchpad_pan_delta_ = ImVec2(0.0f, 0.0f);
    wheel_handled = true;
  } else if (touchpad_pan_pending_) {
    touchpad_pan_pending_ = false;
    touchpad_pan_delta_ = ImVec2(0.0f, 0.0f);
  }
  if (touchpad_wheel_pending_ && !frl_handled) {
    touchpad_wheel_pending_ = false;
    touchpad_wheel_delta_ = 0.0f;
  }

  if (touch_gesture_active_ && allow_wheel_pan_zoom) {
    float zoom_factor = 1.0f + touch_zoom_delta_;
    apply_zoom_at_anchor(zoom_factor, touch_anchor_screen_pos_);

    camera_offset_.x += touch_pan_delta_.x / camera_zoom_;
    camera_offset_.y += touch_pan_delta_.y / camera_zoom_;
    touch_gesture_active_ = false;
    touch_zoom_delta_ = 0.0f;
    touch_pan_delta_ = ImVec2(0.0f, 0.0f);
    wheel_handled = true;
  }

  if (allow_wheel_pan_zoom && wheel_input.pending && !frl_handled &&
      !wheel_handled) {
    if ((wheel_input.synthetic_pinch || wheel_input.physical_ctrl_down) &&
        wheel_input.vertical_delta != 0.0f) {
      float zoom_delta = wheel_input.vertical_delta * 0.08f;
      if (zoom_delta > 0.12f) {
        zoom_delta = 0.12f;
      } else if (zoom_delta < -0.12f) {
        zoom_delta = -0.12f;
      }
      float zoom_factor = 1.0f + zoom_delta;
      if (apply_zoom_at_anchor(zoom_factor, wheel_input.screen_pos)) {
        wheel_handled = true;
        if (IsDebugEnabled()) {
          DebugLog("[INPUT] DeferredCanvasWheel route=zoom " +
                   DescribeInputTargetAtScreenPos(wheel_input.screen_pos));
        }
      }
    } else if (wheel_input.touchpad_like && !wheel_input.physical_ctrl_down &&
               !wheel_input.physical_shift_down) {
      const float pan_step = 36.0f;
      camera_offset_.x +=
          wheel_input.horizontal_delta * pan_step / camera_zoom_;
      camera_offset_.y +=
          (-wheel_input.vertical_delta) * pan_step / camera_zoom_;
      wheel_handled = (wheel_input.horizontal_delta != 0.0f ||
                       wheel_input.vertical_delta != 0.0f);
      if (wheel_handled && IsDebugEnabled()) {
        DebugLog("[INPUT] DeferredCanvasWheel route=touchpad_pan " +
                 DescribeInputTargetAtScreenPos(wheel_input.screen_pos));
      }
    } else if (wheel_input.physical_shift_down ||
               wheel_input.horizontal_delta != 0.0f) {
      // 마우스 휠로 좌우 이동 (Shift + Vertical Wheel 또는 Horizontal Wheel)
      const float pan_step = wheel_input.touchpad_like ? 36.0f : 50.0f;
      float h_delta = wheel_input.horizontal_delta;
      if (h_delta == 0.0f && wheel_input.physical_shift_down) {
        h_delta = wheel_input.vertical_delta;
      }
      camera_offset_.x += h_delta * pan_step / camera_zoom_;
      wheel_handled = (h_delta != 0.0f);
      if (wheel_handled && IsDebugEnabled()) {
        DebugLog("[INPUT] DeferredCanvasWheel route=horizontal_pan " +
                 DescribeInputTargetAtScreenPos(wheel_input.screen_pos));
      }
    } else if (wheel_input.vertical_delta != 0.0f) {
      // Default: Vertical Zoom (consistent with previous behavior but now Ctrl+Wheel also zooms)
      const float zoom_speed = 0.08f;
      float zoom_step =
          1.0f + zoom_speed * std::fabs(wheel_input.vertical_delta);
      if (wheel_input.vertical_delta > 0.0f) {
        apply_zoom_at_anchor(zoom_step, wheel_input.screen_pos);
      } else {
        apply_zoom_at_anchor(1.0f / zoom_step, wheel_input.screen_pos);
      }
      wheel_handled = true;
      if (IsDebugEnabled()) {
        DebugLog("[INPUT] DeferredCanvasWheel route=mouse_zoom " +
                 DescribeInputTargetAtScreenPos(wheel_input.screen_pos));
      }
    }
  }
  
  // Middle-button drag remains the primary mouse pan gesture.
  float pan_sensitivity = 1.0f;
  if (allow_mouse_pan &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    camera_offset_.x += io.MouseDelta.x / camera_zoom_ * pan_sensitivity;
    camera_offset_.y += io.MouseDelta.y / camera_zoom_ * pan_sensitivity;
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] MousePan route=middle_drag " +
               DescribeInputTargetAtScreenPos(io.MousePos));
    }
  }
  
  // Alt+Right drag mirrors the old CAD-style pan gesture.
  if (allow_mouse_pan && io.KeyAlt &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
    camera_offset_.x += io.MouseDelta.x / camera_zoom_ * pan_sensitivity;
    camera_offset_.y += io.MouseDelta.y / camera_zoom_ * pan_sensitivity;
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] MousePan route=alt_right_drag " +
               DescribeInputTargetAtScreenPos(io.MousePos));
    }
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
  if (IsDebugEnabled()) {
    DebugLog("[INPUT] UpdateTouchGesture active=" +
             std::to_string(active ? 1 : 0) + " zoom=" +
             std::to_string(zoom_delta) + " pan=(" +
             std::to_string(pan_delta.x) + "," +
             std::to_string(pan_delta.y) + ")");
  }
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

void Application::QueueTouchpadZoom(float zoom_delta, ImVec2 screen_pos) {
  const bool blocked = ShouldBlockCanvasNavigationAtScreenPos(screen_pos, true);
  if (blocked) {
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] QueueTouchpadZoom blocked delta=" +
               std::to_string(zoom_delta) + " " +
               DescribeInputTargetAtScreenPos(screen_pos));
    }
    return;
  }
  touchpad_zoom_pending_ = true;
  touchpad_zoom_delta_ = zoom_delta;
  touchpad_zoom_anchor_screen_pos_ = screen_pos;
  if (IsDebugEnabled()) {
    DebugLog("[INPUT] QueueTouchpadZoom delta=" + std::to_string(zoom_delta) +
             " anchor=(" + std::to_string(screen_pos.x) + "," +
             std::to_string(screen_pos.y) + ") " +
             DescribeInputTargetAtScreenPos(screen_pos));
  }
}

void Application::QueueTouchpadPan(ImVec2 pan_delta) {
  const bool blocked =
      ShouldBlockCanvasNavigationAtScreenPos(touch_anchor_screen_pos_, true);
  if (blocked) {
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] QueueTouchpadPan blocked delta=(" +
               std::to_string(pan_delta.x) + "," +
               std::to_string(pan_delta.y) + ") " +
               DescribeInputTargetAtScreenPos(touch_anchor_screen_pos_));
    }
    return;
  }
  touchpad_pan_pending_ = true;
  touchpad_pan_delta_.x += pan_delta.x;
  touchpad_pan_delta_.y += pan_delta.y;
  if (IsDebugEnabled()) {
    DebugLog("[INPUT] QueueTouchpadPan delta=(" +
             std::to_string(pan_delta.x) + "," +
             std::to_string(pan_delta.y) + ") " +
             DescribeInputTargetAtScreenPos(touch_anchor_screen_pos_));
  }
}

void Application::QueueTouchpadWheel(float wheel_delta) {
  touchpad_wheel_pending_ = true;
  touchpad_wheel_delta_ += wheel_delta;
  if (IsDebugEnabled()) {
    DebugLog("[INPUT] QueueTouchpadWheel delta=" +
             std::to_string(wheel_delta) + " total=" +
             std::to_string(touchpad_wheel_delta_));
  }
}

void Application::QueueCanvasWheelInput(float vertical_delta,
                                        float horizontal_delta,
                                        ImVec2 screen_pos,
                                        bool physical_ctrl_down,
                                        bool physical_shift_down,
                                        bool synthetic_pinch,
                                        bool touchpad_like) {
  deferred_canvas_wheel_input_.pending = true;
  deferred_canvas_wheel_input_.vertical_delta += vertical_delta;
  deferred_canvas_wheel_input_.horizontal_delta += horizontal_delta;
  deferred_canvas_wheel_input_.screen_pos = screen_pos;
  deferred_canvas_wheel_input_.physical_ctrl_down =
      deferred_canvas_wheel_input_.physical_ctrl_down || physical_ctrl_down;
  deferred_canvas_wheel_input_.physical_shift_down =
      deferred_canvas_wheel_input_.physical_shift_down || physical_shift_down;
  deferred_canvas_wheel_input_.synthetic_pinch =
      deferred_canvas_wheel_input_.synthetic_pinch || synthetic_pinch;
  deferred_canvas_wheel_input_.touchpad_like =
      deferred_canvas_wheel_input_.touchpad_like || touchpad_like;
  if (IsDebugEnabled()) {
    DebugLog("[INPUT] QueueCanvasWheelInput v=" +
             std::to_string(vertical_delta) + " h=" +
             std::to_string(horizontal_delta) + " pos=(" +
             std::to_string(screen_pos.x) + "," +
             std::to_string(screen_pos.y) + ") ctrl=" +
             std::to_string(physical_ctrl_down ? 1 : 0) +
             " synthetic_pinch=" +
             std::to_string(synthetic_pinch ? 1 : 0) +
             " touchpad_like=" +
             std::to_string(touchpad_like ? 1 : 0) + " " +
             DescribeInputTargetAtScreenPos(screen_pos));
  }
}

void Application::HandleCanvasContextActions(bool is_canvas_hovered,
                                             ImVec2 mouse_world_pos,
                                             const ImGuiIO& io) {
  std::vector<CanvasInteractionCommand> commands;
  auto queue_command = [&](CanvasInteractionCommand command) {
    commands.push_back(command);
  };
  auto flush_commands = [&]() {
    if (commands.empty()) {
      return;
    }
    ApplyCanvasInteractionCommands(commands);
    commands.clear();
  };
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

  bool right_click = platform_input_collector_.ConsumeWin32RightClick() ||
                     ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  bool right_double_click =
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right);
  bool right_down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) ==
                    GLFW_PRESS;
  if (!right_click && right_down && !prev_right_button_down_) {
    right_click = true;
  }
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
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::UntagWireById;
      command.wire_id = tagged_wire->id;
      queue_command(command);
      flush_commands();
    }
    return;
  }

  if (right_click && is_canvas_hovered && io.KeyCtrl) {
    int top_index =
        FindTopmostComponentIndexAtPosition(placed_components_,
                                            mouse_world_pos);
    if (top_index != -1) {
      const auto& comp = placed_components_[static_cast<size_t>(top_index)];
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::OpenComponentContextMenu;
      command.component_id = comp.instanceId;
      command.screen_pos = io.MousePos;
      queue_command(command);
      flush_commands();
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
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::CancelWireConnection;
      queue_command(command);
      flush_commands();
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
          CanvasInteractionCommand command;
          command.type = CanvasInteractionCommand::Type::DeleteComponentById;
          command.component_id = comp.instanceId;
          queue_command(command);
          flush_commands();
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
          CanvasInteractionCommand command;
          command.type = CanvasInteractionCommand::Type::DeleteWireById;
          command.wire_id = wire_id;
          queue_command(command);
          flush_commands();
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
  bool side_click = platform_input_collector_.ConsumeWin32SideClick() ||
                    ImGui::IsMouseClicked(3);
  bool side_down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_4) ==
                   GLFW_PRESS;
  bool side_hold = platform_input_collector_.IsWin32SideDown() ||
                   ImGui::IsMouseDown(3) || side_down;
  if (!side_click && side_down && !prev_side_button_down_) {
    side_click = true;
  }
  platform_input_collector_.SetWin32SideDown(side_hold);
  prev_side_button_down_ = side_down;

  if (side_click && !is_canvas_hovered) {
    if (IsDebugEnabled()) {
      DebugLog("[INPUT] Side click: canvas not hovered");
    }
  }
  if ((side_click || side_hold) && is_canvas_hovered) {
    if (side_click && is_connecting_) {
      CanvasInteractionCommand command;
      command.type = CanvasInteractionCommand::Type::CancelWireConnection;
      queue_command(command);
      flush_commands();
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
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::DeleteComponentById;
        command.component_id = comp.instanceId;
        queue_command(command);
        flush_commands();
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
        CanvasInteractionCommand command;
        command.type = CanvasInteractionCommand::Type::DeleteWireById;
        command.wire_id = wire_id;
        queue_command(command);
        flush_commands();
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

void Application::HandleComponentDropAndWireDelete(bool is_canvas_hovered,
                                                   ImVec2 mouse_world_pos,
                                                   const ImGuiIO& io) {
  if (is_dragging_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (is_canvas_hovered) {
      HandleComponentDrop(mouse_world_pos);
    } else {
      is_dragging_ = false;
      dragged_component_index_ = -1;
      dragged_rtl_module_id_.clear();
    }
  }

  HandleCanvasContextActions(is_canvas_hovered, mouse_world_pos, io);
}

bool Application::IsPointInCanvas(ImVec2 screen_pos) const {
  if (current_mode_ != Mode::WIRING) {
    return false;
  }
  if (canvas_size_.x <= 0.0f || canvas_size_.y <= 0.0f) {
    return false;
  }
  return screen_pos.x >= canvas_top_left_.x &&
         screen_pos.x <= canvas_top_left_.x + canvas_size_.x &&
         screen_pos.y >= canvas_top_left_.y &&
         screen_pos.y <= canvas_top_left_.y + canvas_size_.y;
}

bool Application::IsPointOverComponentAtScreenPos(ImVec2 screen_pos) const {
  if (!IsPointInCanvas(screen_pos)) {
    return false;
  }
  ImVec2 world_pos = ScreenToWorld(screen_pos);
  for (auto it = placed_components_.rbegin(); it != placed_components_.rend();
       ++it) {
    if (IsPointInsideComponent(*it, world_pos)) {
      return true;
    }
  }
  return false;
}

bool Application::HasWheelResponsiveComponentAtScreenPos(
    ImVec2 screen_pos) const {
  if (!IsPointInCanvas(screen_pos)) {
    return false;
  }
  ImVec2 world_pos = ScreenToWorld(screen_pos);
  for (auto it = placed_components_.rbegin(); it != placed_components_.rend();
       ++it) {
    if (!IsPointInsideComponent(*it, world_pos)) {
      continue;
    }
    const ComponentBehavior* behavior = GetComponentBehavior(it->type);
    if (behavior && behavior->OnMouseWheel != nullptr) {
      return true;
    }
  }
  return false;
}

bool Application::ShouldBlockCanvasNavigationAtScreenPos(
    ImVec2 screen_pos, bool block_on_component) const {
  if (IsPointInOverlayCaptureRect(screen_pos)) {
    return true;
  }
  if (block_on_component && IsPointOverComponentAtScreenPos(screen_pos)) {
    return true;
  }
  return false;
}

std::string Application::DescribeInputTargetAtScreenPos(ImVec2 screen_pos) const {
  std::string desc = "screen=(" +
                     std::to_string(static_cast<int>(screen_pos.x)) + "," +
                     std::to_string(static_cast<int>(screen_pos.y)) + ")";
  desc += " canvas=" + std::to_string(IsPointInCanvas(screen_pos) ? 1 : 0);
  desc += " overlay_rect=" +
          std::to_string(IsPointInOverlayCaptureRect(screen_pos) ? 1 : 0);
  desc += " overlay_capture_src=disabled";

  if (!IsPointInCanvas(screen_pos)) {
    return desc + " target=outside_canvas";
  }

  ImVec2 world_pos = ScreenToWorld(screen_pos);
  desc += " world=(" + std::to_string(static_cast<int>(world_pos.x)) + "," +
          std::to_string(static_cast<int>(world_pos.y)) + ")";

  int top_index = FindTopmostComponentIndexAtPosition(placed_components_, world_pos);
  if (top_index == -1) {
    return desc + " target=canvas_empty";
  }

  const auto& comp = placed_components_[static_cast<size_t>(top_index)];
  const ComponentBehavior* behavior = GetComponentBehavior(comp.type);
  desc += " target=component";
  desc += " id=" + std::to_string(comp.instanceId);
  desc += " type=" + std::string(DebugComponentTypeLabel(comp.type));
  desc += " selected=" + std::to_string(comp.selected ? 1 : 0);
  desc += " wheel_handler=" +
          std::to_string((behavior && behavior->OnMouseWheel != nullptr) ? 1
                                                                         : 0);
  return desc;
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
  std::vector<int> selected_ids;
  for (const auto& comp : placed_components_) {
    if (comp.selected) {
      selected_ids.push_back(comp.instanceId);
    }
  }
  if (selected_ids.empty() && selected_component_id_ != -1) {
    selected_ids.push_back(selected_component_id_);
  }
  if (selected_ids.empty()) {
    return;
  }
  PushWiringUndoState();
  for (int instance_id : selected_ids) {
    PlacedComponent* comp = FindComponentById(instance_id);
    ApplyRotationToComponent(comp, delta_quadrants);
  }
  RepairPrimarySelection();
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
      PlacedComponent* comp = FindComponentById(component_id);
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
        if (comp) {
          PushWiringUndoState();
          ApplyRotationToComponent(comp, 1);
        }
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.rotate_ccw", "Rotate -90deg"))) {
        if (comp) {
          PushWiringUndoState();
          ApplyRotationToComponent(comp, -1);
        }
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.flip_horizontal", "Flip Horizontal"))) {
        if (comp) {
          PushWiringUndoState();
          comp->flip_x = !comp->flip_x;
        }
      }
      if (ImGui::MenuItem(
              TR("ui.wiring.context.flip_vertical", "Flip Vertical"))) {
        if (comp) {
          PushWiringUndoState();
          comp->flip_y = !comp->flip_y;
        }
      }
      if (comp && comp->type == ComponentType::PLC) {
        ImGui::Separator();
        if (ImGui::BeginMenu(
                TR("ui.wiring.context.plc_input_mode", "PLC Input Mode"))) {
          bool sink_selected = plc_input_mode_ == PlcInputMode::SINK;
          bool source_selected = plc_input_mode_ == PlcInputMode::SOURCE;
          if (ImGui::MenuItem(TR("ui.common.sink", "Sink"), nullptr,
                              sink_selected) &&
              !sink_selected) {
            PushWiringUndoState();
            SetPlcInputMode(PlcInputMode::SINK, true);
          }
          if (ImGui::MenuItem(TR("ui.common.source", "Source"), nullptr,
                              source_selected) &&
              !source_selected) {
            PushWiringUndoState();
            SetPlcInputMode(PlcInputMode::SOURCE, true);
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(
                TR("ui.wiring.context.plc_output_mode", "PLC Output Mode"))) {
          bool sink_selected = plc_output_mode_ == PlcOutputMode::SINK;
          bool source_selected = plc_output_mode_ == PlcOutputMode::SOURCE;
          if (ImGui::MenuItem(TR("ui.common.sink", "Sink"), nullptr,
                              sink_selected) &&
              !sink_selected) {
            PushWiringUndoState();
            SetPlcOutputMode(PlcOutputMode::SINK);
          }
          if (ImGui::MenuItem(TR("ui.common.source", "Source"), nullptr,
                              source_selected) &&
              !source_selected) {
            PushWiringUndoState();
            SetPlcOutputMode(PlcOutputMode::SOURCE);
          }
          ImGui::EndMenu();
        }
      }
      if (comp && comp->type == ComponentType::RTL_MODULE) {
        ImGui::Separator();
        if (ImGui::BeginMenu(TR("ui.wiring.rtl.runtime_menu", "RTL Runtime"))) {
          const RtlLibraryEntry* rtl_entry =
              rtl_project_manager_ ? rtl_project_manager_->FindEntryById(
                                         comp->rtlModuleId)
                                  : nullptr;
          std::vector<std::string> input_pin_names;
          if (rtl_entry) {
            input_pin_names.reserve(rtl_entry->ports.size());
            for (const auto& port : rtl_entry->ports) {
              if (port.isInput) {
                input_pin_names.push_back(port.pinName);
              }
            }
          }
          char buf[256];
          plc::FormatString(buf, sizeof(buf), "ui.wiring.rtl.logic_family_label",
                            "Logic family: %s",
                            RtlLogicFamilyLabel(comp->rtlLogicFamily));
          ImGui::TextDisabled("%s", buf);

          if (ImGui::BeginCombo(TR("ui.wiring.rtl.logic_family", "Logic Family"),
                                RtlLogicFamilyLabel(comp->rtlLogicFamily))) {
            const RtlLogicFamily families[] = {
                RtlLogicFamily::INDUSTRIAL_24V,
                RtlLogicFamily::CMOS_5V,
                RtlLogicFamily::TTL_5V};
            for (RtlLogicFamily family : families) {
              bool selected = comp->rtlLogicFamily == family;
              if (ImGui::Selectable(RtlLogicFamilyLabel(family), selected)) {
                PushWiringUndoState();
                comp->rtlLogicFamily = family;
                InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          plc::FormatString(buf, sizeof(buf), "ui.wiring.rtl.vcc_pin",
                            "VCC pin: %s",
                            comp->rtlPowerPinName.empty()
                                ? TR("ui.wiring.rtl.always_powered",
                                     "(always powered)")
                                : comp->rtlPowerPinName.c_str());
          ImGui::TextDisabled("%s", buf);

          const char* current_power_pin =
              comp->rtlPowerPinName.empty()
                  ? TR("ui.wiring.rtl.always_powered", "(always powered)")
                  : comp->rtlPowerPinName.c_str();
          if (ImGui::BeginCombo(TR("ui.wiring.rtl.vcc_pin_label", "Power Pin"),
                                current_power_pin)) {
            bool selected_none = comp->rtlPowerPinName.empty();
            if (ImGui::Selectable(
                    TR("ui.wiring.rtl.always_powered", "(always powered)"),
                    selected_none)) {
              PushWiringUndoState();
              comp->rtlPowerPinName.clear();
              InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
            }
            if (selected_none) {
              ImGui::SetItemDefaultFocus();
            }
            for (const auto& pin_name : input_pin_names) {
              bool selected = comp->rtlPowerPinName == pin_name;
              if (ImGui::Selectable(pin_name.c_str(), selected)) {
                PushWiringUndoState();
                comp->rtlPowerPinName = pin_name;
                InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          plc::FormatString(buf, sizeof(buf), "ui.wiring.rtl.gnd_pin",
                            "GND pin: %s",
                            comp->rtlGroundPinName.empty()
                                ? TR("ui.wiring.rtl.none", "(none)")
                                : comp->rtlGroundPinName.c_str());
          ImGui::TextDisabled("%s", buf);

          const char* current_ground_pin =
              comp->rtlGroundPinName.empty()
                  ? TR("ui.wiring.rtl.none", "(none)")
                  : comp->rtlGroundPinName.c_str();
          if (ImGui::BeginCombo(TR("ui.wiring.rtl.gnd_pin_label", "Ground Pin"),
                                current_ground_pin)) {
            bool selected_none = comp->rtlGroundPinName.empty();
            if (ImGui::Selectable(TR("ui.wiring.rtl.none", "(none)"),
                                  selected_none)) {
              PushWiringUndoState();
              comp->rtlGroundPinName.clear();
              InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
            }
            if (selected_none) {
              ImGui::SetItemDefaultFocus();
            }
            for (const auto& pin_name : input_pin_names) {
              bool selected = comp->rtlGroundPinName == pin_name;
              if (ImGui::Selectable(pin_name.c_str(), selected)) {
                PushWiringUndoState();
                comp->rtlGroundPinName = pin_name;
                InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          ImGui::Separator();

          plc::FormatString(buf, sizeof(buf), "ui.wiring.rtl.clock_pin",
                            "Clock pin: %s",
                            comp->rtlClockPinName.empty()
                                ? TR("ui.wiring.rtl.none", "(none)")
                                : comp->rtlClockPinName.c_str());
          ImGui::TextDisabled("%s", buf);

          const char* current_clock_pin =
              comp->rtlClockPinName.empty() ? TR("ui.wiring.rtl.none", "(none)")
                                            : comp->rtlClockPinName.c_str();
          if (ImGui::BeginCombo(
                  TR("ui.wiring.rtl.clock_pin_label", "Clock Source Pin"),
                  current_clock_pin)) {
            bool selected_none = comp->rtlClockPinName.empty();
            if (ImGui::Selectable(TR("ui.wiring.rtl.none", "(none)"),
                                  selected_none)) {
              PushWiringUndoState();
              comp->rtlClockPinName.clear();
              comp->rtlUseInternalClock = false;
              InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
            }
            if (selected_none) {
              ImGui::SetItemDefaultFocus();
            }
            for (const auto& pin_name : input_pin_names) {
              bool selected = comp->rtlClockPinName == pin_name;
              if (ImGui::Selectable(pin_name.c_str(), selected)) {
                PushWiringUndoState();
                comp->rtlClockPinName = pin_name;
                InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          bool internal_clock = comp->rtlUseInternalClock;
          if (comp->rtlClockPinName.empty()) {
            ImGui::BeginDisabled();
          }
          if (ImGui::Checkbox(
                  TR("ui.wiring.rtl.use_internal_clock", "Use Internal Clock"),
                  &internal_clock)) {
            PushWiringUndoState();
            comp->rtlUseInternalClock = internal_clock;
            InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
          }
          if (comp->rtlClockPinName.empty()) {
            ImGui::EndDisabled();
          }
          if (comp->rtlUseInternalClock) {
            float freq = std::max(0.1f, comp->rtlClockFrequencyHz);
            if (ImGui::InputFloat(TR("ui.wiring.rtl.clock_hz", "Clock Hz"),
                                  &freq, 0.1f, 1.0f, "%.2f")) {
              PushWiringUndoState();
              comp->rtlClockFrequencyHz = std::max(0.1f, freq);
              InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
            }
          }
          ImGui::Separator();

          plc::FormatString(buf, sizeof(buf), "ui.wiring.rtl.reset_pin",
                            "Reset pin: %s",
                            comp->rtlResetPinName.empty()
                                ? TR("ui.wiring.rtl.none", "(none)")
                                : comp->rtlResetPinName.c_str());
          ImGui::TextDisabled("%s", buf);
          const char* current_reset_pin =
              comp->rtlResetPinName.empty() ? TR("ui.wiring.rtl.none", "(none)")
                                            : comp->rtlResetPinName.c_str();
          if (ImGui::BeginCombo(
                  TR("ui.wiring.rtl.reset_pin_label", "Reset Source Pin"),
                  current_reset_pin)) {
            bool selected_none = comp->rtlResetPinName.empty();
            if (ImGui::Selectable(TR("ui.wiring.rtl.none", "(none)"),
                                  selected_none)) {
              PushWiringUndoState();
              comp->rtlResetPinName.clear();
              comp->rtlUseStartupReset = false;
              InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
            }
            if (selected_none) {
              ImGui::SetItemDefaultFocus();
            }
            for (const auto& pin_name : input_pin_names) {
              bool selected = comp->rtlResetPinName == pin_name;
              if (ImGui::Selectable(pin_name.c_str(), selected)) {
                PushWiringUndoState();
                comp->rtlResetPinName = pin_name;
                InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          bool startup_reset = comp->rtlUseStartupReset;
          if (comp->rtlResetPinName.empty()) {
            ImGui::BeginDisabled();
          }
          if (ImGui::Checkbox(
                  TR("ui.wiring.rtl.startup_reset", "Startup Reset Pulse"),
                  &startup_reset)) {
            PushWiringUndoState();
            comp->rtlUseStartupReset = startup_reset;
            InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
          }
          if (comp->rtlResetPinName.empty()) {
            ImGui::EndDisabled();
          }
          bool active_low = comp->rtlResetActiveLow;
          if (ImGui::Checkbox(
                  TR("ui.wiring.rtl.reset_active_low", "Reset Active Low"),
                  &active_low)) {
            PushWiringUndoState();
            comp->rtlResetActiveLow = active_low;
            InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
          }
          if (comp->rtlUseStartupReset) {
            float pulse_ms = std::max(1.0f, comp->rtlResetPulseMs);
            if (ImGui::InputFloat(
                    TR("ui.wiring.rtl.reset_pulse_ms", "Reset Pulse ms"),
                    &pulse_ms, 1.0f, 10.0f, "%.1f")) {
              PushWiringUndoState();
              comp->rtlResetPulseMs = std::max(1.0f, pulse_ms);
              InvalidateRtlModuleRuntimeState(comp->rtlModuleId);
            }
          }
          ImGui::EndMenu();
        }
      }
    }
    ImGui::EndPopup();
  } else if (!show_component_context_menu_) {
    context_menu_component_id_ = -1;
  }
}

}  // namespace plc
