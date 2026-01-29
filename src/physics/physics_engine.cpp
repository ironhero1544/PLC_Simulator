// physics_engine.cpp
//
// Implementation of physics engine.

// src/PhysicsEngine.cpp
// 물리?�진 ?�심 구현 - ?�제 물리 법칙 기반 ?��??�이??
// 기존 ?�드코딩??물리 ?��??�이?�을 ?�전???�체하???�합 물리?�진
// Wire?� PlacedComponent로�????�동?�로 물리 ?�트?�크�?구성?�고 ?�시�?
// ?��??�이???�행

#include "plc_emulator/physics/physics_engine.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace plc {

namespace {
PhysicsWarningCallback g_warning_callback = nullptr;
}

void SetPhysicsWarningCallback(PhysicsWarningCallback callback) {
  g_warning_callback = callback;
}

void NotifyPhysicsWarning(const char* message) {
  if (g_warning_callback && message) {
    g_warning_callback(message);
  }
}



namespace PhysicsEngineInternal {

//   메모�??�당/?�제 ?�틸리티
// - ?�전??메모�?관리로 메모�??�수 방�?
// - ?�렬??메모�??�당?�로 ?�능 최적??
template <typename T>
T* SafeAllocateArray(int count) {
  if (count <= 0)
    return nullptr;
  T* ptr = new (std::nothrow) T[count];
  if (ptr) {
    std::memset(ptr, 0, sizeof(T) * count);
  }
  return ptr;
}

template <typename T>
void SafeDeallocateArray(T*& ptr) {
  if (ptr) {
    delete[] ptr;
    ptr = nullptr;
  }
}

// 2차원 배열 ?�당/?�제
template <typename T>
T** Allocate2DArray(int rows, int cols) {
  if (rows <= 0 || cols <= 0)
    return nullptr;

  T** ptr = new (std::nothrow) T*[rows];
  if (!ptr)
    return nullptr;

  for (int i = 0; i < rows; i++) {
    ptr[i] = SafeAllocateArray<T>(cols);
    if (!ptr[i]) {
      // 부�??�당 ?�패 ???�리
      for (int j = 0; j < i; j++) {
        SafeDeallocateArray(ptr[j]);
      }
      delete[] ptr;
      return nullptr;
    }
  }
  return ptr;
}

template <typename T>
void Deallocate2DArray(T**& ptr, int rows) {
  if (ptr) {
    for (int i = 0; i < rows; i++) {
      SafeDeallocateArray(ptr[i]);
    }
    delete[] ptr;
    ptr = nullptr;
  }
}

// 컴포?�트 ?�?�별 ?�트 ?�보 가?�오�?
// - �?컴포?�트 ?�?�의 ?�기/공압/기계 ?�트 개수?� ?�성 ?�의
// - 기존 Application_Physics.cpp??GetPortsForComponent 로직 ?�용
struct PortInfo {
  int electricalPorts;      // ?�기 ?�트 ??
  int pneumaticPorts;       // 공압 ?�트 ??
  int mechanicalPorts;      // 기계 ?�트 ??
  bool isElectricalActive;  // ?�기 ?�트?�크 참여 ?��?
  bool isPneumaticActive;   // 공압 ?�트?�크 참여 ?��?
  bool isMechanicalActive;  // 기계 ?�스??참여 ?��?
};

PortInfo GetComponentPortInfo(ComponentType type) {
  PortInfo info = {0, 0, 0, false, false, false};

  switch (type) {
    case ComponentType::PLC:
      info.electricalPorts = 32;  // X0-X15(16) + Y0-Y15(16)
      info.isElectricalActive = true;
      break;

    case ComponentType::POWER_SUPPLY:
      info.electricalPorts = 2;  // 24V, 0V
      info.isElectricalActive = true;
      break;

    case ComponentType::BUTTON_UNIT:
      info.electricalPorts = 15;  // 3�?버튼 × 5?�트/버튼
      info.isElectricalActive = true;
      break;

    case ComponentType::LIMIT_SWITCH:
      info.electricalPorts = 3;  // COM, NO, NC
      info.mechanicalPorts = 1;  // 물리???�촉??
      info.isElectricalActive = true;
      info.isMechanicalActive = true;
      break;

    case ComponentType::SENSOR:
      info.electricalPorts = 3;  // 24V, 0V, OUT
      info.mechanicalPorts = 1;  // 감�???
      info.isElectricalActive = true;
      info.isMechanicalActive = true;
      break;

    case ComponentType::FRL:
      info.pneumaticPorts = 1;  // ?�축공기 출구
      info.isPneumaticActive = true;
      break;

    case ComponentType::MANIFOLD:
      info.pneumaticPorts = 5;  // ?�구1 + 출구4
      info.isPneumaticActive = true;
      break;

    case ComponentType::VALVE_SINGLE:
      info.electricalPorts = 2;  // ?�레?�이??+, -
      info.pneumaticPorts = 3;   // P, A, R
      info.isElectricalActive = true;
      info.isPneumaticActive = true;
      break;

    case ComponentType::VALVE_DOUBLE:
      info.electricalPorts = 4;  // ?�레?�이?�A +,-, ?�레?�이?�B +,-
      info.pneumaticPorts = 3;   // P, A, B
      info.isElectricalActive = true;
      info.isPneumaticActive = true;
      break;

    case ComponentType::CYLINDER:
      info.pneumaticPorts = 2;   // A챔버, B챔버
      info.mechanicalPorts = 1;  // ?�스??로드
      info.isPneumaticActive = true;
      info.isMechanicalActive = true;
      break;

    case ComponentType::WORKPIECE_METAL:
    case ComponentType::WORKPIECE_NONMETAL:
      break;

    case ComponentType::RING_SENSOR:
      info.electricalPorts = 3;
      info.isElectricalActive = true;
      break;

    case ComponentType::METER_VALVE:
      info.pneumaticPorts = 2;
      info.isPneumaticActive = true;
      break;

    case ComponentType::INDUCTIVE_SENSOR:
      info.electricalPorts = 3;
      info.isElectricalActive = true;
      break;

    case ComponentType::CONVEYOR:
      info.electricalPorts = 2;
      info.isElectricalActive = true;
      break;

    case ComponentType::PROCESSING_CYLINDER:
      info.electricalPorts = 5;
      info.pneumaticPorts = 2;
      info.isElectricalActive = true;
      info.isPneumaticActive = true;
      break;

    case ComponentType::BOX:
      break;

    case ComponentType::TOWER_LAMP:
      info.electricalPorts = 4;
      info.isElectricalActive = true;
      break;

    case ComponentType::EMERGENCY_STOP:
      info.electricalPorts = 4;
      info.isElectricalActive = true;
      break;
  }

  return info;
}

// ?�러 메시지 ?�정 ?�틸리티
void SetEngineError(PhysicsEngine* engine, PhysicsEngineError errorCode,
                    const char* message) {
  if (!engine)
    return;

  engine->lastErrorCode = errorCode;
  engine->hasError = true;

  if (message) {
    strncpy(engine->lastError, message, sizeof(engine->lastError) - 1);
    engine->lastError[sizeof(engine->lastError) - 1] = '\0';
  }

  if (engine->enableLogging && engine->logLevel >= 0) {
    std::cout << "[PhysicsEngine ERROR] Code " << errorCode << ": " << message
              << std::endl;
  }
}

// ?�능 ?�?�머 ?�틸리티
class PerformanceTimer {
 private:
  std::chrono::high_resolution_clock::time_point startTime;

