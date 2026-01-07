// application_snap.cpp
//
// Grid snapping functionality.

// src/Application_Snap.cpp

#include "imgui.h"
#include "plc_emulator/core/application.h"

#include <cmath>
#include <iostream>

namespace plc {

ImVec2 Application::ApplySnap(ImVec2 position, bool isWirePoint) {
  if (!isWirePoint) {
    return snap_settings_.enableGridSnap ? ApplyGridSnap(position) : position;
  }

  if (snap_settings_.enablePortSnap) {
    ImVec2 portSnappedPos = ApplyPortSnap(position);
    if (portSnappedPos.x != position.x || portSnappedPos.y != position.y) {
      return portSnappedPos;
    }
  }

  ImVec2 referencePoint =
      current_way_points_.empty()
          ? wire_start_pos_
          : ImVec2(current_way_points_.back().x, current_way_points_.back().y);

  if (snap_settings_.enableVerticalSnap ||
      snap_settings_.enableHorizontalSnap) {
    ImVec2 lineSnappedPos = ApplyLineSnap(position, referencePoint, false);
    if (lineSnappedPos.x != position.x || lineSnappedPos.y != position.y) {
      return lineSnappedPos;
    }
  }

  if (snap_settings_.enableGridSnap) {
    return ApplyGridSnap(position);
  }

  return position;
}

ImVec2 Application::ApplyGridSnap(ImVec2 position) {
  float gridSize = snap_settings_.gridSize;
  float snapDistance = snap_settings_.snapDistance / camera_zoom_;

  float gridX = roundf(position.x / gridSize) * gridSize;
  float gridY = roundf(position.y / gridSize) * gridSize;

  float distX = std::abs(position.x - gridX);
  float distY = std::abs(position.y - gridY);

  if (distX < snapDistance && distY < snapDistance) {
    return {gridX, gridY};
  }
  if (distX < snapDistance) {
    return {gridX, position.y};
  }
  if (distY < snapDistance) {
    return {position.x, gridY};
  }

  return position;
}

ImVec2 Application::ApplyPortSnap(ImVec2 position) {
  int componentId = -1;
  Port* port = FindPortAtPosition(position, componentId);

  if (port && componentId != -1) {
    for (const auto& comp : placed_components_) {
      if (comp.instanceId == componentId) {
        return ImVec2(comp.position.x + port->relativePos.x,
                      comp.position.y + port->relativePos.y);
      }
    }
  }
  return position;
}

ImVec2 Application::ApplyLineSnap(ImVec2 position, ImVec2 referencePoint,
                                  bool force_orthogonal) {
  (void)force_orthogonal;
  float snapDistance = snap_settings_.snapDistance / camera_zoom_;

  bool canSnapV = snap_settings_.enableVerticalSnap &&
                  (std::abs(position.x - referencePoint.x) <= snapDistance);
  bool canSnapH = snap_settings_.enableHorizontalSnap &&
                  (std::abs(position.y - referencePoint.y) <= snapDistance);

  if (canSnapV && canSnapH) {
    if (std::abs(position.x - referencePoint.x) <
        std::abs(position.y - referencePoint.y)) {
      return ImVec2(referencePoint.x, position.y);
    } else {
      return ImVec2(position.x, referencePoint.y);
    }
  }

  if (canSnapV) {
    return ImVec2(referencePoint.x, position.y);
  }

  if (canSnapH) {
    return ImVec2(position.x, referencePoint.y);
  }

  return position;
}

void Application::RenderSnapGuides(ImDrawList* draw_list, ImVec2 worldSnapPos) {
  ImVec2 screen_pos = WorldToScreen(worldSnapPos);
  draw_list->AddCircle(screen_pos, 4.0f, IM_COL32(0, 255, 0, 200), 12, 2.0f);

  ImVec2 referencePoint =
      current_way_points_.empty()
          ? wire_start_pos_
          : ImVec2(current_way_points_.back().x, current_way_points_.back().y);
  ImVec2 ref_screen_pos = WorldToScreen(referencePoint);

  if (std::abs(worldSnapPos.x - referencePoint.x) < 1.0f) {
    draw_list->AddLine(ref_screen_pos, screen_pos, IM_COL32(0, 255, 0, 80),
                       1.5f);
  }
  if (std::abs(worldSnapPos.y - referencePoint.y) < 1.0f) {
    draw_list->AddLine(ref_screen_pos, screen_pos, IM_COL32(0, 255, 0, 80),
                       1.5f);
  }
}

}  // namespace plc
