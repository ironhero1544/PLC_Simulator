// src/Application_Components.cpp

#include "Application.h"
#include <iostream>
#include <cmath>
#include <string>
#include <algorithm> // For std::remove_if

namespace plc {

void Application::HandleComponentDragStart(int componentIndex) {
    if (componentIndex >= 0 && componentIndex < 10) {
        m_isDragging = true;
        m_draggedComponentIndex = componentIndex;
        std::cout << "Drag started successfully! isDragging=" << m_isDragging << ", draggedIndex=" << m_draggedComponentIndex << std::endl;
    }
}

void Application::HandleComponentDrag() {
    if (m_isDragging) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        std::cout << "Component " << m_draggedComponentIndex << " is being dragged!" << std::endl;
    }
}

// src/Application_Components.cpp

// 이 함수 전체를 아래 내용으로 교체해 주세요.
void Application::HandleComponentDrop(Position position) {
    std::cout << "DROP: isDrag=" << m_isDragging << " index=" << m_draggedComponentIndex << std::endl;

    if (m_isDragging && m_draggedComponentIndex >= 0) {
        std::cout << "CREATE: type=" << m_draggedComponentIndex << " at (" << position.x << "," << position.y << ")" << std::endl;

        PlacedComponent newComponent;
        newComponent.instanceId = m_nextInstanceId++;
        newComponent.type = static_cast<ComponentType>(m_draggedComponentIndex);
        newComponent.position = position;

        switch (newComponent.type) {
            case ComponentType::PLC: newComponent.size = {320.0f, 180.0f}; break;
            case ComponentType::FRL: newComponent.size = {80.0f, 100.0f}; break;
            case ComponentType::MANIFOLD: newComponent.size = {120.0f, 60.0f}; break;
            case ComponentType::LIMIT_SWITCH: newComponent.size = {60.0f, 60.0f}; break;
            case ComponentType::SENSOR: newComponent.size = {100.0f, 120.0f}; break;
            // ★★★ FIX: 늘어난 이동 반경을 포함하도록 실린더의 전체 너비 증가
            case ComponentType::CYLINDER: newComponent.size = {340.0f, 60.0f}; break;
            case ComponentType::VALVE_SINGLE: newComponent.size = {80.0f, 100.0f}; break;
            case ComponentType::VALVE_DOUBLE: newComponent.size = {100.0f, 100.0f}; break;
            case ComponentType::BUTTON_UNIT: newComponent.size = {200.0f, 100.0f}; break;
            case ComponentType::POWER_SUPPLY: newComponent.size = {100.0f, 80.0f}; break;
            default: newComponent.size = {80.0f, 60.0f}; break;
        }

        newComponent.position.x -= newComponent.size.width / 2.0f;
        newComponent.position.y -= newComponent.size.height / 2.0f;

        m_placedComponents.push_back(newComponent);

        std::cout << "SUCCESS: Total=" << m_placedComponents.size() << std::endl;

        m_isDragging = false;
        m_draggedComponentIndex = -1;
    } else {
        std::cout << "FAIL: Conditions not met" << std::endl;
    }
}

void Application::HandleComponentSelection(int instanceId) {
    // 와이어 선택 해제
    m_selectedWireId = -1;
    m_wireEditMode = WireEditMode::NONE;
    m_editingWireId = -1;
    m_editingPointIndex = -1;

    // [PPT: 수업 내용] for문을 사용하여 모든 컴포넌트의 선택 상태를 업데이트합니다.
    for (auto& comp : m_placedComponents) {
        comp.selected = (comp.instanceId == instanceId);
    }
    m_selectedComponentId = instanceId;
}

void Application::DeleteSelectedComponent() {
    if (m_selectedComponentId >= 0) {
        // 선택된 컴포넌트와 연결된 모든 와이어 삭제
        m_wires.erase(std::remove_if(m_wires.begin(), m_wires.end(), [this](const Wire& wire){
            return wire.fromComponentId == m_selectedComponentId || wire.toComponentId == m_selectedComponentId;
        }), m_wires.end());

        // 컴포넌트 삭제
        for (auto it = m_placedComponents.begin(); it != m_placedComponents.end(); ++it) {
            if (it->instanceId == m_selectedComponentId) {
                m_placedComponents.erase(it);
                m_selectedComponentId = -1;
                break;
            }
        }
    }
}

