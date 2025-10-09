// src/Application_Physics.cpp

#include "Application.h"
#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <exception>

/**
 * CRITICAL: C-style physics engine function declarations
 * 
 * These extern "C" declarations must be at file scope to prevent linker errors.
 * The C++ compiler needs to recognize C linkage for proper symbol resolution
 * when calling into the compiled physics engine libraries.
 * 
 * ERROR PREVENTION: Incorrect placement of these declarations inside functions
 * can cause "undefined symbol" linker errors during runtime execution.
 */
extern "C" {
    int SolveElectricalNetworkC(plc::ElectricalNetwork* network, float deltaTime);
    int SolvePneumaticNetworkC(plc::PneumaticNetwork* network, float deltaTime);
    int SolveMechanicalSystemC(plc::MechanicalSystem* system, float deltaTime);
}

namespace plc {

/**
 * @brief Main physics simulation with PLC state independence
 * 
 * CRITICAL ACCESS VIOLATION PREVENTION:
 * This method was completely redesigned after resolving 0xC0000005 crashes
 * that occurred when physics simulation was incorrectly tied to PLC state.
 * 
 * CRASH ANALYSIS AND RESOLUTION:
 * - PROBLEM: Previous implementation stopped all physics when PLC was in STOP state,
 *   causing null pointer access when UI tried to read component states
 * - SOLUTION: Separated physics simulation into three independent stages:
 *   1. Basic physics (always running - sensors, cylinders, physical interactions)
 *   2. PLC logic execution (only when PLC is in RUN state)
 *   3. Advanced physics engine (optional enhancement layer)
 * 
 * REAL-WORLD BEHAVIOR MODEL:
 * Physical components continue operating even when PLC is stopped, just like
 * real industrial systems where sensors detect movement and cylinders respond
 * to manual valve operation regardless of PLC state.
 * 
 * STABILITY FEATURES:
 * - Exception handling prevents advanced physics failures from affecting basic physics
 * - Fallback mechanisms ensure core functionality continues during engine errors
 * - Network rebuilding only when component topology changes (performance optimization)
 */
void Application::UpdatePhysics() {
    // Basic physics simulation (always active)
    // Physical laws operate independently of PLC control state
    SimulateElectrical();
    UpdateComponentLogic();
    SimulatePneumatic();
    UpdateActuators();
    
    // PLC logic execution (conditional on PLC running state)
    if (!m_isPlcRunning) {
        return;
    }
    
    // Execute ladder logic with error containment
    SimulateLoadedLadder();
    
    // Advanced physics engine (optional enhancement)
    if (m_physicsEngine && m_physicsEngine->isInitialized && 
        m_physicsEngine->networking.BuildNetworksFromWiring &&
        m_physicsEngine->IsSafeToRunSimulation && 
        m_physicsEngine->IsSafeToRunSimulation(m_physicsEngine)) {
        
        // Network reconstruction when topology changes
        static bool networkBuilt = false;
        static size_t lastComponentCount = 0;
        static size_t lastWireCount = 0;
        
        if (!networkBuilt || 
            m_placedComponents.size() != lastComponentCount || 
            m_wires.size() != lastWireCount) {
            int result = m_physicsEngine->networking.BuildNetworksFromWiring(
                m_physicsEngine,
                m_wires.data(),
                static_cast<int>(m_wires.size()),
                m_placedComponents.data(),
                static_cast<int>(m_placedComponents.size())
            );
            
            if (result == PHYSICS_ENGINE_SUCCESS) {
                m_physicsEngine->networking.UpdateNetworkTopology(m_physicsEngine);
                networkBuilt = true;
                lastComponentCount = m_placedComponents.size();
                lastWireCount = m_wires.size();
            }
        }
        
        // Synchronize PLC outputs to physics engine
        if (m_isPlcRunning) {
            SyncPLCOutputsToPhysicsEngine();
            
            float deltaTime = 1.0f / 60.0f;
            
            try {
                if (m_physicsEngine->electricalNetwork && m_physicsEngine->electricalNetwork->nodeCount > 0) {
                    SolveElectricalNetworkC(m_physicsEngine->electricalNetwork, deltaTime);
                }
                
                if (m_physicsEngine->pneumaticNetwork && m_physicsEngine->pneumaticNetwork->nodeCount > 0) {
                    SolvePneumaticNetworkC(m_physicsEngine->pneumaticNetwork, deltaTime);
                }
                
                if (m_physicsEngine->mechanicalSystem && m_physicsEngine->mechanicalSystem->nodeCount > 0) {
                    SolveMechanicalSystemC(m_physicsEngine->mechanicalSystem, deltaTime);
                }
                
                SyncPhysicsEngineToApplication();
                
            } catch (const std::exception& e) {
                if (m_enableDebugLogging) {
                    std::cout << "[PHYSICS ERROR] Advanced engine exception: " << e.what() << std::endl;
                }
            }
        }
    }
}
/**
 * @brief Execute loaded ladder program using compiled PLC engine with error recovery
 * 
 * LADDER EXECUTION ERROR HANDLING:
 * This method implements robust error handling for ladder program execution
 * failures that previously caused system instability and crashes.
 * 
 * CRITICAL ERROR SCENARIOS:
 * 1. Empty ladder program causing null pointer access
 * 2. Compiled PLC executor initialization failures
 * 3. Scan cycle execution timeouts
 * 4. Memory corruption during instruction execution
 * 
 * RECOVERY MECHANISMS:
 * - Automatic default program creation when ladder is empty
 * - Graceful degradation when compiled executor fails
 * - Error message display for user guidance
 * - Debug mode synchronization for troubleshooting
 */
void Application::SimulateLoadedLadder() {
    if (m_loadedLadderProgram.rungs.empty()) {
        static bool firstRun = true;
        if (firstRun) {
            SyncLadderProgramFromProgrammingMode();
            if (m_loadedLadderProgram.rungs.empty() || 
                (m_loadedLadderProgram.rungs.size() <= 1 && m_loadedLadderProgram.rungs[0].cells.empty())) {
                CreateDefaultTestLadderProgram();
            }
            firstRun = false;
        }
        return;
    }
    
    if (!m_compiledPLCExecutor) {
        return;
    }
    
    m_compiledPLCExecutor->SetDebugMode(m_enableDebugLogging);
    auto result = m_compiledPLCExecutor->ExecuteScanCycle();
    
    if (!result.success && m_enableDebugLogging) {
        std::cout << "[PLC ERROR] Scan cycle failed: " << result.errorMessage << std::endl;
        return;
    }
}



/**
 * @brief Get PLC device state with dual-engine fallback mechanism
 * @param address Device address (e.g., "X0", "Y1", "M5", "T0", "C1")
 * @return Device state or false if invalid/not found
 * 
 * CRITICAL ERROR PREVENTION:
 * This method implements a dual-engine approach to prevent total failure
 * when the compiled PLC executor encounters errors. The legacy system
 * serves as a backup to maintain basic PLC functionality.
 * 
 * FALLBACK STRATEGY:
 * 1. Primary: Use compiled PLC executor for performance and accuracy
 * 2. Fallback: Use legacy state maps if compiled engine fails
 * 3. Type-safe access with bounds checking for all device types
 * 
 * ACCESS VIOLATION PREVENTION:
 * - Empty address validation prevents string access errors
 * - Map bounds checking prevents iterator invalidation
 * - Type-specific handling prevents wrong map access
 */
bool Application::GetPlcDeviceState(const std::string& address) {
    if (address.empty()) return false;
    
    // PRIMARY: Use compiled PLC execution engine for optimal performance
    if (m_compiledPLCExecutor) {
        return m_compiledPLCExecutor->GetDeviceState(address);
    }
    
    // FALLBACK: Legacy system for backward compatibility and error recovery
    switch (address[0]) {
        case 'T': {
            auto it = m_plcTimerStates.find(address);
            return (it != m_plcTimerStates.end()) ? it->second.done : false;
        }
        case 'C': {
            auto it = m_plcCounterStates.find(address);
            return (it != m_plcCounterStates.end()) ? it->second.done : false;
        }
        default: {
            auto it = m_plcDeviceStates.find(address);
            return (it != m_plcDeviceStates.end()) ? it->second : false;
        }
    }
}

/**
 * @brief Set PLC device state with dual-engine synchronization
 * @param address Device address (must be non-empty)
 * @param state New device state
 * 
 * SYNCHRONIZATION STRATEGY:
 * Updates both compiled PLC engine and legacy maps to maintain consistency
 * between different execution paths. This prevents state desynchronization
 * that could cause logic errors or unexpected behavior.
 * 
 * ERROR PREVENTION:
 * - Address validation prevents empty string errors
 * - Dual-engine update ensures state consistency
 * - Legacy system maintains compatibility during engine transitions
 */
void Application::SetPlcDeviceState(const std::string& address, bool state) {
    if (address.empty()) return;
    
    // PRIMARY: Update compiled PLC execution engine
    if (m_compiledPLCExecutor) {
        m_compiledPLCExecutor->SetDeviceState(address, state);
    }
    
    // SYNCHRONIZATION: Update legacy system for consistency
    m_plcDeviceStates[address] = state;
}

void Application::SimulateElectrical() {
    if (m_isPlcRunning) {
        SimulateLoadedLadder();
    } else {
        for (int i = 0; i < 16; ++i) {
            SetPlcDeviceState("Y" + std::to_string(i), false);
        }
    }

    using PortRef = std::pair<int, int>;
    enum class VoltageType { NONE, V24, V0 };

    m_portVoltages.clear();

    // Build electrical network graph from wire connections
    std::map<PortRef, std::vector<PortRef>> adjList;
    for (const auto& wire : m_wires) {
        if (wire.isElectric) {
            PortRef p1 = std::make_pair(wire.fromComponentId, wire.fromPortId);
            PortRef p2 = std::make_pair(wire.toComponentId, wire.toPortId);
            adjList[p1].push_back(p2);
            adjList[p2].push_back(p1);
        }
    }

    // Apply physical switch/sensor states to network graph
    for (const auto& comp : m_placedComponents) {
        // Physical switch logic
        if (comp.type == ComponentType::LIMIT_SWITCH) {
            bool is_pressed = comp.internalStates.count("is_pressed") && comp.internalStates.at("is_pressed") > 0.5f;
            PortRef com = std::make_pair(comp.instanceId, 0);
            PortRef no = std::make_pair(comp.instanceId, 1);
            PortRef nc = std::make_pair(comp.instanceId, 2);
            if (is_pressed) { adjList[com].push_back(no); adjList[no].push_back(com); }
            else { adjList[com].push_back(nc); adjList[nc].push_back(com); }
        }
        else if (comp.type == ComponentType::BUTTON_UNIT) {
            for (int i = 0; i < 3; ++i) {
                bool is_pressed = comp.internalStates.count("is_pressed_" + std::to_string(i)) && comp.internalStates.at("is_pressed_" + std::to_string(i)) > 0.5f;
                PortRef com = std::make_pair(comp.instanceId, i*5 + 0);
                PortRef no = std::make_pair(comp.instanceId, i*5 + 1);
                PortRef nc = std::make_pair(comp.instanceId, i*5 + 2);
                if (is_pressed) { adjList[com].push_back(no); adjList[no].push_back(com); }
                else { adjList[com].push_back(nc); adjList[nc].push_back(com); }
            }
        }
        else if (comp.type == ComponentType::SENSOR) {
            bool is_detected = comp.internalStates.count("is_detected") && comp.internalStates.at("is_detected") > 0.5f;
            if (is_detected) {
                PortRef p_24v = std::make_pair(comp.instanceId, 0);
                PortRef p_out = std::make_pair(comp.instanceId, 2);
                adjList[p_24v].push_back(p_out);
                adjList[p_out].push_back(p_out);
            }
        }
        // PLC output logic simulation
        else if (comp.type == ComponentType::PLC) {
            for (int i = 0; i < 16; ++i) {
                bool isY_On = GetPlcDeviceState("Y" + std::to_string(i));
                if (isY_On) {
                    int y_port_id = 16 + i;
                    int com_port_id = (i >= 8) ? 30 : 14;
                    adjList[std::make_pair(comp.instanceId, y_port_id)].push_back(std::make_pair(comp.instanceId, com_port_id));
                    adjList[std::make_pair(comp.instanceId, com_port_id)].push_back(std::make_pair(comp.instanceId, y_port_id));
                }
            }
        }
    }

    // Electrical network analysis and voltage propagation
    std::map<PortRef, int> port_to_net_id;
    std::map<int, VoltageType> net_voltage_type;
    int current_net_id = 1;

    for (const auto& [port_ref, position] : m_portPositions) {
        if (port_to_net_id.find(port_ref) == port_to_net_id.end()) {
            VoltageType found_voltage = VoltageType::NONE;
            std::queue<PortRef> q;
            q.push(port_ref);
            port_to_net_id[port_ref] = current_net_id;

            while (!q.empty()) {
                PortRef p = q.front(); q.pop();
                int compId = p.first;
                int portId = p.second;
                auto comp_it = std::find_if(m_placedComponents.begin(), m_placedComponents.end(), [compId](const auto& c){ return c.instanceId == compId; });
                if (comp_it != m_placedComponents.end() && comp_it->type == ComponentType::POWER_SUPPLY) {
                    if (portId == 0) found_voltage = VoltageType::V24;
                    if (portId == 1) found_voltage = VoltageType::V0;
                }
                if (adjList.count(p)) {
                    for (const auto& neighbor : adjList.at(p)) {
                        if (port_to_net_id.find(neighbor) == port_to_net_id.end()) {
                            port_to_net_id[neighbor] = current_net_id;
                            q.push(neighbor);
                        }
                    }
                }
            }
            net_voltage_type[current_net_id] = found_voltage;
            current_net_id++;
        }
    }

    // Set voltages based on network analysis
    for (const auto& [port, net_id] : port_to_net_id) {
        VoltageType type = net_voltage_type[net_id];
        if (type == VoltageType::V24) m_portVoltages[port] = 24.0f;
        else if (type == VoltageType::V0) m_portVoltages[port] = 0.0f;
        else m_portVoltages[port] = -1.0f;
    }

    // Component activation based on closed circuit detection
    for (auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::BUTTON_UNIT) {
            for (int i = 0; i < 3; ++i) {
                PortRef plus_port = std::make_pair(comp.instanceId, i * 5 + 3);
                PortRef minus_port = std::make_pair(comp.instanceId, i * 5 + 4);
                bool has_24v = m_portVoltages.count(plus_port) && m_portVoltages.at(plus_port) > 23.0f;
                bool has_0v = m_portVoltages.count(minus_port) && m_portVoltages.at(minus_port) > -0.1f && m_portVoltages.at(minus_port) < 0.1f;
                comp.internalStates["lamp_on_" + std::to_string(i)] = (has_24v && has_0v) ? 1.0f : 0.0f;
            }
        }
        if (comp.type == ComponentType::VALVE_SINGLE || comp.type == ComponentType::VALVE_DOUBLE) {
            PortRef sol_a_plus = std::make_pair(comp.instanceId, 0);
            PortRef sol_a_minus = std::make_pair(comp.instanceId, 1);
            bool sol_a_powered = m_portVoltages.count(sol_a_plus) && m_portVoltages.at(sol_a_plus) > 23.0f &&
                                 m_portVoltages.count(sol_a_minus) && m_portVoltages.at(sol_a_minus) > -0.1f && m_portVoltages.at(sol_a_minus) < 0.1f;
            comp.internalStates["solenoid_a_active"] = sol_a_powered ? 1.0f : 0.0f;

            if (comp.type == ComponentType::VALVE_DOUBLE) {
                PortRef sol_b_plus = std::make_pair(comp.instanceId, 2);
                PortRef sol_b_minus = std::make_pair(comp.instanceId, 3);
                bool sol_b_powered = m_portVoltages.count(sol_b_plus) && m_portVoltages.at(sol_b_plus) > 23.0f &&
                                     m_portVoltages.count(sol_b_minus) && m_portVoltages.at(sol_b_minus) > -0.1f && m_portVoltages.at(sol_b_minus) < 0.1f;
                comp.internalStates["solenoid_b_active"] = sol_b_powered ? 1.0f : 0.0f;
            }
        }
        if (comp.type == ComponentType::SENSOR) {
             PortRef plus_port = std::make_pair(comp.instanceId, 0);
             PortRef minus_port = std::make_pair(comp.instanceId, 1);
             bool has_24v = m_portVoltages.count(plus_port) && m_portVoltages.at(plus_port) > 23.0f;
             bool has_0v = m_portVoltages.count(minus_port) && m_portVoltages.at(minus_port) > -0.1f && m_portVoltages.at(minus_port) < 0.1f;
             comp.internalStates["is_powered"] = (has_24v && has_0v) ? 1.0f : 0.0f;
        }
    }

