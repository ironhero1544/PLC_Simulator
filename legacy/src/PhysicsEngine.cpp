// src/PhysicsEngine.cpp
// 물리엔진 핵심 구현 - 실제 물리 법칙 기반 시뮬레이션
// 기존 하드코딩된 물리 시뮬레이션을 완전히 대체하는 통합 물리엔진
// Wire와 PlacedComponent로부터 자동으로 물리 네트워크를 구성하고 실시간 시뮬레이션 실행

#include "plc_emulator/physics/physics_engine.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <chrono>

namespace plc {

// ========== 내부 유틸리티 함수들 ==========

namespace PhysicsEngineInternal {

//   메모리 할당/해제 유틸리티
// - 안전한 메모리 관리로 메모리 누수 방지
// - 정렬된 메모리 할당으로 성능 최적화
template<typename T>
T* SafeAllocateArray(int count) {
    if (count <= 0) return nullptr;
    T* ptr = new(std::nothrow) T[count];
    if (ptr) {
        std::memset(ptr, 0, sizeof(T) * count);
    }
    return ptr;
}

template<typename T>
void SafeDeallocateArray(T*& ptr) {
    if (ptr) {
        delete[] ptr;
        ptr = nullptr;
    }
}

// 2차원 배열 할당/해제
template<typename T>
T** Allocate2DArray(int rows, int cols) {
    if (rows <= 0 || cols <= 0) return nullptr;
    
    T** ptr = new(std::nothrow) T*[rows];
    if (!ptr) return nullptr;
    
    for (int i = 0; i < rows; i++) {
        ptr[i] = SafeAllocateArray<T>(cols);
        if (!ptr[i]) {
            // 부분 할당 실패 시 정리
            for (int j = 0; j < i; j++) {
                SafeDeallocateArray(ptr[j]);
            }
            delete[] ptr;
            return nullptr;
        }
    }
    return ptr;
}

template<typename T>  
void Deallocate2DArray(T**& ptr, int rows) {
    if (ptr) {
        for (int i = 0; i < rows; i++) {
            SafeDeallocateArray(ptr[i]);
        }
        delete[] ptr;
        ptr = nullptr;
    }
}

// 컴포넌트 타입별 포트 정보 가져오기
// - 각 컴포넌트 타입의 전기/공압/기계 포트 개수와 특성 정의
// - 기존 Application_Physics.cpp의 GetPortsForComponent 로직 활용
struct PortInfo {
    int electricalPorts;    // 전기 포트 수
    int pneumaticPorts;     // 공압 포트 수  
    int mechanicalPorts;    // 기계 포트 수
    bool isElectricalActive; // 전기 네트워크 참여 여부
    bool isPneumaticActive;  // 공압 네트워크 참여 여부
    bool isMechanicalActive; // 기계 시스템 참여 여부
};

PortInfo GetComponentPortInfo(ComponentType type) {
    PortInfo info = {0, 0, 0, false, false, false};
    
    switch (type) {
        case ComponentType::PLC:
            info.electricalPorts = 32;  // X0-X15(16) + Y0-Y15(16)
            info.isElectricalActive = true;
            break;
            
        case ComponentType::POWER_SUPPLY:
            info.electricalPorts = 2;   // 24V, 0V
            info.isElectricalActive = true;
            break;
            
        case ComponentType::BUTTON_UNIT:
            info.electricalPorts = 15;  // 3개 버튼 × 5포트/버튼
            info.isElectricalActive = true;
            break;
            
        case ComponentType::LIMIT_SWITCH:
            info.electricalPorts = 3;   // COM, NO, NC
            info.mechanicalPorts = 1;   // 물리적 접촉점
            info.isElectricalActive = true;
            info.isMechanicalActive = true;
            break;
            
        case ComponentType::SENSOR:
            info.electricalPorts = 3;   // 24V, 0V, OUT
            info.mechanicalPorts = 1;   // 감지점
            info.isElectricalActive = true;
            info.isMechanicalActive = true;
            break;
            
        case ComponentType::FRL:
            info.pneumaticPorts = 1;    // 압축공기 출구
            info.isPneumaticActive = true;
            break;
            
        case ComponentType::MANIFOLD:
            info.pneumaticPorts = 5;    // 입구1 + 출구4
            info.isPneumaticActive = true;
            break;
            
        case ComponentType::VALVE_SINGLE:
            info.electricalPorts = 2;   // 솔레노이드 +, -
            info.pneumaticPorts = 3;    // P, A, R
            info.isElectricalActive = true;
            info.isPneumaticActive = true;
            break;
            
        case ComponentType::VALVE_DOUBLE:
            info.electricalPorts = 4;   // 솔레노이드A +,-, 솔레노이드B +,-
            info.pneumaticPorts = 3;    // P, A, B 
            info.isElectricalActive = true;
            info.isPneumaticActive = true;
            break;
            
        case ComponentType::CYLINDER:
            info.pneumaticPorts = 2;    // A챔버, B챔버
            info.mechanicalPorts = 1;   // 피스톤 로드
            info.isPneumaticActive = true;
            info.isMechanicalActive = true;
            break;
    }
    
    return info;
}


// 에러 메시지 설정 유틸리티
void SetEngineError(PhysicsEngine* engine, PhysicsEngineError errorCode, const char* message) {
    if (!engine) return;
    
    engine->lastErrorCode = errorCode;
    engine->hasError = true;
    
    if (message) {
        strncpy(engine->lastError, message, sizeof(engine->lastError) - 1);
        engine->lastError[sizeof(engine->lastError) - 1] = '\0';
    }
    
    if (engine->enableLogging && engine->logLevel >= 0) {
        std::cout << "[PhysicsEngine ERROR] Code " << errorCode << ": " << message << std::endl;
    }
}

// 성능 타이머 유틸리티
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point startTime;
    
public:
    void Start() {
        startTime = std::chrono::high_resolution_clock::now();
    }
    
    float GetElapsedMs() const {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        return duration.count() / 1000.0f; // ms로 변환
    }
};

} // namespace PhysicsEngineInternal

// ========== 생명주기 관리 함수 구현 ==========