 public:
  void Start() { startTime = std::chrono::high_resolution_clock::now(); }

  float GetElapsedMs() const {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);
    return duration.count() / 1000.0f;  // ms�?변??
  }
};

}  // namespace PhysicsEngineInternal


// 물리?�진 ?�성 ?�수 구현
// - 메모�??�당 �?기본 초기??
// - ?�수 ?�인???�정 �??�트?�크 ?�성
namespace LifecycleFunctions {

int Create(PhysicsEngine* engine, int maxComponents, int maxWires) {
  if (!engine)
    return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
  if (maxComponents <= 0 || maxWires <= 0)
    return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;

  using namespace PhysicsEngineInternal;

  //  기본 초기??
  std::memset(engine, 0, sizeof(PhysicsEngine));
  engine->maxComponents = maxComponents;
  engine->activeComponents = 0;
  engine->isInitialized = false;

  // ?�트?�크 ?�스???�성 - ?�패 ???�전 ?�리
  engine->electricalNetwork = new (std::nothrow) ElectricalNetwork;
  if (!engine->electricalNetwork) {
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical network");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  engine->pneumaticNetwork = new (std::nothrow) PneumaticNetwork;
  if (!engine->pneumaticNetwork) {
    delete engine->electricalNetwork;
    engine->electricalNetwork = nullptr;
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic network");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  engine->mechanicalSystem = new (std::nothrow) MechanicalSystem;
  if (!engine->mechanicalSystem) {
    delete engine->electricalNetwork;
    delete engine->pneumaticNetwork;
    engine->electricalNetwork = nullptr;
    engine->pneumaticNetwork = nullptr;
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical system");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  // 컴포?�트 물리 ?�태 배열 ?�성 - ?�패 ???�리
  engine->componentPhysicsStates =
      SafeAllocateArray<TypedPhysicsState>(maxComponents);
  if (!engine->componentPhysicsStates) {
    delete engine->electricalNetwork;
    delete engine->pneumaticNetwork;
    delete engine->mechanicalSystem;
    engine->electricalNetwork = nullptr;
    engine->pneumaticNetwork = nullptr;
    engine->mechanicalSystem = nullptr;
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate component physics states");
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
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate component ID mapping");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  //  매핑 ?�이�??�성 - ?�패 ???�전 ?�리
  engine->componentPortToElectricalNode =
      Allocate2DArray<int>(maxComponents, 32);
  if (!engine->componentPortToElectricalNode) {
    delete engine->electricalNetwork;
    delete engine->pneumaticNetwork;
    delete engine->mechanicalSystem;
    SafeDeallocateArray(engine->componentPhysicsStates);
    SafeDeallocateArray(engine->componentIdToPhysicsIndex);
    engine->electricalNetwork = nullptr;
    engine->pneumaticNetwork = nullptr;
    engine->mechanicalSystem = nullptr;
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical mapping table");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  engine->componentPortToPneumaticNode =
      Allocate2DArray<int>(maxComponents, 16);
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
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic mapping table");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  engine->componentPortToMechanicalNode =
      Allocate2DArray<int>(maxComponents, 8);
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
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical mapping table");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  // 기본 ?�라미터 ?�정
  engine->deltaTime = PhysicsEngineDefaults::DEFAULT_TIME_STEP;
  engine->convergenceTolerance =
      PhysicsEngineDefaults::DEFAULT_CONVERGENCE_TOLERANCE;
  engine->maxIterations = PhysicsEngineDefaults::DEFAULT_MAX_ITERATIONS;
  engine->enableLogging = true;
  engine->logLevel = PhysicsEngineDefaults::DEFAULT_LOG_LEVEL;
  engine->performanceUpdateInterval =
      PhysicsEngineDefaults::DEFAULT_PERFORMANCE_UPDATE_INTERVAL;

  // 컴포?�트 ID 매핑 초기??- null pointer 방�?
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

// Helper functions for Initialize
namespace {

using PhysicsEngineInternal::SafeAllocateArray;
using PhysicsEngineInternal::SafeDeallocateArray;
using PhysicsEngineInternal::Allocate2DArray;
using PhysicsEngineInternal::Deallocate2DArray;
using PhysicsEngineInternal::SetEngineError;

void CleanupElectricalNetwork(ElectricalNetwork* elec) {
  if (!elec) return;
  SafeDeallocateArray(elec->nodes);
  SafeDeallocateArray(elec->edges);
  if (elec->adjacencyMatrix) {
    Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
  }
  if (elec->conductanceMatrix) {
    Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
  }
  SafeDeallocateArray(elec->currentVector);
  SafeDeallocateArray(elec->voltageVector);
}

void CleanupPneumaticNetwork(PneumaticNetwork* pneu) {
  if (!pneu) return;
  SafeDeallocateArray(pneu->nodes);
  SafeDeallocateArray(pneu->edges);
  if (pneu->adjacencyMatrix) {
    Deallocate2DArray(pneu->adjacencyMatrix, pneu->maxNodes);
  }
  SafeDeallocateArray(pneu->massFlowVector);
  SafeDeallocateArray(pneu->pressureVector);
}

void CleanupMechanicalSystem(MechanicalSystem* mech) {
  if (!mech) return;
  SafeDeallocateArray(mech->nodes);
  SafeDeallocateArray(mech->edges);
  if (mech->adjacencyMatrix) {
    Deallocate2DArray(mech->adjacencyMatrix, mech->maxNodes);
  }
  int dof = 6 * mech->maxNodes;
  if (mech->massMatrix) {
    Deallocate2DArray(mech->massMatrix, dof);
  }
  if (mech->dampingMatrix) {
    Deallocate2DArray(mech->dampingMatrix, dof);
  }
  if (mech->stiffnessMatrix) {
    Deallocate2DArray(mech->stiffnessMatrix, dof);
  }
  SafeDeallocateArray(mech->displacementVector);
  SafeDeallocateArray(mech->velocityVector);
  SafeDeallocateArray(mech->accelerationVector);
  SafeDeallocateArray(mech->forceVector);
}

int InitializeElectricalNetwork(PhysicsEngine* engine) {
  ElectricalNetwork* elec = engine->electricalNetwork;
  elec->maxNodes = engine->maxComponents * 32;
  elec->maxEdges = engine->maxComponents * 50;

  elec->nodes = SafeAllocateArray<ElectricalNode>(elec->maxNodes);
  if (!elec->nodes) {
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical nodes");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  elec->edges = SafeAllocateArray<ElectricalEdge>(elec->maxEdges);
  if (!elec->edges) {
    CleanupElectricalNetwork(elec);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical edges");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  elec->adjacencyMatrix = Allocate2DArray<int>(elec->maxNodes, elec->maxNodes);
  if (!elec->adjacencyMatrix) {
    CleanupElectricalNetwork(elec);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical adjacency matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  elec->conductanceMatrix =
      Allocate2DArray<float>(elec->maxNodes, elec->maxNodes);
  if (!elec->conductanceMatrix) {
    CleanupElectricalNetwork(elec);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical conductance matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  elec->currentVector = SafeAllocateArray<float>(elec->maxNodes);
  if (!elec->currentVector) {
    CleanupElectricalNetwork(elec);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical current vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  elec->voltageVector = SafeAllocateArray<float>(elec->maxNodes);
  if (!elec->voltageVector) {
    CleanupElectricalNetwork(elec);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical voltage vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  elec->convergenceTolerance = engine->convergenceTolerance;
  elec->maxIterations = engine->maxIterations;
  elec->groundNodeId = -1;

  return PHYSICS_ENGINE_SUCCESS;
}

int InitializePneumaticNetwork(PhysicsEngine* engine) {
  PneumaticNetwork* pneu = engine->pneumaticNetwork;
  pneu->maxNodes = engine->maxComponents * 16;
  pneu->maxEdges = engine->maxComponents * 30;

  pneu->nodes = SafeAllocateArray<PneumaticNode>(pneu->maxNodes);
  if (!pneu->nodes) {
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic nodes");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->edges = SafeAllocateArray<PneumaticEdge>(pneu->maxEdges);
  if (!pneu->edges) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic edges");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->adjacencyMatrix = Allocate2DArray<int>(pneu->maxNodes, pneu->maxNodes);
  if (!pneu->adjacencyMatrix) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic adjacency matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->massFlowVector = SafeAllocateArray<float>(pneu->maxEdges);
  if (!pneu->massFlowVector) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic mass flow vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->pressureVector = SafeAllocateArray<float>(pneu->maxNodes);
  if (!pneu->pressureVector) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic pressure vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->convergenceTolerance = engine->convergenceTolerance;
  pneu->maxIterations = engine->maxIterations;
  pneu->atmosphericPressure = PhysicsEngineDefaults::ATMOSPHERIC_PRESSURE;
  pneu->ambientTemperature = 293.15f;
  pneu->gasConstant = PhysicsEngineDefaults::GAS_CONSTANT;
  pneu->specificHeatRatio = PhysicsEngineDefaults::SPECIFIC_HEAT_RATIO;

  return PHYSICS_ENGINE_SUCCESS;
}

int InitializeMechanicalSystem(PhysicsEngine* engine) {
  MechanicalSystem* mech = engine->mechanicalSystem;
  mech->maxNodes = engine->maxComponents * 8;
  mech->maxEdges = engine->maxComponents * 20;

  mech->nodes = SafeAllocateArray<MechanicalNode>(mech->maxNodes);
  if (!mech->nodes) {
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical nodes");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->edges = SafeAllocateArray<MechanicalEdge>(mech->maxEdges);
  if (!mech->edges) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical edges");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->adjacencyMatrix = Allocate2DArray<int>(mech->maxNodes, mech->maxNodes);
  if (!mech->adjacencyMatrix) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical adjacency matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  int dof = 6 * mech->maxNodes;
  mech->massMatrix = Allocate2DArray<float>(dof, dof);
  if (!mech->massMatrix) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical mass matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->dampingMatrix = Allocate2DArray<float>(dof, dof);
  if (!mech->dampingMatrix) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical damping matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->stiffnessMatrix = Allocate2DArray<float>(dof, dof);
  if (!mech->stiffnessMatrix) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical stiffness matrix");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->displacementVector = SafeAllocateArray<float>(dof);
  if (!mech->displacementVector) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical displacement vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->velocityVector = SafeAllocateArray<float>(dof);
  if (!mech->velocityVector) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical velocity vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->accelerationVector = SafeAllocateArray<float>(dof);
  if (!mech->accelerationVector) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical acceleration vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->forceVector = SafeAllocateArray<float>(dof);
  if (!mech->forceVector) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical force vector");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  mech->gravity[0] = 0.0f;
  mech->gravity[1] = -PhysicsEngineDefaults::GRAVITY;
  mech->gravity[2] = 0.0f;
  mech->timeStep = engine->deltaTime;
  mech->isStable = true;

  return PHYSICS_ENGINE_SUCCESS;
}

}  // anonymous namespace

int Initialize(PhysicsEngine* engine) {
  if (!engine)
    return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
  if (engine->isInitialized)
    return PHYSICS_ENGINE_SUCCESS;

  using namespace PhysicsEngineInternal;

  if (!engine->electricalNetwork || !engine->pneumaticNetwork ||
      !engine->mechanicalSystem) {
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_NOT_INITIALIZED,
                   "Network systems not created");
    return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;
  }

  std::memset(engine->electricalNetwork, 0, sizeof(ElectricalNetwork));
  std::memset(engine->pneumaticNetwork, 0, sizeof(PneumaticNetwork));
  std::memset(engine->mechanicalSystem, 0, sizeof(MechanicalSystem));

  int result = InitializeElectricalNetwork(engine);
  if (result != PHYSICS_ENGINE_SUCCESS) {
    return result;
  }

  result = InitializePneumaticNetwork(engine);
  if (result != PHYSICS_ENGINE_SUCCESS) {
    CleanupElectricalNetwork(engine->electricalNetwork);
    return result;
  }

  result = InitializeMechanicalSystem(engine);
  if (result != PHYSICS_ENGINE_SUCCESS) {
    CleanupPneumaticNetwork(engine->pneumaticNetwork);
    CleanupElectricalNetwork(engine->electricalNetwork);
    return result;
  }

  engine->isInitialized = true;

  if (engine->enableLogging) {
    std::cout << "[PhysicsEngine] Successfully initialized with "
              << engine->maxComponents << " max components" << std::endl;
  }

  return PHYSICS_ENGINE_SUCCESS;
}

int Reset(PhysicsEngine* engine) {
  if (!engine || !engine->isInitialized)
    return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;

  // ?��??�이???�태 리셋
  engine->isRunning = false;
  engine->isPaused = false;
  engine->currentTime = 0.0f;
  engine->stepCount = 0;
  engine->activeComponents = 0;
  engine->hasError = false;
  engine->lastErrorCode = PHYSICS_ENGINE_SUCCESS;

  // ?�트?�크 ?�태 리셋
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

  // ?�능 ?�계 리셋
  std::memset(&engine->performanceStats, 0, sizeof(PhysicsPerformanceStats));

  // 컴포?�트 매핑 리셋
  for (int i = 0; i < engine->maxComponents; i++) {
    engine->componentIdToPhysicsIndex[i] = -1;
  }

  if (engine->enableLogging) {
    std::cout << "[PhysicsEngine] System reset completed" << std::endl;
  }

  return PHYSICS_ENGINE_SUCCESS;
}

int Destroy(PhysicsEngine* engine) {
  if (!engine)
    return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;

  using namespace PhysicsEngineInternal;

  // ?�트?�크 ?�스??메모�??�제
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

  // 컴포?�트 관??메모�??�제
  SafeDeallocateArray(engine->componentPhysicsStates);
  SafeDeallocateArray(engine->componentIdToPhysicsIndex);
  Deallocate2DArray(engine->componentPortToElectricalNode,
                    engine->maxComponents);
  Deallocate2DArray(engine->componentPortToPneumaticNode,
                    engine->maxComponents);
  Deallocate2DArray(engine->componentPortToMechanicalNode,
                    engine->maxComponents);

  // ?�진 ?�태 초기??
  engine->isInitialized = false;
  engine->maxComponents = 0;
  engine->activeComponents = 0;

  if (engine->enableLogging) {
    std::cout << "[PhysicsEngine] Engine destroyed successfully" << std::endl;
  }

  //  ?�태 ?�래�??�정
  engine->isNetworkBuilt = false;
  engine->isReadyForSimulation = false;
  engine->isElectricalNetworkReady = true;  // 초기???�료
  engine->isPneumaticNetworkReady = true;   // 초기???�료
  engine->isMechanicalSystemReady = true;   // 초기???�료

  return PHYSICS_ENGINE_SUCCESS;
}

}  // namespace LifecycleFunctions


namespace StateValidationFunctions {

// ?�진 준�??�태 검�?
// - 모든 ?�트?�크 ?�스?�이 준비되?�는지 ?�인
// - 메모�??�당???�료?�었?��? ?�인
bool IsEngineReady(PhysicsEngine* engine) {
  if (!engine)
    return false;

  // 기본 초기???�인
  if (!engine->isInitialized)
    return false;

  // ?�트?�크 ?�스??존재 ?�인
  if (!engine->electricalNetwork || !engine->pneumaticNetwork ||
      !engine->mechanicalSystem) {
    return false;
  }

  // 컴포?�트 배열 존재 ?�인
  if (!engine->componentPhysicsStates || !engine->componentIdToPhysicsIndex) {
    return false;
  }

  // 매핑 ?�이�?존재 ?�인
  if (!engine->componentPortToElectricalNode ||
      !engine->componentPortToPneumaticNode ||
      !engine->componentPortToMechanicalNode) {
    return false;
  }

  // 개별 ?�트?�크 준�??�태 ?�인
  return engine->isElectricalNetworkReady && engine->isPneumaticNetworkReady &&
         engine->isMechanicalSystemReady;
}

//  ?�트?�크 ?�효??검�?
// - ?�트?�크 구성??물리?�으�??�?�한지 ?�인
// - ?�드?� ?��????��????�인
bool IsNetworkValid(PhysicsEngine* engine) {
  if (!engine || !IsEngineReady(engine))
    return false;

  // ?�기 ?�트?�크 ?�효???�인
  ElectricalNetwork* elec = engine->electricalNetwork;
  if (elec->nodeCount < 0 || elec->nodeCount > elec->maxNodes ||
      elec->edgeCount < 0 || elec->edgeCount > elec->maxEdges) {
    return false;
  }

  // 공압 ?�트?�크 ?�효???�인
  PneumaticNetwork* pneu = engine->pneumaticNetwork;
  if (pneu->nodeCount < 0 || pneu->nodeCount > pneu->maxNodes ||
      pneu->edgeCount < 0 || pneu->edgeCount > pneu->maxEdges) {
    return false;
  }

  // 기계 ?�스???�효???�인
  MechanicalSystem* mech = engine->mechanicalSystem;
  if (mech->nodeCount < 0 || mech->nodeCount > mech->maxNodes ||
      mech->edgeCount < 0 || mech->edgeCount > mech->maxEdges) {
    return false;
  }

  // ?�성 컴포?�트 ???�인
  if (engine->activeComponents < 0 ||
      engine->activeComponents > engine->maxComponents) {
    return false;
  }

  return engine->isNetworkBuilt;
}

// ?��??�이???�행 ?�전??검�?
// - ?�버 ?�출 ??모든 조건??만족?�는지 ?�인
// - 메모�??�태 �??�치???�정???�인
bool IsSafeToRunSimulation(PhysicsEngine* engine) {
  if (!engine || !IsNetworkValid(engine))
    return false;

  // ?�러 ?�태 ?�인
  if (engine->hasError)
    return false;

  // ?��??�이???�라미터 ?�효???�인
  if (engine->deltaTime <= 0.0f || engine->deltaTime > 1.0f)
    return false;
  if (engine->maxIterations <= 0 || engine->maxIterations > 10000)
    return false;
  if (engine->convergenceTolerance <= 0.0f)
    return false;

  // ?�트?�크�??�버 준�??�태 ?�인
  ElectricalNetwork* elec = engine->electricalNetwork;
  if (elec->nodeCount > 0) {
    if (!elec->nodes || !elec->edges || !elec->voltageVector ||
        !elec->currentVector) {
      return false;
    }
  }

  PneumaticNetwork* pneu = engine->pneumaticNetwork;
  if (pneu->nodeCount > 0) {
    if (!pneu->nodes || !pneu->edges || !pneu->pressureVector ||
        !pneu->massFlowVector) {
      return false;
    }
  }

  MechanicalSystem* mech = engine->mechanicalSystem;
  if (mech->nodeCount > 0) {
    if (!mech->nodes || !mech->edges || !mech->displacementVector ||
        !mech->velocityVector) {
      return false;
    }
  }

  return engine->isReadyForSimulation;
}

}  // namespace StateValidationFunctions


// 배선 ?�보로�???물리 ?�트?�크 ?�동 구성
// - ?�심 기능: Wire?� PlacedComponent�?분석?�여 물리 ?�트?�크 ?�성
// - 기존 ?�드코딩???��??�이?�을 ?�체하??가??중요??기능
namespace NetworkingFunctions {

int BuildNetworksFromWiring(PhysicsEngine* engine, const Wire* wires,
                            int wireCount, const PlacedComponent* components,
                            int componentCount) {
  if (!engine || !engine->isInitialized)
    return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;
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

  // 1. 기존 ?�트?�크 초기??
  engine->electricalNetwork->nodeCount = 0;
  engine->electricalNetwork->edgeCount = 0;
  engine->pneumaticNetwork->nodeCount = 0;
  engine->pneumaticNetwork->edgeCount = 0;
  engine->mechanicalSystem->nodeCount = 0;
  engine->mechanicalSystem->edgeCount = 0;

  // 2. 컴포?�트�??�드 ?�성
  for (int c = 0; c < componentCount; c++) {
    const PlacedComponent& comp = components[c];
    PortInfo portInfo = GetComponentPortInfo(comp.type);

    //  컴포?�트 물리 ?�태 초기??- 배열 경계 검??강화
    if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
        engine->activeComponents < engine->maxComponents &&
        engine->componentIdToPhysicsIndex != nullptr) {
      engine->componentIdToPhysicsIndex[comp.instanceId] =
          engine->activeComponents;

      TypedPhysicsState& physState =
          engine->componentPhysicsStates[engine->activeComponents];
      physState.type = PHYSICS_STATE_NONE;  // ?�단 초기?? ?�중???�?�별�??�정

      // ?�?�별 물리 ?�태 ?�정
      switch (comp.type) {
        case ComponentType::PLC:
          physState.type = PHYSICS_STATE_PLC;
          // PLC 물리 ?�태 초기??
          std::memset(&physState.state.plc, 0, sizeof(PLCPhysicsState));
          break;

        case ComponentType::CYLINDER: {
          physState.type = PHYSICS_STATE_CYLINDER;
          // ?�린??물리 ?�태 초기??(기존 internalStates?�서 변??
          CylinderPhysicsState& cylState = physState.state.cylinder;
          std::memset(&cylState, 0, sizeof(CylinderPhysicsState));

          // 기본�??�정
          cylState.maxStroke = 160.0f;
          cylState.pistonMass = 0.5f;
          cylState.pistonAreaA = 314.16f;  // ? × (10mm)² [cm²]
          cylState.pistonAreaB =
              235.62f;  // ? × (10mm)² - ? × (5mm)² [cm²] (로드 ?�외)
          cylState.staticFriction = 0.3f;
          cylState.kineticFriction = 0.2f;
          cylState.viscousDamping = 0.02f;

          // 기존 internalStates?�서 �?복사 (?�다�?
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
          std::memset(&physState.state.limitSwitch, 0,
                      sizeof(LimitSwitchPhysicsState));
          break;

        case ComponentType::SENSOR:
          physState.type = PHYSICS_STATE_SENSOR;
          std::memset(&physState.state.sensor, 0, sizeof(SensorPhysicsState));
          break;

        case ComponentType::VALVE_SINGLE:
          physState.type = PHYSICS_STATE_VALVE_SINGLE;
          std::memset(&physState.state.valveSingle, 0,
                      sizeof(ValveSinglePhysicsState));
          break;

        case ComponentType::VALVE_DOUBLE:
          physState.type = PHYSICS_STATE_VALVE_DOUBLE;
          std::memset(&physState.state.valveDouble, 0,
                      sizeof(ValveDoublePhysicsState));
          break;

        case ComponentType::BUTTON_UNIT:
          physState.type = PHYSICS_STATE_BUTTON_UNIT;
          std::memset(&physState.state.buttonUnit, 0,
                      sizeof(ButtonUnitPhysicsState));
          break;

        // 물리 ?��??�이?�이 ?�요?��? ?��? 컴포?�트??
        case ComponentType::FRL:
        case ComponentType::MANIFOLD:
        case ComponentType::POWER_SUPPLY:
        default:
          physState.type = PHYSICS_STATE_NONE;
          break;
      }

      engine->activeComponents++;
    }

    // ?�기 ?�트?�크 ?�드 ?�성
    if (portInfo.isElectricalActive) {
      ElectricalNetwork* elec = engine->electricalNetwork;
      for (int p = 0; p < portInfo.electricalPorts; p++) {
        if (elec->nodeCount >= elec->maxNodes)
          break;

        ElectricalNode& node = elec->nodes[elec->nodeCount];
        std::memset(&node, 0, sizeof(ElectricalNode));

        node.base.componentId = comp.instanceId;
        node.base.portId = p;
        node.base.nodeId = elec->nodeCount;
        node.base.isActive = true;

        // 컴포?�트 ?�?�별 ?�기???�성 ?�정
        switch (comp.type) {
          case ComponentType::POWER_SUPPLY:
            if (p == 0) {  // 24V ?�트
              node.isVoltageSource = true;
              node.sourceVoltage = 24.0f;
              node.sourceResistance = 0.1f;  // ?��??�??0.1Ω
              engine->electricalNetwork->groundNodeId =
                  elec->nodeCount;  // ?��????�정
            } else if (p == 1) {    // 0V ?�트
              node.voltage = 0.0f;
            }
            break;

          case ComponentType::PLC:
            // PLC ?�력?�트 (X0-X15): 고임?�던??
            if (p < 16) {
              node.inputImpedance[0] = 10000.0f;  // 10kΩ ?�력?�피?�스
            } else {                           // 출력?�트 (Y0-Y15): ?�?�피?�스
              node.outputImpedance[0] = 1.0f;  // 1Ω 출력?�피?�스
            }
            break;

          default:
            node.nodeCapacitance = 1e-12f;     // 기본 1pF
            node.leakageConductance = 1e-12f;  // 기본 1pS ?�설
            break;
        }

        //  매핑 ?�이�??�데?�트 - 범위 검??
        if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
            p >= 0 && p < 32 &&
            engine->componentPortToElectricalNode != nullptr) {
          engine->componentPortToElectricalNode[comp.instanceId][p] =
              elec->nodeCount;
        }
        elec->nodeCount++;
      }
    }

    // 공압 ?�트?�크 ?�드 ?�성
    if (portInfo.isPneumaticActive) {
      PneumaticNetwork* pneu = engine->pneumaticNetwork;
      for (int p = 0; p < portInfo.pneumaticPorts; p++) {
        if (pneu->nodeCount >= pneu->maxNodes)
          break;

        int port_id = p;
        if (portInfo.isElectricalActive) {
          port_id += portInfo.electricalPorts;
        }

        PneumaticNode& node = pneu->nodes[pneu->nodeCount];
        std::memset(&node, 0, sizeof(PneumaticNode));

        node.base.componentId = comp.instanceId;
        node.base.portId = port_id;
        node.base.nodeId = pneu->nodeCount;
        node.base.isActive = true;

        // 컴포?�트 ?�?�별 공압 ?�성 ?�정
        switch (comp.type) {
          case ComponentType::FRL:
            node.isPressureSource = true;
            node.sourcePressure = 6.0f * 100000.0f;  // 6bar ??Pa
            node.sourceTemperature = 293.15f;        // 20°C
            break;

          case ComponentType::CYLINDER:
            if (p == 0) {           // A챔버
              node.volume = 0.05f;  // 50mL 기본 체적
            } else if (p == 1) {    // B챔버
              node.volume = 0.03f;  // 30mL (로드 체적 ?�외)
            }
            node.temperature = 293.15f;
            node.pressure = pneu->atmosphericPressure;
            break;

          default:
            node.volume = 0.001f;  // 기본 1mL
            node.temperature = 293.15f;
            node.pressure = pneu->atmosphericPressure;
            break;
        }

        // 공압 매핑 ?�이�??�데?�트 - 범위 검??
        if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
            port_id >= 0 && port_id < 16 &&
            engine->componentPortToPneumaticNode != nullptr) {
          engine->componentPortToPneumaticNode[comp.instanceId][port_id] =
              pneu->nodeCount;
        }
        pneu->nodeCount++;
      }
    }

    // 기계 ?�스???�드 ?�성
    if (portInfo.isMechanicalActive) {
      MechanicalSystem* mech = engine->mechanicalSystem;
      if (mech->nodeCount < mech->maxNodes) {
        MechanicalNode& node = mech->nodes[mech->nodeCount];
        std::memset(&node, 0, sizeof(MechanicalNode));

        node.base.componentId = comp.instanceId;
        node.base.portId = 0;
        node.base.nodeId = mech->nodeCount;
        node.base.isActive = true;

        // 컴포?�트 ?�?�별 기계???�성 ?�정
        switch (comp.type) {
          case ComponentType::CYLINDER:
            node.mass = 0.5f;         // ?�스??로드 질량 0.5kg
            node.position[0] = 0.0f;  // 초기 ?�치 (?�전 ?�축)
            // 구속조건: Y, Z�?고정, X축만 ?�동 가??
            node.isFixed[1] = node.isFixed[2] = true;
            node.isFixed[3] = node.isFixed[4] = node.isFixed[5] = true;
            break;

          case ComponentType::LIMIT_SWITCH:
            node.mass = 0.01f;  // ?�위�??�추?�이??질량 10g
            // 모든 �?고정 (?�촉?�만 측정)
            for (int i = 0; i < 6; i++)
              node.isFixed[i] = true;
            break;

          default:
            node.mass = 0.001f;  // 기본 1g
            for (int i = 0; i < 6; i++)
              node.isFixed[i] = true;
            break;
        }

        //  기계 매핑 ?�이�??�데?�트 - 범위 검??
        if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
            engine->componentPortToMechanicalNode != nullptr) {
          engine->componentPortToMechanicalNode[comp.instanceId][0] =
              mech->nodeCount;
        }
        mech->nodeCount++;
      }
    }
  }

  // 3. ?�?�어�??��? ?�성
  for (int w = 0; w < wireCount; w++) {
    const Wire& wire = wires[w];

    if (wire.isElectric) {
      // ?�기 ?��? ?�성
      ElectricalNetwork* elec = engine->electricalNetwork;
      if (elec->edgeCount >= elec->maxEdges)
        continue;

      // ?�?�어 ?�결 ?�보로�????�드 ID 찾기
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

        // ?�?�어 물리???�성?�로부???�기???�성 계산
        float wireLength =
            100.0f;  // 기본 100mm (?�제로는 wayPoints�?계산?�야 ??
        edge.length = wireLength;
        edge.crossSectionArea = 0.5f;  // 0.5mm² ?�선
        edge.resistivity = 1.7e-8f;    // 구리 비�???[Ω?�m]
        edge.temperature = 20.0f;      // ?�온

        edge.resistance = (edge.resistivity * 1000.0f) *
                          (wireLength / 1000.0f) /
                          (edge.crossSectionArea / 1000000.0f);
        edge.conductance = 1.0f / edge.resistance;
        edge.maxCurrent = 1.0f;  // 최�? 1A

        elec->edgeCount++;
      }
    } else {
      // 공압 ?��? ?�성
      PneumaticNetwork* pneu = engine->pneumaticNetwork;
      if (pneu->edgeCount >= pneu->maxEdges)
        continue;

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

        // 공압 배�? ?�성 ?�정 (기본�?
        edge.diameter = 4.0f;  // 4mm ?�경
        edge.length = 200.0f;  // 200mm 길이
        edge.crossSectionArea =
            3.14159f * (edge.diameter / 2.0f) * (edge.diameter / 2.0f);  // ?×r²
        edge.roughness = 0.05f;  // 0.05μm ?�면거칠�?
        edge.curvature = 0.1f;   // �?��?�실계수

        pneu->edgeCount++;
      }
    }
  }

  float buildTime = timer.GetElapsedMs();
  engine->performanceStats.networkBuildTime = buildTime;

  //  ?�트?�크 구성 ?�료 ?�태 ?�정
  engine->isNetworkBuilt = true;

  // ?��??�이??준�??�태 ?�인 (모든 조건??만족?�면 준�??�료)
  if (StateValidationFunctions::IsEngineReady(engine) &&
      StateValidationFunctions::IsNetworkValid(engine)) {
    engine->isReadyForSimulation = true;

    if (engine->enableLogging) {
      std::cout << "[PhysicsEngine] Engine is ready for simulation"
                << std::endl;
    }
  }

  if (engine->enableLogging) {
    std::cout << "[PhysicsEngine] Network built successfully in " << buildTime
              << "ms" << std::endl;
    std::cout << "  - Electrical: " << engine->electricalNetwork->nodeCount
              << " nodes, " << engine->electricalNetwork->edgeCount << " edges"
              << std::endl;
    std::cout << "  - Pneumatic: " << engine->pneumaticNetwork->nodeCount
              << " nodes, " << engine->pneumaticNetwork->edgeCount << " edges"
              << std::endl;
    std::cout << "  - Mechanical: " << engine->mechanicalSystem->nodeCount
              << " nodes, " << engine->mechanicalSystem->edgeCount << " edges"
              << std::endl;
  }

  return PHYSICS_ENGINE_SUCCESS;
}

int UpdateNetworkTopology(PhysicsEngine* engine) {
  if (!engine || !engine->isInitialized)
    return PHYSICS_ENGINE_ERROR_NOT_INITIALIZED;

  // ?�접 ?�렬 ?�데?�트
  ElectricalNetwork* elec = engine->electricalNetwork;
  PneumaticNetwork* pneu = engine->pneumaticNetwork;
  MechanicalSystem* mech = engine->mechanicalSystem;

  // ?�기 ?�트?�크 ?�접 ?�렬 구성
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

  // 공압 ?�트?�크 ?�접 ?�렬 구성
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

  // 기계 ?�스???�접 ?�렬 구성
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

}  // namespace NetworkingFunctions


extern "C" {

// 물리?�진 ?�토�??�수??
PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires) {
  PhysicsEngine* engine = new (std::nothrow) PhysicsEngine;
  if (!engine)
    return nullptr;

  //  Create() ?�출 먼�? - memset???��??�서 ?�행??
  // ?�시�?Create ?�수 ?�인?�만 ?�정
  engine->lifecycle.Create = LifecycleFunctions::Create;

  if (engine->lifecycle.Create(engine, maxComponents, maxWires) !=
      PHYSICS_ENGINE_SUCCESS) {
    delete engine;
    return nullptr;
  }

  //  memset ?�후??모든 ?�수 ?�인???�설??
  engine->lifecycle.Create = LifecycleFunctions::Create;
  engine->lifecycle.Initialize = LifecycleFunctions::Initialize;
  engine->lifecycle.Reset = LifecycleFunctions::Reset;
  engine->lifecycle.Destroy = LifecycleFunctions::Destroy;

  engine->networking.BuildNetworksFromWiring =
      NetworkingFunctions::BuildNetworksFromWiring;
  engine->networking.UpdateNetworkTopology =
      NetworkingFunctions::UpdateNetworkTopology;

  //  ?�태 검�??�수 ?�인???�정
  engine->IsEngineReady = StateValidationFunctions::IsEngineReady;
  engine->IsNetworkValid = StateValidationFunctions::IsNetworkValid;
  engine->IsSafeToRunSimulation =
      StateValidationFunctions::IsSafeToRunSimulation;

  //  물리?�진 초기??- ?�수 ?�인???�설????Initialize ?�출
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
  return CreatePhysicsEngine(PhysicsEngineDefaults::DEFAULT_MAX_COMPONENTS,
                             PhysicsEngineDefaults::DEFAULT_MAX_WIRES);
}

void DestroyPhysicsEngine(PhysicsEngine* engine) {
  // ?�전???�진 ?�멸 - null 체크 �??�수 ?�인??검�?
  if (engine) {
    if (engine->lifecycle.Destroy) {
      engine->lifecycle.Destroy(engine);
    }
    delete engine;
  }
}

}  // extern "C"


const char* PhysicsEngineErrorToString(PhysicsEngineError error) {
  switch (error) {
    case PHYSICS_ENGINE_SUCCESS:
      return "Success";
    case PHYSICS_ENGINE_ERROR_INITIALIZATION:
      return "Initialization failed";
    case PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION:
      return "Memory allocation failed";
    case PHYSICS_ENGINE_ERROR_INVALID_PARAMETER:
      return "Invalid parameter";
    case PHYSICS_ENGINE_ERROR_NOT_INITIALIZED:
      return "Engine not initialized";
    case PHYSICS_ENGINE_ERROR_NETWORK_BUILD:
      return "Network build failed";
    case PHYSICS_ENGINE_ERROR_INVALID_TOPOLOGY:
      return "Invalid network topology";
    case PHYSICS_ENGINE_ERROR_DISCONNECTED_NETWORK:
      return "Disconnected network";
    case PHYSICS_ENGINE_ERROR_COMPONENT_NOT_FOUND:
      return "Component not found";
    case PHYSICS_ENGINE_ERROR_SIMULATION_FAILED:
      return "Simulation failed";
    case PHYSICS_ENGINE_ERROR_CONVERGENCE_FAILED:
      return "Convergence failed";
    case PHYSICS_ENGINE_ERROR_NUMERICAL_INSTABILITY:
      return "Numerical instability";
    case PHYSICS_ENGINE_ERROR_PHYSICAL_INCONSISTENCY:
      return "Physical inconsistency";
    case PHYSICS_ENGINE_ERROR_DATA_SYNC:
      return "Data synchronization failed";
    case PHYSICS_ENGINE_ERROR_INVALID_STATE:
      return "Invalid state";
    case PHYSICS_ENGINE_ERROR_BUFFER_OVERFLOW:
      return "Buffer overflow";
    case PHYSICS_ENGINE_ERROR_TYPE_MISMATCH:
      return "Type mismatch";
    case PHYSICS_ENGINE_ERROR_SYSTEM:
      return "System error";
    case PHYSICS_ENGINE_ERROR_THREAD_FAILURE:
      return "Thread failure";
    case PHYSICS_ENGINE_ERROR_IO_ERROR:
      return "I/O error";
    default:
      return "Unknown error";
  }
}

}  // namespace plc