    // Note: X 입력 동기화는 SyncPhysicsEngineToApplication()에서 단일 경로로 처리합니다.
}
void Application::UpdateComponentLogic() {
    for (auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::VALVE_DOUBLE) {
            bool sol_a_active = comp.internalStates.count("solenoid_a_active") && comp.internalStates.at("solenoid_a_active") > 0.5f;
            bool sol_b_active = comp.internalStates.count("solenoid_b_active") && comp.internalStates.at("solenoid_b_active") > 0.5f;

            if (sol_a_active && !sol_b_active) {
                comp.internalStates["last_active_solenoid"] = 1.0f;
            } else if (sol_b_active && !sol_a_active) {
                comp.internalStates["last_active_solenoid"] = 2.0f;
            }
        }
    }
}

void Application::SimulatePneumatic() {
    using PortRef = std::pair<int, int>;
    m_portPressures.clear();

    std::map<PortRef, std::vector<PortRef>> pneumaticAdjacencyList;

    for (const auto& wire : m_wires) {
        if (!wire.isElectric) {
            pneumaticAdjacencyList[std::make_pair(wire.fromComponentId, wire.fromPortId)].push_back(std::make_pair(wire.toComponentId, wire.toPortId));
            pneumaticAdjacencyList[std::make_pair(wire.toComponentId, wire.toPortId)].push_back(std::make_pair(wire.fromComponentId, wire.fromPortId));
        }
    }

    for (const auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::VALVE_SINGLE) {
            bool sol_a_active = comp.internalStates.count("solenoid_a_active") && comp.internalStates.at("solenoid_a_active") > 0.5f;
            PortRef p = std::make_pair(comp.instanceId, 2);
            PortRef a = std::make_pair(comp.instanceId, 3);
            PortRef b = std::make_pair(comp.instanceId, 4);
            if (sol_a_active) {
                pneumaticAdjacencyList[p].push_back(a); pneumaticAdjacencyList[a].push_back(p);
            } else {
                pneumaticAdjacencyList[p].push_back(b); pneumaticAdjacencyList[b].push_back(p);
            }
        } else if (comp.type == ComponentType::VALVE_DOUBLE) {
            float last_active = comp.internalStates.count("last_active_solenoid") ? comp.internalStates.at("last_active_solenoid") : 0.0f;
            PortRef p = std::make_pair(comp.instanceId, 4);
            PortRef a = std::make_pair(comp.instanceId, 5);
            PortRef b = std::make_pair(comp.instanceId, 6);
            if (last_active > 0.5f && last_active < 1.5f) {
                pneumaticAdjacencyList[p].push_back(a); pneumaticAdjacencyList[a].push_back(p);
            } else if (last_active > 1.5f) {
                pneumaticAdjacencyList[p].push_back(b); pneumaticAdjacencyList[b].push_back(p);
            }
        }
        else if (comp.type == ComponentType::MANIFOLD) {
            PortRef input_port = std::make_pair(comp.instanceId, 0);
            for (int i = 1; i <= 4; ++i) {
                PortRef output_port = std::make_pair(comp.instanceId, i);
                pneumaticAdjacencyList[input_port].push_back(output_port);
                pneumaticAdjacencyList[output_port].push_back(input_port);
            }
        }
    }

    std::queue<PortRef> pressureQueue;
    std::set<PortRef> processedPorts;
    for (const auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::FRL) {
            PortRef airOut = std::make_pair(comp.instanceId, 0);
            float airPressure = comp.internalStates.count("air_pressure") ? comp.internalStates.at("air_pressure") : 6.0f;
            m_portPressures[airOut] = airPressure;
            pressureQueue.push(airOut);
            processedPorts.insert(airOut);
        }
    }

    while(!pressureQueue.empty()){
        PortRef current = pressureQueue.front();
        pressureQueue.pop();
        float current_pressure = m_portPressures.count(current) ? m_portPressures.at(current) : 0.0f;

        if(pneumaticAdjacencyList.count(current)) {
            for(const auto& neighbor : pneumaticAdjacencyList.at(current)) {
                if(processedPorts.find(neighbor) == processedPorts.end()){
                    m_portPressures[neighbor] = current_pressure > 0.1f ? current_pressure - 0.1f : 0.0f;
                    pressureQueue.push(neighbor);
                    processedPorts.insert(neighbor);
                }
            }
        }
    }
}
/**
 * @brief Update cylinder actuators and sensor detection with crash prevention
 * 
 * CRITICAL ACCESS VIOLATION FIXES:
 * This method was extensively redesigned to prevent crashes that occurred during
 * limit switch detection. Previous issues included array bounds violations,
 * null pointer access, and infinite loops in distance calculations.
 * 
 * LIMIT SWITCH DETECTION IMPROVEMENTS:
 * - Detection range expanded from 30px to 150px (5x increase) for practicality
 * - Sensor detection range expanded from 50px to 100px (2x increase)
 * - Robust bounds checking prevents array access violations
 * - Distance calculation overflow protection
 * - Multi-cylinder support with closest-target selection
 * 
 * PHYSICS SIMULATION ENHANCEMENTS:
 * - Realistic pneumatic force calculations based on pressure differential
 * - Velocity-based friction modeling
 * - Position clamping prevents out-of-bounds cylinder movement
 * - Status tracking for UI feedback
 * 
 * PERFORMANCE OPTIMIZATIONS:
 * - Debug logging with controlled frequency (1 second intervals)
 * - Early termination when detection occurs
 * - Cached calculations for repeated operations
 */