void Application::HandleComponentMoveStart(int instanceId, ImVec2 mousePos) {
    // [PPT: 수업 내용] for문을 사용하여 이동할 컴포넌트를 찾습니다.
    for (auto& comp : m_placedComponents) {
        if (comp.instanceId == instanceId) {
            m_isMovingComponent = true;
            m_movingComponentId = instanceId;
            m_dragStartOffset.x = mousePos.x - comp.position.x;
            m_dragStartOffset.y = mousePos.y - comp.position.y;
            break;
        }
    }
}

void Application::HandleComponentMoveEnd() {
    m_isMovingComponent = false;
    m_movingComponentId = -1;
}

void Application::RenderPlacedComponents(ImDrawList* draw_list) {
    // [PPT: 수업 내용] for문을 사용하여 배치된 모든 컴포넌트를 렌더링합니다.
    for (const auto& comp : m_placedComponents) {
        ImVec2 screen_pos = WorldToScreen({comp.position.x, comp.position.y});

        // [PPT: 수업 내용] switch문을 사용하여 컴포넌트 타입별로 다른 렌더링을 수행합니다.
        switch (comp.type) {
            case ComponentType::PLC:
                RenderPLCComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::FRL:
                RenderFRLComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::MANIFOLD:
                RenderManifoldComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::LIMIT_SWITCH:
                RenderLimitSwitchComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::SENSOR:
                RenderSensorComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::CYLINDER:
                RenderCylinderComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::VALVE_SINGLE:
                RenderValveSingleComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::VALVE_DOUBLE:
                RenderValveDoubleComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::BUTTON_UNIT:
                RenderButtonUnitComponent(draw_list, comp, screen_pos);
                break;
            case ComponentType::POWER_SUPPLY:
                RenderPowerSupplyComponent(draw_list, comp, screen_pos);
                break;
            default:
                // 기본 렌더링
                ImVec2 end_pos = {screen_pos.x + comp.size.width * m_cameraZoom,
                                  screen_pos.y + comp.size.height * m_cameraZoom};
                draw_list->AddRectFilled(screen_pos, end_pos, IM_COL32(128, 128, 128, 255));
                break;
        }

        // 선택된 컴포넌트 하이라이트
        if (comp.selected) {
            ImVec2 end_pos = {screen_pos.x + comp.size.width * m_cameraZoom,
                              screen_pos.y + comp.size.height * m_cameraZoom};
            draw_list->AddRect(screen_pos, end_pos, IM_COL32(0, 123, 255, 255), 0.0f, 0, 3.0f);
        }
    }
}

