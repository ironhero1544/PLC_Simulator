// application_wiring.cpp
//
// Wire drawing and editing.

// src/Application_Wiring.cpp

#include "imgui.h"
#include "plc_emulator/components/component_behavior.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/lang/lang_manager.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>

#include <GLFW/glfw3.h>

namespace plc {

namespace {

bool IsPointInsideComponent(const PlacedComponent& comp, ImVec2 world_pos) {
  return world_pos.x >= comp.position.x &&
         world_pos.x <= comp.position.x + comp.size.width &&
         world_pos.y >= comp.position.y &&
         world_pos.y <= comp.position.y + comp.size.height;
}

int FindFirstComponentIndexAtPosition(
    const std::vector<PlacedComponent>& components, ImVec2 world_pos) {
  for (size_t i = 0; i < components.size(); ++i) {
    if (IsPointInsideComponent(components[i], world_pos)) {
      return static_cast<int>(i);
    }
  }
  return -1;
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
  for (auto& wire : wires_) {
    auto startIt =
        port_positions_.find({wire.fromComponentId, wire.fromPortId});
    auto endIt = port_positions_.find({wire.toComponentId, wire.toPortId});

    if (startIt == port_positions_.end() || endIt == port_positions_.end()) {
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

    ImVec2 current_pos = start_screen_pos;
    for (const auto& wp : wire.wayPoints) {
      ImVec2 next_pos = WorldToScreen(wp);
      draw_list->AddLine(current_pos, next_pos, wire_color, wire_thickness);
      current_pos = next_pos;
    }
    draw_list->AddLine(current_pos, end_screen_pos, wire_color, wire_thickness);

    if (wire.id == selected_wire_id_) {
      ImU32 highlight_color = IM_COL32(255, 255, 0, 150);
      ImVec2 p1 = start_screen_pos;
      for (const auto& wp : wire.wayPoints) {
        ImVec2 p2 = WorldToScreen(wp);
        draw_list->AddLine(p1, p2, highlight_color, wire_thickness + 3.0f);
        p1 = p2;
      }
      draw_list->AddLine(p1, end_screen_pos, highlight_color,
                         wire_thickness + 3.0f);
    }

    if (wire.id == selected_wire_id_ ||
        (editing_wire_id_ == wire.id &&
         wire_edit_mode_ == WireEditMode::MOVING_POINT)) {
      for (size_t i = 0; i < wire.wayPoints.size(); ++i) {
        ImVec2 wp_pos = WorldToScreen(wire.wayPoints[i]);
        float radius = (editing_wire_id_ == wire.id && editing_point_index_ == static_cast<int>(i))
                           ? 8.0f
                           : 5.0f;
        draw_list->AddCircleFilled(wp_pos, radius, IM_COL32(255, 255, 0, 255));
        draw_list->AddCircle(wp_pos, radius, IM_COL32(0, 0, 0, 255), 12, 1.5f);
      }
    }
  }
}

void Application::DeleteWire(int wireId) {
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

    ImGui::SetCursorScreenPos(canvas_top_left_);
    ImGui::InvisibleButton("canvas_interaction_area", canvas_size_);
    ImVec2 mouse_screen_pos =
        last_pointer_is_pan_input_ ? touch_anchor_screen_pos_ : io.MousePos;
    ImVec2 mouse_world_pos = ScreenToWorld(mouse_screen_pos);

    const bool is_canvas_hovered =
        ImGui::IsItemHovered() ||
        (mouse_screen_pos.x >= canvas_top_left_.x &&
         mouse_screen_pos.x <= canvas_top_left_.x + canvas_size_.x &&
         mouse_screen_pos.y >= canvas_top_left_.y &&
         mouse_screen_pos.y <= canvas_top_left_.y + canvas_size_.y);
    const bool allow_pan_zoom = is_canvas_hovered || touch_gesture_active_;
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

    // Handle FRL pressure adjustment
    bool frl_interaction_handled = HandleFRLPressureAdjustment(mouse_world_pos, io);

    // Handle zoom and pan
    bool wheel_handled = false;
    HandleZoomAndPan(allow_pan_zoom, io, frl_interaction_handled, wheel_handled);
    
    if ((wheel_handled || frl_interaction_handled) && is_canvas_hovered) {
      ImGui::SetItemDefaultFocus();
    }

    // Render grid
    RenderGrid(draw_list);

    // Handle component drop and wire deletion
    HandleComponentDropAndWireDelete(is_canvas_hovered, mouse_world_pos, io);

    if (current_tool_ == ToolType::SELECT) {
      if (wire_edit_mode_ == WireEditMode::MOVING_POINT) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          for (auto& wire : wires_) {
            if (wire.id == editing_wire_id_ && editing_point_index_ >= 0 &&
                editing_point_index_ < static_cast<int>(wire.wayPoints.size())) {
          ImVec2 reference_point = mouse_world_pos;
          if (editing_point_index_ > 0) {
            const Position& prev =
                wire.wayPoints[static_cast<size_t>(editing_point_index_ - 1)];
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
          editing_point_index_ = -1;
        }
      } else if (is_moving_component_) {
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
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
          bool interaction_handled = false;
          for (int i = static_cast<int>(placed_components_.size()) - 1; i >= 0; --i) {
            const auto& comp = placed_components_[static_cast<size_t>(i)];
            if (mouse_world_pos.x >= comp.position.x &&
                mouse_world_pos.x <= comp.position.x + comp.size.width &&
                mouse_world_pos.y >= comp.position.y &&
                mouse_world_pos.y <= comp.position.y + comp.size.height) {
              HandleComponentSelection(comp.instanceId);
              interaction_handled = true;
              break;
            }
          }
          if (!interaction_handled)
            HandleComponentSelection(-1);
        }

        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            is_canvas_hovered) {
          for (auto& comp : placed_components_) {
            if (mouse_world_pos.x >= comp.position.x &&
                mouse_world_pos.x <= comp.position.x + comp.size.width &&
                mouse_world_pos.y >= comp.position.y &&
                mouse_world_pos.y <= comp.position.y + comp.size.height) {
              const ComponentBehavior* behavior =
                  GetComponentBehavior(comp.type);
              if (behavior && behavior->OnDoubleClick &&
                  behavior->OnDoubleClick(&comp, mouse_world_pos,
                                          ImGuiMouseButton_Left)) {
                break;
              }
            }
          }
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && is_canvas_hovered &&
            !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          for (auto& comp : placed_components_) {
            if (mouse_world_pos.x >= comp.position.x &&
                mouse_world_pos.x <= comp.position.x + comp.size.width &&
                mouse_world_pos.y >= comp.position.y &&
                mouse_world_pos.y <= comp.position.y + comp.size.height) {
              const ComponentBehavior* behavior =
                  GetComponentBehavior(comp.type);
              if (behavior && behavior->OnMouseDown) {
                behavior->OnMouseDown(&comp, mouse_world_pos,
                                      ImGuiMouseButton_Left);
              }
            }
          }
        } else {
          for (auto& comp : placed_components_) {
            const ComponentBehavior* behavior =
                GetComponentBehavior(comp.type);
            if (behavior && behavior->OnMouseUp) {
              behavior->OnMouseUp(&comp, mouse_world_pos,
                                  ImGuiMouseButton_Left);
            }
          }
        }

        if (is_canvas_hovered &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
            selected_component_id_ != -1 && !is_moving_component_) {
          for (const auto& comp : placed_components_) {
            if (comp.instanceId == selected_component_id_) {
              if (mouse_world_pos.x >= comp.position.x &&
                  mouse_world_pos.x <= comp.position.x + comp.size.width &&
                  mouse_world_pos.y >= comp.position.y &&
                  mouse_world_pos.y <= comp.position.y + comp.size.height) {
                HandleComponentMoveStart(selected_component_id_,
                                         mouse_world_pos);
              }
              break;
            }
          }
        }
      }
    } else {
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
          if (!is_connecting_)
            StartWireConnection(clickedComponentId, clickedPort->id,
                                port_world_pos);
          else
            CompleteWireConnection(clickedComponentId, clickedPort->id,
                                   port_world_pos);
          } else if (is_connecting_) {
            HandleWayPointClick(mouse_world_pos, true);
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
          if ((dx * dx + dy * dy) >= (min_waypoint_dist * min_waypoint_dist)) {
            AddWayPoint(candidate);
            last_auto_waypoint_time_ = now;
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
                              ? FindFirstComponentIndexAtPosition(
                                    placed_components_, mouse_world_pos)
                              : -1;
        if (hover_index != -1) {
          const auto& comp = placed_components_[static_cast<size_t>(hover_index)];
          const ComponentBehavior* behavior =
              GetComponentBehavior(comp.type);
          if (behavior && behavior->BuildTooltip &&
              behavior->BuildTooltip(comp, mouse_world_pos)) {
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
                  behavior->BuildTooltip(comp, mouse_world_pos)) {
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
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
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
    HandleComponentSelection(-1);
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

// ============================================================================
// RenderWiringCanvas Helper Functions (Phase 6 Refactoring)
// ============================================================================

bool Application::HandleFRLPressureAdjustment(ImVec2 mouse_world_pos, 
                                               const ImGuiIO& io) {
  if (!io.MouseWheel)
    return false;

  for (auto& comp : placed_components_) {
    if (mouse_world_pos.x >= comp.position.x &&
        mouse_world_pos.x <= comp.position.x + comp.size.width &&
        mouse_world_pos.y >= comp.position.y &&
        mouse_world_pos.y <= comp.position.y + comp.size.height) {
      const ComponentBehavior* behavior =
          GetComponentBehavior(comp.type);
      if (behavior && behavior->OnMouseWheel &&
          behavior->OnMouseWheel(&comp, mouse_world_pos, io)) {
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
  bool right_down = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) ==
                    GLFW_PRESS;
  if (!right_click && right_down && !prev_right_button_down_) {
    right_click = true;
  }
  win32_right_click_ = false;
  prev_right_button_down_ = right_down;

  // Right click: cancel wire if connecting, otherwise delete under cursor.
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
      for (int i = static_cast<int>(placed_components_.size()) - 1; i >= 0; --i) {
        const auto& comp = placed_components_[static_cast<size_t>(i)];
        if (mouse_world_pos.x >= comp.position.x &&
            mouse_world_pos.x <= comp.position.x + comp.size.width &&
            mouse_world_pos.y >= comp.position.y &&
            mouse_world_pos.y <= comp.position.y + comp.size.height) {
          HandleComponentSelection(comp.instanceId);
          DeleteSelectedComponent();
          deleted = true;
          if (IsDebugEnabled()) {
            DebugLog("[INPUT] Right click: delete component " +
                     std::to_string(comp.instanceId));
          }
          break;
        }
      }
      if (!deleted) {
      Wire* wire_to_delete = FindWireAtPosition(mouse_world_pos);
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
      for (int i = static_cast<int>(placed_components_.size()) - 1; i >= 0; --i) {
        const auto& comp = placed_components_[static_cast<size_t>(i)];
        if (mouse_world_pos.x >= comp.position.x &&
            mouse_world_pos.x <= comp.position.x + comp.size.width &&
            mouse_world_pos.y >= comp.position.y &&
            mouse_world_pos.y <= comp.position.y + comp.size.height) {
          HandleComponentSelection(comp.instanceId);
          DeleteSelectedComponent();
          deleted = true;
          if (IsDebugEnabled()) {
            DebugLog("[INPUT] Side click: delete component " +
                     std::to_string(comp.instanceId));
          }
          break;
        }
      }
    }
    if (!deleted) {
      const float delete_tolerance = side_hold ? 20.0f : 10.0f;
      Wire* wire_to_delete = FindWireAtPosition(mouse_world_pos,
                                                delete_tolerance);
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

}  // namespace plc