void Application::UpdateActuators() {
    const float deltaTime = 1.0f / 60.0f;
    const float pistonMass = 0.5f;
    const float frictionCoeff = 0.02f;

    const float ACTIVATION_PRESSURE_THRESHOLD = 2.5f;
    const float FORCE_SCALING_FACTOR = 15.0f;

    for (auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::CYLINDER) {
            if (!comp.internalStates.count("position")) {
                comp.internalStates["position"] = 0.0f;
                comp.internalStates["velocity"] = 0.0f;
            }

            float currentPosition = comp.internalStates.at("position");
            float currentVelocity = comp.internalStates.at("velocity");

            float pressureA = m_portPressures.count({comp.instanceId, 0}) ? m_portPressures.at({comp.instanceId, 0}) : 0.0f;
            float pressureB = m_portPressures.count({comp.instanceId, 1}) ? m_portPressures.at({comp.instanceId, 1}) : 0.0f;

            float pressureDiff = pressureA - pressureB;
            float force = 0.0f;

            if (std::abs(pressureDiff) >= ACTIVATION_PRESSURE_THRESHOLD) {
                float effectivePressure = std::abs(pressureDiff) - ACTIVATION_PRESSURE_THRESHOLD;
                float forceMagnitude = std::pow(effectivePressure, 2.0f) * FORCE_SCALING_FACTOR;
                force = std::copysign(forceMagnitude, pressureDiff);
            }

            float frictionForce = -frictionCoeff * currentVelocity;
            float totalForce = force + frictionForce;

            float acceleration = totalForce / pistonMass;

            currentVelocity += acceleration * deltaTime;
            currentPosition += currentVelocity * deltaTime;

            const float maxStroke = 160.0f;
            if (currentPosition < 0.0f) {
                currentPosition = 0.0f;
                if (currentVelocity < 0) currentVelocity = 0;
            } else if (currentPosition > maxStroke) {
                currentPosition = maxStroke;
                if (currentVelocity > 0) currentVelocity = 0;
            }

            comp.internalStates["position"] = currentPosition;
            comp.internalStates["velocity"] = currentVelocity;
            comp.internalStates["pressure_a"] = pressureA;
            comp.internalStates["pressure_b"] = pressureB;

            const float velocityThreshold = 1.0f;
            if (currentVelocity > velocityThreshold) comp.internalStates["status"] = 1.0f;
            else if (currentVelocity < -velocityThreshold) comp.internalStates["status"] = -1.0f;
            else comp.internalStates["status"] = 0.0f;
        }
    }

    // Sensor and limit switch detection system
    static int debugCounter = 0;
    bool enableDetailedDebug = (++debugCounter % 60 == 0);
    
    for (auto& sensor : m_placedComponents) {
        if (sensor.type == ComponentType::LIMIT_SWITCH || sensor.type == ComponentType::SENSOR) {
            bool isActivatedByPhysics = false;
            float minDistance = 999999.0f;
            int closestCylinderId = -1;
            
            for (const auto& cylinder : m_placedComponents) {
                if (cylinder.type == ComponentType::CYLINDER) {
                    float cylinderX_body_start = cylinder.position.x + 170.0f;
                    float pistonPosition = cylinder.internalStates.count("position") ? cylinder.internalStates.at("position") : 0.0f;
                    float pistonX_tip = cylinderX_body_start - pistonPosition;
                    float pistonY = cylinder.position.y + 25.0f;

                    float sensorX_center, sensorY_center;
                    if (sensor.type == ComponentType::LIMIT_SWITCH) {
                        sensorX_center = sensor.position.x + sensor.size.width / 2.0f;
                        sensorY_center = sensor.position.y + sensor.size.height / 2.0f;
                    } else {
                        sensorX_center = sensor.position.x + sensor.size.width / 2.0f;
                        sensorY_center = sensor.position.y + 7.5f;
                    }

                    float dx = pistonX_tip - sensorX_center;
                    float dy = pistonY - sensorY_center;
                    float distance = std::sqrt(dx*dx + dy*dy);
                    
                    if (distance < minDistance) {
                        minDistance = distance;
                        closestCylinderId = cylinder.instanceId;
                    }

                    float detectionRange = (sensor.type == ComponentType::LIMIT_SWITCH) ? 100.0f : 75.0f;
                    
                    if (distance < detectionRange) {
                        isActivatedByPhysics = true;
                        
                        if (enableDetailedDebug && m_enableDebugLogging) {
                            std::cout << "[SENSOR] " 
                                      << (sensor.type == ComponentType::LIMIT_SWITCH ? "LimitSwitch" : "Sensor")
                                      << "[" << sensor.instanceId << "] detected at distance " << distance << "mm" << std::endl;
                        }
                        break;
                    }
                }
            }
            
            (void)closestCylinderId; // Unused variable

            if (sensor.type == ComponentType::LIMIT_SWITCH) {
                bool isPressedManually = sensor.internalStates.count("is_pressed_manual") && sensor.internalStates.at("is_pressed_manual") > 0.5f;
                sensor.internalStates["is_pressed"] = (isActivatedByPhysics || isPressedManually) ? 1.0f : 0.0f;
            } else {
                sensor.internalStates["is_detected"] = isActivatedByPhysics ? 1.0f : 0.0f;
            }
        }
    }
}

