// src/IOMapper.cpp
// Phase 2: I/O 매핑 시스템 구현 - 실배선 정보를 분석하여 자동 I/O 매핑 생성

#include "plc_emulator/io/io_mapper.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <sstream>
#include <chrono>

namespace plc {

// === IOMapper 생성/초기화 함수들 ===

IOMapper* CreateIOMapper() {
    IOMapper* mapper = new IOMapper();
    if (!mapper) {
        std::cerr << "Error: Failed to allocate IOMapper" << std::endl;
        return nullptr;
    }
    
    // 함수 포인터들 연결
    mapper->ExtractMapping = IOMapper_ExtractMapping;
    mapper->FindPLCComponent = IOMapper_FindPLCComponent;
    mapper->TraceAllConnections = IOMapper_TraceAllConnections;
    mapper->TraceWireConnection = IOMapper_TraceWireConnection;
    mapper->GeneratePLCAddress = IOMapper_GeneratePLCAddress;
    mapper->ValidateConnection = IOMapper_ValidateConnection;
    mapper->ValidateMappingResult = IOMapper_ValidateMappingResult;
    
    // 내부 데이터 초기화
    mapper->isInitialized = false;
    mapper->maxInputPorts = 16;   // FX3U PLC 기본값
    mapper->maxOutputPorts = 16;  // FX3U PLC 기본값
    
    // 추적용 임시 데이터 할당
    mapper->visitedComponents = new std::vector<bool>();
    mapper->bfsQueue = new std::queue<int>();
    
    if (!InitializeIOMapper(mapper)) {
        DestroyIOMapper(mapper);
        return nullptr;
    }
    
    return mapper;
}

void DestroyIOMapper(IOMapper* mapper) {
    if (!mapper) return;
    
    ShutdownIOMapper(mapper);
    
    // 임시 데이터 해제
    delete mapper->visitedComponents;
    delete mapper->bfsQueue;
    
    delete mapper;
}

bool InitializeIOMapper(IOMapper* mapper) {
    if (!mapper) return false;
    
    // 마지막 결과 초기화
    mapper->lastResult = MappingResult();
    
    // 추적용 데이터 초기화
    mapper->visitedComponents->clear();
    while (!mapper->bfsQueue->empty()) {
        mapper->bfsQueue->pop();
    }
    
    mapper->isInitialized = true;
    std::cout << "IOMapper initialized successfully." << std::endl;
    return true;
}

void ShutdownIOMapper(IOMapper* mapper) {
    if (!mapper) return;
    
    // 추적용 데이터 정리
    if (mapper->visitedComponents) {
        mapper->visitedComponents->clear();
    }
    if (mapper->bfsQueue) {
        while (!mapper->bfsQueue->empty()) {
            mapper->bfsQueue->pop();
        }
    }
    
    mapper->isInitialized = false;
    std::cout << "IOMapper shutdown completed." << std::endl;
}

// === 메인 매핑 추출 함수 ===

MappingResult IOMapper_ExtractMapping(IOMapper* mapper,
                                     const std::vector<Wire>* wires,
                                     const std::vector<PlacedComponent>* components) {
    MappingResult result;
    
    if (!mapper || !wires || !components) {
        result.AddError("IOMapper_ExtractMapping: Invalid parameters");
        return result;
    }
    
    if (!mapper->isInitialized) {
        result.AddError("IOMapper_ExtractMapping: IOMapper not initialized");
        return result;
    }
    
    std::cout << "🔍 Starting I/O mapping extraction..." << std::endl;
    
    // === 1단계: PLC 컴포넌트 찾기 ===
    PlacedComponent* plcComponent = mapper->FindPLCComponent(mapper, components);
    if (!plcComponent) {
        result.AddError("No PLC component found in the circuit");
        return result;
    }
    
    result.mapping.plcComponentId = plcComponent->instanceId;
    std::cout << "✅ Found PLC component (ID: " << plcComponent->instanceId << ")" << std::endl;
    
    // === 2단계: 모든 연결 추적 ===
    std::vector<ConnectionTrace> connections = 
        mapper->TraceAllConnections(mapper, plcComponent->instanceId, wires, components);
    
    result.connections = connections;
    std::cout << "🔗 Found " << connections.size() << " connections from PLC" << std::endl;
    
    // === 3단계: 연결별 매핑 생성 ===
    int inputCounter = 0;
    int outputCounter = 0;
    
    for (const auto& connection : connections) {
        // 전기 연결만 매핑 (공압은 Phase 3에서 처리)
        if (!connection.isElectrical) {
            result.AddWarning("Skipped pneumatic connection (will be handled in Phase 3)");
            continue;
        }
        
        // PLC 포트 분석
        bool isInput = IOMapper_IsPLCInputPort(plcComponent, connection.fromPortId);
        std::string plcAddress;
        
        if (isInput && inputCounter < mapper->maxInputPorts) {
            plcAddress = mapper->GeneratePLCAddress(mapper, inputCounter, true, PortType::ELECTRIC);
            inputCounter++;
        } else if (!isInput && outputCounter < mapper->maxOutputPorts) {
            plcAddress = mapper->GeneratePLCAddress(mapper, outputCounter, false, PortType::ELECTRIC);
            outputCounter++;
        } else {
            result.AddError("Exceeded maximum port count (X: 16, Y: 16)");
            continue;
        }
        
        // 대상 컴포넌트 찾기
        auto targetComponent = std::find_if(components->begin(), components->end(),
            [&](const PlacedComponent& comp) {
                return comp.instanceId == connection.toComponentId;
            });
        
        if (targetComponent != components->end()) {
            ComponentMapping mapping(
                connection.toComponentId,
                connection.toPortId,
                plcAddress,
                targetComponent->type,
                IOMapper_GetComponentRole(targetComponent->type)
            );
            
            result.mapping.AddMapping(mapping);
            std::cout << "📍 Mapped " << IOMapper_GetComponentRole(targetComponent->type) 
                     << " (ID:" << connection.toComponentId << ") → " << plcAddress << std::endl;
        }
    }
    
    // === 4단계: 매핑 검증 및 완료 ===
    result.mapping.isValid = result.mapping.ValidateMapping();
    result.success = result.mapping.isValid && !result.HasErrors();
    
    // 타임스탬프 설정
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    result.mapping.lastUpdated = timestamp;
    
    // 결과 저장
    mapper->lastResult = result;
    
    std::cout << "✅ I/O mapping extraction completed. Success: " 
             << (result.success ? "Yes" : "No") << std::endl;
    std::cout << "📊 Generated mappings: " << result.mapping.GetMappingCount() 
             << " (X: " << result.mapping.inputCount 
             << ", Y: " << result.mapping.outputCount << ")" << std::endl;
    
    return result;
}

// === PLC 컴포넌트 관련 함수들 ===

PlacedComponent* IOMapper_FindPLCComponent(IOMapper* mapper,
                                          const std::vector<PlacedComponent>* components) {
    if (!mapper || !components) return nullptr;
    
    for (auto& component : *components) {
        if (component.type == ComponentType::PLC) {
            return const_cast<PlacedComponent*>(&component);
        }
    }
    
    return nullptr;
}

bool IOMapper_IsValidPLCComponent(const PlacedComponent* component) {
    return component && component->type == ComponentType::PLC;
}

// === 배선 추적 관련 함수들 ===

std::vector<ConnectionTrace> IOMapper_TraceAllConnections(IOMapper* mapper,
                                                         int plcComponentId,
                                                         const std::vector<Wire>* wires,
                                                         const std::vector<PlacedComponent>* components) {
    std::vector<ConnectionTrace> connections;
    
    if (!mapper || !wires || !components) return connections;
    
    // PLC의 모든 포트에 대해 연결 추적
    int maxPorts = IOMapper_GetMaxPortCount(ComponentType::PLC); // 32포트
    
    for (int portId = 0; portId < maxPorts; portId++) {
        ConnectionTrace trace = IOMapper_TraceSpecificConnection(
            mapper, plcComponentId, portId, wires, components);
            
        if (trace.toComponentId != -1) {
            connections.push_back(trace);
        }
    }
    
    return connections;
}

std::vector<int> IOMapper_TraceWireConnection(IOMapper* mapper,
                                            int fromComponentId, int fromPortId,
                                            const std::vector<Wire>* wires) {
    std::vector<int> connectedComponents;
    
    if (!mapper || !wires) return connectedComponents;
    
    // 미사용 파라미터 경고 제거
    (void)fromPortId;
    
    // BFS를 위한 초기화
    mapper->visitedComponents->clear();
    mapper->visitedComponents->resize(1000, false); // 충분한 크기로 할당
    
    while (!mapper->bfsQueue->empty()) {
        mapper->bfsQueue->pop();
    }
    
    // BFS 시작
    mapper->bfsQueue->push(fromComponentId);
    (*mapper->visitedComponents)[fromComponentId] = true;
    
    while (!mapper->bfsQueue->empty()) {
        int currentId = mapper->bfsQueue->front();
        mapper->bfsQueue->pop();
        
        // 현재 컴포넌트에서 나가는 모든 와이어 찾기
        for (const auto& wire : *wires) {
            if (!wire.isElectric) continue; // 전기 배선만 추적
            
            int nextId = -1;
            
            if (wire.fromComponentId == currentId) {
                nextId = wire.toComponentId;
            } else if (wire.toComponentId == currentId) {
                nextId = wire.fromComponentId;
            }
            
            if (nextId != -1 && nextId < (int)mapper->visitedComponents->size() &&
                !(*mapper->visitedComponents)[nextId]) {
                
                (*mapper->visitedComponents)[nextId] = true;
                mapper->bfsQueue->push(nextId);
                connectedComponents.push_back(nextId);
            }
        }
    }
    
    return connectedComponents;
}

ConnectionTrace IOMapper_TraceSpecificConnection(IOMapper* mapper,
                                               int plcComponentId, int plcPortId,
                                               const std::vector<Wire>* wires,
                                               const std::vector<PlacedComponent>* components) {
    // 미사용 파라미터 경고 제거
    (void)mapper;
    (void)components;
    
    ConnectionTrace trace;
    trace.fromComponentId = plcComponentId;
    trace.fromPortId = plcPortId;
    
    // 해당 포트에서 나가는 와이어 찾기
    for (const auto& wire : *wires) {
        if (!wire.isElectric) continue;
        
        if (wire.fromComponentId == plcComponentId && wire.fromPortId == plcPortId) {
            trace.toComponentId = wire.toComponentId;
            trace.toPortId = wire.toPortId;
            trace.wireIds.push_back(wire.id);
            trace.isElectrical = true;
            break;
        } else if (wire.toComponentId == plcComponentId && wire.toPortId == plcPortId) {
            trace.toComponentId = wire.fromComponentId;
            trace.toPortId = wire.fromPortId;
            trace.wireIds.push_back(wire.id);
            trace.isElectrical = true;
            break;
        }
    }
    
    return trace;
}

// === PLC 주소 생성 함수들 ===

std::string IOMapper_GeneratePLCAddress(IOMapper* mapper,
                                       int portId, bool isInput, PortType type) {
    if (!mapper) return "";
    
    // 미사용 파라미터 경고 제거
    (void)type;
    
    std::ostringstream oss;
    
    if (isInput) {
        oss << "X" << portId;
    } else {
        oss << "Y" << portId;
    }
    
    return oss.str();
}

bool IOMapper_IsValidPLCAddress(const std::string& address) {
    if (address.length() < 2) return false;
    
    char prefix = address[0];
    if (prefix != 'X' && prefix != 'Y') return false;
    
    std::string numberPart = address.substr(1);
    try {
        int portNum = std::stoi(numberPart);
        return portNum >= 0 && portNum < 16;
    } catch (...) {
        return false;
    }
}

int IOMapper_ParsePLCPortNumber(const std::string& address) {
    if (!IOMapper_IsValidPLCAddress(address)) return -1;
    
    try {
        return std::stoi(address.substr(1));
    } catch (...) {
        return -1;
    }
}

// === 검증 함수들 ===

bool IOMapper_ValidateConnection(IOMapper* mapper,
                                const Wire* wire,
                                const std::vector<PlacedComponent>* components) {
    if (!mapper || !wire || !components) return false;
    
    // 기본 검증: 유효한 컴포넌트 ID인지 확인
    bool fromValid = false, toValid = false;
    
    for (const auto& comp : *components) {
        if (comp.instanceId == wire->fromComponentId) fromValid = true;
        if (comp.instanceId == wire->toComponentId) toValid = true;
    }
    
    return fromValid && toValid && wire->isElectric;
}

bool IOMapper_ValidateMappingResult(IOMapper* mapper,
                                   const MappingResult* result) {
    if (!mapper || !result) return false;
    
    return result->success && result->mapping.isValid && !result->HasErrors();
}

bool IOMapper_CheckCircularConnection(IOMapper* mapper,
                                     int startComponentId,
                                     const std::vector<Wire>* wires) {
    // 미사용 파라미터 경고 제거
    (void)mapper;
    (void)startComponentId;
    (void)wires;
    // 순환 연결 감지 로직 (간단한 버전)
    // 실제로는 더 복잡한 그래프 알고리즘이 필요할 수 있음
    return false; // 현재는 false 반환 (Phase 3에서 개선)
}

// === 유틸리티 함수들 ===

std::vector<Wire*> IOMapper_FindWiresByComponent(const std::vector<Wire>* wires,
                                               int componentId, int portId) {
    std::vector<Wire*> result;
    
    if (!wires) return result;
    
    for (auto& wire : *const_cast<std::vector<Wire>*>(wires)) {
        bool matches = false;
        
        if (portId == -1) {
            // 모든 포트
            matches = (wire.fromComponentId == componentId || wire.toComponentId == componentId);
        } else {
            // 특정 포트
            matches = (wire.fromComponentId == componentId && wire.fromPortId == portId) ||
                     (wire.toComponentId == componentId && wire.toPortId == portId);
        }
        
        if (matches) {
            result.push_back(&wire);
        }
    }
    
    return result;
}

int IOMapper_GetOtherComponentId(const Wire* wire, int currentComponentId) {
    if (!wire) return -1;
    
    if (wire->fromComponentId == currentComponentId) {
        return wire->toComponentId;
    } else if (wire->toComponentId == currentComponentId) {
        return wire->fromComponentId;
    }
    
    return -1;
}

int IOMapper_GetMaxPortCount(ComponentType type) {
    switch (type) {
        case ComponentType::PLC: return 32;
        case ComponentType::MANIFOLD: return 5;
        case ComponentType::VALVE_DOUBLE: return 7;
        case ComponentType::VALVE_SINGLE: return 5;
        case ComponentType::BUTTON_UNIT: return 15;
        case ComponentType::LIMIT_SWITCH: return 3;
        case ComponentType::SENSOR: return 3;
        case ComponentType::CYLINDER: return 2;
        case ComponentType::POWER_SUPPLY: return 2;
        case ComponentType::FRL: return 1;
        default: return 1;
    }
}

std::string IOMapper_GetComponentRole(ComponentType type) {
    switch (type) {
        case ComponentType::PLC: return "PLC";
        case ComponentType::LIMIT_SWITCH: return "LIMIT_SWITCH";
        case ComponentType::SENSOR: return "SENSOR";
        case ComponentType::CYLINDER: return "ACTUATOR";
        case ComponentType::VALVE_SINGLE: return "VALVE";
        case ComponentType::VALVE_DOUBLE: return "VALVE";
        case ComponentType::BUTTON_UNIT: return "BUTTON_PANEL";
        case ComponentType::POWER_SUPPLY: return "POWER_SUPPLY";
        case ComponentType::MANIFOLD: return "MANIFOLD";
        case ComponentType::FRL: return "FRL";
        default: return "UNKNOWN";
    }
}

bool IOMapper_IsElectricPort(const PlacedComponent* component, int portId) {
    // 미사용 파라미터 경고 제거
    (void)component;
    (void)portId;
    // 모든 포트를 전기로 가정 (Phase 3에서 공압 구분 구현)
    return true;
}

bool IOMapper_IsPLCInputPort(const PlacedComponent* plcComponent, int portId) {
    (void)plcComponent; // unused parameter
    // FX3U PLC 포트 규칙: 0-15는 입력(X), 16-31은 출력(Y)
    return portId >= 0 && portId < 16;
}

// === 디버그 및 로깅 함수들 ===

void IOMapper_PrintMappingResult(const MappingResult* result) {
    if (!result) return;
    
    std::cout << "=== I/O Mapping Result ===" << std::endl;
    std::cout << "Success: " << (result->success ? "Yes" : "No") << std::endl;
    std::cout << "Connections: " << result->connections.size() << std::endl;
    std::cout << "Mappings: " << result->mapping.GetMappingCount() << std::endl;
    
    for (const auto& mapping : result->mapping.mappings) {
        std::cout << "  " << mapping.plcAddress << " → Component " 
                 << mapping.componentId << " (" << mapping.componentRole << ")" << std::endl;
    }
    
    if (result->HasErrors()) {
        std::cout << "Errors:" << std::endl;
        for (const auto& error : result->errors) {
            std::cout << "  - " << error << std::endl;
        }
    }
}

void IOMapper_PrintConnectionTrace(const ConnectionTrace* trace) {
    if (!trace) return;
    
    std::cout << "Connection: Component " << trace->fromComponentId 
             << " (Port " << trace->fromPortId << ") → Component " 
             << trace->toComponentId << " (Port " << trace->toPortId << ")" << std::endl;
}

std::string IOMapper_MappingResultToString(const MappingResult* result) {
    if (!result) return "Invalid result";
    
    std::ostringstream oss;
    oss << "I/O Mapping: " << result->mapping.GetMappingCount() << " mappings, ";
    oss << "Success: " << (result->success ? "Yes" : "No");
    
    return oss.str();
}

} // namespace plc