void Application::RenderComponentList() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    if (ImGui::BeginChild("ComponentList", ImVec2(0, 0), true)) {
        ImGui::SetCursorPos(ImVec2(15, 15));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::Text("실배선 오브젝트");
        ImGui::PopStyleColor();
        ImGui::SetCursorPosX(15);
        ImGui::Separator();
        ImGui::Spacing();

        const char* components[] = { "FX3U-32M PLC", "에어 서비스 유닛", "에어 메니폴드", "리밋 스위치", "정전 용량형 센서", "복동 공압 실린더", "단측 솔레노이드 밸브", "양측 솔레노이드 밸브", "푸시버튼 유닛", "파워 서플라이" };
        const char* descriptions[] = { "입력 16개, 출력 16개", "공압 공급원", "에어 분배기", "물리적 접촉 센서", "비접촉 센서", "공압 액추에이터", "전자석 1개, 스프링 리턴", "전자석 2개", "입력/표시 장치", "DC 전원 공급" };

        ImGui::SetCursorPosX(10);

        for (int i = 0; i < 10; i++) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

            if (ImGui::BeginChild(("Component" + std::to_string(i)).c_str(), ImVec2(-10, 85), true, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::SetCursorPos(ImVec2(10, 10));

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
                ImGui::TextWrapped("%s", components[i]);
                ImGui::PopStyleColor();

                ImGui::SetCursorPosX(10);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                ImGui::TextWrapped("%s", descriptions[i]);
                ImGui::PopStyleColor();

                ImGui::SetCursorPos(ImVec2(0, 0));
                ImGui::InvisibleButton(("DragButton" + std::to_string(i)).c_str(), ImVec2(-1, -1));

                bool is_hovered = ImGui::IsItemHovered();
                bool is_dragging_this = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);

                if (is_dragging_this && !m_isDragging) {
                    HandleComponentDragStart(i);
                }

                if (is_dragging_this) {
                    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 0, 0, 50), 6.0f);
                    ImGui::SetTooltip("드래그 중... 캔버스에 놓으세요!");
                } else if (is_hovered) {
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 123, 255, 150), 6.0f, 0, 2.0f);
                    ImGui::SetTooltip("드래그하여 캔버스에 배치하세요\n%s", descriptions[i]);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::SetCursorPosX(10);
        }

        if (m_isDragging) {
            HandleComponentDrag();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Application::RenderPLCComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    // PLC 본체
    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(240, 240, 240, 255), 5.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 5.0f * zoom, 0, 2.0f);

    // 입력 단자대
    ImVec2 input_start = {screen_pos.x + 20 * zoom, screen_pos.y + 15 * zoom};
    ImVec2 input_end = {screen_pos.x + 230 * zoom, screen_pos.y + 65 * zoom};
    draw_list->AddRectFilled(input_start, input_end, IM_COL32(232, 232, 232, 255), 3.0f * zoom);
    draw_list->AddRect(input_start, input_end, IM_COL32(100, 100, 100, 255), 3.0f * zoom, 0, 1.0f);

    // 출력 단자대
    ImVec2 output_start = {screen_pos.x + 20 * zoom, screen_pos.y + 115 * zoom};
    ImVec2 output_end = {screen_pos.x + 230 * zoom, screen_pos.y + 165 * zoom};
    draw_list->AddRectFilled(output_start, output_end, IM_COL32(232, 232, 232, 255), 3.0f * zoom);
    draw_list->AddRect(output_start, output_end, IM_COL32(100, 100, 100, 255), 3.0f * zoom, 0, 1.0f);

    // 입력 포트 (X0-X15)
    for (int i = 0; i < 16; i++) {
        ImVec2 port_pos = WorldToScreen({comp.position.x + 30 + (i % 8) * 25, comp.position.y + 25 + (i / 8) * 20});
        draw_list->AddCircleFilled(port_pos, 6 * zoom, IM_COL32(76, 175, 80, 255));
        draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0, 2.0f);
    }

    // 출력 포트 (Y0-Y15)
    for (int i = 0; i < 16; i++) {
        ImVec2 port_pos = WorldToScreen({comp.position.x + 30 + (i % 8) * 25, comp.position.y + 135 + (i / 8) * 20});
        draw_list->AddCircleFilled(port_pos, 6 * zoom, IM_COL32(255, 152, 0, 255));
        draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0, 2.0f);
    }

    // 전원 및 COM 단자대
    ImVec2 power_block_start = {screen_pos.x + 245 * zoom, screen_pos.y + 15 * zoom};
    ImVec2 power_block_end = {screen_pos.x + 310 * zoom, screen_pos.y + 140 * zoom};
    draw_list->AddRectFilled(power_block_start, power_block_end, IM_COL32(232, 232, 232, 255), 3.0f * zoom);
    draw_list->AddRect(power_block_start, power_block_end, IM_COL32(100, 100, 100, 255), 3.0f * zoom, 0, 1.0f);

    // 전원 및 COM 포트들
    const char* labels[] = {"24V", "0V", "COM0", "COM1"};
    ImU32 colors[] = {IM_COL32(255, 0, 0, 255), IM_COL32(0, 0, 0, 255), IM_COL32(128, 128, 128, 255), IM_COL32(128, 128, 128, 255)};
    for (int i = 0; i < 4; ++i) {
        ImVec2 port_pos = WorldToScreen({comp.position.x + 265, comp.position.y + 40 + i * 25});
        draw_list->AddCircleFilled(port_pos, 6 * zoom, colors[i]);
        draw_list->AddCircle(port_pos, 6 * zoom, IM_COL32(50, 50, 50, 255), 0, 2.0f);
        if (zoom > 0.4f) {
            draw_list->AddText(ImVec2(port_pos.x + 10 * zoom, port_pos.y - 8 * zoom), IM_COL32(50, 50, 50, 255), labels[i]);
        }
    }

    // PLC 상태 LED
    ImVec2 status_led_area_start = {screen_pos.x + 250 * zoom, screen_pos.y + 145 * zoom};
    ImVec2 status_led_area_end = {screen_pos.x + 300 * zoom, screen_pos.y + 165 * zoom};
    draw_list->AddRectFilled(status_led_area_start, status_led_area_end, IM_COL32(44, 62, 80, 255), 3.0f * zoom);

    bool is_running = comp.internalStates.count("is_running") && comp.internalStates.at("is_running") > 0.5f;
    ImVec2 led_pos = {screen_pos.x + 275 * zoom, screen_pos.y + 155 * zoom};
    draw_list->AddCircleFilled(led_pos, 5 * zoom, is_running ? IM_COL32(0, 255, 0, 255) : IM_COL32(100, 100, 100, 255));

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 5 * zoom),
                          IM_COL32(50, 50, 50, 255), "FX3U-32M");
    }
}
void Application::RenderFRLComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    // 메인 본체 - 연한 파란색
    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(232, 248, 253, 255), 5.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 5.0f * zoom, 0, 2.0f);

    // 출력 포트 (하단 중앙)
    ImVec2 output_port = {screen_pos.x + 40 * zoom, screen_pos.y + 90 * zoom};
    draw_list->AddCircleFilled(output_port, 6 * zoom, IM_COL32(33, 150, 243, 255));
    draw_list->AddCircle(output_port, 6 * zoom, IM_COL32(50, 50, 50, 255), 0, 2.0f);

    // ★★★ FIX: 압력 조절 핸들 렌더링 추가 ★★★
    float pressure = comp.internalStates.count("air_pressure") ? comp.internalStates.at("air_pressure") : 6.0f;
    float angle = (pressure / 10.0f) * 2.0f * 3.14159265f; // 0~10 bar를 0~360도로 매핑

    ImVec2 handle_center = {screen_pos.x + 40 * zoom, screen_pos.y + 25 * zoom};
    float handle_radius = 15 * zoom;
    draw_list->AddCircleFilled(handle_center, handle_radius, IM_COL32(80, 80, 80, 255));
    draw_list->AddCircle(handle_center, handle_radius, IM_COL32(50, 50, 50, 255), 12, 1.5f * zoom);
    // 핸들 위 표시선
    ImVec2 line_end = {
        static_cast<float>(handle_center.x + (handle_radius - 2 * zoom) * cos(angle - 1.57f)),
        static_cast<float>(handle_center.y + (handle_radius - 2 * zoom) * sin(angle - 1.57f))
    };
    draw_list->AddLine(handle_center, line_end, IM_COL32(255, 255, 255, 200), 2.0f * zoom);

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 50 * zoom),
                          IM_COL32(50, 50, 50, 255), "FRL UNIT");
        // 압력 텍스트 표시
        char pressure_text[16];
        sprintf(pressure_text, "%.1f bar", pressure);
        draw_list->AddText(ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 65 * zoom),
                          IM_COL32(50, 50, 50, 255), pressure_text);
    }
}
void Application::RenderManifoldComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(180, 180, 180, 255), 3.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 3.0f * zoom, 0, 2.0f);

    ImVec2 input_port = {screen_pos.x + 10 * zoom, screen_pos.y + 30 * zoom};
    draw_list->AddCircleFilled(input_port, 6 * zoom, IM_COL32(33, 150, 243, 255));

    for (int i = 0; i < 4; i++) {
        ImVec2 output_port = {screen_pos.x + (30 + i * 20) * zoom, screen_pos.y + 50 * zoom};
        draw_list->AddCircleFilled(output_port, 5 * zoom, IM_COL32(33, 150, 243, 255));
    }

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 25 * zoom, screen_pos.y + 15 * zoom),
                          IM_COL32(50, 50, 50, 255), "MANIFOLD");
    }
}