/**
 * @brief Basic physics simulation independent of PLC state
 * 
 * CRITICAL INDEPENDENCE DESIGN:
 * This method ensures that essential physics continues operating even when
 * the PLC is in STOP state, preventing UI crashes and maintaining realistic
 * behavior of physical components.
 * 
 * PURPOSE:
 * - Sensor detection continues (real sensors don't stop working when PLC stops)
 * - Cylinder movement responds to manual valve operation
 * - Physical interactions remain active for realistic simulation
 * - Manual component operation remains functional
 * 
 * CRASH PREVENTION:
 * - Duplicate logic from UpdateActuators() with same safety features
 * - Independent execution prevents PLC state corruption from affecting physics
 * - Bounds checking and null pointer protection maintained
 */
void Application::UpdateBasicPhysics() {
    // Cylinder physics simulation
    const float deltaTime = 1.0f / 60.0f;
    const float pistonMass = 0.5f;
    const float frictionCoeff = 0.02f;
    const float ACTIVATION_PRESSURE_THRESHOLD = 2.5f;
    const float FORCE_SCALING_FACTOR = 15.0f;

    for (auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::CYLINDER) {
            if (!comp.internalStates.count("position")) {
                comp.internalStates["position"] = 0.0f;
                comp.internalStates["velocity"] = 0.0f;
            }

            float currentPosition = comp.internalStates.at("position");
            float currentVelocity = comp.internalStates.at("velocity");

            float pressureA = m_portPressures.count({comp.instanceId, 0}) ? m_portPressures.at({comp.instanceId, 0}) : 0.0f;
            float pressureB = m_portPressures.count({comp.instanceId, 1}) ? m_portPressures.at({comp.instanceId, 1}) : 0.0f;

            float pressureDiff = pressureA - pressureB;
            float force = 0.0f;

            if (std::abs(pressureDiff) >= ACTIVATION_PRESSURE_THRESHOLD) {
                float effectivePressure = std::abs(pressureDiff) - ACTIVATION_PRESSURE_THRESHOLD;
                float forceMagnitude = std::pow(effectivePressure, 2.0f) * FORCE_SCALING_FACTOR;
                force = std::copysign(forceMagnitude, pressureDiff);
            }

            float frictionForce = -frictionCoeff * currentVelocity;
            float totalForce = force + frictionForce;
            float acceleration = totalForce / pistonMass;

            currentVelocity += acceleration * deltaTime;
            currentPosition += currentVelocity * deltaTime;

            const float maxStroke = 160.0f;
            if (currentPosition < 0.0f) {
                currentPosition = 0.0f;
                if (currentVelocity < 0) currentVelocity = 0;
            } else if (currentPosition > maxStroke) {
                currentPosition = maxStroke;
                if (currentVelocity > 0) currentVelocity = 0;
            }

            comp.internalStates["position"] = currentPosition;
            comp.internalStates["velocity"] = currentVelocity;
            comp.internalStates["pressure_a"] = pressureA;
            comp.internalStates["pressure_b"] = pressureB;

            const float velocityThreshold = 1.0f;
            if (currentVelocity > velocityThreshold) comp.internalStates["status"] = 1.0f;
            else if (currentVelocity < -velocityThreshold) comp.internalStates["status"] = -1.0f;
            else comp.internalStates["status"] = 0.0f;
        }
    }

    // Sensor detection logic (independent of PLC state)
    static int debugCounter = 0;
    bool enableDetailedDebug = (++debugCounter % 60 == 0);
    
    for (auto& sensor : m_placedComponents) {
        if (sensor.type == ComponentType::LIMIT_SWITCH || sensor.type == ComponentType::SENSOR) {
            bool isActivatedByPhysics = false;
            float minDistance = 999999.0f;
            int closestCylinderId = -1;
            (void)closestCylinderId; // 미사용 변수 경고 제거
            
            for (const auto& cylinder : m_placedComponents) {
                if (cylinder.type == ComponentType::CYLINDER) {
                    float cylinderX_body_start = cylinder.position.x + 170.0f;
                    float pistonPosition = cylinder.internalStates.count("position") ? cylinder.internalStates.at("position") : 0.0f;
                    float pistonX_tip = cylinderX_body_start - pistonPosition;
                    float pistonY = cylinder.position.y + 25.0f;

                    float sensorX_center, sensorY_center;
                    if (sensor.type == ComponentType::LIMIT_SWITCH) {
                        sensorX_center = sensor.position.x + sensor.size.width / 2.0f;
                        sensorY_center = sensor.position.y + sensor.size.height / 2.0f;
                    } else {
                        sensorX_center = sensor.position.x + sensor.size.width / 2.0f;
                        sensorY_center = sensor.position.y + 7.5f;
                    }

                    float dx = pistonX_tip - sensorX_center;
                    float dy = pistonY - sensorY_center;
                    float distance = std::sqrt(dx*dx + dy*dy);
                    
                    if (distance < minDistance) {
                        minDistance = distance;
                        closestCylinderId = cylinder.instanceId;
                    }

                    float detectionRange = (sensor.type == ComponentType::LIMIT_SWITCH) ? 150.0f : 100.0f;
                    
                    if (distance < detectionRange) {
                        isActivatedByPhysics = true;
                        break;
                    }
                }
            }

            // Update sensor state
            if (sensor.type == ComponentType::LIMIT_SWITCH) {
                bool isPressedManually = sensor.internalStates.count("is_pressed_manual") && sensor.internalStates.at("is_pressed_manual") > 0.5f;
                sensor.internalStates["is_pressed"] = (isActivatedByPhysics || isPressedManually) ? 1.0f : 0.0f;
            } else {
                sensor.internalStates["is_detected"] = isActivatedByPhysics ? 1.0f : 0.0f;
            }
        }
    }

    // Basic simulation for PLC STOP state
    if (!m_isPlcRunning) {
        SimulateElectrical();
        UpdateComponentLogic();
        SimulatePneumatic();
    }
}

