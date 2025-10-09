// src/Application_Wiring.cpp

#include "Application.h"
#include "imgui.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

namespace plc {

// ★★★ FIX: 아래의 불필요한 함수 선언을 삭제했습니다. ★★★
// 이 선언은 Application.h에 이미 존재하며, 여기서 다시 선언할 필요가 없습니다.
// Position GetPortRelativePositionById(const PlacedComponent& comp, int portId);


void Application::StartWireConnection(int componentId, int portId, ImVec2 portWorldPos) {
    if (!m_isConnecting) {
        m_isConnecting = true;
        m_wireStartComponentId = componentId;
        m_wireStartPortId = portId;
        m_wireStartPos = portWorldPos;
        m_wireCurrentPos = portWorldPos;
        m_currentWayPoints.clear();
    }
}

void Application::UpdateWireDrawing(ImVec2 mousePos) {
    if (m_isConnecting) {
        m_wireCurrentPos = mousePos;
    }
}

void Application::CompleteWireConnection(int componentId, int portId, ImVec2 portWorldPos) {
    if (m_isConnecting && componentId != m_wireStartComponentId) {
        int targetComponentId = -1;
        Port* targetPort = FindPortAtPosition(portWorldPos, targetComponentId);
        if (targetPort && targetComponentId == componentId) {
            int startComponentId = -1;
            Port* startPort = FindPortAtPosition(m_wireStartPos, startComponentId);
            if (startPort && startComponentId == m_wireStartComponentId) {
                if (IsValidWireConnection(*startPort, *targetPort)) {
                    Wire newWire;
                    newWire.id = m_nextWireId++;
                    newWire.fromComponentId = m_wireStartComponentId;
                    newWire.fromPortId = m_wireStartPortId;
                    newWire.toComponentId = componentId;
                    newWire.toPortId = portId;
                    newWire.wayPoints = m_currentWayPoints;
                    newWire.isElectric = (m_currentTool == ToolType::ELECTRIC);

                    if (newWire.isElectric) {
                        newWire.color = {1.0f, 0.27f, 0.0f, 1.0f};
                        newWire.thickness = 2.0f;
                    } else {
                        newWire.color = {0.13f, 0.59f, 0.95f, 1.0f};
                        newWire.thickness = 3.0f;
                    }
                    m_wires.push_back(newWire);
                }
            }
        }
        m_isConnecting = false;
        m_currentWayPoints.clear();
    }
}

void Application::RenderWires(ImDrawList* draw_list) {
    for (auto& wire : m_wires) {
        auto startIt = m_portPositions.find({wire.fromComponentId, wire.fromPortId});
        auto endIt = m_portPositions.find({wire.toComponentId, wire.toPortId});

        if (startIt == m_portPositions.end() || endIt == m_portPositions.end()) {
            continue;
        }

        ImVec2 start_screen_pos = WorldToScreen(startIt->second);
        ImVec2 end_screen_pos = WorldToScreen(endIt->second);

        ImU32 wire_color;
        float wire_thickness = (wire.isElectric ? 2.0f : 3.0f);

        if (wire.isElectric) {
            auto port1_ref = std::make_pair(wire.fromComponentId, wire.fromPortId);
            auto port2_ref = std::make_pair(wire.toComponentId, wire.toPortId);
            float v1 = m_portVoltages.count(port1_ref) ? m_portVoltages.at(port1_ref) : -1.0f;
            float v2 = m_portVoltages.count(port2_ref) ? m_portVoltages.at(port2_ref) : -1.0f;

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
            wire_color = IM_COL32((int)(wire.color.r * 255), (int)(wire.color.g * 255), (int)(wire.color.b * 255), (int)(wire.color.a * 255));
        }

        ImVec2 current_pos = start_screen_pos;
        for(const auto& wp : wire.wayPoints) {
            ImVec2 next_pos = WorldToScreen(wp);
            draw_list->AddLine(current_pos, next_pos, wire_color, wire_thickness);
            current_pos = next_pos;
        }
        draw_list->AddLine(current_pos, end_screen_pos, wire_color, wire_thickness);

        if (wire.id == m_selectedWireId) {
            ImU32 highlight_color = IM_COL32(255, 255, 0, 150);
            ImVec2 p1 = start_screen_pos;
            for(const auto& wp : wire.wayPoints) {
                ImVec2 p2 = WorldToScreen(wp);
                draw_list->AddLine(p1, p2, highlight_color, wire_thickness + 3.0f);
                p1 = p2;
            }
            draw_list->AddLine(p1, end_screen_pos, highlight_color, wire_thickness + 3.0f);
        }

        if (wire.id == m_selectedWireId || (m_editingWireId == wire.id && m_wireEditMode == WireEditMode::MOVING_POINT)) {
            for (int i = 0; i < (int)wire.wayPoints.size(); ++i) {
                ImVec2 wp_pos = WorldToScreen(wire.wayPoints[i]);
                float radius = (m_editingWireId == wire.id && m_editingPointIndex == i) ? 8.0f : 5.0f;
                draw_list->AddCircleFilled(wp_pos, radius, IM_COL32(255, 255, 0, 255));
                draw_list->AddCircle(wp_pos, radius, IM_COL32(0,0,0,255), 12, 1.5f);
            }
        }
    }
}

void Application::DeleteWire(int wireId) {
    m_wires.erase(std::remove_if(m_wires.begin(), m_wires.end(),
        [wireId](const Wire& wire) { return wire.id == wireId; }), m_wires.end());
    m_selectedWireId = -1;
}

void Application::RenderWiringCanvas() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);

    ImGuiWindowFlags canvas_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::BeginChild("Canvas", ImVec2(0, 0), true, canvas_flags)) {
        m_canvasTopLeft = ImGui::GetCursorScreenPos();
        m_canvasSize = ImGui::GetContentRegionAvail();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetCursorScreenPos(m_canvasTopLeft);
        ImGui::InvisibleButton("canvas_interaction_area", m_canvasSize);
        ImVec2 mouse_world_pos = ScreenToWorld(io.MousePos);

        const bool is_canvas_hovered = ImGui::IsItemHovered() ||
            (io.MousePos.x >= m_canvasTopLeft.x && io.MousePos.x <= m_canvasTopLeft.x + m_canvasSize.x &&
             io.MousePos.y >= m_canvasTopLeft.y && io.MousePos.y <= m_canvasTopLeft.y + m_canvasSize.y);

        bool frl_interaction_handled = false;
        if (is_canvas_hovered && io.MouseWheel != 0) {
            for (auto& comp : m_placedComponents) {
                if (comp.type == ComponentType::FRL &&
                    mouse_world_pos.x >= comp.position.x && mouse_world_pos.x <= comp.position.x + comp.size.width &&
                    mouse_world_pos.y >= comp.position.y && mouse_world_pos.y <= comp.position.y + comp.size.height)
                {
                    float current_pressure = comp.internalStates.count("air_pressure") ? comp.internalStates.at("air_pressure") : 6.0f;
                    float step = io.KeyCtrl ? 0.1f : 0.5f;

                    if (io.MouseWheel > 0) current_pressure += step;
                    else current_pressure -= step;

                    current_pressure = std::max(0.0f, std::min(10.0f, current_pressure));
                    comp.internalStates["air_pressure"] = current_pressure;
                    frl_interaction_handled = true;
                    break;
                }
            }
        }

        bool wheel_handled = false;
        if (is_canvas_hovered && io.MouseWheel != 0 && !io.KeyCtrl && !frl_interaction_handled) {
            ImVec2 mouse_pos_before_zoom = ScreenToWorld(io.MousePos);
            float zoom_speed = 0.15f;
            if (io.MouseWheel > 0) m_cameraZoom *= (1.0f + zoom_speed);
            else m_cameraZoom *= (1.0f - zoom_speed);
            m_cameraZoom = std::max(0.05f, std::min(m_cameraZoom, 10.0f));
            ImVec2 mouse_pos_after_zoom = ScreenToWorld(io.MousePos);
            m_cameraOffset.x += (mouse_pos_before_zoom.x - mouse_pos_after_zoom.x);
            m_cameraOffset.y += (mouse_pos_before_zoom.y - mouse_pos_after_zoom.y);
            wheel_handled = true;
        }

        float pan_sensitivity = 1.0f;
        if (is_canvas_hovered && io.KeyCtrl && (io.MouseWheelH != 0 || io.MouseWheel != 0) && !frl_interaction_handled) {
            float trackpad_sensitivity = 0.8f;
            m_cameraOffset.x += io.MouseWheelH * 20.0f * trackpad_sensitivity;
            m_cameraOffset.y += io.MouseWheel * 20.0f * trackpad_sensitivity;
            wheel_handled = true;
        }
        if (is_canvas_hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || (io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))) {
            m_cameraOffset.x += io.MouseDelta.x / m_cameraZoom * pan_sensitivity;
            m_cameraOffset.y += io.MouseDelta.y / m_cameraZoom * pan_sensitivity;
        }
        if (is_canvas_hovered && io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            m_cameraOffset.x += io.MouseDelta.x / m_cameraZoom * pan_sensitivity;
            m_cameraOffset.y += io.MouseDelta.y / m_cameraZoom * pan_sensitivity;
        }
        if ((wheel_handled || frl_interaction_handled) && is_canvas_hovered) {
            ImGui::SetItemDefaultFocus();
        }

        float grid_step = m_snapSettings.gridSize;
        ImVec2 world_top_left = ScreenToWorld(m_canvasTopLeft);
        ImVec2 world_bottom_right = ScreenToWorld(ImVec2(m_canvasTopLeft.x + m_canvasSize.x, m_canvasTopLeft.y + m_canvasSize.y));
        for (float x = floorf(world_top_left.x / grid_step) * grid_step; x < world_bottom_right.x; x += grid_step) {
            draw_list->AddLine(WorldToScreen({x, world_top_left.y}), WorldToScreen({x, world_bottom_right.y}), IM_COL32(220, 220, 220, 80));
        }
        for (float y = floorf(world_top_left.y / grid_step) * grid_step; y < world_bottom_right.y; y += grid_step) {
            draw_list->AddLine(WorldToScreen({world_top_left.x, y}), WorldToScreen({world_bottom_right.x, y}), IM_COL32(220, 220, 220, 80));
        }

        if (m_isDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (is_canvas_hovered) HandleComponentDrop(mouse_world_pos);
            else { m_isDragging = false; m_draggedComponentIndex = -1; }
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && is_canvas_hovered) {
            if (m_isConnecting) { m_isConnecting = false; m_currentWayPoints.clear(); }
            else { Wire* wire_to_delete = FindWireAtPosition(mouse_world_pos); if (wire_to_delete) DeleteWire(wire_to_delete->id); }
        }

        if (m_currentTool == ToolType::SELECT) {
            if (m_wireEditMode == WireEditMode::MOVING_POINT) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    for (auto& wire : m_wires) {
                        if (wire.id == m_editingWireId && m_editingPointIndex >= 0 && m_editingPointIndex < (int)wire.wayPoints.size()) {
                            ImVec2 snapped_pos = ApplySnap(mouse_world_pos, true);
                            wire.wayPoints[m_editingPointIndex] = snapped_pos;
                        }
                    }
                } else if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    m_wireEditMode = WireEditMode::NONE; m_editingPointIndex = -1;
                }
            } else if (m_isMovingComponent) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                     for (auto& comp : m_placedComponents) {
                        if (comp.instanceId == m_movingComponentId) {
                            comp.position.x = mouse_world_pos.x - m_dragStartOffset.x;
                            comp.position.y = mouse_world_pos.y - m_dragStartOffset.y;
                            break;
                        }
                    }
                }
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) HandleComponentMoveEnd();
            } else {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
                    bool interaction_handled = false;
                    for (int i = m_placedComponents.size() - 1; i >= 0; --i) {
                        const auto& comp = m_placedComponents[i];
                        if (mouse_world_pos.x >= comp.position.x && mouse_world_pos.x <= comp.position.x + comp.size.width &&
                            mouse_world_pos.y >= comp.position.y && mouse_world_pos.y <= comp.position.y + comp.size.height) {
                            HandleComponentSelection(comp.instanceId);
                            interaction_handled = true;
                            break;
                        }
                    }
                     if (!interaction_handled) HandleComponentSelection(-1);
                }

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
                    for(auto& comp : m_placedComponents) {
                         if (mouse_world_pos.x >= comp.position.x && mouse_world_pos.x <= comp.position.x + comp.size.width &&
                            mouse_world_pos.y >= comp.position.y && mouse_world_pos.y <= comp.position.y + comp.size.height) {

                            if(comp.type == ComponentType::LIMIT_SWITCH) {
                                comp.internalStates["is_pressed_manual"] = 1.0f - (comp.internalStates.count("is_pressed_manual") ? comp.internalStates.at("is_pressed_manual") : 0.0f);
                                break;
                            }
                            if(comp.type == ComponentType::BUTTON_UNIT) {
                                float local_y = mouse_world_pos.y - comp.position.y;

                                if(local_y >= 70.0f && local_y <= 90.0f) {
                                    std::string key = "is_pressed_2";
                                    comp.internalStates[key] = 1.0f - (comp.internalStates.count(key) ? comp.internalStates.at(key) : 0.0f);
                                }
                                break;
                            }
                        }
                    }
                }

                if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && is_canvas_hovered && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    for(auto& comp : m_placedComponents) {
                        if(comp.type == ComponentType::BUTTON_UNIT) {
                           if (mouse_world_pos.x >= comp.position.x && mouse_world_pos.x <= comp.position.x + comp.size.width &&
                                mouse_world_pos.y >= comp.position.y && mouse_world_pos.y <= comp.position.y + comp.size.height) {
                                for(int i = 0; i < 2; ++i) {
                                    float y_offset = 20 + i * 30;
                                    float local_y = mouse_world_pos.y - comp.position.y;
                                     if(local_y > y_offset - 8 && local_y < y_offset + 8) {
                                        comp.internalStates["is_pressed_" + std::to_string(i)] = 1.0f;
                                    }
                                }
                            }
                        }
                    }
                } else {
                     for(auto& comp : m_placedComponents) {
                        if(comp.type == ComponentType::BUTTON_UNIT) {
                           comp.internalStates["is_pressed_0"] = 0.0f;
                           comp.internalStates["is_pressed_1"] = 0.0f;
                        }
                     }
                }

                if (is_canvas_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_selectedComponentId != -1 && !m_isMovingComponent) {
                    for (const auto& comp : m_placedComponents) {
                        if (comp.instanceId == m_selectedComponentId) {
                            if (mouse_world_pos.x >= comp.position.x && mouse_world_pos.x <= comp.position.x + comp.size.width &&
                                mouse_world_pos.y >= comp.position.y && mouse_world_pos.y <= comp.position.y + comp.size.height) {
                                HandleComponentMoveStart(m_selectedComponentId, mouse_world_pos);
                            }
                            break;
                        }
                    }
                }
            }
        } else {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_canvas_hovered) {
                ImVec2 snapped_pos = ApplySnap(mouse_world_pos, true);
                int clickedComponentId = -1;
                Port* clickedPort = FindPortAtPosition(snapped_pos, clickedComponentId);
                if (clickedPort && clickedComponentId != -1) {
                    if (!m_isConnecting) StartWireConnection(clickedComponentId, clickedPort->id, snapped_pos);
                    else CompleteWireConnection(clickedComponentId, clickedPort->id, snapped_pos);
                } else if (m_isConnecting) HandleWayPointClick(snapped_pos);
            }
        }

        RenderPlacedComponents(draw_list);
        RenderWires(draw_list);
        if (m_isConnecting) {
             ImVec2 snapped_mouse_pos = ApplySnap(mouse_world_pos, true);
             UpdateWireDrawing(snapped_mouse_pos);
             if (snapped_mouse_pos.x != mouse_world_pos.x || snapped_mouse_pos.y != mouse_world_pos.y) {
                 RenderSnapGuides(draw_list, snapped_mouse_pos);
             }
            RenderTemporaryWire(draw_list);
        }

        if (is_canvas_hovered && !m_isDragging && !m_isMovingComponent && !m_isConnecting) {
            bool tooltip_shown = false;
            int hoveredComponentId = -1;
            Port* hoveredPort = FindPortAtPosition(mouse_world_pos, hoveredComponentId);
            if (hoveredPort) {
                ImGui::BeginTooltip();
                ImGui::Text("컴포넌트 ID: %d, 포트 ID: %d", hoveredComponentId, hoveredPort->id);
                ImGui::Separator();
                ImGui::Text("타입: %s", (hoveredPort->type == PortType::ELECTRIC) ? "전기" : "공압");
                ImGui::Text("역할: %s", hoveredPort->role.c_str());
                ImGui::Text("방향: %s", hoveredPort->isInput ? "입력" : "출력");
                ImGui::EndTooltip();
                tooltip_shown = true;
            }

            if(!tooltip_shown) {
                for(const auto& comp : m_placedComponents) {
                     if (mouse_world_pos.x >= comp.position.x && mouse_world_pos.x <= comp.position.x + comp.size.width &&
                        mouse_world_pos.y >= comp.position.y && mouse_world_pos.y <= comp.position.y + comp.size.height) {
                         if(comp.type == ComponentType::LIMIT_SWITCH || comp.type == ComponentType::BUTTON_UNIT) {
                             bool is_pressed = false;
                             if(comp.type == ComponentType::LIMIT_SWITCH && comp.internalStates.count("is_pressed")) {
                                 is_pressed = comp.internalStates.at("is_pressed") > 0.5f;
                             }
                             if(comp.type == ComponentType::BUTTON_UNIT) {
                                for(int i=0; i<3; ++i) {
                                    if(comp.internalStates.count("is_pressed_" + std::to_string(i)) && comp.internalStates.at("is_pressed_" + std::to_string(i)) > 0.5f) {
                                        is_pressed = true;
                                        break;
                                    }
                                }
                             }
                             ImGui::BeginTooltip();
                             ImGui::Text("상태: %s", is_pressed ? "눌림" : "안 눌림");
                             ImGui::EndTooltip();
                             break;
                         } else if (comp.type == ComponentType::FRL) {
                             float pressure = comp.internalStates.count("air_pressure") ? comp.internalStates.at("air_pressure") : 6.0f;
                             ImGui::BeginTooltip();
                             ImGui::Text("현재 압력: %.1f bar", pressure);
                             ImGui::TextDisabled("마우스 휠로 조절하세요");
                             ImGui::EndTooltip();
                             break;
                         }
                     }
                }
            }
        }

        if (is_canvas_hovered) {
            ImVec2 help_pos = ImVec2(m_canvasTopLeft.x + m_canvasSize.x - 200, m_canvasTopLeft.y + m_canvasSize.y - 100);
            ImGui::SetCursorScreenPos(help_pos);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

            if (ImGui::BeginChild("CameraHelp", ImVec2(190, 90), false, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
                ImGui::Text("카메라 제어:");
                ImGui::Separator();
                ImGui::Text("줌: 마우스 휠");
                ImGui::Text("팬: 중간버튼 드래그");
                ImGui::Text("팬: Shift + 좌클릭 드래그");
                ImGui::Text("트랙패드: Ctrl + 스크롤");
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
void Application::HandleWayPointClick(ImVec2 worldPos) {
    if (m_isConnecting) {
        ImVec2 snappedPos = ApplySnap(worldPos, true);
        AddWayPoint(snappedPos);
    }
}

void Application::AddWayPoint(ImVec2 position) {
    m_currentWayPoints.push_back(position);
}

    void Application::RenderTemporaryWire(ImDrawList* draw_list) {
    if (!m_isConnecting) return;
    ImU32 temp_color = (m_currentTool == ToolType::ELECTRIC) ? IM_COL32(255, 69, 0, 150) : IM_COL32(33, 150, 243, 150);

    ImVec2 current_pos_on_screen = WorldToScreen(m_wireStartPos);

    for (const auto& wayPoint : m_currentWayPoints) {
        ImVec2 next_pos_on_screen = WorldToScreen(wayPoint);
        draw_list->AddLine(current_pos_on_screen, next_pos_on_screen, temp_color, 2.0f);
        current_pos_on_screen = next_pos_on_screen;
    }

    ImVec2 end_pos_on_screen = WorldToScreen(m_wireCurrentPos);
    draw_list->AddLine(current_pos_on_screen, end_pos_on_screen, temp_color, 2.0f);
}

void Application::HandleWireSelection(ImVec2 worldPos) {
    Wire* selectedWire = FindWireAtPosition(worldPos);
    if (selectedWire) {
        m_selectedWireId = selectedWire->id;
        HandleComponentSelection(-1);
    } else {
        m_selectedWireId = -1;
        m_wireEditMode = WireEditMode::NONE;
        m_editingWireId = -1;
        m_editingPointIndex = -1;
    }
}

int Application::FindWayPointAtPosition(Wire& wire, ImVec2 worldPos, float radius) {
    for (int i = 0; i < (int)wire.wayPoints.size(); i++) {
        float dist_sq = pow(worldPos.x - wire.wayPoints[i].x, 2) + pow(worldPos.y - wire.wayPoints[i].y, 2);
        if (dist_sq < (radius / m_cameraZoom) * (radius / m_cameraZoom)) {
            return i;
        }
    }
    return -1;
}

Wire* Application::FindWireAtPosition(ImVec2 worldPos, float tolerance) {
    Wire* closestWire = nullptr;
    float min_dist_sq = (tolerance / m_cameraZoom) * (tolerance / m_cameraZoom);

    for (auto& wire : m_wires) {
        auto startIt = m_portPositions.find({wire.fromComponentId, wire.fromPortId});
        auto endIt = m_portPositions.find({wire.toComponentId, wire.toPortId});
        if (startIt == m_portPositions.end() || endIt == m_portPositions.end()) continue;

        ImVec2 p1 = startIt->second;

        std::vector<ImVec2> points_for_check;
        for(const auto& wp : wire.wayPoints) {
            points_for_check.push_back(wp);
        }
        points_for_check.push_back(endIt->second);

        for (const auto& p2 : points_for_check) {
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;

            if (dx == 0 && dy == 0) { p1 = p2; continue; }

            float t = ((worldPos.x - p1.x) * dx + (worldPos.y - p1.y) * dy) / (dx * dx + dy * dy);
            t = std::max(0.0f, std::min(1.0f, t));

            float closestX = p1.x + t * dx;
            float closestY = p1.y + t * dy;

            float dist_sq = (worldPos.x - closestX) * (worldPos.x - closestX) + (worldPos.y - closestY) * (worldPos.y - closestY);

            if (dist_sq < min_dist_sq) {
                min_dist_sq = dist_sq;
                closestWire = &wire;
            }
            p1 = p2;
        }
    }
    return closestWire;
}

} // namespace plc