// 물리엔진 생성 함수 구현
// - 메모리 할당 및 기본 초기화
// - 함수 포인터 설정 및 네트워크 생성
namespace LifecycleFunctions {

int Create(PhysicsEngine* engine, int maxComponents, int maxWires) {
    if (!engine) return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
    if (maxComponents <= 0 || maxWires <= 0) return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
    
    using namespace PhysicsEngineInternal;
    
    //  기본 초기화
    std::memset(engine, 0, sizeof(PhysicsEngine));
    engine->maxComponents = maxComponents;
    engine->activeComponents = 0;
    engine->isInitialized = false;
    
    // 네트워크 시스템 생성 - 실패 시 완전 정리
    engine->electricalNetwork = new(std::nothrow) ElectricalNetwork;
    if (!engine->electricalNetwork) {
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical network");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    engine->pneumaticNetwork = new(std::nothrow) PneumaticNetwork;  
    if (!engine->pneumaticNetwork) {
        delete engine->electricalNetwork;
        engine->electricalNetwork = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic network");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    engine->mechanicalSystem = new(std::nothrow) MechanicalSystem;
    if (!engine->mechanicalSystem) {
        delete engine->electricalNetwork;
        delete engine->pneumaticNetwork;
        engine->electricalNetwork = nullptr;
        engine->pneumaticNetwork = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical system");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    // 컴포넌트 물리 상태 배열 생성 - 실패 시 정리
    engine->componentPhysicsStates = SafeAllocateArray<TypedPhysicsState>(maxComponents);
    if (!engine->componentPhysicsStates) {
        delete engine->electricalNetwork;
        delete engine->pneumaticNetwork;
        delete engine->mechanicalSystem;
        engine->electricalNetwork = nullptr;
        engine->pneumaticNetwork = nullptr;
        engine->mechanicalSystem = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate component physics states");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    engine->componentIdToPhysicsIndex = SafeAllocateArray<int>(maxComponents);
    if (!engine->componentIdToPhysicsIndex) {
        delete engine->electricalNetwork;
        delete engine->pneumaticNetwork;
        delete engine->mechanicalSystem;
        SafeDeallocateArray(engine->componentPhysicsStates);
        engine->electricalNetwork = nullptr;
        engine->pneumaticNetwork = nullptr;
        engine->mechanicalSystem = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate component ID mapping");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    //  매핑 테이블 생성 - 실패 시 완전 정리
    engine->componentPortToElectricalNode = Allocate2DArray<int>(maxComponents, 32);
    if (!engine->componentPortToElectricalNode) {
        delete engine->electricalNetwork;
        delete engine->pneumaticNetwork;
        delete engine->mechanicalSystem;
        SafeDeallocateArray(engine->componentPhysicsStates);
        SafeDeallocateArray(engine->componentIdToPhysicsIndex);
        engine->electricalNetwork = nullptr;
        engine->pneumaticNetwork = nullptr;
        engine->mechanicalSystem = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical mapping table");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    engine->componentPortToPneumaticNode = Allocate2DArray<int>(maxComponents, 16);
    if (!engine->componentPortToPneumaticNode) {
        delete engine->electricalNetwork;
        delete engine->pneumaticNetwork;
        delete engine->mechanicalSystem;
        SafeDeallocateArray(engine->componentPhysicsStates);
        SafeDeallocateArray(engine->componentIdToPhysicsIndex);
        Deallocate2DArray(engine->componentPortToElectricalNode, maxComponents);
        engine->electricalNetwork = nullptr;
        engine->pneumaticNetwork = nullptr;
        engine->mechanicalSystem = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic mapping table");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    engine->componentPortToMechanicalNode = Allocate2DArray<int>(maxComponents, 8);
    if (!engine->componentPortToMechanicalNode) {
        delete engine->electricalNetwork;
        delete engine->pneumaticNetwork;
        delete engine->mechanicalSystem;
        SafeDeallocateArray(engine->componentPhysicsStates);
        SafeDeallocateArray(engine->componentIdToPhysicsIndex);
        Deallocate2DArray(engine->componentPortToElectricalNode, maxComponents);
        Deallocate2DArray(engine->componentPortToPneumaticNode, maxComponents);
        engine->electricalNetwork = nullptr;
        engine->pneumaticNetwork = nullptr;
        engine->mechanicalSystem = nullptr;
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical mapping table");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    // 기본 파라미터 설정
    engine->deltaTime = PhysicsEngineDefaults::DEFAULT_TIME_STEP;
    engine->convergenceTolerance = PhysicsEngineDefaults::DEFAULT_CONVERGENCE_TOLERANCE;
    engine->maxIterations = PhysicsEngineDefaults::DEFAULT_MAX_ITERATIONS;
    engine->enableLogging = true;
    engine->logLevel = PhysicsEngineDefaults::DEFAULT_LOG_LEVEL;
    engine->performanceUpdateInterval = PhysicsEngineDefaults::DEFAULT_PERFORMANCE_UPDATE_INTERVAL;
    
    // 컴포넌트 ID 매핑 초기화 - null pointer 방지
    if (engine->componentIdToPhysicsIndex && 
        engine->componentPortToElectricalNode && 
        engine->componentPortToPneumaticNode && 
        engine->componentPortToMechanicalNode) {
        
        for (int i = 0; i < maxComponents; i++) {
            engine->componentIdToPhysicsIndex[i] = -1;
            
            for (int j = 0; j < 32; j++) {
                engine->componentPortToElectricalNode[i][j] = -1;
            }
            for (int j = 0; j < 16; j++) {
                engine->componentPortToPneumaticNode[i][j] = -1;
            }
            for (int j = 0; j < 8; j++) {
                engine->componentPortToMechanicalNode[i][j] = -1;
            }
        }
    }
    
    return PHYSICS_ENGINE_SUCCESS;
}

int Initialize(PhysicsEngine* engine) {
    if (!engine) return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
    if (engine->isInitialized) return PHYSICS_ENGINE_SUCCESS;
    
    using namespace PhysicsEngineInternal;
    
    // 기본 네트워크 시스템 존재 확인
    if (!engine->electricalNetwork || !engine->pneumaticNetwork || !engine->mechanicalSystem) {
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_NOT_INITIALIZED, "Network systems not created");
        return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;
    }
    
    // 각 네트워크 시스템 초기화
    std::memset(engine->electricalNetwork, 0, sizeof(ElectricalNetwork));
    std::memset(engine->pneumaticNetwork, 0, sizeof(PneumaticNetwork));
    std::memset(engine->mechanicalSystem, 0, sizeof(MechanicalSystem));
    
    // 전기 네트워크 초기화 - 실패 시 정리
    ElectricalNetwork* elec = engine->electricalNetwork;
    elec->maxNodes = engine->maxComponents * 32; // 컴포넌트당 최대 32포트
    elec->maxEdges = engine->maxComponents * 50; // 와이어 여유분
    
    elec->nodes = SafeAllocateArray<ElectricalNode>(elec->maxNodes);
    if (!elec->nodes) {
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical nodes");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    elec->edges = SafeAllocateArray<ElectricalEdge>(elec->maxEdges);
    if (!elec->edges) {
        SafeDeallocateArray(elec->nodes);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical edges");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    elec->adjacencyMatrix = Allocate2DArray<int>(elec->maxNodes, elec->maxNodes);
    if (!elec->adjacencyMatrix) {
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical adjacency matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    elec->conductanceMatrix = Allocate2DArray<float>(elec->maxNodes, elec->maxNodes);
    if (!elec->conductanceMatrix) {
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical conductance matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    elec->currentVector = SafeAllocateArray<float>(elec->maxNodes);
    if (!elec->currentVector) {
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical current vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    elec->voltageVector = SafeAllocateArray<float>(elec->maxNodes);
    if (!elec->voltageVector) {
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate electrical voltage vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    elec->convergenceTolerance = engine->convergenceTolerance;
    elec->maxIterations = engine->maxIterations;
    elec->groundNodeId = -1; // 나중에 설정
    
    //  공압 네트워크 초기화 - 실패 시 전기 네트워크도 정리
    PneumaticNetwork* pneu = engine->pneumaticNetwork;
    pneu->maxNodes = engine->maxComponents * 16;
    pneu->maxEdges = engine->maxComponents * 30;
    
    pneu->nodes = SafeAllocateArray<PneumaticNode>(pneu->maxNodes);
    if (!pneu->nodes) {
        // 전기 네트워크 정리
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic nodes");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    pneu->edges = SafeAllocateArray<PneumaticEdge>(pneu->maxEdges);
    if (!pneu->edges) {
        // 공압 네트워크 일부 정리
        SafeDeallocateArray(pneu->nodes);
        // 전기 네트워크 정리
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic edges");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    pneu->adjacencyMatrix = Allocate2DArray<int>(pneu->maxNodes, pneu->maxNodes);
    if (!pneu->adjacencyMatrix) {
        // 공압 네트워크 일부 정리
        SafeDeallocateArray(pneu->nodes);
        SafeDeallocateArray(pneu->edges);
        // 전기 네트워크 정리
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic adjacency matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    pneu->massFlowVector = SafeAllocateArray<float>(pneu->maxEdges);
    if (!pneu->massFlowVector) {
        // 공압 네트워크 일부 정리
        SafeDeallocateArray(pneu->nodes);
        SafeDeallocateArray(pneu->edges);
        Deallocate2DArray(pneu->adjacencyMatrix, pneu->maxNodes);
        // 전기 네트워크 정리
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic mass flow vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    pneu->pressureVector = SafeAllocateArray<float>(pneu->maxNodes);
    if (!pneu->pressureVector) {
        // 공압 네트워크 일부 정리
        SafeDeallocateArray(pneu->nodes);
        SafeDeallocateArray(pneu->edges);
        Deallocate2DArray(pneu->adjacencyMatrix, pneu->maxNodes);
        SafeDeallocateArray(pneu->massFlowVector);
        // 전기 네트워크 정리
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate pneumatic pressure vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    pneu->convergenceTolerance = engine->convergenceTolerance;
    pneu->maxIterations = engine->maxIterations;
    pneu->atmosphericPressure = PhysicsEngineDefaults::ATMOSPHERIC_PRESSURE;
    pneu->ambientTemperature = 293.15f; // 20°C
    pneu->gasConstant = PhysicsEngineDefaults::GAS_CONSTANT;
    pneu->specificHeatRatio = PhysicsEngineDefaults::SPECIFIC_HEAT_RATIO;
    
    // 기계 시스템 초기화 - 실패 시 전체 네트워크 정리
    MechanicalSystem* mech = engine->mechanicalSystem;
    mech->maxNodes = engine->maxComponents * 8;
    mech->maxEdges = engine->maxComponents * 20;
    
    // 정리 매크로 정의 (코드 중복 방지)
    auto CleanupAllNetworks = [&]() {
        // 공압 네트워크 정리
        SafeDeallocateArray(pneu->nodes);
        SafeDeallocateArray(pneu->edges);
        Deallocate2DArray(pneu->adjacencyMatrix, pneu->maxNodes);
        SafeDeallocateArray(pneu->massFlowVector);
        SafeDeallocateArray(pneu->pressureVector);
        // 전기 네트워크 정리
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
    };
    
    mech->nodes = SafeAllocateArray<MechanicalNode>(mech->maxNodes);
    if (!mech->nodes) {
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical nodes");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->edges = SafeAllocateArray<MechanicalEdge>(mech->maxEdges);
    if (!mech->edges) {
        SafeDeallocateArray(mech->nodes);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical edges");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->adjacencyMatrix = Allocate2DArray<int>(mech->maxNodes, mech->maxNodes);
    if (!mech->adjacencyMatrix) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical adjacency matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    int dof = 6 * mech->maxNodes;
    mech->massMatrix = Allocate2DArray<float>(dof, dof);
    if (!mech->massMatrix) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical mass matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->dampingMatrix = Allocate2DArray<float>(dof, dof);
    if (!mech->dampingMatrix) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        Deallocate2DArray(mech->massMatrix, dof);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical damping matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->stiffnessMatrix = Allocate2DArray<float>(dof, dof);
    if (!mech->stiffnessMatrix) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        Deallocate2DArray(mech->massMatrix, dof);
        Deallocate2DArray(mech->dampingMatrix, dof);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical stiffness matrix");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->displacementVector = SafeAllocateArray<float>(dof);
    if (!mech->displacementVector) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        Deallocate2DArray(mech->massMatrix, dof);
        Deallocate2DArray(mech->dampingMatrix, dof);
        Deallocate2DArray(mech->stiffnessMatrix, dof);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical displacement vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->velocityVector = SafeAllocateArray<float>(dof);
    if (!mech->velocityVector) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        Deallocate2DArray(mech->massMatrix, dof);
        Deallocate2DArray(mech->dampingMatrix, dof);
        Deallocate2DArray(mech->stiffnessMatrix, dof);
        SafeDeallocateArray(mech->displacementVector);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical velocity vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->accelerationVector = SafeAllocateArray<float>(dof);
    if (!mech->accelerationVector) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        Deallocate2DArray(mech->massMatrix, dof);
        Deallocate2DArray(mech->dampingMatrix, dof);
        Deallocate2DArray(mech->stiffnessMatrix, dof);
        SafeDeallocateArray(mech->displacementVector);
        SafeDeallocateArray(mech->velocityVector);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical acceleration vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->forceVector = SafeAllocateArray<float>(dof);
    if (!mech->forceVector) {
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        Deallocate2DArray(mech->massMatrix, dof);
        Deallocate2DArray(mech->dampingMatrix, dof);
        Deallocate2DArray(mech->stiffnessMatrix, dof);
        SafeDeallocateArray(mech->displacementVector);
        SafeDeallocateArray(mech->velocityVector);
        SafeDeallocateArray(mech->accelerationVector);
        CleanupAllNetworks();
        SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION, "Failed to allocate mechanical force vector");
        return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
    }
    
    mech->gravity[0] = 0.0f; mech->gravity[1] = -PhysicsEngineDefaults::GRAVITY; mech->gravity[2] = 0.0f;
    mech->timeStep = engine->deltaTime;
    mech->isStable = true;
    
    engine->isInitialized = true;
    
    if (engine->enableLogging) {
        std::cout << "[PhysicsEngine] Successfully initialized with " 
                  << engine->maxComponents << " max components" << std::endl;
    }
    
    return PHYSICS_ENGINE_SUCCESS;
}

int Reset(PhysicsEngine* engine) {
    if (!engine || !engine->isInitialized) return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;
    
    // 시뮬레이션 상태 리셋
    engine->isRunning = false;
    engine->isPaused = false;
    engine->currentTime = 0.0f;
    engine->stepCount = 0;
    engine->activeComponents = 0;
    engine->hasError = false;
    engine->lastErrorCode = PHYSICS_ENGINE_SUCCESS;
    
    // 네트워크 상태 리셋
    engine->electricalNetwork->nodeCount = 0;
    engine->electricalNetwork->edgeCount = 0;
    engine->electricalNetwork->isConverged = false;
    engine->electricalNetwork->currentIteration = 0;
    
    engine->pneumaticNetwork->nodeCount = 0;
    engine->pneumaticNetwork->edgeCount = 0;
    engine->pneumaticNetwork->isConverged = false;
    engine->pneumaticNetwork->currentIteration = 0;
    
    engine->mechanicalSystem->nodeCount = 0;
    engine->mechanicalSystem->edgeCount = 0;
    engine->mechanicalSystem->currentTime = 0.0f;
    engine->mechanicalSystem->currentStep = 0;
    engine->mechanicalSystem->isStable = true;
    
    // 성능 통계 리셋
    std::memset(&engine->performanceStats, 0, sizeof(PhysicsPerformanceStats));
    
    // 컴포넌트 매핑 리셋
    for (int i = 0; i < engine->maxComponents; i++) {
        engine->componentIdToPhysicsIndex[i] = -1;
    }
    
    if (engine->enableLogging) {
        std::cout << "[PhysicsEngine] System reset completed" << std::endl;
    }
    
    return PHYSICS_ENGINE_SUCCESS;
}

int Destroy(PhysicsEngine* engine) {
    if (!engine) return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
    
    using namespace PhysicsEngineInternal;
    
    // 네트워크 시스템 메모리 해제
    if (engine->electricalNetwork) {
        ElectricalNetwork* elec = engine->electricalNetwork;
        SafeDeallocateArray(elec->nodes);
        SafeDeallocateArray(elec->edges);
        Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
        Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
        SafeDeallocateArray(elec->currentVector);
        SafeDeallocateArray(elec->voltageVector);
        delete engine->electricalNetwork;
        engine->electricalNetwork = nullptr;
    }
    
    if (engine->pneumaticNetwork) {
        PneumaticNetwork* pneu = engine->pneumaticNetwork;
        SafeDeallocateArray(pneu->nodes);
        SafeDeallocateArray(pneu->edges);
        Deallocate2DArray(pneu->adjacencyMatrix, pneu->maxNodes);
        SafeDeallocateArray(pneu->massFlowVector);
        SafeDeallocateArray(pneu->pressureVector);
        delete engine->pneumaticNetwork;
        engine->pneumaticNetwork = nullptr;
    }
    
    if (engine->mechanicalSystem) {
        MechanicalSystem* mech = engine->mechanicalSystem;
        SafeDeallocateArray(mech->nodes);
        SafeDeallocateArray(mech->edges);
        Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
        
        int dof = 6 * mech->maxNodes;
        Deallocate2DArray(mech->massMatrix, dof);
        Deallocate2DArray(mech->dampingMatrix, dof);
        Deallocate2DArray(mech->stiffnessMatrix, dof);
        SafeDeallocateArray(mech->displacementVector);
        SafeDeallocateArray(mech->velocityVector);
        SafeDeallocateArray(mech->accelerationVector);
        SafeDeallocateArray(mech->forceVector);
        delete engine->mechanicalSystem;
        engine->mechanicalSystem = nullptr;
    }
    
    // 컴포넌트 관련 메모리 해제
    SafeDeallocateArray(engine->componentPhysicsStates);
    SafeDeallocateArray(engine->componentIdToPhysicsIndex);
    Deallocate2DArray(engine->componentPortToElectricalNode, engine->maxComponents);
    Deallocate2DArray(engine->componentPortToPneumaticNode, engine->maxComponents);
    Deallocate2DArray(engine->componentPortToMechanicalNode, engine->maxComponents);
    
    // 엔진 상태 초기화
    engine->isInitialized = false;
    engine->maxComponents = 0;
    engine->activeComponents = 0;
    
    if (engine->enableLogging) {
        std::cout << "[PhysicsEngine] Engine destroyed successfully" << std::endl;
    }
    
    //  상태 플래그 설정
    engine->isNetworkBuilt = false;
    engine->isReadyForSimulation = false;
    engine->isElectricalNetworkReady = true;  // 초기화 완료
    engine->isPneumaticNetworkReady = true;   // 초기화 완료
    engine->isMechanicalSystemReady = true;   // 초기화 완료
    
    return PHYSICS_ENGINE_SUCCESS;
}

} // namespace LifecycleFunctions

// ========== [Phase 3: 상태 관리 개선] 상태 검증 함수들 ==========

namespace StateValidationFunctions {

// 엔진 준비 상태 검증
// - 모든 네트워크 시스템이 준비되었는지 확인
// - 메모리 할당이 완료되었는지 확인
bool IsEngineReady(PhysicsEngine* engine) {
    if (!engine) return false;
    
    // 기본 초기화 확인
    if (!engine->isInitialized) return false;
    
    // 네트워크 시스템 존재 확인
    if (!engine->electricalNetwork || !engine->pneumaticNetwork || !engine->mechanicalSystem) {
        return false;
    }
    
    // 컴포넌트 배열 존재 확인
    if (!engine->componentPhysicsStates || !engine->componentIdToPhysicsIndex) {
        return false;
    }
    
    // 매핑 테이블 존재 확인
    if (!engine->componentPortToElectricalNode || 
        !engine->componentPortToPneumaticNode || 
        !engine->componentPortToMechanicalNode) {
        return false;
    }
    
    // 개별 네트워크 준비 상태 확인
    return engine->isElectricalNetworkReady && 
           engine->isPneumaticNetworkReady && 
           engine->isMechanicalSystemReady;
}

//  네트워크 유효성 검증
// - 네트워크 구성이 물리적으로 타당한지 확인
// - 노드와 엣지의 일관성 확인
bool IsNetworkValid(PhysicsEngine* engine) {
    if (!engine || !IsEngineReady(engine)) return false;
    
    // 전기 네트워크 유효성 확인
    ElectricalNetwork* elec = engine->electricalNetwork;
    if (elec->nodeCount < 0 || elec->nodeCount > elec->maxNodes ||
        elec->edgeCount < 0 || elec->edgeCount > elec->maxEdges) {
        return false;
    }
    
    // 공압 네트워크 유효성 확인
    PneumaticNetwork* pneu = engine->pneumaticNetwork;
    if (pneu->nodeCount < 0 || pneu->nodeCount > pneu->maxNodes ||
        pneu->edgeCount < 0 || pneu->edgeCount > pneu->maxEdges) {
        return false;
    }
    
    // 기계 시스템 유효성 확인
    MechanicalSystem* mech = engine->mechanicalSystem;
    if (mech->nodeCount < 0 || mech->nodeCount > mech->maxNodes ||
        mech->edgeCount < 0 || mech->edgeCount > mech->maxEdges) {
        return false;
    }
    
    // 활성 컴포넌트 수 확인
    if (engine->activeComponents < 0 || engine->activeComponents > engine->maxComponents) {
        return false;
    }
    
    return engine->isNetworkBuilt;
}

// 시뮬레이션 실행 안전성 검증
// - 솔버 호출 전 모든 조건이 만족되는지 확인  
// - 메모리 상태 및 수치적 안정성 확인
bool IsSafeToRunSimulation(PhysicsEngine* engine) {
    if (!engine || !IsNetworkValid(engine)) return false;
    
    // 에러 상태 확인
    if (engine->hasError) return false;
    
    // 시뮬레이션 파라미터 유효성 확인
    if (engine->deltaTime <= 0.0f || engine->deltaTime > 1.0f) return false;
    if (engine->maxIterations <= 0 || engine->maxIterations > 10000) return false;
    if (engine->convergenceTolerance <= 0.0f) return false;
    
    // 네트워크별 솔버 준비 상태 확인
    ElectricalNetwork* elec = engine->electricalNetwork;
    if (elec->nodeCount > 0) {
        if (!elec->nodes || !elec->edges || !elec->voltageVector || !elec->currentVector) {
            return false;
        }
    }
    
    PneumaticNetwork* pneu = engine->pneumaticNetwork;
    if (pneu->nodeCount > 0) {
        if (!pneu->nodes || !pneu->edges || !pneu->pressureVector || !pneu->massFlowVector) {
            return false;
        }
    }
    
    MechanicalSystem* mech = engine->mechanicalSystem;
    if (mech->nodeCount > 0) {
        if (!mech->nodes || !mech->edges || !mech->displacementVector || !mech->velocityVector) {
            return false;
        }
    }
    
    return engine->isReadyForSimulation;
}

} // namespace StateValidationFunctions

// ========== 네트워크 구성 함수 구현 ==========

// 배선 정보로부터 물리 네트워크 자동 구성
// - 핵심 기능: Wire와 PlacedComponent를 분석하여 물리 네트워크 생성
// - 기존 하드코딩된 시뮬레이션을 대체하는 가장 중요한 기능
namespace NetworkingFunctions {

int BuildNetworksFromWiring(PhysicsEngine* engine, const Wire* wires, int wireCount,
                           const PlacedComponent* components, int componentCount) {
    if (!engine || !engine->isInitialized) return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;
    if (!wires || !components || wireCount < 0 || componentCount < 0) {
        return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
    }
    
    using namespace PhysicsEngineInternal;
    PerformanceTimer timer;
    timer.Start();
    
    if (engine->enableLogging) {
        std::cout << "[PhysicsEngine] Building networks from " << componentCount 
                  << " components and " << wireCount << " wires..." << std::endl;
    }
    
    // 1. 기존 네트워크 초기화
    engine->electricalNetwork->nodeCount = 0;
    engine->electricalNetwork->edgeCount = 0;
    engine->pneumaticNetwork->nodeCount = 0;
    engine->pneumaticNetwork->edgeCount = 0;
    engine->mechanicalSystem->nodeCount = 0;
    engine->mechanicalSystem->edgeCount = 0;
    
    // 2. 컴포넌트별 노드 생성
    for (int c = 0; c < componentCount; c++) {
        const PlacedComponent& comp = components[c];
        PortInfo portInfo = GetComponentPortInfo(comp.type);
        
        //  컴포넌트 물리 상태 초기화 - 배열 경계 검사 강화
        if (comp.instanceId >= 0 && 
            comp.instanceId < engine->maxComponents && 
            engine->activeComponents < engine->maxComponents &&
            engine->componentIdToPhysicsIndex != nullptr) {
            
            engine->componentIdToPhysicsIndex[comp.instanceId] = engine->activeComponents;
            
            TypedPhysicsState& physState = engine->componentPhysicsStates[engine->activeComponents];
            physState.type = PHYSICS_STATE_NONE; // 일단 초기화, 나중에 타입별로 설정
            
            // 타입별 물리 상태 설정
            switch (comp.type) {
                case ComponentType::PLC:
                    physState.type = PHYSICS_STATE_PLC;
                    // PLC 물리 상태 초기화
                    std::memset(&physState.state.plc, 0, sizeof(PLCPhysicsState));
                    break;
                    
                case ComponentType::CYLINDER: {
                    physState.type = PHYSICS_STATE_CYLINDER;
                    // 실린더 물리 상태 초기화 (기존 internalStates에서 변환)
                    CylinderPhysicsState& cylState = physState.state.cylinder;
                    std::memset(&cylState, 0, sizeof(CylinderPhysicsState));
                    
                    // 기본값 설정
                    cylState.maxStroke = 160.0f;
                    cylState.pistonMass = 0.5f;
                    cylState.pistonAreaA = 314.16f; // π × (10mm)² [cm²] 
                    cylState.pistonAreaB = 235.62f; // π × (10mm)² - π × (5mm)² [cm²] (로드 제외)
                    cylState.staticFriction = 0.3f;
                    cylState.kineticFriction = 0.2f;
                    cylState.viscousDamping = 0.02f;
                    
                    // 기존 internalStates에서 값 복사 (있다면)
                    auto it = comp.internalStates.find("position");
                    if (it != comp.internalStates.end()) {
                        cylState.position = it->second;
                    }
                    it = comp.internalStates.find("velocity");
                    if (it != comp.internalStates.end()) {
                        cylState.velocity = it->second;
                    }
                    break;
                }
                    
                case ComponentType::LIMIT_SWITCH:
                    physState.type = PHYSICS_STATE_LIMIT_SWITCH;
                    std::memset(&physState.state.limitSwitch, 0, sizeof(LimitSwitchPhysicsState));
                    break;
                    
                case ComponentType::SENSOR:
                    physState.type = PHYSICS_STATE_SENSOR;
                    std::memset(&physState.state.sensor, 0, sizeof(SensorPhysicsState));
                    break;
                    
                case ComponentType::VALVE_SINGLE:
                    physState.type = PHYSICS_STATE_VALVE_SINGLE;
                    std::memset(&physState.state.valveSingle, 0, sizeof(ValveSinglePhysicsState));
                    break;
                    
                case ComponentType::VALVE_DOUBLE:
                    physState.type = PHYSICS_STATE_VALVE_DOUBLE;
                    std::memset(&physState.state.valveDouble, 0, sizeof(ValveDoublePhysicsState));
                    break;
                    
                case ComponentType::BUTTON_UNIT:
                    physState.type = PHYSICS_STATE_BUTTON_UNIT;
                    std::memset(&physState.state.buttonUnit, 0, sizeof(ButtonUnitPhysicsState));
                    break;
                    
                // 물리 시뮬레이션이 필요하지 않은 컴포넌트들
                case ComponentType::FRL:
                case ComponentType::MANIFOLD:
                case ComponentType::POWER_SUPPLY:
                default:
                    physState.type = PHYSICS_STATE_NONE;
                    break;
            }
            
            engine->activeComponents++;
        }
        
        // 전기 네트워크 노드 생성
        if (portInfo.isElectricalActive) {
            ElectricalNetwork* elec = engine->electricalNetwork;
            for (int p = 0; p < portInfo.electricalPorts; p++) {
                if (elec->nodeCount >= elec->maxNodes) break;
                
                ElectricalNode& node = elec->nodes[elec->nodeCount];
                std::memset(&node, 0, sizeof(ElectricalNode));
                
                node.base.componentId = comp.instanceId;
                node.base.portId = p;
                node.base.nodeId = elec->nodeCount;
                node.base.isActive = true;
                
                // 컴포넌트 타입별 전기적 특성 설정
                switch (comp.type) {
                    case ComponentType::POWER_SUPPLY:
                        if (p == 0) { // 24V 포트
                            node.isVoltageSource = true;
                            node.sourceVoltage = 24.0f;
                            node.sourceResistance = 0.1f; // 내부저항 0.1Ω
                            engine->electricalNetwork->groundNodeId = elec->nodeCount; // 접지점 설정
                        } else if (p == 1) { // 0V 포트  
                            node.voltage = 0.0f;
                        }
                        break;
                        
                    case ComponentType::PLC:
                        // PLC 입력포트 (X0-X15): 고임피던스
                        if (p < 16) {
                            node.inputImpedance[0] = 10000.0f; // 10kΩ 입력임피던스
                        } else { // 출력포트 (Y0-Y15): 저임피던스 
                            node.outputImpedance[0] = 1.0f; // 1Ω 출력임피던스
                        }
                        break;
                        
                    default:
                        node.nodeCapacitance = 1e-12f; // 기본 1pF 
                        node.leakageConductance = 1e-12f; // 기본 1pS 누설
                        break;
                }
                
                //  매핑 테이블 업데이트 - 범위 검사
                if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
                    p >= 0 && p < 32 && 
                    engine->componentPortToElectricalNode != nullptr) {
                    engine->componentPortToElectricalNode[comp.instanceId][p] = elec->nodeCount;
                }
                elec->nodeCount++;
            }
        }
        
        // 공압 네트워크 노드 생성  
        if (portInfo.isPneumaticActive) {
            PneumaticNetwork* pneu = engine->pneumaticNetwork;
            for (int p = 0; p < portInfo.pneumaticPorts; p++) {
                if (pneu->nodeCount >= pneu->maxNodes) break;
                
                PneumaticNode& node = pneu->nodes[pneu->nodeCount];
                std::memset(&node, 0, sizeof(PneumaticNode));
                
                node.base.componentId = comp.instanceId;
                node.base.portId = p;
                node.base.nodeId = pneu->nodeCount;
                node.base.isActive = true;
                
                // 컴포넌트 타입별 공압 특성 설정
                switch (comp.type) {
                    case ComponentType::FRL:
                        node.isPressureSource = true;
                        node.sourcePressure = 6.0f * 100000.0f; // 6bar → Pa
                        node.sourceTemperature = 293.15f; // 20°C
                        break;
                        
                    case ComponentType::CYLINDER:
                        if (p == 0) { // A챔버
                            node.volume = 0.05f; // 50mL 기본 체적
                        } else if (p == 1) { // B챔버
                            node.volume = 0.03f; // 30mL (로드 체적 제외)
                        }
                        node.temperature = 293.15f;
                        node.pressure = pneu->atmosphericPressure;
                        break;
                        
                    default:
                        node.volume = 0.001f; // 기본 1mL
                        node.temperature = 293.15f;
                        node.pressure = pneu->atmosphericPressure;
                        break;
                }
                
                // 공압 매핑 테이블 업데이트 - 범위 검사
                if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
                    p >= 0 && p < 16 && 
                    engine->componentPortToPneumaticNode != nullptr) {
                    engine->componentPortToPneumaticNode[comp.instanceId][p] = pneu->nodeCount;
                }
                pneu->nodeCount++;
            }
        }
        
        // 기계 시스템 노드 생성
        if (portInfo.isMechanicalActive) {
            MechanicalSystem* mech = engine->mechanicalSystem;
            if (mech->nodeCount < mech->maxNodes) {
                MechanicalNode& node = mech->nodes[mech->nodeCount];
                std::memset(&node, 0, sizeof(MechanicalNode));
                
                node.base.componentId = comp.instanceId;
                node.base.portId = 0;
                node.base.nodeId = mech->nodeCount;
                node.base.isActive = true;
                
                // 컴포넌트 타입별 기계적 특성 설정
                switch (comp.type) {
                    case ComponentType::CYLINDER:
                        node.mass = 0.5f; // 피스톤+로드 질량 0.5kg
                        node.position[0] = 0.0f; // 초기 위치 (완전 수축)
                        // 구속조건: Y, Z축 고정, X축만 이동 가능
                        node.isFixed[1] = node.isFixed[2] = true;
                        node.isFixed[3] = node.isFixed[4] = node.isFixed[5] = true;
                        break;
                        
                    case ComponentType::LIMIT_SWITCH:
                        node.mass = 0.01f; // 스위치 액추에이터 질량 10g
                        // 모든 축 고정 (접촉력만 측정)
                        for (int i = 0; i < 6; i++) node.isFixed[i] = true;
                        break;
                        
                    default:
                        node.mass = 0.001f; // 기본 1g
                        for (int i = 0; i < 6; i++) node.isFixed[i] = true;
                        break;
                }
                
                //  기계 매핑 테이블 업데이트 - 범위 검사
                if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
                    engine->componentPortToMechanicalNode != nullptr) {
                    engine->componentPortToMechanicalNode[comp.instanceId][0] = mech->nodeCount;
                }
                mech->nodeCount++;
            }
        }
    }
    
    // 3. 와이어별 엣지 생성
    for (int w = 0; w < wireCount; w++) {
        const Wire& wire = wires[w];
        
        if (wire.isElectric) {
            // 전기 엣지 생성
            ElectricalNetwork* elec = engine->electricalNetwork;
            if (elec->edgeCount >= elec->maxEdges) continue;
            
            // 와이어 연결 정보로부터 노드 ID 찾기
            int fromNodeId = -1, toNodeId = -1;
            
            for (int n = 0; n < elec->nodeCount; n++) {
                ElectricalNode& node = elec->nodes[n];
                if (node.base.componentId == wire.fromComponentId && 
                    node.base.portId == wire.fromPortId) {
                    fromNodeId = n;
                }
                if (node.base.componentId == wire.toComponentId &&
                    node.base.portId == wire.toPortId) {
                    toNodeId = n;
                }
            }
            
            if (fromNodeId >= 0 && toNodeId >= 0) {
                ElectricalEdge& edge = elec->edges[elec->edgeCount];
                std::memset(&edge, 0, sizeof(ElectricalEdge));
                
                edge.base.edgeId = elec->edgeCount;
                edge.base.fromNodeId = fromNodeId;
                edge.base.toNodeId = toNodeId;
                edge.base.wireId = wire.id;
                edge.base.isActive = true;
                
                // 와이어 물리적 특성으로부터 전기적 특성 계산
                float wireLength = 100.0f; // 기본 100mm (실제로는 wayPoints로 계산해야 함)
                edge.length = wireLength;
                edge.crossSectionArea = 0.5f; // 0.5mm² 전선
                edge.resistivity = 1.7e-8f; // 구리 비저항 [Ω⋅m]
                edge.temperature = 20.0f; // 상온
                
                // 저항 계산: R = ρ × L / A
                edge.resistance = (edge.resistivity * 1000.0f) * (wireLength / 1000.0f) / (edge.crossSectionArea / 1000000.0f);
                edge.conductance = 1.0f / edge.resistance;
                edge.maxCurrent = 1.0f; // 최대 1A
                
                elec->edgeCount++;
            }
        } else {
            // 공압 엣지 생성
            PneumaticNetwork* pneu = engine->pneumaticNetwork;
            if (pneu->edgeCount >= pneu->maxEdges) continue;
            
            int fromNodeId = -1, toNodeId = -1;
            
            for (int n = 0; n < pneu->nodeCount; n++) {
                PneumaticNode& node = pneu->nodes[n];
                if (node.base.componentId == wire.fromComponentId &&
                    node.base.portId == wire.fromPortId) {
                    fromNodeId = n;
                }
                if (node.base.componentId == wire.toComponentId &&
                    node.base.portId == wire.toPortId) {
                    toNodeId = n;
                }
            }
            
            if (fromNodeId >= 0 && toNodeId >= 0) {
                PneumaticEdge& edge = pneu->edges[pneu->edgeCount];
                std::memset(&edge, 0, sizeof(PneumaticEdge));
                
                edge.base.edgeId = pneu->edgeCount;
                edge.base.fromNodeId = fromNodeId;
                edge.base.toNodeId = toNodeId;
                edge.base.wireId = wire.id;
                edge.base.isActive = true;
                
                // 공압 배관 특성 설정 (기본값)
                edge.diameter = 4.0f; // 4mm 내경
                edge.length = 200.0f; // 200mm 길이
                edge.crossSectionArea = 3.14159f * (edge.diameter/2.0f) * (edge.diameter/2.0f); // π×r²
                edge.roughness = 0.05f; // 0.05μm 표면거칠기
                edge.curvature = 0.1f; // 국소손실계수
                
                pneu->edgeCount++;
            }
        }
    }
    
    float buildTime = timer.GetElapsedMs();
    engine->performanceStats.networkBuildTime = buildTime;
    
    //  네트워크 구성 완료 상태 설정
    engine->isNetworkBuilt = true;
    
    // 시뮬레이션 준비 상태 확인 (모든 조건이 만족되면 준비 완료)
    if (StateValidationFunctions::IsEngineReady(engine) && 
        StateValidationFunctions::IsNetworkValid(engine)) {
        engine->isReadyForSimulation = true;
        
        if (engine->enableLogging) {
            std::cout << "[PhysicsEngine] Engine is ready for simulation" << std::endl;
        }
    }
    
    if (engine->enableLogging) {
        std::cout << "[PhysicsEngine] Network built successfully in " << buildTime << "ms" << std::endl;
        std::cout << "  - Electrical: " << engine->electricalNetwork->nodeCount << " nodes, " 
                  << engine->electricalNetwork->edgeCount << " edges" << std::endl;
        std::cout << "  - Pneumatic: " << engine->pneumaticNetwork->nodeCount << " nodes, "
                  << engine->pneumaticNetwork->edgeCount << " edges" << std::endl;
        std::cout << "  - Mechanical: " << engine->mechanicalSystem->nodeCount << " nodes, "
                  << engine->mechanicalSystem->edgeCount << " edges" << std::endl;
    }
    
    return PHYSICS_ENGINE_SUCCESS;
}

int UpdateNetworkTopology(PhysicsEngine* engine) {
    if (!engine || !engine->isInitialized) return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;
    
    // 인접 행렬 업데이트
    ElectricalNetwork* elec = engine->electricalNetwork;
    PneumaticNetwork* pneu = engine->pneumaticNetwork;
    MechanicalSystem* mech = engine->mechanicalSystem;
    
    // 전기 네트워크 인접 행렬 구성
    for (int i = 0; i < elec->nodeCount; i++) {
        for (int j = 0; j < elec->nodeCount; j++) {
            elec->adjacencyMatrix[i][j] = 0;
        }
    }
    
    for (int e = 0; e < elec->edgeCount; e++) {
        ElectricalEdge& edge = elec->edges[e];
        if (edge.base.isActive) {
            int i = edge.base.fromNodeId;
            int j = edge.base.toNodeId;
            if (i >= 0 && i < elec->nodeCount && j >= 0 && j < elec->nodeCount) {
                elec->adjacencyMatrix[i][j] = 1;
                elec->adjacencyMatrix[j][i] = 1;
            }
        }
    }
    
    // 공압 네트워크 인접 행렬 구성
    for (int i = 0; i < pneu->nodeCount; i++) {
        for (int j = 0; j < pneu->nodeCount; j++) {
            pneu->adjacencyMatrix[i][j] = 0;
        }
    }
    
    for (int e = 0; e < pneu->edgeCount; e++) {
        PneumaticEdge& edge = pneu->edges[e];
        if (edge.base.isActive) {
            int i = edge.base.fromNodeId;
            int j = edge.base.toNodeId;
            if (i >= 0 && i < pneu->nodeCount && j >= 0 && j < pneu->nodeCount) {
                pneu->adjacencyMatrix[i][j] = 1;
                pneu->adjacencyMatrix[j][i] = 1;
            }
        }
    }
    
    // 기계 시스템 인접 행렬 구성
    for (int i = 0; i < mech->nodeCount; i++) {
        for (int j = 0; j < mech->nodeCount; j++) {
            mech->adjacencyMatrix[i][j] = 0;
        }
    }
    
    for (int e = 0; e < mech->edgeCount; e++) {
        MechanicalEdge& edge = mech->edges[e];
        if (edge.base.isActive) {
            int i = edge.base.fromNodeId;
            int j = edge.base.toNodeId;
            if (i >= 0 && i < mech->nodeCount && j >= 0 && j < mech->nodeCount) {
                mech->adjacencyMatrix[i][j] = 1;
                mech->adjacencyMatrix[j][i] = 1;
            }
        }
    }
    
    return PHYSICS_ENGINE_SUCCESS;
}

} // namespace NetworkingFunctions

// ========== 외부 C 인터페이스 함수들 ==========

extern "C" {

// 물리엔진 팩토리 함수들
PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires) {
    PhysicsEngine* engine = new(std::nothrow) PhysicsEngine;
    if (!engine) return nullptr;
    
    //  Create() 호출 먼저 - memset이 내부에서 실행됨
    // 임시로 Create 함수 포인터만 설정
    engine->lifecycle.Create = LifecycleFunctions::Create;
    
    if (engine->lifecycle.Create(engine, maxComponents, maxWires) != PHYSICS_ENGINE_SUCCESS) {
        delete engine;
        return nullptr;
    }
    
    //  memset 이후에 모든 함수 포인터 재설정
    engine->lifecycle.Create = LifecycleFunctions::Create;
    engine->lifecycle.Initialize = LifecycleFunctions::Initialize;
    engine->lifecycle.Reset = LifecycleFunctions::Reset;
    engine->lifecycle.Destroy = LifecycleFunctions::Destroy;
    
    engine->networking.BuildNetworksFromWiring = NetworkingFunctions::BuildNetworksFromWiring;
    engine->networking.UpdateNetworkTopology = NetworkingFunctions::UpdateNetworkTopology;
    
    //  상태 검증 함수 포인터 설정
    engine->IsEngineReady = StateValidationFunctions::IsEngineReady;
    engine->IsNetworkValid = StateValidationFunctions::IsNetworkValid;
    engine->IsSafeToRunSimulation = StateValidationFunctions::IsSafeToRunSimulation;
    
    //  물리엔진 초기화 - 함수 포인터 재설정 후 Initialize 호출
    if (!engine->lifecycle.Initialize || !engine->lifecycle.Destroy) {
        if (engine->lifecycle.Destroy) {
            engine->lifecycle.Destroy(engine);
        }
        delete engine;
        return nullptr;
    }
    
    if (engine->lifecycle.Initialize(engine) != PHYSICS_ENGINE_SUCCESS) {
        if (engine->lifecycle.Destroy) {
            engine->lifecycle.Destroy(engine);
        }
        delete engine;
        return nullptr;
    }
    
    return engine;
}

PhysicsEngine* CreateDefaultPhysicsEngine() {
    return CreatePhysicsEngine(
        PhysicsEngineDefaults::DEFAULT_MAX_COMPONENTS,
        PhysicsEngineDefaults::DEFAULT_MAX_WIRES
    );
}

void DestroyPhysicsEngine(PhysicsEngine* engine) {
    // 안전한 엔진 소멸 - null 체크 및 함수 포인터 검증
    if (engine) {
        if (engine->lifecycle.Destroy) {
            engine->lifecycle.Destroy(engine);
        }
        delete engine;
    }
}

} // extern "C"

// ========== 에러 메시지 변환 함수 ==========

const char* PhysicsEngineErrorToString(PhysicsEngineError error) {
    switch (error) {
        case PHYSICS_ENGINE_SUCCESS: return "Success";
        case PHYSICS_ENGINE_ERROR_INITIALIZATION: return "Initialization failed";
        case PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case PHYSICS_ENGINE_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case PHYSICS_ENGINE_ERROR_NOT_INITIALIZED: return "Engine not initialized";
        case PHYSICS_ENGINE_ERROR_NETWORK_BUILD: return "Network build failed";
        case PHYSICS_ENGINE_ERROR_INVALID_TOPOLOGY: return "Invalid network topology";
        case PHYSICS_ENGINE_ERROR_DISCONNECTED_NETWORK: return "Disconnected network";
        case PHYSICS_ENGINE_ERROR_COMPONENT_NOT_FOUND: return "Component not found";
        case PHYSICS_ENGINE_ERROR_SIMULATION_FAILED: return "Simulation failed";
        case PHYSICS_ENGINE_ERROR_CONVERGENCE_FAILED: return "Convergence failed";
        case PHYSICS_ENGINE_ERROR_NUMERICAL_INSTABILITY: return "Numerical instability";
        case PHYSICS_ENGINE_ERROR_PHYSICAL_INCONSISTENCY: return "Physical inconsistency";
        case PHYSICS_ENGINE_ERROR_DATA_SYNC: return "Data synchronization failed";
        case PHYSICS_ENGINE_ERROR_INVALID_STATE: return "Invalid state";
        case PHYSICS_ENGINE_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case PHYSICS_ENGINE_ERROR_TYPE_MISMATCH: return "Type mismatch";
        case PHYSICS_ENGINE_ERROR_SYSTEM: return "System error";
        case PHYSICS_ENGINE_ERROR_THREAD_FAILURE: return "Thread failure";
        case PHYSICS_ENGINE_ERROR_IO_ERROR: return "I/O error";
        default: return "Unknown error";
    }
}

} // namespace plc