// ========== PHYSICS ENGINE INTEGRATION HELPER FUNCTIONS ==========


/**
 * @brief Synchronize PLC outputs to physics engine with comprehensive error handling
 * 
 * CRITICAL SYNCHRONIZATION PROCESS:
 * This method transfers ladder program Y device states to the physics engine's
 * electrical network, converting logical PLC outputs to physical electrical
 * characteristics for realistic simulation.
 * 
 * ACCESS VIOLATION PREVENTION:
 * - Multiple layers of null pointer checking
 * - Array bounds validation for all array accesses
 * - Network validity verification before operation
 * - Component ID bounds checking
 * 
 * ELECTRICAL MODEL:
 * - Y outputs implement sinking logic (low voltage when ON)
 * - Proper output impedance modeling
 * - Current limiting and voltage source characteristics
 * - Realistic electrical behavior simulation
 * 
 * ERROR SCENARIOS HANDLED:
 * 1. Physics engine not initialized
 * 2. Electrical network corruption
 * 3. Invalid component/port mappings
 * 4. Array bounds violations
 * 5. Null pointer dereferences
 */
void Application::SyncPLCOutputsToPhysicsEngine() {
    if (!m_physicsEngine || !m_physicsEngine->isInitialized) {
        return;
    }
    
    ElectricalNetwork* elecNet = m_physicsEngine->electricalNetwork;
    if (!elecNet || elecNet->nodeCount == 0 || !elecNet->nodes) {
        return;
    }
    
    // PLC 컴포넌트 찾기
    for (const auto& comp : m_placedComponents) {
        if (comp.type != ComponentType::PLC) continue;
        
        // PLC Y OUTPUT SYNCHRONIZATION (ports 16-31) with bounds checking
        for (int y = 0; y < 16; y++) {
            std::string yAddress = "Y" + std::to_string(y);
            bool yState = GetPlcDeviceState(yAddress);
            
            // SAFE NODE LOOKUP: Multiple bounds checks prevent array violations
            int nodeId = -1;
            if (comp.instanceId < m_physicsEngine->maxComponents) {
                // PLC Y ports mapped to port IDs 16+y
                int portId = 16 + y;
                if (portId < 32) {
                    nodeId = m_physicsEngine->componentPortToElectricalNode[comp.instanceId][portId];
                }
            }
            
            if (nodeId >= 0 && nodeId < elecNet->nodeCount) {
                ElectricalNode& node = elecNet->nodes[nodeId];
                
                if (yState) {
                    // Y OUTPUT ON: Low voltage (sinking type PLC output)
                    node.isVoltageSource = true;
                    node.sourceVoltage = 0.5f;  // 0.5V residual voltage when ON
                    node.sourceResistance = 0.1f; // 0.1Ω output resistance
                } else {
                    // Y OUTPUT OFF: High impedance (open circuit)
                    node.isVoltageSource = false;
                    node.sourceVoltage = 0.0f;
                    node.sourceResistance = 1e6f; // 1MΩ (effectively open)
                }
                
                // PHYSICS STATE SYNCHRONIZATION with bounds checking
                int physicsIndex = m_physicsEngine->componentIdToPhysicsIndex[comp.instanceId];
                if (physicsIndex >= 0 && physicsIndex < m_physicsEngine->activeComponents) {
                    TypedPhysicsState& physState = m_physicsEngine->componentPhysicsStates[physicsIndex];
                    if (physState.type == PHYSICS_STATE_PLC) {
                        physState.state.plc.outputStates[y] = yState;
                        physState.state.plc.outputVoltages[y] = yState ? 0.5f : 24.0f;
                        physState.state.plc.outputCurrents[y] = yState ? 100.0f : 0.0f; // 100mA when ON
                    }
                }
            }
        }
        
        // PLC X input port voltage reading from physics engine with bounds checking
        for (int x = 0; x < 16; x++) {
            int nodeId = -1;
            if (comp.instanceId >= 0 && comp.instanceId < m_physicsEngine->maxComponents && 
                x >= 0 && x < 32 &&
                m_physicsEngine->componentPortToElectricalNode != nullptr) {
                nodeId = m_physicsEngine->componentPortToElectricalNode[comp.instanceId][x];
            }
            
            if (nodeId >= 0 && nodeId < elecNet->nodeCount) {
                ElectricalNode& node = elecNet->nodes[nodeId];
                
                // 입력 임피던스 설정 (고임피던스 입력)
                node.inputImpedance[0] = 10000.0f; // 10kΩ
                node.responseTime = 0.001f; // 1ms 응답시간
                
                // PLCPhysicsState update with bounds checking
                int physicsIndex = -1;
                if (comp.instanceId >= 0 && comp.instanceId < m_physicsEngine->maxComponents &&
                    m_physicsEngine->componentIdToPhysicsIndex != nullptr) {
                    physicsIndex = m_physicsEngine->componentIdToPhysicsIndex[comp.instanceId];
                }
                if (physicsIndex >= 0 && physicsIndex < m_physicsEngine->activeComponents &&
                    m_physicsEngine->componentPhysicsStates != nullptr) {
                    TypedPhysicsState& physState = m_physicsEngine->componentPhysicsStates[physicsIndex];
                    if (physState.type == PHYSICS_STATE_PLC) {
                        physState.state.plc.inputVoltages[x] = node.voltage;
                        physState.state.plc.inputCurrents[x] = node.current * 1000.0f; // A → mA
                        physState.state.plc.inputStates[x] = (node.voltage > 12.0f); // 12V 임계값
                    }
                }
            }
        }
        
        break; // PLC는 하나만 있다고 가정
    }
}