// src/Application_Components.cpp

// 이 함수 전체를 아래 내용으로 교체해 주세요.
void Application::RenderLimitSwitchComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    // 메인 본체 - 어두운 회색
    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 3.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(80, 80, 80, 255), 3.0f * zoom, 0, 2.0f);

    // ★★★ FIX: 물리 엔진 상태를 읽어 시각적 피드백 구현 ★★★
    bool is_pressed = comp.internalStates.count("is_pressed") && comp.internalStates.at("is_pressed") > 0.5f;
    float actuator_y_offset = is_pressed ? 5.0f * zoom : -5.0f * zoom;
    ImU32 wheel_color = is_pressed ? IM_COL32(255, 220, 50, 255) : IM_COL32(255, 193, 7, 255);

    // 액추에이터 (눌림 상태에 따라 Y 위치 변경)
    ImVec2 actuator_base_pos = {screen_pos.x + 30 * zoom, screen_pos.y};
    draw_list->AddRectFilled(ImVec2(actuator_base_pos.x - 5 * zoom, actuator_base_pos.y + actuator_y_offset),
                           ImVec2(actuator_base_pos.x + 5 * zoom, actuator_base_pos.y + 15 * zoom + actuator_y_offset),
                           IM_COL32(150, 150, 150, 255));
    draw_list->AddCircleFilled(ImVec2(actuator_base_pos.x, actuator_base_pos.y + actuator_y_offset), 5 * zoom, wheel_color);

    // 접점 포트들 (하단)
    ImVec2 p_com = {screen_pos.x + 15 * zoom, screen_pos.y + 55 * zoom};
    ImVec2 p_no = {screen_pos.x + 30 * zoom, screen_pos.y + 55 * zoom};
    ImVec2 p_nc = {screen_pos.x + 45 * zoom, screen_pos.y + 55 * zoom};

    draw_list->AddCircleFilled(p_com, 3 * zoom, IM_COL32(158, 158, 158, 255));
    draw_list->AddCircleFilled(p_no, 3 * zoom, IM_COL32(158, 158, 158, 255));
    draw_list->AddCircleFilled(p_nc, 3 * zoom, IM_COL32(158, 158, 158, 255));

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 25 * zoom),
                          IM_COL32(255, 255, 255, 255), "LIMIT");
    }
}

