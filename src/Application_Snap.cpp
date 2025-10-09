// src/Application_Snap.cpp

#include "Application.h"
#include <iostream>
#include <cmath>
#include "imgui.h"

namespace plc {

ImVec2 Application::ApplySnap(ImVec2 position, bool isWirePoint) {
    if (!isWirePoint) {
        return m_snapSettings.enableGridSnap ? ApplyGridSnap(position) : position;
    }

    if (m_snapSettings.enablePortSnap) {
        ImVec2 portSnappedPos = ApplyPortSnap(position);
        if (portSnappedPos.x != position.x || portSnappedPos.y != position.y) {
            return portSnappedPos;
        }
    }

    ImVec2 referencePoint = m_currentWayPoints.empty() ? m_wireStartPos : ImVec2(m_currentWayPoints.back().x, m_currentWayPoints.back().y);

    if (m_snapSettings.enableVerticalSnap || m_snapSettings.enableHorizontalSnap) {
        ImVec2 lineSnappedPos = ApplyLineSnap(position, referencePoint);
         if (lineSnappedPos.x != position.x || lineSnappedPos.y != position.y) {
            return lineSnappedPos;
        }
    }

    if (m_snapSettings.enableGridSnap) {
        return ApplyGridSnap(position);
    }

    return position;
}

ImVec2 Application::ApplyGridSnap(ImVec2 position) {
    float gridSize = m_snapSettings.gridSize;
    float snapDistance = m_snapSettings.snapDistance / m_cameraZoom;

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

    if(port && componentId != -1) {
        for(const auto& comp : m_placedComponents) {
            if (comp.instanceId == componentId) {
                return ImVec2(comp.position.x + port->relativePos.x, comp.position.y + port->relativePos.y);
            }
        }
    }
    return position;
}

ImVec2 Application::ApplyLineSnap(ImVec2 position, ImVec2 referencePoint) {
    float snapDistance = m_snapSettings.snapDistance / m_cameraZoom;

    bool canSnapV = m_snapSettings.enableVerticalSnap && (std::abs(position.x - referencePoint.x) <= snapDistance);
    bool canSnapH = m_snapSettings.enableHorizontalSnap && (std::abs(position.y - referencePoint.y) <= snapDistance);

    if (canSnapV && canSnapH) {
        if (std::abs(position.x - referencePoint.x) < std::abs(position.y - referencePoint.y)) {
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

    ImVec2 referencePoint = m_currentWayPoints.empty() ? m_wireStartPos : ImVec2(m_currentWayPoints.back().x, m_currentWayPoints.back().y);
    ImVec2 ref_screen_pos = WorldToScreen(referencePoint);

    if (std::abs(worldSnapPos.x - referencePoint.x) < 1.0f) {
        draw_list->AddLine(ref_screen_pos, screen_pos, IM_COL32(0, 255, 0, 80), 1.5f);
    }
    if (std::abs(worldSnapPos.y - referencePoint.y) < 1.0f) {
        draw_list->AddLine(ref_screen_pos, screen_pos, IM_COL32(0, 255, 0, 80), 1.5f);
    }
}

} // namespace plc