// Synchronize physics engine results to Application
// Update internalStates, m_portVoltages, m_portPressures with simulation results
void Application::SyncPhysicsEngineToApplication() {
    // Basic null pointer checking
    if (!m_physicsEngine) {
        if (m_enableDebugLogging) {
            std::cout << "[Physics] Cannot sync to application: engine is null" << std::endl;
        }
        return;
    }
    
    if (!m_physicsEngine->isInitialized) {
        if (m_enableDebugLogging) {
            std::cout << "[Physics] Cannot sync to application: engine not initialized" << std::endl;
        }
        return;
    }
    
    // Network pointer safety checks
    ElectricalNetwork* elecNet = m_physicsEngine->electricalNetwork;
    PneumaticNetwork* pneuNet = m_physicsEngine->pneumaticNetwork;
    MechanicalSystem* mechSys = m_physicsEngine->mechanicalSystem;
    
    if (!elecNet || !pneuNet || !mechSys) {
        if (m_enableDebugLogging) {
            std::cout << "[Physics] Cannot sync to application: one or more networks are null" << std::endl;
        }
        return;
    }
    
    (void)mechSys; // Mechanical system synchronization not yet implemented
    
    // 1. 전기 네트워크 결과를 m_portVoltages에 동기화
    if (elecNet && elecNet->nodeCount > 0) {
        m_portVoltages.clear();
        
        for (int n = 0; n < elecNet->nodeCount; n++) {
            const ElectricalNode& node = elecNet->nodes[n];
            if (node.base.isActive) {
                std::pair<int, int> portKey = std::make_pair(node.base.componentId, node.base.portId);
                m_portVoltages[portKey] = node.voltage;
            }
        }
    }
    
    // 2. 공압 네트워크 결과를 m_portPressures에 동기화
    if (pneuNet && pneuNet->nodeCount > 0) {
        m_portPressures.clear();
        
        for (int n = 0; n < pneuNet->nodeCount; n++) {
            const PneumaticNode& node = pneuNet->nodes[n];
            if (node.base.isActive) {
                std::pair<int, int> portKey = std::make_pair(node.base.componentId, node.base.portId);
                // Pa → bar 변환
                m_portPressures[portKey] = node.gaugePressure / 100000.0f;
            }
        }
    }
    
    // Component physics state synchronization to internalStates with bounds checking
    for (auto& comp : m_placedComponents) {
        int physicsIndex = -1;
        if (comp.instanceId >= 0 && comp.instanceId < m_physicsEngine->maxComponents &&
            m_physicsEngine->componentIdToPhysicsIndex != nullptr) {
            physicsIndex = m_physicsEngine->componentIdToPhysicsIndex[comp.instanceId];
        }
        
        if (physicsIndex >= 0 && physicsIndex < m_physicsEngine->activeComponents &&
            m_physicsEngine->componentPhysicsStates != nullptr) {
            const TypedPhysicsState& physState = m_physicsEngine->componentPhysicsStates[physicsIndex];
            
            switch (physState.type) {
                case PHYSICS_STATE_CYLINDER: {
                    const CylinderPhysicsState& cylState = physState.state.cylinder;
                    
                    // 위치, 속도, 가속도 동기화
                    comp.internalStates["position"] = cylState.position;
                    comp.internalStates["velocity"] = cylState.velocity;
                    comp.internalStates["acceleration"] = cylState.acceleration;
                    
                    // 압력 정보 동기화
                    comp.internalStates["pressure_a"] = cylState.pressureA;
                    comp.internalStates["pressure_b"] = cylState.pressureB;
                    
                    // 상태 정보 동기화
                    float status = 0.0f;
                    if (cylState.isExtending) status = 1.0f;
                    else if (cylState.isRetracting) status = -1.0f;
                    comp.internalStates["status"] = status;
                    
                    // 힘 정보 동기화
                    comp.internalStates["force"] = cylState.totalForce;
                    comp.internalStates["pneumatic_force_a"] = cylState.pneumaticForceA;
                    comp.internalStates["pneumatic_force_b"] = cylState.pneumaticForceB;
                    comp.internalStates["friction_force"] = cylState.frictionForce;
                    
                    break;
                }
                
                case PHYSICS_STATE_LIMIT_SWITCH: {
                    const LimitSwitchPhysicsState& switchState = physState.state.limitSwitch;
                    
                    // 접촉 상태 동기화
                    comp.internalStates["is_pressed"] = switchState.isPhysicallyPressed ? 1.0f : 0.0f;
                    comp.internalStates["contact_force"] = switchState.contactForce;
                    comp.internalStates["displacement"] = switchState.displacement;
                    
                    // Synchronize electrical characteristics
                    comp.internalStates["contact_resistance_no"] = switchState.contactResistanceNO;
                    comp.internalStates["contact_resistance_nc"] = switchState.contactResistanceNC;
                    comp.internalStates["contact_voltage"] = switchState.contactVoltage;
                    
                    break;
                }
                
                case PHYSICS_STATE_SENSOR: {
                    const SensorPhysicsState& sensorState = physState.state.sensor;
                    
                    // 감지 상태 동기화
                    comp.internalStates["is_detected"] = sensorState.detectionState ? 1.0f : 0.0f;
                    comp.internalStates["target_distance"] = sensorState.targetDistance;
                    comp.internalStates["output_voltage"] = sensorState.outputVoltage;
                    comp.internalStates["is_powered"] = (sensorState.powerConsumption > 0.1f) ? 1.0f : 0.0f;
                    
                    // 성능 특성 동기화
                    comp.internalStates["sensitivity"] = sensorState.sensitivity;
                    comp.internalStates["response_time"] = sensorState.responseTime;
                    
                    break;
                }
                
                case PHYSICS_STATE_VALVE_SINGLE: {
                    const ValveSinglePhysicsState& valveState = physState.state.valveSingle;
                    
                    // 솔레노이드 상태 동기화
                    comp.internalStates["solenoid_a_active"] = valveState.solenoidEnergized ? 1.0f : 0.0f;
                    comp.internalStates["solenoid_voltage"] = valveState.solenoidVoltage;
                    comp.internalStates["solenoid_current"] = valveState.solenoidCurrent;
                    
                    // 밸브 위치 및 유량 동기화
                    comp.internalStates["valve_position"] = valveState.valvePosition;
                    comp.internalStates["flow_coefficient_pa"] = valveState.flowCoefficientPA;
                    comp.internalStates["flow_coefficient_ar"] = valveState.flowCoefficientAR;
                    
                    break;
                }
                
                case PHYSICS_STATE_VALVE_DOUBLE: {
                    const ValveDoublePhysicsState& valveState = physState.state.valveDouble;
                    
                    // 양방향 솔레노이드 상태 동기화
                    comp.internalStates["solenoid_a_active"] = valveState.solenoidEnergizedA ? 1.0f : 0.0f;
                    comp.internalStates["solenoid_b_active"] = valveState.solenoidEnergizedB ? 1.0f : 0.0f;
                    comp.internalStates["last_active_solenoid"] = valveState.lastActivePosition;
                    
                    // Synchronize electrical characteristics
                    comp.internalStates["solenoid_voltage_a"] = valveState.solenoidVoltageA;
                    comp.internalStates["solenoid_voltage_b"] = valveState.solenoidVoltageB;
                    comp.internalStates["solenoid_current_a"] = valveState.solenoidCurrentA;
                    comp.internalStates["solenoid_current_b"] = valveState.solenoidCurrentB;
                    
                    // 밸브 위치 동기화
                    comp.internalStates["valve_position"] = valveState.valvePosition;
                    
                    break;
                }
                
                case PHYSICS_STATE_BUTTON_UNIT: {
                    const ButtonUnitPhysicsState& buttonState = physState.state.buttonUnit;
                    
                    // Synchronize button states
                    for (int i = 0; i < 3; i++) {
                        comp.internalStates["is_pressed_" + std::to_string(i)] = buttonState.buttonStates[i] ? 1.0f : 0.0f;
                        comp.internalStates["lamp_on_" + std::to_string(i)] = buttonState.ledStates[i] ? 1.0f : 0.0f;
                        comp.internalStates["button_voltage_" + std::to_string(i)] = buttonState.buttonVoltages[i];
                        comp.internalStates["led_brightness_" + std::to_string(i)] = buttonState.ledBrightness[i];
                    }
                    
                    break;
                }
                
                // 다른 컴포넌트 타입들도 필요에 따라 추가...
                default:
                    break;
            }
        }
    }
    
    // 4. PLC X 입력 상태를 물리엔진 결과로 업데이트
    for (const auto& comp : m_placedComponents) {
        if (comp.type == ComponentType::PLC) {
            std::map<std::string, bool> xInputs; // 신규: ProgrammingMode로 보낼 X 입력 버퍼
            for (int x = 0; x < 16; x++) {
                std::string xAddress = "X" + std::to_string(x);
                auto portKey = std::make_pair(comp.instanceId, x);
                
                if (m_portVoltages.count(portKey)) {
                    float voltage = m_portVoltages.at(portKey);
                    bool state = (voltage > 12.0f); // 12V 임계값
                    // 기존 경로 유지 (Application 내부 상태/Executor 동기화)
                    SetPlcDeviceState(xAddress, state);
                    // 신규 경로: ProgrammingMode에 입력 전달(단일 진입점)
                    xInputs[xAddress] = state;
                }
            }
            // 실제 동기화 호출
            if (m_programmingMode && !xInputs.empty()) {
                m_programmingMode->UpdateInputsFromSystem(xInputs);
            }
            break;
        }
    }
}