void Application::RenderSensorComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    ImVec2 body_start = {screen_pos.x + 15 * zoom, screen_pos.y + 6 * zoom};
    ImVec2 body_end = {screen_pos.x + 85 * zoom, screen_pos.y + 107 * zoom};
    draw_list->AddRectFilled(body_start, body_end, IM_COL32(192, 192, 192, 255), 12.0f * zoom);
    draw_list->AddRect(body_start, body_end, IM_COL32(51, 51, 51, 255), 12.0f * zoom, 0, 2.0f);

    ImVec2 head_start = {screen_pos.x + 25 * zoom, screen_pos.y + 2 * zoom};
    ImVec2 head_end = {screen_pos.x + 75 * zoom, screen_pos.y + 15 * zoom};
    draw_list->AddRectFilled(head_start, head_end, IM_COL32(25, 118, 210, 255), 3.0f * zoom);
    draw_list->AddRect(head_start, head_end, IM_COL32(51, 51, 51, 255), 3.0f * zoom, 0, 2.0f);

    // ★★★ FIX: 전원 상태와 감지 상태를 모두 반영하여 LED 색상 결정 ★★★
    bool is_powered = comp.internalStates.count("is_powered") && comp.internalStates.at("is_powered") > 0.5f;
    bool is_detected = comp.internalStates.count("is_detected") && comp.internalStates.at("is_detected") > 0.5f;

    ImU32 led_color;
    if (is_powered) {
        led_color = is_detected ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255); // 전원 ON: 감지(초록), 비감지(빨강)
    } else {
        led_color = IM_COL32(80, 80, 80, 255); // 전원 OFF: 꺼진 상태(회색)
    }

    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 50 * zoom, screen_pos.y + 19 * zoom), 4 * zoom, led_color);
    draw_list->AddCircle(ImVec2(screen_pos.x + 50 * zoom, screen_pos.y + 19 * zoom), 4 * zoom, IM_COL32(51, 51, 51, 255), 0, 1.0f);

    ImVec2 p1 = {screen_pos.x + 30 * zoom, screen_pos.y + 110 * zoom};
    ImVec2 p2 = {screen_pos.x + 50 * zoom, screen_pos.y + 110 * zoom};
    ImVec2 p3 = {screen_pos.x + 70 * zoom, screen_pos.y + 110 * zoom};

    draw_list->AddCircleFilled(p1, 4 * zoom, IM_COL32(255, 0, 0, 255));
    draw_list->AddCircleFilled(p2, 4 * zoom, IM_COL32(0, 0, 0, 255));
    draw_list->AddCircleFilled(p3, 4 * zoom, IM_COL32(128, 128, 128, 255));
}
void Application::RenderCylinderComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    // 1. 물리 상태 읽기
    float position = comp.internalStates.count("position") ? comp.internalStates.at("position") : 0.0f;
    float status = comp.internalStates.count("status") ? comp.internalStates.at("status") : 0.0f;
    float pressure_a = comp.internalStates.count("pressure_a") ? comp.internalStates.at("pressure_a") : 0.0f;
    float pressure_b = comp.internalStates.count("pressure_b") ? comp.internalStates.at("pressure_b") : 0.0f;

    // 2. 실린더 몸체(Casing) 렌더링
    ImVec2 body_start = {screen_pos.x + 170 * zoom, screen_pos.y + 10 * zoom};
    ImVec2 body_end = {screen_pos.x + 310 * zoom, screen_pos.y + 50 * zoom};
    draw_list->AddRectFilled(body_start, body_end, IM_COL32(210, 210, 210, 255), 4.0f * zoom);
    draw_list->AddRect(body_start, body_end, IM_COL32(100, 100, 100, 255), 4.0f * zoom, 0, 2.0f);

    draw_list->AddRectFilled(ImVec2(body_start.x, body_start.y), ImVec2(body_start.x + 15 * zoom, body_end.y), IM_COL32(180, 180, 180, 255), 4.0f * zoom, ImDrawFlags_RoundCornersLeft);
    draw_list->AddRectFilled(ImVec2(body_end.x - 15 * zoom, body_start.y), ImVec2(body_end.x, body_end.y), IM_COL32(180, 180, 180, 255), 4.0f * zoom, ImDrawFlags_RoundCornersRight);

    // 3. 피스톤 로드(Rod) 렌더링
    float rod_extended_length = position;
    ImVec2 rod_base_pos = {body_start.x, screen_pos.y + 25 * zoom};
    ImVec2 rod_tip_pos = {rod_base_pos.x - rod_extended_length * zoom, rod_base_pos.y};

    draw_list->AddRectFilled(ImVec2(rod_tip_pos.x - 15 * zoom, rod_tip_pos.y - 5 * zoom), ImVec2(rod_tip_pos.x, rod_tip_pos.y + 5 * zoom), IM_COL32(150, 150, 150, 255));
    // ★★★ FIX: 로드 두께 증가 (3.0f -> 5.0f)
    draw_list->AddLine(rod_tip_pos, ImVec2(body_start.x + 125 * zoom, rod_base_pos.y), IM_COL32(120, 120, 120, 255), 5.0f * zoom);

    // 4. 움직임 상태 시각화 (화살표)
    if (std::abs(status) > 0.1f && zoom > 0.4f) {
        ImVec2 arrow_center = { screen_pos.x + 240 * zoom, screen_pos.y + 5 * zoom };
        ImU32 arrow_color = (status > 0) ? IM_COL32(0, 200, 0, 200) : IM_COL32(255, 0, 0, 200);
        float dir = (status > 0) ? -1.0f : 1.0f;

        draw_list->AddLine(ImVec2(arrow_center.x - 10 * dir * zoom, arrow_center.y), ImVec2(arrow_center.x + 10 * dir * zoom, arrow_center.y), arrow_color, 3.0f * zoom);
        draw_list->AddTriangleFilled(ImVec2(arrow_center.x + 10 * dir * zoom, arrow_center.y - 5 * zoom), ImVec2(arrow_center.x + 10 * dir * zoom, arrow_center.y + 5 * zoom), ImVec2(arrow_center.x + 18 * dir * zoom, arrow_center.y), arrow_color);
    }

    // 5. 공압 포트
    ImVec2 port_a_pos = {body_start.x + 5 * zoom, screen_pos.y + 40 * zoom};
    ImVec2 port_b_pos = {body_end.x - 5 * zoom, screen_pos.y + 40 * zoom};

    ImU32 port_a_color = (pressure_a > 1.0f) ? IM_COL32(0, 180, 255, 255) : IM_COL32(33, 150, 243, 128);
    ImU32 port_b_color = (pressure_b > 1.0f) ? IM_COL32(0, 180, 255, 255) : IM_COL32(33, 150, 243, 128);

    draw_list->AddCircleFilled(port_a_pos, 5 * zoom, port_a_color);
    draw_list->AddCircle(port_a_pos, 5 * zoom, IM_COL32(50, 50, 50, 255), 12, 1.5f * zoom);
    draw_list->AddCircleFilled(port_b_pos, 5 * zoom, port_b_color);
    draw_list->AddCircle(port_b_pos, 5 * zoom, IM_COL32(50, 50, 50, 255), 12, 1.5f * zoom);

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(port_a_pos.x - 5 * zoom, port_a_pos.y - 20 * zoom), IM_COL32(50, 50, 50, 255), "A");
        draw_list->AddText(ImVec2(port_b_pos.x - 5 * zoom, port_b_pos.y - 20 * zoom), IM_COL32(50, 50, 50, 255), "B");
    }

    if(comp.selected && zoom > 0.3f) {
        // ★★★ FIX: 시각적 감지 범위 증가 (35.0f -> 50.0f)
        float detection_range_pixels = 50.0f * zoom;
        draw_list->AddCircle(rod_tip_pos, detection_range_pixels, IM_COL32(255, 165, 0, 100), 32, 2.0f);
        draw_list->AddCircleFilled(rod_tip_pos, 4.0f, IM_COL32(255, 165, 0, 200));
    }
}
void Application::RenderValveSingleComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(224, 224, 224, 255), 5.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 5.0f * zoom, 0, 2.0f);

    ImVec2 coil_start = {screen_pos.x + 5 * zoom, screen_pos.y + 10 * zoom};
    ImVec2 coil_end = {screen_pos.x + 25 * zoom, screen_pos.y + 40 * zoom};
    draw_list->AddRectFilled(coil_start, coil_end, IM_COL32(200, 200, 200, 255), 3.0f * zoom);
    draw_list->AddRect(coil_start, coil_end, IM_COL32(100, 100, 100, 255), 3.0f * zoom, 0, 1.0f);

    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 15 * zoom), 5 * zoom, IM_COL32(244, 67, 54, 255));
    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 30 * zoom), 5 * zoom, IM_COL32(0, 0, 0, 255));

    ImVec2 port_p = {screen_pos.x + 40 * zoom, screen_pos.y + 90 * zoom};
    ImVec2 port_a = {screen_pos.x + 25 * zoom, screen_pos.y + 55 * zoom};
    ImVec2 port_b = {screen_pos.x + 55 * zoom, screen_pos.y + 55 * zoom};

    draw_list->AddCircleFilled(port_p, 6 * zoom, IM_COL32(33, 150, 243, 255));
    draw_list->AddCircleFilled(port_a, 6 * zoom, IM_COL32(33, 150, 243, 255));
    draw_list->AddCircleFilled(port_b, 6 * zoom, IM_COL32(33, 150, 243, 255));

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 40 * zoom),
                          IM_COL32(50, 50, 50, 255), "3/2WAY");
    }
}

