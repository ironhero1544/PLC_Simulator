// physics_engine.cpp
// Implementation of physics engine.
//
// Core physics engine implementation based on real-world physical laws.
// Unified physics engine replacing hardcoded physics simulation.
// Automatically constructs physics networks from Wire and PlacedComponent data
// and performs real-time simulation.

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

// Memory allocation/deallocation utilities
// - Safe memory management to prevent memory leaks
// - Aligned memory allocation for performance optimization
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

struct PortInfo {
  int electricalPorts;
  int pneumaticPorts;
  int mechanicalPorts;
  bool isElectricalActive;
  bool isPneumaticActive;
  bool isMechanicalActive;
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
      info.electricalPorts = 15;
      info.isElectricalActive = true;
      break;

    case ComponentType::LIMIT_SWITCH:
      info.electricalPorts = 3;  // COM, NO, NC
      info.mechanicalPorts = 1;
      info.isElectricalActive = true;
      info.isMechanicalActive = true;
      break;

    case ComponentType::SENSOR:
      info.electricalPorts = 3;  // 24V, 0V, OUT
      info.mechanicalPorts = 1;
      info.isElectricalActive = true;
      info.isMechanicalActive = true;
      break;

    case ComponentType::FRL:
      info.pneumaticPorts = 1;
      info.isPneumaticActive = true;
      break;

    case ComponentType::MANIFOLD:
      info.pneumaticPorts = 5;
      info.isPneumaticActive = true;
      break;

    case ComponentType::VALVE_SINGLE:
      info.electricalPorts = 2;
      info.pneumaticPorts = 3;   // P, A, R
      info.isElectricalActive = true;
      info.isPneumaticActive = true;
      break;

    case ComponentType::VALVE_DOUBLE:
      info.electricalPorts = 4;
      info.pneumaticPorts = 3;   // P, A, B
      info.isElectricalActive = true;
      info.isPneumaticActive = true;
      break;

    case ComponentType::CYLINDER:
      info.pneumaticPorts = 2;
      info.mechanicalPorts = 1;
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

class PerformanceTimer {
 private:
  std::chrono::high_resolution_clock::time_point startTime;

 public:
  void Start() { startTime = std::chrono::high_resolution_clock::now(); }

  float GetElapsedMs() const {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);
    return duration.count() / 1000.0f;
  }
};

}  // namespace PhysicsEngineInternal