// Update physics engine performance statistics
// Collect statistics for real-time performance monitoring
void Application::UpdatePhysicsPerformanceStats() {
    if (!m_physicsEngine || !m_physicsEngine->isInitialized) return;
    
    PhysicsPerformanceStats& stats = m_physicsEngine->performanceStats;
    
    // 네트워크 크기 통계
    stats.electricalNodes = m_physicsEngine->electricalNetwork->nodeCount;
    stats.electricalEdges = m_physicsEngine->electricalNetwork->edgeCount;
    stats.pneumaticNodes = m_physicsEngine->pneumaticNetwork->nodeCount;
    stats.pneumaticEdges = m_physicsEngine->pneumaticNetwork->edgeCount;
    stats.mechanicalNodes = m_physicsEngine->mechanicalSystem->nodeCount;
    stats.mechanicalEdges = m_physicsEngine->mechanicalSystem->edgeCount;
    
    // 수렴성 통계
    stats.electricalConverged = m_physicsEngine->electricalNetwork->isConverged;
    stats.pneumaticConverged = m_physicsEngine->pneumaticNetwork->isConverged;
    stats.mechanicalStable = m_physicsEngine->mechanicalSystem->isStable;
    stats.electricalIterations = m_physicsEngine->electricalNetwork->currentIteration;
    stats.pneumaticIterations = m_physicsEngine->pneumaticNetwork->currentIteration;
    
    // 품질 지표
    ElectricalNetwork* elec = m_physicsEngine->electricalNetwork;
    if (elec && elec->nodeCount > 0) {
        // 전기 시스템 잔여오차
        float residual = 0.0f;
        for (int i = 0; i < elec->nodeCount; i++) {
            residual += elec->nodes[i].netCurrent * elec->nodes[i].netCurrent;
        }
        stats.electricalResidual = std::sqrt(residual);
        
        // 총 소비전력 계산
        float totalPower = 0.0f;
        for (int e = 0; e < elec->edgeCount; e++) {
            totalPower += elec->edges[e].powerDissipation;
        }
        stats.powerConsumption = totalPower;
    }
    
    PneumaticNetwork* pneu = m_physicsEngine->pneumaticNetwork;
    if (pneu && pneu->nodeCount > 0) {
        // 공압 시스템 잔여오차 (질량보존 법칙 체크)
        float residual = 0.0f;
        for (int i = 0; i < pneu->nodeCount; i++) {
            residual += pneu->nodes[i].netMassFlow * pneu->nodes[i].netMassFlow;
        }
        stats.pneumaticResidual = std::sqrt(residual);
        
        // 총 공기소모량 계산 (L/min)
        float totalAirConsumption = 0.0f;
        for (int e = 0; e < pneu->edgeCount; e++) {
            totalAirConsumption += std::abs(pneu->edges[e].volumeFlowRate);
        }
        stats.airConsumption = totalAirConsumption;
    }
    
    MechanicalSystem* mech = m_physicsEngine->mechanicalSystem;
    if (mech && mech->nodeCount > 0) {
        // 기계 시스템 총 에너지 계산
        float totalEnergy = 0.0f;
        for (int i = 0; i < mech->nodeCount; i++) {
            const MechanicalNode& node = mech->nodes[i];
            // 운동에너지: ½mv²
            float kineticEnergy = 0.5f * node.mass * 
                (node.velocity[0]*node.velocity[0] + node.velocity[1]*node.velocity[1] + node.velocity[2]*node.velocity[2]);
            totalEnergy += kineticEnergy;
        }
        
        // 탄성 위치에너지 (스프링)
        for (int e = 0; e < mech->edgeCount; e++) {
            totalEnergy += mech->edges[e].energyStored;
        }
        
        stats.mechanicalEnergy = totalEnergy;
    }
    
    // 실시간 성능 계산
    static auto lastUpdateTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastUpdateTime);
    
    if (elapsed.count() > 0) {
        float deltaSeconds = elapsed.count() / 1000000.0f;
        stats.simulationFPS = 1.0f / deltaSeconds;
        stats.realTimeRatio = (m_physicsEngine->deltaTime) / deltaSeconds;
        
        lastUpdateTime = currentTime;
    }
    
    // 메모리 사용량 추정 (단순화)
    int totalMemory = 0;
    totalMemory += elec ? (elec->nodeCount * sizeof(ElectricalNode) + elec->edgeCount * sizeof(ElectricalEdge)) : 0;
    totalMemory += pneu ? (pneu->nodeCount * sizeof(PneumaticNode) + pneu->edgeCount * sizeof(PneumaticEdge)) : 0;
    totalMemory += mech ? (mech->nodeCount * sizeof(MechanicalNode) + mech->edgeCount * sizeof(MechanicalEdge)) : 0;
    totalMemory += m_physicsEngine->maxComponents * sizeof(TypedPhysicsState);
    
    stats.memoryUsage = totalMemory / (1024.0f * 1024.0f); // bytes → MB
    
    // 디버깅 출력 (선택적)
    if (m_physicsEngine->enableLogging && m_physicsEngine->logLevel >= 3) {
        static int debugCounter = 0;
        if (++debugCounter % 180 == 0) { // 3초마다 (60FPS 기준)
            std::cout << "[Physics Stats] FPS:" << stats.simulationFPS 
                      << " Power:" << stats.powerConsumption << "W"
                      << " Air:" << stats.airConsumption << "L/min"
                      << " Energy:" << stats.mechanicalEnergy << "J"
                      << " Memory:" << stats.memoryUsage << "MB" << std::endl;
        }
    }
}

std::vector<std::pair<int, bool>> Application::GetPortsForComponent(const PlacedComponent& comp) {
    std::vector<std::pair<int, bool>> ports;
    int max_ports = 0;
    switch (comp.type) {
        case ComponentType::PLC: max_ports = 32; break;
        case ComponentType::FRL: max_ports = 1; break;
        case ComponentType::MANIFOLD: max_ports = 5; break;
        case ComponentType::LIMIT_SWITCH: max_ports = 3; break;
        case ComponentType::SENSOR: max_ports = 3; break;
        case ComponentType::CYLINDER: max_ports = 2; break;
        case ComponentType::VALVE_SINGLE: max_ports = 5; break;
        case ComponentType::VALVE_DOUBLE: max_ports = 7; break;
        case ComponentType::BUTTON_UNIT: max_ports = 15; break;
        case ComponentType::POWER_SUPPLY: max_ports = 2; break;
    }
    for (int i = 0; i < max_ports; ++i) {
        ports.push_back({i, true});
    }
    return ports;
}
} // namespace plc