void Application::RenderValveDoubleComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(224, 224, 224, 255), 5.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 5.0f * zoom, 0, 2.0f);

    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 15 * zoom), 5 * zoom, IM_COL32(244, 67, 54, 255));
    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 15 * zoom, screen_pos.y + 30 * zoom), 5 * zoom, IM_COL32(0, 0, 0, 255));

    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 85 * zoom, screen_pos.y + 15 * zoom), 5 * zoom, IM_COL32(244, 67, 54, 255));
    draw_list->AddCircleFilled(ImVec2(screen_pos.x + 85 * zoom, screen_pos.y + 30 * zoom), 5 * zoom, IM_COL32(0, 0, 0, 255));

    ImVec2 port_p = {screen_pos.x + 50 * zoom, screen_pos.y + 90 * zoom};
    ImVec2 port_a = {screen_pos.x + 25 * zoom, screen_pos.y + 55 * zoom};
    ImVec2 port_b = {screen_pos.x + 75 * zoom, screen_pos.y + 55 * zoom};

    draw_list->AddCircleFilled(port_p, 6 * zoom, IM_COL32(33, 150, 243, 255));
    draw_list->AddCircleFilled(port_a, 6 * zoom, IM_COL32(33, 150, 243, 255));
    draw_list->AddCircleFilled(port_b, 6 * zoom, IM_COL32(33, 150, 243, 255));

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 20 * zoom, screen_pos.y + 40 * zoom),
                          IM_COL32(50, 50, 50, 255), "3/2WAY");
    }
}