namespace LifecycleFunctions {

int Create(PhysicsEngine* engine, int maxComponents, int maxWires) {
  if (!engine)
    return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;
  if (maxComponents <= 0 || maxWires <= 0)
    return PHYSICS_ENGINE_ERROR_INVALID_PARAMETER;

  using namespace PhysicsEngineInternal;

  std::memset(engine, 0, sizeof(PhysicsEngine));
  engine->maxComponents = maxComponents;
  engine->activeComponents = 0;
  engine->isInitialized = false;

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

  engine->deltaTime = PhysicsEngineDefaults::DEFAULT_TIME_STEP;
  engine->convergenceTolerance =
      PhysicsEngineDefaults::DEFAULT_CONVERGENCE_TOLERANCE;
  engine->maxIterations = PhysicsEngineDefaults::DEFAULT_MAX_ITERATIONS;
  engine->enableLogging = true;
  engine->logLevel = PhysicsEngineDefaults::DEFAULT_LOG_LEVEL;
  engine->performanceUpdateInterval =
      PhysicsEngineDefaults::DEFAULT_PERFORMANCE_UPDATE_INTERVAL;

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
  SafeDeallocateArray(elec->solverVoltageOld);
  elec->solverVoltageCapacity = 0;
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
  SafeDeallocateArray(pneu->solverResidualBuffer);
  SafeDeallocateArray(pneu->solverDeltaBuffer);
  SafeDeallocateArray(pneu->solverJacobianBuffer);
  SafeDeallocateArray(pneu->solverJacobianRows);
  pneu->solverBufferSize = 0;
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
  SafeDeallocateArray(mech->solverState);
  SafeDeallocateArray(mech->solverK1);
  SafeDeallocateArray(mech->solverK2);
  SafeDeallocateArray(mech->solverK3);
  SafeDeallocateArray(mech->solverK4);
  SafeDeallocateArray(mech->solverTempState);
  SafeDeallocateArray(mech->solverKq);
  SafeDeallocateArray(mech->solverCqdot);
  SafeDeallocateArray(mech->solverRhs);
  mech->solverDofCapacity = 0;
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

  elec->solverVoltageOld = SafeAllocateArray<float>(elec->maxNodes);
  if (!elec->solverVoltageOld) {
    CleanupElectricalNetwork(elec);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate electrical solver scratch buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  elec->solverVoltageCapacity = elec->maxNodes;

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

  pneu->solverBufferSize = pneu->maxNodes + pneu->maxEdges;
  if (pneu->solverBufferSize <= 0) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Invalid pneumatic solver buffer size");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->solverResidualBuffer =
      SafeAllocateArray<float>(pneu->solverBufferSize);
  if (!pneu->solverResidualBuffer) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic residual buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->solverDeltaBuffer = SafeAllocateArray<float>(pneu->solverBufferSize);
  if (!pneu->solverDeltaBuffer) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic delta buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  const int jacobian_size = pneu->solverBufferSize * pneu->solverBufferSize;
  pneu->solverJacobianBuffer = SafeAllocateArray<float>(jacobian_size);
  if (!pneu->solverJacobianBuffer) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic Jacobian buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }

  pneu->solverJacobianRows =
      SafeAllocateArray<float*>(pneu->solverBufferSize);
  if (!pneu->solverJacobianRows) {
    CleanupPneumaticNetwork(pneu);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate pneumatic Jacobian row pointers");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  for (int i = 0; i < pneu->solverBufferSize; ++i) {
    pneu->solverJacobianRows[i] =
        &pneu->solverJacobianBuffer[i * pneu->solverBufferSize];
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

  mech->solverDofCapacity = dof;
  const int state_size = 2 * dof;
  mech->solverState = SafeAllocateArray<float>(state_size);
  if (!mech->solverState) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical state buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverK1 = SafeAllocateArray<float>(state_size);
  if (!mech->solverK1) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical k1 buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverK2 = SafeAllocateArray<float>(state_size);
  if (!mech->solverK2) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical k2 buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverK3 = SafeAllocateArray<float>(state_size);
  if (!mech->solverK3) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical k3 buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverK4 = SafeAllocateArray<float>(state_size);
  if (!mech->solverK4) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical k4 buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverTempState = SafeAllocateArray<float>(state_size);
  if (!mech->solverTempState) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical temporary state buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverKq = SafeAllocateArray<float>(dof);
  if (!mech->solverKq) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical Kq buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverCqdot = SafeAllocateArray<float>(dof);
  if (!mech->solverCqdot) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical Cqdot buffer");
    return PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION;
  }
  mech->solverRhs = SafeAllocateArray<float>(dof);
  if (!mech->solverRhs) {
    CleanupMechanicalSystem(mech);
    SetEngineError(engine, PHYSICS_ENGINE_ERROR_MEMORY_ALLOCATION,
                   "Failed to allocate mechanical RHS buffer");
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

  engine->isRunning = false;
  engine->isPaused = false;
  engine->currentTime = 0.0f;
  engine->stepCount = 0;
  engine->activeComponents = 0;
  engine->hasError = false;
  engine->lastErrorCode = PHYSICS_ENGINE_SUCCESS;

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

  std::memset(&engine->performanceStats, 0, sizeof(PhysicsPerformanceStats));

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

  if (engine->electricalNetwork) {
    ElectricalNetwork* elec = engine->electricalNetwork;
    SafeDeallocateArray(elec->nodes);
    SafeDeallocateArray(elec->edges);
    Deallocate2DArray(elec->adjacencyMatrix, elec->maxNodes);
    Deallocate2DArray(elec->conductanceMatrix, elec->maxNodes);
    SafeDeallocateArray(elec->currentVector);
    SafeDeallocateArray(elec->voltageVector);
    SafeDeallocateArray(elec->solverVoltageOld);
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
    SafeDeallocateArray(pneu->solverResidualBuffer);
    SafeDeallocateArray(pneu->solverDeltaBuffer);
    SafeDeallocateArray(pneu->solverJacobianBuffer);
    SafeDeallocateArray(pneu->solverJacobianRows);
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
    SafeDeallocateArray(mech->solverState);
    SafeDeallocateArray(mech->solverK1);
    SafeDeallocateArray(mech->solverK2);
    SafeDeallocateArray(mech->solverK3);
    SafeDeallocateArray(mech->solverK4);
    SafeDeallocateArray(mech->solverTempState);
    SafeDeallocateArray(mech->solverKq);
    SafeDeallocateArray(mech->solverCqdot);
    SafeDeallocateArray(mech->solverRhs);
    delete engine->mechanicalSystem;
    engine->mechanicalSystem = nullptr;
  }

  SafeDeallocateArray(engine->componentPhysicsStates);
  SafeDeallocateArray(engine->componentIdToPhysicsIndex);
  Deallocate2DArray(engine->componentPortToElectricalNode,
                    engine->maxComponents);
  Deallocate2DArray(engine->componentPortToPneumaticNode,
                    engine->maxComponents);
  Deallocate2DArray(engine->componentPortToMechanicalNode,
                    engine->maxComponents);

  engine->isInitialized = false;
  engine->maxComponents = 0;
  engine->activeComponents = 0;

  if (engine->enableLogging) {
    std::cout << "[PhysicsEngine] Engine destroyed successfully" << std::endl;
  }

  engine->isNetworkBuilt = false;
  engine->isReadyForSimulation = false;
  engine->isElectricalNetworkReady = true;
  engine->isPneumaticNetworkReady = true;
  engine->isMechanicalSystemReady = true;

  return PHYSICS_ENGINE_SUCCESS;
}

}  // namespace LifecycleFunctions


namespace StateValidationFunctions {

bool IsEngineReady(PhysicsEngine* engine) {
  if (!engine)
    return false;

  if (!engine->isInitialized)
    return false;

  if (!engine->electricalNetwork || !engine->pneumaticNetwork ||
      !engine->mechanicalSystem) {
    return false;
  }

  if (!engine->componentPhysicsStates || !engine->componentIdToPhysicsIndex) {
    return false;
  }

  if (!engine->componentPortToElectricalNode ||
      !engine->componentPortToPneumaticNode ||
      !engine->componentPortToMechanicalNode) {
    return false;
  }

  return engine->isElectricalNetworkReady && engine->isPneumaticNetworkReady &&
         engine->isMechanicalSystemReady;
}

bool IsNetworkValid(PhysicsEngine* engine) {
  if (!engine || !IsEngineReady(engine))
    return false;

  ElectricalNetwork* elec = engine->electricalNetwork;
  if (elec->nodeCount < 0 || elec->nodeCount > elec->maxNodes ||
      elec->edgeCount < 0 || elec->edgeCount > elec->maxEdges) {
    return false;
  }

  PneumaticNetwork* pneu = engine->pneumaticNetwork;
  if (pneu->nodeCount < 0 || pneu->nodeCount > pneu->maxNodes ||
      pneu->edgeCount < 0 || pneu->edgeCount > pneu->maxEdges) {
    return false;
  }

  MechanicalSystem* mech = engine->mechanicalSystem;
  if (mech->nodeCount < 0 || mech->nodeCount > mech->maxNodes ||
      mech->edgeCount < 0 || mech->edgeCount > mech->maxEdges) {
    return false;
  }

  if (engine->activeComponents < 0 ||
      engine->activeComponents > engine->maxComponents) {
    return false;
  }

  return engine->isNetworkBuilt;
}

bool IsSafeToRunSimulation(PhysicsEngine* engine) {
  if (!engine || !IsNetworkValid(engine))
    return false;

  if (engine->hasError)
    return false;

  if (engine->deltaTime <= 0.0f || engine->deltaTime > 1.0f)
    return false;
  if (engine->maxIterations <= 0 || engine->maxIterations > 10000)
    return false;
  if (engine->convergenceTolerance <= 0.0f)
    return false;

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

  engine->electricalNetwork->nodeCount = 0;
  engine->electricalNetwork->edgeCount = 0;
  engine->pneumaticNetwork->nodeCount = 0;
  engine->pneumaticNetwork->edgeCount = 0;
  engine->mechanicalSystem->nodeCount = 0;
  engine->mechanicalSystem->edgeCount = 0;

  for (int c = 0; c < componentCount; c++) {
    const PlacedComponent& comp = components[c];
    PortInfo portInfo = GetComponentPortInfo(comp.type);

    if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
        engine->activeComponents < engine->maxComponents &&
        engine->componentIdToPhysicsIndex != nullptr) {
      engine->componentIdToPhysicsIndex[comp.instanceId] =
          engine->activeComponents;

      TypedPhysicsState& physState =
          engine->componentPhysicsStates[engine->activeComponents];
      physState.type = PHYSICS_STATE_NONE;

      switch (comp.type) {
        case ComponentType::PLC:
          physState.type = PHYSICS_STATE_PLC;
          std::memset(&physState.state.plc, 0, sizeof(PLCPhysicsState));
          break;

        case ComponentType::CYLINDER: {
          physState.type = PHYSICS_STATE_CYLINDER;
          CylinderPhysicsState& cylState = physState.state.cylinder;
          std::memset(&cylState, 0, sizeof(CylinderPhysicsState));

          cylState.maxStroke = 160.0f;
          cylState.pistonMass = 0.5f;
          cylState.pistonAreaA = 314.16f;
          cylState.pistonAreaB =
              235.62f;
          cylState.staticFriction = 0.3f;
          cylState.kineticFriction = 0.2f;
          cylState.viscousDamping = 0.02f;

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

        case ComponentType::FRL:
        case ComponentType::MANIFOLD:
        case ComponentType::POWER_SUPPLY:
        default:
          physState.type = PHYSICS_STATE_NONE;
          break;
      }

      engine->activeComponents++;
    }

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

        switch (comp.type) {
          case ComponentType::POWER_SUPPLY:
            if (p == 0) {
              node.isVoltageSource = true;
              node.sourceVoltage = 24.0f;
              node.sourceResistance = 0.1f;
              engine->electricalNetwork->groundNodeId =
                  elec->nodeCount;
            } else if (p == 1) {
              node.voltage = 0.0f;
            }
            break;

          case ComponentType::PLC:
            if (p < 16) {
              node.inputImpedance[0] = 10000.0f;
            } else {
              node.outputImpedance[0] = 1.0f;
            }
            break;

          default:
            node.nodeCapacitance = 1e-12f;
            node.leakageConductance = 1e-12f;
            break;
        }

        if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
            p >= 0 && p < 32 &&
            engine->componentPortToElectricalNode != nullptr) {
          engine->componentPortToElectricalNode[comp.instanceId][p] =
              elec->nodeCount;
        }
        elec->nodeCount++;
      }
    }

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

