// src/Application_Ports.cpp

#include "Application.h"
#include <iostream>
#include <cmath> // ★★★ FIX: sqrt, pow 사용을 위해 <cmath> 헤더 추가
#include <string>

namespace plc {


// [CRITICAL FIX] 이 함수 전체를 아래의 새 코드로 완전히 교체하십시오.
Position GetPortRelativePositionById(const PlacedComponent& comp, int portId) {
    switch (comp.type) {
        case ComponentType::PLC:
            // 입력 포트 (X0-X15, ID: 0-15)
            if (portId >= 0 && portId < 16) {
                return {30.0f + (portId % 8) * 25.0f, 25.0f + (portId / 8) * 20.0f};
            }
            // 출력 포트 (Y0-Y15, ID: 16-31)
            else if (portId >= 16 && portId < 32) {
                int relativeId = portId - 16;
                return {30.0f + (relativeId % 8) * 25.0f, 135.0f + (relativeId / 8) * 20.0f};
            }
            // 전원 및 COM 포트 (ID: 32-35) - 시뮬레이션에서는 숨김 처리
            /*
            else if (portId >= 32 && portId < 36) {
                // ID: 32(24V), 33(0V), 34(COM0), 35(COM1)
                return {265.0f, 40.0f + (portId - 32) * 25.0f};
            }
            */
            break;
        case ComponentType::FRL:
            if (portId == 0) return {40.0f, 90.0f};
            break;
        case ComponentType::MANIFOLD:
            if (portId == 0) return {10.0f, 30.0f};
            if (portId >= 1 && portId <= 4) return {30.0f + (portId - 1) * 20.0f, 50.0f};
            break;
        case ComponentType::LIMIT_SWITCH: {
            Position relPos[] = {{15, 55}, {30, 55}, {45, 55}};
            if (portId >= 0 && portId < 3) return relPos[portId];
            break;
        }
        case ComponentType::SENSOR: {
            Position relPos[] = {{30, 110}, {50, 110}, {70, 110}};
            if (portId >= 0 && portId < 3) return relPos[portId];
            break;
        }
        case ComponentType::CYLINDER:
            if (portId == 0) return {175.0f, 40.0f};
            if (portId == 1) return {305.0f, 40.0f};
            break;
        case ComponentType::VALVE_SINGLE: {
            Position relPos[] = {{15, 15}, {15, 30}, {40, 90}, {25, 55}, {55, 55}};
            if (portId >= 0 && portId < 5) return relPos[portId];
            break;
        }
        case ComponentType::VALVE_DOUBLE: {
            Position relPos[] = {{15, 15}, {15, 30}, {85, 15}, {85, 30}, {50, 90}, {25, 55}, {75, 55}};
            if (portId >= 0 && portId < 7) return relPos[portId];
            break;
        }
        case ComponentType::BUTTON_UNIT: {
            int btn_index = portId / 5;
            int port_in_btn = portId % 5;
            float y_pos = 20.0f + btn_index * 30.0f;
            float x_pos_table[] = {80.0f, 100.0f, 120.0f, 155.0f, 175.0f};
            if (btn_index < 3 && port_in_btn < 5) {
                return {x_pos_table[port_in_btn], y_pos};
            }
            break;
        }
        case ComponentType::POWER_SUPPLY:
            if (portId == 0) return {85.0f, 20.0f};
            if (portId == 1) return {85.0f, 40.0f};
            break;
    }
    // Return a default position if no match is found
    return {0.0f, 0.0f};
}
bool Application::IsValidWireConnection(const Port& fromPort, const Port& toPort) {
    if (m_currentTool == ToolType::ELECTRIC && fromPort.type != PortType::ELECTRIC) return false;
    if (m_currentTool == ToolType::PNEUMATIC && fromPort.type != PortType::PNEUMATIC) return false;
    if (fromPort.type != toPort.type) return false;
    return true;
}
// src/Application_Ports.cpp

// [CRITICAL FIX] 이 함수 전체를 아래의 코드로 완전히 교체하십시오.
Port* Application::FindPortAtPosition(ImVec2 worldPos, int& outComponentId) {
    const float PORT_RADIUS = 12.0f;

    for (const auto& component : m_placedComponents) {
        // 컴포넌트 바운딩 박스 빠른 검사
        if (worldPos.x < component.position.x - PORT_RADIUS || worldPos.x > component.position.x + component.size.width + PORT_RADIUS ||
            worldPos.y < component.position.y - PORT_RADIUS || worldPos.y > component.position.y + component.size.height + PORT_RADIUS) {
            continue;
        }

        outComponentId = component.instanceId;

        switch (component.type) {
            case ComponentType::PLC: {
                // 입력 포트 (X0-X15, ID: 0-15)
                for (int i = 0; i < 16; i++) {
                    Position relPos = {30.0f + (i % 8) * 25, 25.0f + (i / 8) * 20};
                    ImVec2 port_world = ImVec2(component.position.x + relPos.x, component.position.y + relPos.y);
                    if (sqrt(pow(worldPos.x - port_world.x, 2) + pow(worldPos.y - port_world.y, 2)) <= PORT_RADIUS) {
                        std::string role = "X" + std::to_string(i);
                        m_tempPortBuffer = {i, relPos, {0.3f, 0.7f, 0.3f, 1.0f}, true, PortType::ELECTRIC, role};
                        return &m_tempPortBuffer;
                    }
                }
                // 출력 포트 (Y0-Y15, ID: 16-31)
                for (int i = 0; i < 16; i++) {
                    Position relPos = {30.0f + (i % 8) * 25, 135.0f + (i / 8) * 20};
                    ImVec2 port_world = ImVec2(component.position.x + relPos.x, component.position.y + relPos.y);
                    if (sqrt(pow(worldPos.x - port_world.x, 2) + pow(worldPos.y - port_world.y, 2)) <= PORT_RADIUS) {
                        std::string role = "Y" + std::to_string(i);
                        int portId = i + 16;
                        m_tempPortBuffer = {portId, relPos, {1.0f, 0.6f, 0.0f, 1.0f}, false, PortType::ELECTRIC, role};
                        return &m_tempPortBuffer;
                    }
                }
                // 전원 및 COM 포트 (ID: 32-35) - 시뮬레이션에서는 숨김 처리
                /*
                for (int i = 0; i < 4; ++i) {
                    int portId = 32 + i;
                    Position relPos = {265.0f, 40.0f + i * 25.0f};
                    ImVec2 port_world = ImVec2(component.position.x + relPos.x, component.position.y + relPos.y);
                    if (sqrt(pow(worldPos.x - port_world.x, 2) + pow(worldPos.y - port_world.y, 2)) <= PORT_RADIUS) {
                        std::string roles[] = {"PLC_24V_IN", "PLC_0V_IN", "COM0 (Y0-7)", "COM1 (Y8-15)"};
                        m_tempPortBuffer = {portId, relPos, {0.6f, 0.6f, 0.6f, 1.0f}, true, PortType::ELECTRIC, roles[i]};
                        return &m_tempPortBuffer;
                    }
                }
                */
                break;
            }
            case ComponentType::FRL: {
                Position relPos = {40, 90};
                ImVec2 port_world = ImVec2(component.position.x + relPos.x, component.position.y + relPos.y);
                if (sqrt(pow(worldPos.x - port_world.x, 2) + pow(worldPos.y - port_world.y, 2)) <= PORT_RADIUS) {
                    m_tempPortBuffer = {0, relPos, {0.13f, 0.59f, 0.95f, 1.0f}, false, PortType::PNEUMATIC, "PNEUMATIC_OUT"};
                    return &m_tempPortBuffer;
                }
                break;
            }
            case ComponentType::MANIFOLD: {
                Position relPosInput = {10, 30};
                ImVec2 input_port = ImVec2(component.position.x + relPosInput.x, component.position.y + relPosInput.y);
                if (sqrt(pow(worldPos.x - input_port.x, 2) + pow(worldPos.y - input_port.y, 2)) <= PORT_RADIUS) {
                    m_tempPortBuffer = {0, relPosInput, {0.13f, 0.59f, 0.95f, 1.0f}, true, PortType::PNEUMATIC, "PNEUMATIC_IN"};
                    return &m_tempPortBuffer;
                }
                for (int i = 0; i < 4; i++) {
                    Position relPosOutput = {30.0f + i * 20, 50.0f};
                    ImVec2 output_port = ImVec2(component.position.x + relPosOutput.x, component.position.y + relPosOutput.y);
                    if (sqrt(pow(worldPos.x - output_port.x, 2) + pow(worldPos.y - output_port.y, 2)) <= PORT_RADIUS) {
                        m_tempPortBuffer = {i + 1, relPosOutput, {0.13f, 0.59f, 0.95f, 1.0f}, false, PortType::PNEUMATIC, "PNEUMATIC_OUT"};
                        return &m_tempPortBuffer;
                    }
                }
                break;
            }
            case ComponentType::LIMIT_SWITCH: {
                Position relPos[] = {{15, 55}, {30, 55}, {45, 55}};
                std::string roles[] = {"COM", "NO", "NC"};
                for(int i=0; i<3; ++i) {
                    ImVec2 p_world = {component.position.x + relPos[i].x, component.position.y + relPos[i].y};
                    if (sqrt(pow(worldPos.x - p_world.x, 2) + pow(worldPos.y - p_world.y, 2)) <= PORT_RADIUS) {
                         m_tempPortBuffer = {i, relPos[i], {0.6f, 0.6f, 0.6f, 1.0f}, true, PortType::ELECTRIC, roles[i]};
                         return &m_tempPortBuffer;
                    }
                }
                break;
            }
            case ComponentType::SENSOR: {
                Position relPos[] = {{30, 110}, {50, 110}, {70, 110}};
                Color colors[] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.6f, 0.6f, 0.6f, 1.0f}};
                std::string roles[] = {"POWER_24V", "POWER_0V", "SIGNAL"};
                for(int i=0; i<3; ++i) {
                    ImVec2 p_world = {component.position.x + relPos[i].x, component.position.y + relPos[i].y};
                    if (sqrt(pow(worldPos.x - p_world.x, 2) + pow(worldPos.y - p_world.y, 2)) <= PORT_RADIUS) {
                         m_tempPortBuffer = {i, relPos[i], colors[i], true, PortType::ELECTRIC, roles[i]};
                         return &m_tempPortBuffer;
                    }
                }
                break;
            }
            case ComponentType::CYLINDER: {
                Position relPosA = {175, 40};
                ImVec2 port_a = ImVec2(component.position.x + relPosA.x, component.position.y + relPosA.y);
                if (sqrt(pow(worldPos.x - port_a.x, 2) + pow(worldPos.y - port_a.y, 2)) <= PORT_RADIUS) {
                    m_tempPortBuffer = {0, relPosA, {0.13f, 0.59f, 0.95f, 1.0f}, true, PortType::PNEUMATIC, "PNEUMATIC_IN_A"};
                    return &m_tempPortBuffer;
                }
                Position relPosB = {305, 40};
                ImVec2 port_b = ImVec2(component.position.x + relPosB.x, component.position.y + relPosB.y);
                if (sqrt(pow(worldPos.x - port_b.x, 2) + pow(worldPos.y - port_b.y, 2)) <= PORT_RADIUS) {
                    m_tempPortBuffer = {1, relPosB, {0.13f, 0.59f, 0.95f, 1.0f}, true, PortType::PNEUMATIC, "PNEUMATIC_IN_B"};
                    return &m_tempPortBuffer;
                }
                break;
            }
            case ComponentType::VALVE_SINGLE: {
                Position relPos[] = {{15, 15}, {15, 30}, {40, 90}, {25, 55}, {55, 55}};
                Color colors[] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.13f, 0.59f, 0.95f, 1.0f}, {0.13f, 0.59f, 0.95f, 1.0f}, {0.13f, 0.59f, 0.95f, 1.0f}};
                PortType types[] = {PortType::ELECTRIC, PortType::ELECTRIC, PortType::PNEUMATIC, PortType::PNEUMATIC, PortType::PNEUMATIC};
                std::string roles[] = {"SOL_A_24V", "SOL_A_0V", "P_IN", "PORT_A_OUT", "PORT_B_OUT"};
                bool isInput[] = {true, true, true, false, false};
                for(int i=0; i<5; ++i) {
                    ImVec2 p_world = {component.position.x + relPos[i].x, component.position.y + relPos[i].y};
                    if (sqrt(pow(worldPos.x - p_world.x, 2) + pow(worldPos.y - p_world.y, 2)) <= PORT_RADIUS) {
                         m_tempPortBuffer = {i, relPos[i], colors[i], isInput[i], types[i], roles[i]}; return &m_tempPortBuffer;
                    }
                }
                break;
            }
             case ComponentType::VALVE_DOUBLE: {
                Position relPos[] = { {15, 15}, {15, 30}, {85, 15}, {85, 30}, {50, 90}, {25, 55}, {75, 55} };
                Color black = {0.0f, 0.0f, 0.0f, 1.0f}, red = {1.0f, 0.0f, 0.0f, 1.0f}, blue = {0.13f, 0.59f, 0.95f, 1.0f};
                Color colors[] = {red, black, red, black, blue, blue, blue};
                PortType types[] = {PortType::ELECTRIC, PortType::ELECTRIC, PortType::ELECTRIC, PortType::ELECTRIC, PortType::PNEUMATIC, PortType::PNEUMATIC, PortType::PNEUMATIC};
                std::string roles[] = {"SOL_A_24V", "SOL_A_0V", "SOL_B_24V", "SOL_B_0V", "P_IN", "PORT_A_OUT", "PORT_B_OUT"};
                bool isInput[] = {true, true, true, true, true, false, false};
                for(int i=0; i<7; ++i) {
                    ImVec2 p_world = {component.position.x + relPos[i].x, component.position.y + relPos[i].y};
                    if (sqrt(pow(worldPos.x - p_world.x, 2) + pow(worldPos.y - p_world.y, 2)) <= PORT_RADIUS) {
                         m_tempPortBuffer = {i, relPos[i], colors[i], isInput[i], types[i], roles[i]}; return &m_tempPortBuffer;
                    }
                }
                break;
            }
            case ComponentType::BUTTON_UNIT: {
                for (int btn = 0; btn < 3; btn++) {
                    Position relPos[] = {{80, 20.0f + btn * 30}, {100, 20.0f + btn * 30}, {120, 20.0f + btn * 30}, {155, 20.0f + btn*30}, {175, 20.0f + btn*30}};
                    std::string roles[] = {"COM", "NO", "NC", "LAMP_24V", "LAMP_0V"};
                    for (int i = 0; i < 5; i++) {
                        ImVec2 port_pos = ImVec2(component.position.x + relPos[i].x, component.position.y + relPos[i].y);
                        if (sqrt(pow(worldPos.x - port_pos.x, 2) + pow(worldPos.y - port_pos.y, 2)) <= PORT_RADIUS) {
                            int port_id = btn * 5 + i;
                            Color port_color = {0.6f, 0.6f, 0.6f, 1.0f};
                            if (i == 3) port_color = {1.0f, 0.0f, 0.0f, 1.0f};
                            if (i == 4) port_color = {0.0f, 0.0f, 0.0f, 1.0f};
                            m_tempPortBuffer = {port_id, relPos[i], port_color, true, PortType::ELECTRIC, roles[i]};
                            return &m_tempPortBuffer;
                        }
                    }
                }
                break;
            }
            case ComponentType::POWER_SUPPLY: {
                Position relPosPlus = {85, 20};
                ImVec2 p_plus = {component.position.x + relPosPlus.x, component.position.y + relPosPlus.y};
                if (sqrt(pow(worldPos.x - p_plus.x, 2) + pow(worldPos.y - p_plus.y, 2)) <= PORT_RADIUS) {
                    m_tempPortBuffer = {0, relPosPlus, {1.0f, 0.0f, 0.0f, 1.0f}, false, PortType::ELECTRIC, "POWER_24V"};
                    return &m_tempPortBuffer;
                }
                Position relPosMinus = {85, 40};
                ImVec2 p_minus = {component.position.x + relPosMinus.x, component.position.y + relPosMinus.y};
                if (sqrt(pow(worldPos.x - p_minus.x, 2) + pow(worldPos.y - p_minus.y, 2)) <= PORT_RADIUS) {
                    m_tempPortBuffer = {1, relPosMinus, {0.0f, 0.0f, 0.0f, 1.0f}, false, PortType::ELECTRIC, "POWER_0V"};
                    return &m_tempPortBuffer;
                }
                break;
            }
            default:
                break;
        }
    }
    outComponentId = -1;
    return nullptr;
}
} // namespace plc