void Application::RenderButtonUnitComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(240, 240, 240, 255), 5.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 5.0f * zoom, 0, 2.0f);

    ImU32 colors_off[] = {IM_COL32(180, 40, 30, 255), IM_COL32(190, 170, 40, 255), IM_COL32(50, 130, 50, 255)};
    ImU32 colors_on[] = {IM_COL32(255, 80, 70, 255), IM_COL32(255, 235, 60, 255), IM_COL32(90, 220, 90, 255)};

    for (int btn = 0; btn < 3; btn++) {
        float y_offset = screen_pos.y + (20 + btn * 30) * zoom;
        ImVec2 button_pos = ImVec2(screen_pos.x + 25 * zoom, y_offset);

        bool lamp_on = comp.internalStates.count("lamp_on_" + std::to_string(btn)) &&
                      comp.internalStates.at("lamp_on_" + std::to_string(btn)) > 0.5f;
        bool is_pressed = comp.internalStates.count("is_pressed_" + std::to_string(btn)) &&
                         comp.internalStates.at("is_pressed_" + std::to_string(btn)) > 0.5f;

        if(is_pressed) {
            draw_list->AddCircleFilled(ImVec2(button_pos.x + 2 * zoom, button_pos.y + 2 * zoom),
                                     10 * zoom, IM_COL32(0, 0, 0, 50));
        }

        draw_list->AddCircleFilled(button_pos, 8 * zoom, lamp_on ? colors_on[btn] : colors_off[btn]);
        draw_list->AddCircle(button_pos, 8 * zoom, IM_COL32(50, 50, 50, 255), 0, 2.0f);

        ImVec2 port_com = ImVec2(screen_pos.x + 80 * zoom, y_offset);
        ImVec2 port_no = ImVec2(screen_pos.x + 100 * zoom, y_offset);
        ImVec2 port_nc = ImVec2(screen_pos.x + 120 * zoom, y_offset);
        ImVec2 port_led_plus = ImVec2(screen_pos.x + 155 * zoom, y_offset);
        ImVec2 port_led_minus = ImVec2(screen_pos.x + 175 * zoom, y_offset);

        draw_list->AddCircleFilled(port_com, 5 * zoom, IM_COL32(128, 128, 128, 255));
        draw_list->AddCircleFilled(port_no, 5 * zoom, IM_COL32(128, 128, 128, 255));
        draw_list->AddCircleFilled(port_nc, 5 * zoom, IM_COL32(128, 128, 128, 255));
        draw_list->AddCircleFilled(port_led_plus, 5 * zoom, IM_COL32(244, 67, 54, 255));
        draw_list->AddCircleFilled(port_led_minus, 5 * zoom, IM_COL32(0, 0, 0, 255));
    }

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 5 * zoom, screen_pos.y + 5 * zoom),
                          IM_COL32(50, 50, 50, 255), "BUTTON UNIT");
    }
}

void Application::RenderPowerSupplyComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos) {
    float zoom = m_cameraZoom;

    ImVec2 body_end = {screen_pos.x + comp.size.width * zoom, screen_pos.y + comp.size.height * zoom};
    draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(44, 62, 80, 255), 5.0f * zoom);
    draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255), 5.0f * zoom, 0, 2.0f);

    ImVec2 plus_port = {screen_pos.x + 85 * zoom, screen_pos.y + 20 * zoom};
    ImVec2 minus_port = {screen_pos.x + 85 * zoom, screen_pos.y + 40 * zoom};

    draw_list->AddCircleFilled(plus_port, 6 * zoom, IM_COL32(244, 67, 54, 255));
    draw_list->AddCircleFilled(minus_port, 6 * zoom, IM_COL32(0, 0, 0, 255));

    if (zoom > 0.5f) {
        draw_list->AddText(ImVec2(screen_pos.x + 10 * zoom, screen_pos.y + 25 * zoom), 
                          IM_COL32(255, 255, 255, 255), "24V DC");
    }
}

} // namespace plc