        switch (comp.type) {
          case ComponentType::FRL:
            node.isPressureSource = true;
            node.sourcePressure = 6.0f * 100000.0f;  // 6bar ??Pa
            node.sourceTemperature = 293.15f;
            break;

          case ComponentType::CYLINDER:
            if (p == 0) {
              node.volume = 0.05f;
            } else if (p == 1) {
              node.volume = 0.03f;
            }
            node.temperature = 293.15f;
            node.pressure = pneu->atmosphericPressure;
            break;

          default:
            node.volume = 0.001f;
            node.temperature = 293.15f;
            node.pressure = pneu->atmosphericPressure;
            break;
        }

        if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
            port_id >= 0 && port_id < 16 &&
            engine->componentPortToPneumaticNode != nullptr) {
          engine->componentPortToPneumaticNode[comp.instanceId][port_id] =
              pneu->nodeCount;
        }
        pneu->nodeCount++;
      }
    }

    if (portInfo.isMechanicalActive) {
      MechanicalSystem* mech = engine->mechanicalSystem;
      if (mech->nodeCount < mech->maxNodes) {
        MechanicalNode& node = mech->nodes[mech->nodeCount];
        std::memset(&node, 0, sizeof(MechanicalNode));

        node.base.componentId = comp.instanceId;
        node.base.portId = 0;
        node.base.nodeId = mech->nodeCount;
        node.base.isActive = true;

        switch (comp.type) {
          case ComponentType::CYLINDER:
            node.mass = 0.5f;
            node.position[0] = 0.0f;
            node.isFixed[1] = node.isFixed[2] = true;
            node.isFixed[3] = node.isFixed[4] = node.isFixed[5] = true;
            break;

          case ComponentType::LIMIT_SWITCH:
            node.mass = 0.01f;
            for (int i = 0; i < 6; i++)
              node.isFixed[i] = true;
            break;

          default:
            node.mass = 0.001f;
            for (int i = 0; i < 6; i++)
              node.isFixed[i] = true;
            break;
        }

        if (comp.instanceId >= 0 && comp.instanceId < engine->maxComponents &&
            engine->componentPortToMechanicalNode != nullptr) {
          engine->componentPortToMechanicalNode[comp.instanceId][0] =
              mech->nodeCount;
        }
        mech->nodeCount++;
      }
    }
  }

  for (int w = 0; w < wireCount; w++) {
    const Wire& wire = wires[w];

    if (wire.isElectric) {
      ElectricalNetwork* elec = engine->electricalNetwork;
      if (elec->edgeCount >= elec->maxEdges)
        continue;

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

        float wireLength =
            100.0f;
        edge.length = wireLength;
        edge.crossSectionArea = 0.5f;
        edge.resistivity = 1.7e-8f;
        edge.temperature = 20.0f;

        edge.resistance = (edge.resistivity * 1000.0f) *
                          (wireLength / 1000.0f) /
                          (edge.crossSectionArea / 1000000.0f);
        edge.conductance = 1.0f / edge.resistance;
        edge.maxCurrent = 1.0f;

        elec->edgeCount++;
      }
    } else {
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

        edge.diameter = 4.0f;
        edge.length = 200.0f;
        edge.crossSectionArea =
            3.14159f * (edge.diameter / 2.0f) * (edge.diameter / 2.0f);
        edge.roughness = 0.05f;
        edge.curvature = 0.1f;

        pneu->edgeCount++;
      }
    }
  }

  float buildTime = timer.GetElapsedMs();
  engine->performanceStats.networkBuildTime = buildTime;

  engine->isNetworkBuilt = true;

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

  ElectricalNetwork* elec = engine->electricalNetwork;
  PneumaticNetwork* pneu = engine->pneumaticNetwork;
  MechanicalSystem* mech = engine->mechanicalSystem;

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

PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires) {
  PhysicsEngine* engine = new (std::nothrow) PhysicsEngine;
  if (!engine)
    return nullptr;

  engine->lifecycle.Create = LifecycleFunctions::Create;

  if (engine->lifecycle.Create(engine, maxComponents, maxWires) !=
      PHYSICS_ENGINE_SUCCESS) {
    delete engine;
    return nullptr;
  }

  engine->lifecycle.Create = LifecycleFunctions::Create;
  engine->lifecycle.Initialize = LifecycleFunctions::Initialize;
  engine->lifecycle.Reset = LifecycleFunctions::Reset;
  engine->lifecycle.Destroy = LifecycleFunctions::Destroy;

  engine->networking.BuildNetworksFromWiring =
      NetworkingFunctions::BuildNetworksFromWiring;
  engine->networking.UpdateNetworkTopology =
      NetworkingFunctions::UpdateNetworkTopology;

  engine->IsEngineReady = StateValidationFunctions::IsEngineReady;
  engine->IsNetworkValid = StateValidationFunctions::IsNetworkValid;
  engine->IsSafeToRunSimulation =
      StateValidationFunctions::IsSafeToRunSimulation;

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
