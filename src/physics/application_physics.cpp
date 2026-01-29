// application_physics.cpp
//
// Physics integration with application.

// src/Application_Physics.cpp

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/physics/component_physics_adapter.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cmath>
#include <exception>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

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
namespace {

using PortRef = std::pair<int, int>;

struct PneumaticLink {
  PortRef to;
  float attenuation = 1.0f;
  bool is_vent = false;
};

using PneumaticAdjacency = std::map<PortRef, std::vector<PneumaticLink>>;

constexpr int kAtmosphereComponentId = -1;
constexpr int kAtmospherePortId = -1;
const PortRef kAtmospherePort = {kAtmosphereComponentId, kAtmospherePortId};

struct Aabb {
  float min_x = 0.0f;
  float min_y = 0.0f;
  float max_x = 0.0f;
  float max_y = 0.0f;
};

bool IsWorkpiece(ComponentType type) {
  return type == ComponentType::WORKPIECE_METAL ||
         type == ComponentType::WORKPIECE_NONMETAL;
}

Aabb GetAabb(const PlacedComponent& comp) {
  Aabb box;
  box.min_x = comp.position.x;
  box.min_y = comp.position.y;
  box.max_x = comp.position.x + comp.size.width;
  box.max_y = comp.position.y + comp.size.height;
  return box;
}

bool AabbOverlaps(const Aabb& a, const Aabb& b) {
  return a.min_x < b.max_x && a.max_x > b.min_x && a.min_y < b.max_y &&
         a.max_y > b.min_y;
}

Aabb ExpandAabb(const Aabb& box, float pad_x, float pad_y) {
  Aabb expanded = box;
  expanded.min_x -= pad_x;
  expanded.max_x += pad_x;
  expanded.min_y -= pad_y;
  expanded.max_y += pad_y;
  return expanded;
}

Aabb GetWorkpieceDetectionAabb(const PlacedComponent& workpiece) {
  Aabb box = GetAabb(workpiece);
  float width = box.max_x - box.min_x;
  float height = box.max_y - box.min_y;
  float min_dim = (width < height) ? width : height;
  float pad = min_dim * 1.5f;
  if (pad < 20.0f) {
    pad = 20.0f;
  } else if (pad > 60.0f) {
    pad = 60.0f;
  }
  return ExpandAabb(box, pad, pad);
}

float GetCenterX(const Aabb& box) {
  return (box.min_x + box.max_x) * 0.5f;
}

float GetCenterY(const Aabb& box) {
  return (box.min_y + box.max_y) * 0.5f;
}

float Clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float GetMeterValveFlowSetting(const PlacedComponent& comp) {
  auto it = comp.internalStates.find(state_keys::kFlowSetting);
  float setting = (it != comp.internalStates.end()) ? it->second : 1.0f;
  return Clamp01(setting);
}

bool IsMeterValveInMode(const PlacedComponent& comp) {
  auto it = comp.internalStates.find(state_keys::kMeterMode);
  if (it == comp.internalStates.end()) {
    return true;
  }
  return it->second < 0.5f;
}

float ComputeMeterValveAttenuation(float flow_setting) {
  constexpr float kMinAttenuation = 0.1f;
  float clamped = Clamp01(flow_setting);
  return 1.0f - clamped * (1.0f - kMinAttenuation);
}

void AddDirectedEdge(PneumaticAdjacency& adjacency,
                     const PortRef& from,
                     const PortRef& to,
                     float attenuation,
                     bool is_vent) {
  PneumaticLink link;
  link.to = to;
  link.attenuation = attenuation;
  link.is_vent = is_vent;
  adjacency[from].push_back(link);
}

void AddBidirectionalEdge(PneumaticAdjacency& adjacency,
                          const PortRef& a,
                          const PortRef& b,
                          float attenuation) {
  AddDirectedEdge(adjacency, a, b, attenuation, false);
  AddDirectedEdge(adjacency, b, a, attenuation, false);
}

constexpr int kMaxWorkerThreads = 4;
constexpr bool kForceSerialComponentUpdates = true;

using ComponentTaskFn = void (*)(size_t index, void* context);

class ComponentTaskRunner {
 public:
  ComponentTaskRunner() {
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    size_t desired = hardware_threads > 1 ? hardware_threads - 1 : 1;
    worker_count_ = std::min(static_cast<size_t>(kMaxWorkerThreads), desired);

    for (size_t i = 0; i < worker_count_; ++i) {
      workers_[i] = std::thread([this]() { WorkerLoop(); });
    }
  }

  ~ComponentTaskRunner() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
      has_task_ = true;
    }
    cv_.notify_all();
    for (size_t i = 0; i < worker_count_; ++i) {
      if (workers_[i].joinable()) {
        workers_[i].join();
      }
    }
  }

  void Run(size_t count, ComponentTaskFn fn, void* context) {
    if (count == 0) {
      return;
    }
    if (kForceSerialComponentUpdates || worker_count_ == 0 || count < 2) {
      for (size_t i = 0; i < count; ++i) {
        fn(i, context);
      }
      return;
    }

    {
      std::unique_lock<std::mutex> lock(mutex_);
      done_cv_.wait(lock, [this]() { return !has_task_; });
      task_fn_ = fn;
      task_context_ = context;
      task_count_ = count;
      next_index_.store(0);
      active_workers_ = worker_count_;
      has_task_ = true;
    }

    cv_.notify_all();

    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this]() { return !has_task_; });
  }

 private:
  void WorkerLoop() {
    for (;;) {
      ComponentTaskFn fn = nullptr;
      void* context = nullptr;
      size_t count = 0;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return has_task_; });
        if (shutdown_) {
          return;
        }
        fn = task_fn_;
        context = task_context_;
        count = task_count_;
      }

      for (;;) {
        size_t index = next_index_.fetch_add(1);
        if (index >= count) {
          break;
        }
        fn(index, context);
      }

      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_workers_ > 0) {
          active_workers_--;
        }
        if (active_workers_ == 0) {
          has_task_ = false;
          done_cv_.notify_one();
        }
      }
    }
  }

  std::array<std::thread, kMaxWorkerThreads> workers_;
  size_t worker_count_ = 0;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable done_cv_;
  std::atomic<size_t> next_index_{0};
  ComponentTaskFn task_fn_ = nullptr;
  void* task_context_ = nullptr;
  size_t task_count_ = 0;
  size_t active_workers_ = 0;
  bool has_task_ = false;
  bool shutdown_ = false;
};

ComponentTaskRunner& GetComponentTaskRunner() {
  static ComponentTaskRunner runner;
  return runner;
}

struct ElectricalUpdateContext {
  std::vector<PlacedComponent>* components = nullptr;
  const std::map<PortRef, float>* voltages = nullptr;
};

void UpdateElectricalComponent(size_t index, void* context) {
  auto* ctx = static_cast<ElectricalUpdateContext*>(context);
  if (!ctx || !ctx->components || !ctx->voltages) {
    return;
  }
  if (index >= ctx->components->size()) {
    return;
  }

  auto& comp = (*ctx->components)[index];
  const auto& port_voltages = *ctx->voltages;
  const ComponentPhysicsAdapter* adapter =
      GetComponentPhysicsAdapter(comp.type);
  if (adapter && adapter->UpdateElectrical) {
    adapter->UpdateElectrical(&comp, port_voltages);
  }
}

struct ValveLogicContext {
  std::vector<PlacedComponent>* components = nullptr;
};

void UpdateValveLogicComponent(size_t index, void* context) {
  auto* ctx = static_cast<ValveLogicContext*>(context);
  if (!ctx || !ctx->components) {
    return;
  }
  if (index >= ctx->components->size()) {
    return;
  }

  auto& comp = (*ctx->components)[index];
  const ComponentPhysicsAdapter* adapter =
      GetComponentPhysicsAdapter(comp.type);
  if (adapter && adapter->UpdateValveLogic) {
    adapter->UpdateValveLogic(&comp);
  }
}

struct CylinderUpdateContext {
  std::vector<PlacedComponent>* components = nullptr;
  const std::map<PortRef, float>* pressures = nullptr;
  const std::map<PortRef, float>* exhaust_quality = nullptr;
  const std::map<PortRef, bool>* pneumatic_connected = nullptr;
  float delta_time = 0.0f;
  float piston_mass = 0.0f;
  float friction_coeff = 0.0f;
  float activation_threshold = 0.0f;
  float force_scale = 0.0f;
};

void UpdateCylinderComponent(size_t index, void* context) {
  auto* ctx = static_cast<CylinderUpdateContext*>(context);
  if (!ctx || !ctx->components || !ctx->pressures ||
      !ctx->exhaust_quality || !ctx->pneumatic_connected) {
    return;
  }
  if (index >= ctx->components->size()) {
    return;
  }

  auto& comp = (*ctx->components)[index];
  const ComponentPhysicsAdapter* adapter =
      GetComponentPhysicsAdapter(comp.type);
  if (adapter && adapter->UpdateCylinder) {
    adapter->UpdateCylinder(&comp, *ctx->pressures, *ctx->exhaust_quality,
                            *ctx->pneumatic_connected, ctx->delta_time,
                            ctx->piston_mass, ctx->friction_coeff,
                            ctx->activation_threshold, ctx->force_scale);
  }
}

}  // namespace

/**
 * @brief Main physics simulation with PLC state independence
  * M내 물리 시뮬레이션 와 함께 PLC st에서e 내dependence
 *
 * CRITICAL ACCESS VIOLATION PREVENTION:
 * This method was completely redesigned after resolving 0xC0000005 crashes
 * that occurred when physics simulation was incorrectly tied to PLC state.
 *
 * CRASH ANALYSIS AND RESOLUTION:
 * - PROBLEM: Previous implementation stopped all physics when PLC was in STOP
 * state, causing null pointer access when UI tried to read component states
 * - SOLUTION: Separated physics simulation into three independent stages:
 *   1. Basic physics (always running - sensors, cylinders, physical
 * interactions)
 *   2. PLC logic execution (only when PLC is in RUN state)
 *   3. Advanced physics engine (optional enhancement layer)
 *
 * REAL-WORLD BEHAVIOR MODEL:
 * Physical components continue operating even when PLC is stopped, just like
 * real industrial systems where sensors detect movement and cylinders respond
 * to manual valve operation regardless of PLC state.
 *
 * STABILITY FEATURES:
 * - Exception handling prevents advanced physics failures from affecting basic
 * physics
 * - Fallback mechanisms ensure core functionality continues during engine
 * errors
 * - Network rebuilding only when component topology changes (performance
 * optimization)
 */
void Application::UpdatePhysics() {
  // Basic physics simulation (always active)
  // Physical laws operate independently of PLC control state
  SimulateElectrical();
  UpdateComponentLogic();
  SimulatePneumatic();
  UpdateActuators();

  // PLC logic execution (conditional on PLC running state)
  if (!is_plc_running_) {
    return;
  }

  // Execute ladder logic with error containment
  SimulateLoadedLadder();

  // Advanced physics engine (optional enhancement)
  if (physics_engine_ && physics_engine_->isInitialized &&
      physics_engine_->networking.BuildNetworksFromWiring &&
      physics_engine_->IsSafeToRunSimulation &&
      physics_engine_->IsSafeToRunSimulation(physics_engine_)) {
    // Network reconstruction when topology changes
    static bool networkBuilt = false;
    static size_t lastComponentCount = 0;
    static size_t lastWireCount = 0;

    if (!networkBuilt || placed_components_.size() != lastComponentCount ||
        wires_.size() != lastWireCount) {
      int result = physics_engine_->networking.BuildNetworksFromWiring(
          physics_engine_, wires_.data(), static_cast<int>(wires_.size()),
          placed_components_.data(),
          static_cast<int>(placed_components_.size()));

      if (result == PHYSICS_ENGINE_SUCCESS) {
        physics_engine_->networking.UpdateNetworkTopology(physics_engine_);
        networkBuilt = true;
        lastComponentCount = placed_components_.size();
        lastWireCount = wires_.size();
      }
    }

    // Synchronize PLC outputs to physics engine
    if (is_plc_running_) {
      SyncPLCOutputsToPhysicsEngine();

      float deltaTime = 1.0f / 60.0f;

      try {
        if (physics_engine_->electricalNetwork &&
            physics_engine_->electricalNetwork->nodeCount > 0) {
          SolveElectricalNetworkC(physics_engine_->electricalNetwork,
                                  deltaTime);
        }

        if (physics_engine_->pneumaticNetwork &&
            physics_engine_->pneumaticNetwork->nodeCount > 0) {
          SolvePneumaticNetworkC(physics_engine_->pneumaticNetwork, deltaTime);
        }

        if (physics_engine_->mechanicalSystem &&
            physics_engine_->mechanicalSystem->nodeCount > 0) {
          SolveMechanicalSystemC(physics_engine_->mechanicalSystem, deltaTime);
        }

        SyncPhysicsEngineToApplication();

      } catch (const std::exception& e) {
        if (enable_debug_logging_) {
          std::cout << "[PHYSICS ERROR] Advanced engine exception: " << e.what()
                    << std::endl;
        }
      }
    }
  }
}
/**
 * @brief Execute loaded ladder program using compiled PLC engine with error
  * 실행 로드ed 래더 프로그램 us내g 컴파일d PLC 엔진 와 함께 오류
 * recovery
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
  if (loaded_ladder_program_.rungs.empty()) {
    static bool firstRun = true;
    if (firstRun) {
      SyncLadderProgramFromProgrammingMode();
      if (loaded_ladder_program_.rungs.empty() ||
          (loaded_ladder_program_.rungs.size() <= 1 &&
           loaded_ladder_program_.rungs[0].cells.empty())) {
        CreateDefaultTestLadderProgram();
      }
      firstRun = false;
    }
    return;
  }

  if (!compiled_plc_executor_) {
    return;
  }

  compiled_plc_executor_->SetDebugMode(enable_debug_logging_);
  auto result = compiled_plc_executor_->ExecuteScanCycle();

  if (!result.success && enable_debug_logging_) {
    std::cout << "[PLC ERROR] Scan cycle failed: " << result.errorMessage
              << std::endl;
    return;
  }
}

/**
 * @brief Get PLC device state with dual-engine fallback mechanism
  * 가져오기 PLC device st에서e 와 함께 dul-엔진 fllbck mech는m
 * @param address Device address (e.g., "X0", "Y1", "M5", "T0", "C1")
  * Device 추가ress (e.g., "X0", "Y1", "M5", "T0", "C1")
 * @return Device state or false if invalid/not found
  * Device st에서e 또는 거짓 만약 내vlID/not found
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
  if (address.empty())
    return false;

  // PRIMARY: Use compiled PLC execution engine for optimal performance
  if (compiled_plc_executor_) {
    return compiled_plc_executor_->GetDeviceState(address);
  }

  // FALLBACK: Legacy system for backward compatibility and error recovery
  switch (address[0]) {
    case 'T': {
      auto it = plc_timer_states_.find(address);
      return (it != plc_timer_states_.end()) ? it->second.done : false;
    }
    case 'C': {
      auto it = plc_counter_states_.find(address);
      return (it != plc_counter_states_.end()) ? it->second.done : false;
    }
    default: {
      auto it = plc_device_states_.find(address);
      return (it != plc_device_states_.end()) ? it->second : false;
    }
  }
}

/**
 * @brief Set PLC device state with dual-engine synchronization
  * 설정 PLC device st에서e 와 함께 dul-엔진 synchr위iz에서i위
 * @param address Device address (must be non-empty)
  * Device 추가ress (해야 한다 이다 n위-empty)
 * @param state New device state
  * New device st에서e
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
  if (address.empty())
    return;

  // PRIMARY: Update compiled PLC execution engine
  if (compiled_plc_executor_) {
    compiled_plc_executor_->SetDeviceState(address, state);
  }

  // SYNCHRONIZATION: Update legacy system for consistency
  plc_device_states_[address] = state;
}

void Application::SimulateElectrical() {
  if (is_plc_running_) {
    SimulateLoadedLadder();
  } else {
    for (int i = 0; i < 16; ++i) {
      SetPlcDeviceState("Y" + std::to_string(i), false);
    }
  }

  enum class VoltageType { NONE, V24, V0 };
  constexpr int kMaxPortsPerComponent = 32;
  constexpr int kMaxElectricalComponents = 512;
  constexpr int kMaxElectricalPorts =
      kMaxElectricalComponents * kMaxPortsPerComponent;

  struct ElectricalCache {
    std::array<PortRef, kMaxElectricalPorts> ports;
    int port_count = 0;
    std::array<int, kMaxElectricalComponents * kMaxPortsPerComponent>
        port_index_by_id;
    std::array<int, kMaxElectricalPorts> base_parent;
    std::array<int, kMaxElectricalPorts> base_rank;
    std::array<int, kMaxElectricalPorts> parent;
    std::array<int, kMaxElectricalPorts> rank;
    std::array<int, kMaxElectricalPorts> port_net_id;
    std::array<int, kMaxElectricalPorts> root_to_net;
    std::array<int, kMaxElectricalPorts> net_voltage;
    int net_count = 0;
    std::array<ComponentType, kMaxElectricalComponents> component_types;
    uint64_t last_wiring_hash = 0;
    uint64_t last_component_hash = 0;
    int last_max_component_id = -1;
    bool topology_valid = false;
  };

  static ElectricalCache cache;

  auto hash_combine = [](uint64_t* seed, uint64_t value) {
    *seed ^= value + 0x9e3779b97f4a7c15ULL + (*seed << 6) + (*seed >> 2);
  };

  auto compute_wiring_hash = [&]() {
    uint64_t seed = 0;
    for (const auto& wire : wires_) {
      hash_combine(&seed, static_cast<uint64_t>(wire.id));
      hash_combine(&seed, static_cast<uint64_t>(wire.fromComponentId));
      hash_combine(&seed, static_cast<uint64_t>(wire.fromPortId));
      hash_combine(&seed, static_cast<uint64_t>(wire.toComponentId));
      hash_combine(&seed, static_cast<uint64_t>(wire.toPortId));
      hash_combine(&seed, wire.isElectric ? 1U : 0U);
    }
    return seed;
  };

  auto compute_component_hash = [&]() {
    uint64_t seed = 0;
    for (const auto& comp : placed_components_) {
      hash_combine(&seed, static_cast<uint64_t>(comp.instanceId));
      hash_combine(&seed, static_cast<uint64_t>(comp.type));
    }
    return seed;
  };

  auto find_root = [&](int idx, auto&& find_root_ref) -> int {
    if (cache.parent[idx] != idx) {
      cache.parent[idx] = find_root_ref(cache.parent[idx], find_root_ref);
    }
    return cache.parent[idx];
  };

  auto unite = [&](int a, int b) {
    int root_a = find_root(a, find_root);
    int root_b = find_root(b, find_root);
    if (root_a == root_b) {
      return;
    }
    if (cache.rank[root_a] < cache.rank[root_b]) {
      cache.parent[root_a] = root_b;
    } else if (cache.rank[root_a] > cache.rank[root_b]) {
      cache.parent[root_b] = root_a;
    } else {
      cache.parent[root_b] = root_a;
      cache.rank[root_a]++;
    }
  };

  uint64_t wiring_hash = compute_wiring_hash();
  uint64_t component_hash = compute_component_hash();

  int max_component_id = -1;
  for (const auto& entry : port_positions_) {
    int comp_id = entry.first.first;
    if (comp_id >= 0 && comp_id < kMaxElectricalComponents &&
        comp_id > max_component_id) {
      max_component_id = comp_id;
    }
  }

  bool topology_changed = !cache.topology_valid ||
                          wiring_hash != cache.last_wiring_hash ||
                          component_hash != cache.last_component_hash ||
                          max_component_id != cache.last_max_component_id;

  if (topology_changed) {
    cache.port_count = 0;
    std::fill(cache.port_index_by_id.begin(), cache.port_index_by_id.end(), -1);
    for (const auto& entry : port_positions_) {
      int comp_id = entry.first.first;
      int port_id = entry.first.second;
      if (comp_id < 0 || comp_id > max_component_id ||
          comp_id >= kMaxElectricalComponents) {
        continue;
      }
      if (port_id < 0 || port_id >= kMaxPortsPerComponent) {
        continue;
      }
      if (cache.port_count >= kMaxElectricalPorts) {
        break;
      }
      cache.ports[cache.port_count] = entry.first;
      cache.port_index_by_id[comp_id * kMaxPortsPerComponent + port_id] =
          cache.port_count;
      cache.port_count++;
    }

    std::fill(cache.rank.begin(), cache.rank.end(), 0);
    for (int i = 0; i < cache.port_count; ++i) {
      cache.parent[i] = i;
    }

    for (const auto& wire : wires_) {
      if (!wire.isElectric) {
        continue;
      }
      int from_index = -1;
      int to_index = -1;
      if (wire.fromComponentId >= 0 && wire.fromComponentId <= max_component_id &&
          wire.fromPortId >= 0 && wire.fromPortId < kMaxPortsPerComponent) {
        from_index =
            cache.port_index_by_id[wire.fromComponentId * kMaxPortsPerComponent +
                                     wire.fromPortId];
      }
      if (wire.toComponentId >= 0 && wire.toComponentId <= max_component_id &&
          wire.toPortId >= 0 && wire.toPortId < kMaxPortsPerComponent) {
        to_index =
            cache.port_index_by_id[wire.toComponentId * kMaxPortsPerComponent +
                                     wire.toPortId];
      }
      if (from_index >= 0 && to_index >= 0) {
        unite(from_index, to_index);
      }
    }

    for (int i = 0; i < cache.port_count; ++i) {
      cache.base_parent[i] = cache.parent[i];
      cache.base_rank[i] = cache.rank[i];
    }

    std::fill(cache.component_types.begin(), cache.component_types.end(),
              ComponentType::PLC);
    for (const auto& comp : placed_components_) {
      if (comp.instanceId >= 0 && comp.instanceId <= max_component_id &&
          comp.instanceId < kMaxElectricalComponents) {
        cache.component_types[comp.instanceId] = comp.type;
      }
    }

    port_voltages_.clear();

    cache.last_wiring_hash = wiring_hash;
    cache.last_component_hash = component_hash;
    cache.last_max_component_id = max_component_id;
    cache.topology_valid = true;
  }

  if (cache.port_count == 0) {
    return;
  }

  for (int i = 0; i < cache.port_count; ++i) {
    cache.parent[i] = cache.base_parent[i];
    cache.rank[i] = cache.base_rank[i];
  }

  auto get_index = [&](int comp_id, int port_id) -> int {
    if (comp_id < 0 || comp_id > cache.last_max_component_id ||
        comp_id >= kMaxElectricalComponents) {
      return -1;
    }
    if (port_id < 0 || port_id >= kMaxPortsPerComponent) {
      return -1;
    }
    int offset = comp_id * kMaxPortsPerComponent + port_id;
    if (offset < 0 ||
        offset >= static_cast<int>(cache.port_index_by_id.size())) {
      return -1;
    }
    return cache.port_index_by_id[offset];
  };

  for (const auto& comp : placed_components_) {
    if (comp.type == ComponentType::LIMIT_SWITCH) {
      bool is_pressed = comp.internalStates.count("is_pressed") &&
                        comp.internalStates.at("is_pressed") > 0.5f;
      int com_index = get_index(comp.instanceId, 0);
      int target_port = is_pressed ? 1 : 2;
      int target_index = get_index(comp.instanceId, target_port);
      if (com_index >= 0 && target_index >= 0) {
        unite(com_index, target_index);
      }
    } else if (comp.type == ComponentType::EMERGENCY_STOP) {
      bool is_pressed = comp.internalStates.count(state_keys::kIsPressed) &&
                        comp.internalStates.at(state_keys::kIsPressed) > 0.5f;
      int nc_index_a = get_index(comp.instanceId, 0);
      int nc_index_b = get_index(comp.instanceId, 1);
      int no_index_a = get_index(comp.instanceId, 2);
      int no_index_b = get_index(comp.instanceId, 3);
      if (!is_pressed) {
        if (nc_index_a >= 0 && nc_index_b >= 0) {
          unite(nc_index_a, nc_index_b);
        }
      } else {
        if (no_index_a >= 0 && no_index_b >= 0) {
          unite(no_index_a, no_index_b);
        }
      }
    } else if (comp.type == ComponentType::PROCESSING_CYLINDER) {
      constexpr float kProcDrillUp = 30.0f;
      constexpr float kProcDrillDown = 15.0f;
      constexpr float kProcEpsilon = 0.1f;
      float pos_val = comp.internalStates.count(state_keys::kPosition)
                          ? comp.internalStates.at(state_keys::kPosition)
                          : kProcDrillUp;
      bool at_down = pos_val <= (kProcDrillDown + kProcEpsilon);
      bool at_up = pos_val >= (kProcDrillUp - kProcEpsilon);
      int power0_index = get_index(comp.instanceId, 4);
      int sensor_fwd_index = get_index(comp.instanceId, 1);
      int sensor_rev_index = get_index(comp.instanceId, 2);
      if (power0_index >= 0) {
        if (at_down && sensor_fwd_index >= 0) {
          unite(power0_index, sensor_fwd_index);
        }
        if (at_up && sensor_rev_index >= 0) {
          unite(power0_index, sensor_rev_index);
        }
      }
    } else if (comp.type == ComponentType::BUTTON_UNIT) {
      for (int i = 0; i < 3; ++i) {
        bool is_pressed =
            comp.internalStates.count("is_pressed_" + std::to_string(i)) &&
            comp.internalStates.at("is_pressed_" + std::to_string(i)) > 0.5f;
        int com_index = get_index(comp.instanceId, i * 5 + 0);
        int target_port = is_pressed ? (i * 5 + 1) : (i * 5 + 2);
        int target_index = get_index(comp.instanceId, target_port);
        if (com_index >= 0 && target_index >= 0) {
          unite(com_index, target_index);
        }
      }
    } else if (comp.type == ComponentType::SENSOR ||
               comp.type == ComponentType::INDUCTIVE_SENSOR) {
      bool is_detected = comp.internalStates.count("is_detected") &&
                         comp.internalStates.at("is_detected") > 0.5f;
      if (is_detected) {
        int p24_index = get_index(comp.instanceId, 0);
        int out_index = get_index(comp.instanceId, 2);
        if (p24_index >= 0 && out_index >= 0) {
          unite(p24_index, out_index);
        }
      }
    }
  }

  for (int i = 0; i < cache.port_count; ++i) {
    cache.port_net_id[i] = -1;
    cache.root_to_net[i] = -1;
  }
  int net_count = 0;
  for (int i = 0; i < cache.port_count; ++i) {
    int root = find_root(i, find_root);
    if (cache.root_to_net[root] == -1) {
      cache.root_to_net[root] = net_count++;
    }
    cache.port_net_id[i] = cache.root_to_net[root];
  }

  cache.net_count = net_count;
  for (int i = 0; i < cache.net_count; ++i) {
    cache.net_voltage[i] = static_cast<int>(VoltageType::NONE);
  }

  for (int i = 0; i < cache.port_count; ++i) {
    int comp_id = cache.ports[i].first;
    int port_id = cache.ports[i].second;
    int net_id = cache.port_net_id[i];
    if (net_id < 0) {
      continue;
    }

    if (comp_id >= 0 && comp_id <= cache.last_max_component_id) {
      ComponentType type = cache.component_types[comp_id];
      if (type == ComponentType::POWER_SUPPLY) {
        if (port_id == 0 && cache.net_voltage[net_id] !=
                                static_cast<int>(VoltageType::V0)) {
          cache.net_voltage[net_id] = static_cast<int>(VoltageType::V24);
        }
        if (port_id == 1) {
          cache.net_voltage[net_id] = static_cast<int>(VoltageType::V0);
        }
      } else if (type == ComponentType::PLC && port_id >= 16 && port_id < 32) {
        int y_index = port_id - 16;
        bool y_state = GetPlcDeviceState("Y" + std::to_string(y_index));
        if (y_state) {
          cache.net_voltage[net_id] = static_cast<int>(VoltageType::V0);
        }
      }
    }
  }

  for (int i = 0; i < cache.port_count; ++i) {
    int net_id = cache.port_net_id[i];
    float voltage = -1.0f;
    if (net_id >= 0) {
      VoltageType net_voltage =
          static_cast<VoltageType>(cache.net_voltage[net_id]);
      if (net_voltage == VoltageType::V24) {
        voltage = 24.0f;
      } else if (net_voltage == VoltageType::V0) {
        voltage = 0.0f;
      }
    }
    port_voltages_[cache.ports[i]] = voltage;
  }

  ElectricalUpdateContext update_context;
  update_context.components = &placed_components_;
  update_context.voltages = &port_voltages_;
  GetComponentTaskRunner().Run(placed_components_.size(), UpdateElectricalComponent,
                               &update_context);

  for (const auto& comp : placed_components_) {
    if (comp.type != ComponentType::PLC) {
      continue;
    }
    for (int i = 0; i < 16; ++i) {
      auto key = std::make_pair(comp.instanceId, i);
      float voltage = 0.0f;
      if (port_voltages_.count(key)) {
        voltage = port_voltages_.at(key);
      }
      SetPlcDeviceState("X" + std::to_string(i), voltage > 0.0f);
    }
  }
}
void Application::UpdateComponentLogic() {
  ValveLogicContext update_context;
  update_context.components = &placed_components_;
  GetComponentTaskRunner().Run(placed_components_.size(),
                               UpdateValveLogicComponent, &update_context);
}

void Application::SimulatePneumatic() {
  port_pressures_.clear();
  port_supply_reachable_.clear();
  port_exhaust_quality_.clear();
  port_pneumatic_connected_.clear();

  PneumaticAdjacency pneumatic_adjacency;

  for (const auto& wire : wires_) {
    if (!wire.isElectric) {
      AddBidirectionalEdge(pneumatic_adjacency,
                           std::make_pair(wire.fromComponentId,
                                          wire.fromPortId),
                           std::make_pair(wire.toComponentId, wire.toPortId),
                           1.0f);
    }
  }

  for (const auto& comp : placed_components_) {
    if (comp.type == ComponentType::VALVE_SINGLE) {
      bool sol_a_active =
          comp.internalStates.count(state_keys::kSolenoidAActive) &&
          comp.internalStates.at(state_keys::kSolenoidAActive) > 0.5f;
      PortRef p = std::make_pair(comp.instanceId, 2);
      PortRef a = std::make_pair(comp.instanceId, 3);
      PortRef b = std::make_pair(comp.instanceId, 4);
      if (sol_a_active) {
        AddBidirectionalEdge(pneumatic_adjacency, p, a, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, b, kAtmospherePort, 1.0f, true);
      } else {
        AddBidirectionalEdge(pneumatic_adjacency, p, b, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, a, kAtmospherePort, 1.0f, true);
      }
    } else if (comp.type == ComponentType::VALVE_DOUBLE) {
      float last_active =
          comp.internalStates.count(state_keys::kLastActiveSolenoid)
              ? comp.internalStates.at(state_keys::kLastActiveSolenoid)
              : 0.0f;
      PortRef p = std::make_pair(comp.instanceId, 4);
      PortRef a = std::make_pair(comp.instanceId, 5);
      PortRef b = std::make_pair(comp.instanceId, 6);
      if (last_active > 0.5f && last_active < 1.5f) {
        AddBidirectionalEdge(pneumatic_adjacency, p, a, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, b, kAtmospherePort, 1.0f, true);
      } else if (last_active > 1.5f) {
        AddBidirectionalEdge(pneumatic_adjacency, p, b, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, a, kAtmospherePort, 1.0f, true);
      } else {
        AddBidirectionalEdge(pneumatic_adjacency, p, a, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, b, kAtmospherePort, 1.0f, true);
      }
    } else if (comp.type == ComponentType::MANIFOLD) {
      PortRef input_port = std::make_pair(comp.instanceId, 0);
      for (int i = 1; i <= 4; ++i) {
        PortRef output_port = std::make_pair(comp.instanceId, i);
        AddBidirectionalEdge(pneumatic_adjacency, input_port, output_port,
                             1.0f);
      }
    } else if (comp.type == ComponentType::METER_VALVE) {
      PortRef in_port = std::make_pair(comp.instanceId, 0);
      PortRef out_port = std::make_pair(comp.instanceId, 1);
      float flow_setting = GetMeterValveFlowSetting(comp);
      float restrict_attenuation = ComputeMeterValveAttenuation(flow_setting);
      bool meter_in = IsMeterValveInMode(comp);
      float forward_attenuation = meter_in ? restrict_attenuation : 1.0f;
      float reverse_attenuation = meter_in ? 1.0f : restrict_attenuation;
      AddDirectedEdge(pneumatic_adjacency, in_port, out_port,
                      forward_attenuation, false);
      AddDirectedEdge(pneumatic_adjacency, out_port, in_port,
                      reverse_attenuation, false);
    }
  }

  for (const auto& entry : pneumatic_adjacency) {
    const PortRef& from = entry.first;
    for (const auto& edge : entry.second) {
      if (edge.is_vent) {
        continue;
      }
      port_pneumatic_connected_[from] = true;
      port_pneumatic_connected_[edge.to] = true;
    }
  }

  std::queue<PortRef> pressureQueue;
  constexpr float kPressureEpsilon = 0.001f;
  for (const auto& comp : placed_components_) {
    if (comp.type == ComponentType::FRL) {
      PortRef airOut = std::make_pair(comp.instanceId, 0);
      float airPressure =
          comp.internalStates.count(state_keys::kAirPressure)
              ? comp.internalStates.at(state_keys::kAirPressure)
              : 6.0f;
      port_pressures_[airOut] = airPressure;
      port_supply_reachable_[airOut] = true;
      pressureQueue.push(airOut);
    }
  }

  while (!pressureQueue.empty()) {
    PortRef current = pressureQueue.front();
    pressureQueue.pop();
    float current_pressure =
        port_pressures_.count(current) ? port_pressures_.at(current) : 0.0f;

    auto adjacency_it = pneumatic_adjacency.find(current);
    if (adjacency_it == pneumatic_adjacency.end()) {
      continue;
    }
    for (const auto& edge : adjacency_it->second) {
      if (edge.is_vent) {
        continue;
      }
      float next_pressure = current_pressure * edge.attenuation;
      auto existing = port_pressures_.find(edge.to);
      bool has_reachability =
          port_supply_reachable_.count(edge.to) &&
          port_supply_reachable_.at(edge.to);
      bool should_push = false;
      if (existing == port_pressures_.end() ||
          next_pressure > existing->second + kPressureEpsilon) {
        port_pressures_[edge.to] = next_pressure;
        should_push = true;
      }
      if (!has_reachability) {
        port_supply_reachable_[edge.to] = true;
        should_push = true;
        if (existing == port_pressures_.end()) {
          port_pressures_[edge.to] = next_pressure;
        }
      }
      if (should_push) {
        pressureQueue.push(edge.to);
      }
    }
  }

  PneumaticAdjacency reverse_adjacency;
  std::queue<PortRef> exhaust_queue;
  for (const auto& entry : pneumatic_adjacency) {
    const PortRef& from = entry.first;
    for (const auto& edge : entry.second) {
      if (edge.is_vent) {
        if (!port_exhaust_quality_.count(from)) {
          port_exhaust_quality_[from] = 1.0f;
          exhaust_queue.push(from);
        }
        continue;
      }
      AddDirectedEdge(reverse_adjacency, edge.to, from, edge.attenuation,
                      false);
    }
  }

  while (!exhaust_queue.empty()) {
    PortRef current = exhaust_queue.front();
    exhaust_queue.pop();
    float current_quality = port_exhaust_quality_.count(current)
                                ? port_exhaust_quality_.at(current)
                                : 0.0f;

    auto adjacency_it = reverse_adjacency.find(current);
    if (adjacency_it == reverse_adjacency.end()) {
      continue;
    }
    for (const auto& edge : adjacency_it->second) {
      float next_quality = current_quality * edge.attenuation;
      auto existing = port_exhaust_quality_.find(edge.to);
      if (existing == port_exhaust_quality_.end() ||
          next_quality > existing->second + kPressureEpsilon) {
        port_exhaust_quality_[edge.to] = next_quality;
        exhaust_queue.push(edge.to);
      }
    }
  }
}
/**
 * @brief Update cylinder actuators and sensor detection with crash prevention
  * 업데이트 cyl내der ctu에서또는s 및 sens또는 detecti위 와 함께 cr로서h preventi위
 *
 * CRITICAL ACCESS VIOLATION FIXES:
 * This method was extensively redesigned to prevent crashes that occurred
 * during limit switch detection. Previous issues included array bounds
 * violations, null pointer access, and infinite loops in distance calculations.
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

  CylinderUpdateContext update_context;
  update_context.components = &placed_components_;
  update_context.pressures = &port_pressures_;
  update_context.exhaust_quality = &port_exhaust_quality_;
  update_context.pneumatic_connected = &port_pneumatic_connected_;
  update_context.delta_time = deltaTime;
  update_context.piston_mass = pistonMass;
  update_context.friction_coeff = frictionCoeff;
  update_context.activation_threshold = ACTIVATION_PRESSURE_THRESHOLD;
  update_context.force_scale = FORCE_SCALING_FACTOR;
  GetComponentTaskRunner().Run(placed_components_.size(), UpdateCylinderComponent,
                               &update_context);

  constexpr float kProcDrillUp = 30.0f;
  constexpr float kProcDrillDown = 15.0f;
  constexpr float kProcDrillSpeed = 20.0f;
  constexpr float kProcPressureThreshold = 2.5f;
  constexpr float kProcRotationSpeed = 6.0f;

  for (auto& comp : placed_components_) {
    if (comp.type != ComponentType::PROCESSING_CYLINDER) {
      continue;
    }
    if (!comp.internalStates.count(state_keys::kPosition)) {
      comp.internalStates[state_keys::kPosition] = kProcDrillUp;
    }

    float pos_val = comp.internalStates.at(state_keys::kPosition);
    PortRef port_a = {comp.instanceId, 5};
    PortRef port_b = {comp.instanceId, 6};
    float pressure_a =
        port_pressures_.count(port_a) ? port_pressures_.at(port_a) : 0.0f;
    float pressure_b =
        port_pressures_.count(port_b) ? port_pressures_.at(port_b) : 0.0f;
    bool connected_a = port_pneumatic_connected_.count(port_a) &&
                       port_pneumatic_connected_.at(port_a);
    bool connected_b = port_pneumatic_connected_.count(port_b) &&
                       port_pneumatic_connected_.at(port_b);
    float exhaust_quality_a =
        port_exhaust_quality_.count(port_a)
            ? Clamp01(port_exhaust_quality_.at(port_a))
            : 0.0f;
    float exhaust_quality_b =
        port_exhaust_quality_.count(port_b)
            ? Clamp01(port_exhaust_quality_.at(port_b))
            : 0.0f;
    float effective_a = pressure_a * exhaust_quality_b;
    float effective_b = pressure_b * exhaust_quality_a;

    bool up_cmd = connected_a && connected_b &&
                  effective_a >= kProcPressureThreshold;
    bool down_cmd = connected_a && connected_b &&
                    effective_b >= kProcPressureThreshold;
    float target = pos_val;
    if (up_cmd && !down_cmd) {
      target = kProcDrillUp;
    } else if (down_cmd && !up_cmd) {
      target = kProcDrillDown;
    } else if (up_cmd && down_cmd) {
      target = (effective_a >= effective_b) ? kProcDrillUp : kProcDrillDown;
    }

    if (target > pos_val) {
      pos_val += kProcDrillSpeed * deltaTime;
      if (pos_val > target) {
        pos_val = target;
      }
    } else if (target < pos_val) {
      pos_val -= kProcDrillSpeed * deltaTime;
      if (pos_val < target) {
        pos_val = target;
      }
    }
    pos_val = std::max(kProcDrillDown, std::min(kProcDrillUp, pos_val));
    comp.internalStates[state_keys::kPosition] = pos_val;

    bool motor_on = comp.internalStates.count(state_keys::kMotorOn) &&
                    comp.internalStates.at(state_keys::kMotorOn) > 0.5f;
    if (motor_on) {
      float angle = comp.internalStates.count(state_keys::kRotationAngle)
                        ? comp.internalStates.at(state_keys::kRotationAngle)
                        : 0.0f;
      angle += kProcRotationSpeed * deltaTime;
      if (angle > 6.28318530718f) {
        angle -= 6.28318530718f;
      }
      comp.internalStates[state_keys::kRotationAngle] = angle;
    }
  }

  // Sensor and limit switch detection system
  static int debugCounter = 0;
  bool enableDetailedDebug = (++debugCounter % 60 == 0);

  for (auto& sensor : placed_components_) {
    if (sensor.type == ComponentType::LIMIT_SWITCH ||
        sensor.type == ComponentType::SENSOR) {
      bool isActivatedByPhysics = false;
      bool isActivatedByWorkpiece = false;
      float minDistance = 999999.0f;
      int closestCylinderId = -1;
      Aabb sensor_box = GetAabb(sensor);

      for (const auto& workpiece : placed_components_) {
        if (!IsWorkpiece(workpiece.type)) {
          continue;
        }
        Aabb workpiece_box = GetWorkpieceDetectionAabb(workpiece);
        if (AabbOverlaps(sensor_box, workpiece_box)) {
          isActivatedByWorkpiece = true;
          break;
        }
      }

      for (const auto& cylinder : placed_components_) {
        if (cylinder.type == ComponentType::CYLINDER) {
          float cylinderX_body_start = cylinder.position.x + 170.0f;
          float pistonPosition = cylinder.internalStates.count("position")
                                     ? cylinder.internalStates.at("position")
                                     : 0.0f;
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
          float distance = std::sqrt(dx * dx + dy * dy);

          if (distance < minDistance) {
            minDistance = distance;
            closestCylinderId = cylinder.instanceId;
          }

          float detectionRange =
              (sensor.type == ComponentType::LIMIT_SWITCH) ? 100.0f : 75.0f;

          if (distance < detectionRange) {
            isActivatedByPhysics = true;

            if (enableDetailedDebug && enable_debug_logging_) {
              std::cout << "[SENSOR] "
                        << (sensor.type == ComponentType::LIMIT_SWITCH
                                ? "LimitSwitch"
                                : "Sensor")
                        << "[" << sensor.instanceId << "] detected at distance "
                        << distance << "mm" << std::endl;
            }
            break;
          }
        }
      }

      (void)closestCylinderId;  // Unused variable

      if (sensor.type == ComponentType::LIMIT_SWITCH) {
        bool isPressedManually =
            sensor.internalStates.count(state_keys::kIsPressedManual) &&
            sensor.internalStates.at(state_keys::kIsPressedManual) > 0.5f;
        sensor.internalStates["is_pressed"] =
            (isActivatedByPhysics || isActivatedByWorkpiece ||
             isPressedManually)
                ? 1.0f
                : 0.0f;
      } else {
        sensor.internalStates["is_detected"] =
            (isActivatedByPhysics || isActivatedByWorkpiece) ? 1.0f : 0.0f;
      }
    }
  }

  UpdateWorkpieceInteractions(deltaTime);
}

void Application::UpdateWorkpieceInteractions(float delta_time) {
  const float kSnapDistance = 10.0f;
  const float kConveyorSpeed = 60.0f;
  const int kMaxWorkpieceSubsteps = 60;

  for (auto& sensor : placed_components_) {
    if (sensor.type == ComponentType::RING_SENSOR ||
        sensor.type == ComponentType::INDUCTIVE_SENSOR) {
      sensor.internalStates[state_keys::kIsDetected] = 0.0f;
    }
  }

  float max_speed = 0.0f;
  for (const auto& workpiece : placed_components_) {
    if (!IsWorkpiece(workpiece.type)) {
      continue;
    }
    float vel_x = workpiece.internalStates.count(state_keys::kVelX)
                      ? workpiece.internalStates.at(state_keys::kVelX)
                      : 0.0f;
    float vel_y = workpiece.internalStates.count(state_keys::kVelY)
                      ? workpiece.internalStates.at(state_keys::kVelY)
                      : 0.0f;
    float speed = std::sqrt(vel_x * vel_x + vel_y * vel_y);
    if (speed > max_speed) {
      max_speed = speed;
    }
  }

  int substeps = static_cast<int>(std::ceil(max_speed));
  if (substeps < 4) {
    substeps = 4;
  } else if (substeps > kMaxWorkpieceSubsteps) {
    substeps = kMaxWorkpieceSubsteps;
  }
  float step_dt = delta_time / static_cast<float>(substeps);

  auto apply_sensor_detection = [&](PlacedComponent& workpiece,
                                    bool allow_snap) {
    Aabb workpiece_detection = GetWorkpieceDetectionAabb(workpiece);
    for (auto& sensor : placed_components_) {
      if (sensor.type != ComponentType::RING_SENSOR &&
          sensor.type != ComponentType::INDUCTIVE_SENSOR) {
        continue;
      }
      Aabb sensor_box = GetAabb(sensor);
      if (!AabbOverlaps(workpiece_detection, sensor_box)) {
        continue;
      }
      if (sensor.type == ComponentType::INDUCTIVE_SENSOR) {
        bool is_metal =
            workpiece.type == ComponentType::WORKPIECE_METAL ||
            (workpiece.internalStates.count(state_keys::kIsMetal) &&
             workpiece.internalStates.at(state_keys::kIsMetal) > 0.5f);
        if (!is_metal) {
          continue;
        }
      }
      sensor.internalStates[state_keys::kIsDetected] = 1.0f;
      if (allow_snap && sensor.type == ComponentType::RING_SENSOR) {
        float center_x = sensor.position.x + sensor.size.width * 0.5f;
        float center_y = sensor.position.y + 28.0f;
        float delta_x =
            (workpiece.position.x + workpiece.size.width * 0.5f) - center_x;
        float delta_y =
            (workpiece.position.y + workpiece.size.height * 0.5f) - center_y;
        if (std::abs(delta_x) <= kSnapDistance &&
            std::abs(delta_y) <= kSnapDistance) {
          workpiece.position.x = center_x - workpiece.size.width * 0.5f;
          workpiece.position.y = center_y - workpiece.size.height * 0.5f;
        }
      }
    }
  };

  for (int step = 0; step < substeps; ++step) {
    for (auto& workpiece : placed_components_) {
      if (!IsWorkpiece(workpiece.type)) {
        continue;
      }

      bool manual_drag =
          workpiece.internalStates.count(state_keys::kIsManualDrag) &&
          workpiece.internalStates.at(state_keys::kIsManualDrag) > 0.5f;
      if (manual_drag) {
        apply_sensor_detection(workpiece, false);
        continue;
      }

      bool stuck_box =
          workpiece.internalStates.count(state_keys::kIsStuckBox) &&
          workpiece.internalStates.at(state_keys::kIsStuckBox) > 0.5f;
      if (stuck_box) {
        workpiece.internalStates[state_keys::kVelX] = 0.0f;
        workpiece.internalStates[state_keys::kVelY] = 0.0f;
        continue;
      }

      workpiece.internalStates[state_keys::kIsContacted] = 0.0f;
      float vel_x = workpiece.internalStates.count(state_keys::kVelX)
                        ? workpiece.internalStates.at(state_keys::kVelX)
                        : 0.0f;
      float vel_y = workpiece.internalStates.count(state_keys::kVelY)
                        ? workpiece.internalStates.at(state_keys::kVelY)
                        : 0.0f;

      Aabb workpiece_box = GetAabb(workpiece);
      bool on_conveyor = false;
      bool pushed_by_cylinder = false;

      for (const auto& comp : placed_components_) {
        if (comp.type != ComponentType::CONVEYOR) {
          continue;
        }
        bool active = comp.internalStates.count(state_keys::kMotorActive) &&
                      comp.internalStates.at(state_keys::kMotorActive) > 0.5f;
        if (!active) {
          continue;
        }
        Aabb conveyor_box = GetAabb(comp);
        if (AabbOverlaps(workpiece_box, conveyor_box)) {
          on_conveyor = true;
          vel_x = kConveyorSpeed;
          float target_y = GetCenterY(conveyor_box) -
                           (workpiece.size.height * 0.5f);
          float delta_y =
              (workpiece.position.y + workpiece.size.height * 0.5f) -
              GetCenterY(conveyor_box);
          if (std::abs(delta_y) <= kSnapDistance) {
            workpiece.position.y = target_y;
          }
        }
      }

      if (!on_conveyor) {
        vel_x = 0.0f;
      }

      for (const auto& comp : placed_components_) {
        if (comp.type != ComponentType::CYLINDER) {
          continue;
        }
        float pos_val =
            comp.internalStates.count(state_keys::kPosition)
                ? comp.internalStates.at(state_keys::kPosition)
                : 0.0f;
        float velocity =
            comp.internalStates.count(state_keys::kVelocity)
                ? comp.internalStates.at(state_keys::kVelocity)
                : 0.0f;
        float rod_tip_x = comp.position.x + 170.0f - pos_val;
        float rod_tip_y = comp.position.y + 25.0f;
        const float kRodHitLeft = 12.0f;
        const float kRodHitRight = 20.0f;
        float rod_world_vel_x = -velocity;
        float sweep = rod_world_vel_x * step_dt;
        float rod_min_x = rod_tip_x - kRodHitLeft;
        float rod_max_x = rod_tip_x + kRodHitRight;
        if (sweep < 0.0f) {
          rod_min_x += sweep;
        } else if (sweep > 0.0f) {
          rod_max_x += sweep;
        }
        Aabb rod_swept = {rod_min_x, rod_tip_y - kRodHitLeft, rod_max_x,
                          rod_tip_y + kRodHitRight};
        if (!AabbOverlaps(workpiece_box, rod_swept)) {
          continue;
        }
        workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
        float delta_y =
            (workpiece.position.y + workpiece.size.height * 0.5f) - rod_tip_y;
        if (std::abs(delta_y) <= kSnapDistance) {
          workpiece.position.y = rod_tip_y - workpiece.size.height * 0.5f;
        }
        const float kContactGap = 1.0f;
        if (rod_world_vel_x > 1.0f || rod_world_vel_x < -1.0f) {
          float workpiece_center_x =
              workpiece.position.x + workpiece.size.width * 0.5f;
          bool in_front =
              (rod_world_vel_x < 0.0f)
                  ? (workpiece_center_x <= rod_tip_x)
                  : (workpiece_center_x >= rod_tip_x);
          if (in_front) {
            float desired_center_x =
                rod_tip_x +
                (rod_world_vel_x < 0.0f
                     ? -(workpiece.size.width * 0.5f + kContactGap)
                     : (workpiece.size.width * 0.5f + kContactGap));
            if (rod_world_vel_x < 0.0f) {
              if (workpiece_center_x > desired_center_x) {
                workpiece_center_x = desired_center_x;
              }
            } else {
              if (workpiece_center_x < desired_center_x) {
                workpiece_center_x = desired_center_x;
              }
            }
            workpiece.position.x =
                workpiece_center_x - workpiece.size.width * 0.5f;
            vel_x = rod_world_vel_x;
            pushed_by_cylinder = true;
          }
        }
      }

      if (pushed_by_cylinder) {
        on_conveyor = false;
      }

      for (const auto& comp : placed_components_) {
        if (comp.type != ComponentType::PROCESSING_CYLINDER) {
          continue;
        }
        constexpr float kProcDrillUp = 30.0f;
        constexpr float kProcDrillDown = 15.0f;
        constexpr float kProcEpsilon = 0.1f;

        Aabb cyl_box = GetAabb(comp);
        float pos_val =
            comp.internalStates.count(state_keys::kPosition)
                ? comp.internalStates.at(state_keys::kPosition)
                : kProcDrillUp;
        float pos_norm =
            (kProcDrillUp - pos_val) / (kProcDrillUp - kProcDrillDown);
        pos_norm = std::max(0.0f, std::min(1.0f, pos_norm));
        float center_x = cyl_box.min_x + 60.0f;
        float center_y = cyl_box.min_y + 90.0f;
        float head_radius = 30.0f;
        Aabb head_box;
        head_box.min_x = center_x - head_radius;
        head_box.max_x = center_x + head_radius;
        head_box.min_y = center_y - head_radius;
        head_box.max_y = center_y + head_radius;

        bool is_up = pos_val >= (kProcDrillUp - kProcEpsilon);
        bool is_down = pos_val <= (kProcDrillDown + kProcEpsilon);
        bool motor_on =
            comp.internalStates.count(state_keys::kMotorOn) &&
            comp.internalStates.at(state_keys::kMotorOn) > 0.5f;
        std::string ready_key =
            std::string("proc_ready_") + std::to_string(comp.instanceId);
        bool ready = workpiece.internalStates.count(ready_key) &&
                     workpiece.internalStates.at(ready_key) > 0.5f;

        bool overlapping = AabbOverlaps(workpiece_box, head_box);
        if (overlapping && is_up) {
          workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
          workpiece.position.y = head_box.max_y;
          float delta_x =
              (workpiece.position.x + workpiece.size.width * 0.5f) - center_x;
          if (std::abs(delta_x) <= kSnapDistance) {
            workpiece.position.x = center_x - workpiece.size.width * 0.5f;
          }
          ready = true;
        } else if (!overlapping) {
          ready = false;
        }

        if (ready && is_down) {
          if (motor_on) {
            workpiece.internalStates[state_keys::kIsProcessed] = 1.0f;
            ready = false;
          } else {
            ready = false;
          }
        }

        workpiece.internalStates[ready_key] = ready ? 1.0f : 0.0f;
      }

      for (const auto& comp : placed_components_) {
        if (comp.type != ComponentType::BOX) {
          continue;
        }
        Aabb box = GetAabb(comp);
        if (AabbOverlaps(workpiece_box, box)) {
          workpiece.internalStates[state_keys::kIsStuckBox] = 1.0f;
          workpiece.position.x = GetCenterX(box) - workpiece.size.width * 0.5f;
          workpiece.position.y = GetCenterY(box) - workpiece.size.height * 0.5f;
          vel_x = 0.0f;
          vel_y = 0.0f;
          break;
        }
      }

      workpiece.position.x += vel_x * step_dt;
      workpiece.position.y += vel_y * step_dt;
      workpiece.internalStates[state_keys::kVelX] = vel_x;
      workpiece.internalStates[state_keys::kVelY] = vel_y;

      workpiece_box = GetAabb(workpiece);
      apply_sensor_detection(workpiece, true);
    }
  }
}

/**
 * @brief Basic physics simulation independent of PLC state
  * B로서ic 물리 시뮬레이션 내dependent 의 PLC st에서e
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

  for (auto& comp : placed_components_) {
    if (comp.type == ComponentType::CYLINDER) {
      if (!comp.internalStates.count("position")) {
        comp.internalStates["position"] = 0.0f;
        comp.internalStates["velocity"] = 0.0f;
      }

      float currentPosition = comp.internalStates.at("position");
      float currentVelocity = comp.internalStates.at("velocity");

      float pressureA = port_pressures_.count({comp.instanceId, 0})
                            ? port_pressures_.at({comp.instanceId, 0})
                            : 0.0f;
      float pressureB = port_pressures_.count({comp.instanceId, 1})
                            ? port_pressures_.at({comp.instanceId, 1})
                            : 0.0f;

      float pressureDiff = pressureA - pressureB;
      float force = 0.0f;

      if (std::abs(pressureDiff) >= ACTIVATION_PRESSURE_THRESHOLD) {
        float effectivePressure =
            std::abs(pressureDiff) - ACTIVATION_PRESSURE_THRESHOLD;
        float forceMagnitude =
            std::pow(effectivePressure, 2.0f) * FORCE_SCALING_FACTOR;
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
        if (currentVelocity < 0)
          currentVelocity = 0;
      } else if (currentPosition > maxStroke) {
        currentPosition = maxStroke;
        if (currentVelocity > 0)
          currentVelocity = 0;
      }

      comp.internalStates["position"] = currentPosition;
      comp.internalStates["velocity"] = currentVelocity;
      comp.internalStates["pressure_a"] = pressureA;
      comp.internalStates["pressure_b"] = pressureB;

      const float velocityThreshold = 1.0f;
      if (currentVelocity > velocityThreshold)
        comp.internalStates["status"] = 1.0f;
      else if (currentVelocity < -velocityThreshold)
        comp.internalStates["status"] = -1.0f;
      else
        comp.internalStates["status"] = 0.0f;
    }
  }

  // Sensor detection logic (independent of PLC state)
  static int debugCounter = 0;
  (void)(++debugCounter);  // Suppress unused variable warning

  for (auto& sensor : placed_components_) {
    if (sensor.type == ComponentType::LIMIT_SWITCH ||
        sensor.type == ComponentType::SENSOR) {
      bool isActivatedByPhysics = false;
      bool isActivatedByWorkpiece = false;
      float minDistance = 999999.0f;
      int closestCylinderId = -1;
      (void)closestCylinderId;  // 미사용 변수 경고 제거

      Aabb sensor_box = GetAabb(sensor);
      for (const auto& workpiece : placed_components_) {
        if (!IsWorkpiece(workpiece.type)) {
          continue;
        }
        Aabb workpiece_box = GetWorkpieceDetectionAabb(workpiece);
        if (AabbOverlaps(sensor_box, workpiece_box)) {
          isActivatedByWorkpiece = true;
          break;
        }
      }

      for (const auto& cylinder : placed_components_) {
        if (cylinder.type == ComponentType::CYLINDER) {
          float cylinderX_body_start = cylinder.position.x + 170.0f;
          float pistonPosition = cylinder.internalStates.count("position")
                                     ? cylinder.internalStates.at("position")
                                     : 0.0f;
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
          float distance = std::sqrt(dx * dx + dy * dy);

          if (distance < minDistance) {
            minDistance = distance;
            closestCylinderId = cylinder.instanceId;
          }

          float detectionRange =
              (sensor.type == ComponentType::LIMIT_SWITCH) ? 150.0f : 100.0f;

          if (distance < detectionRange) {
            isActivatedByPhysics = true;
            break;
          }
        }
      }

      // Update sensor state
      if (sensor.type == ComponentType::LIMIT_SWITCH) {
        bool isPressedManually =
            sensor.internalStates.count("is_pressed_manual") &&
            sensor.internalStates.at("is_pressed_manual") > 0.5f;
        sensor.internalStates["is_pressed"] =
            (isActivatedByPhysics || isActivatedByWorkpiece ||
             isPressedManually)
                ? 1.0f
                : 0.0f;
      } else {
        sensor.internalStates["is_detected"] =
            (isActivatedByPhysics || isActivatedByWorkpiece) ? 1.0f : 0.0f;
      }
    }
  }

  // Basic simulation for PLC STOP state
  if (!is_plc_running_) {
    SimulateElectrical();
    UpdateComponentLogic();
    SimulatePneumatic();
  }
}


/**
 * @brief Synchronize PLC outputs to physics engine with comprehensive error
  * Synchr위ize PLC 출력s 물리 엔진 와 함께 comprehensive 오류
 * handling
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
  if (!physics_engine_ || !physics_engine_->isInitialized) {
    return;
  }

  ElectricalNetwork* elecNet = physics_engine_->electricalNetwork;
  if (!elecNet || elecNet->nodeCount == 0 || !elecNet->nodes) {
    return;
  }

  // PLC 컴포넌트 찾기
  for (const auto& comp : placed_components_) {
    if (comp.type != ComponentType::PLC)
      continue;

    // PLC Y OUTPUT SYNCHRONIZATION (ports 16-31) with bounds checking
    for (int y = 0; y < 16; y++) {
      std::string yAddress = "Y" + std::to_string(y);
      bool yState = GetPlcDeviceState(yAddress);

      // SAFE NODE LOOKUP: Multiple bounds checks prevent array violations
      int nodeId = -1;
      if (comp.instanceId < physics_engine_->maxComponents) {
        // PLC Y ports mapped to port IDs 16+y
        int portId = 16 + y;
        if (portId < 32) {
          nodeId = physics_engine_
                       ->componentPortToElectricalNode[comp.instanceId][portId];
        }
      }

      if (nodeId >= 0 && nodeId < elecNet->nodeCount) {
        ElectricalNode& node = elecNet->nodes[nodeId];

        if (yState) {
          // Y OUTPUT ON: Low voltage (sinking type PLC output)
          node.isVoltageSource = true;
          node.sourceVoltage = 0.0f;
          node.sourceResistance = 0.1f;  // 0.1Ω output resistance
        } else {
          // Y OUTPUT OFF: High impedance (open circuit)
          node.isVoltageSource = false;
          node.sourceVoltage = 0.0f;
          node.sourceResistance = 1e6f;  // 1MΩ (effectively open)
        }

        // PHYSICS STATE SYNCHRONIZATION with bounds checking
        int physicsIndex =
            physics_engine_->componentIdToPhysicsIndex[comp.instanceId];
        if (physicsIndex >= 0 &&
            physicsIndex < physics_engine_->activeComponents) {
          TypedPhysicsState& physState =
              physics_engine_->componentPhysicsStates[physicsIndex];
          if (physState.type == PHYSICS_STATE_PLC) {
            physState.state.plc.outputStates[y] = yState;
            physState.state.plc.outputVoltages[y] = yState ? 0.0f : -1.0f;
            physState.state.plc.outputCurrents[y] =
                yState ? 100.0f : 0.0f;  // 100mA when ON
          }
        }
      }
    }

    // PLC X input port voltage reading from physics engine with bounds checking
    for (int x = 0; x < 16; x++) {
      int nodeId = -1;
      if (comp.instanceId >= 0 &&
          comp.instanceId < physics_engine_->maxComponents && x >= 0 &&
          x < 32 && physics_engine_->componentPortToElectricalNode != nullptr) {
        nodeId =
            physics_engine_->componentPortToElectricalNode[comp.instanceId][x];
      }

      if (nodeId >= 0 && nodeId < elecNet->nodeCount) {
        ElectricalNode& node = elecNet->nodes[nodeId];

        // 입력 임피던스 설정 (고임피던스 입력)
        node.inputImpedance[0] = 10000.0f;  // 10kΩ
        node.responseTime = 0.001f;         // 1ms 응답시간

        // PLCPhysicsState update with bounds checking
        int physicsIndex = -1;
        if (comp.instanceId >= 0 &&
            comp.instanceId < physics_engine_->maxComponents &&
            physics_engine_->componentIdToPhysicsIndex != nullptr) {
          physicsIndex =
              physics_engine_->componentIdToPhysicsIndex[comp.instanceId];
        }
        if (physicsIndex >= 0 &&
            physicsIndex < physics_engine_->activeComponents &&
            physics_engine_->componentPhysicsStates != nullptr) {
          TypedPhysicsState& physState =
              physics_engine_->componentPhysicsStates[physicsIndex];
          if (physState.type == PHYSICS_STATE_PLC) {
            physState.state.plc.inputVoltages[x] = node.voltage;
            physState.state.plc.inputCurrents[x] =
                node.current * 1000.0f;  // A → mA
            physState.state.plc.inputStates[x] = (node.voltage > 0.0f);
          }
        }
      }
    }

    break;  // PLC는 하나만 있다고 가정
  }
}

// Synchronize physics engine results to Application
// Update internalStates, port_voltages_, port_pressures_ with simulation
// results
void Application::SyncPhysicsEngineToApplication() {
  // Basic null pointer checking
  if (!physics_engine_) {
    if (enable_debug_logging_) {
      std::cout << "[Physics] Cannot sync to application: engine is null"
                << std::endl;
    }
    return;
  }

  if (!physics_engine_->isInitialized) {
    if (enable_debug_logging_) {
      std::cout
          << "[Physics] Cannot sync to application: engine not initialized"
          << std::endl;
    }
    return;
  }

  // Network pointer safety checks
  ElectricalNetwork* elecNet = physics_engine_->electricalNetwork;
  PneumaticNetwork* pneuNet = physics_engine_->pneumaticNetwork;
  MechanicalSystem* mechSys = physics_engine_->mechanicalSystem;

  if (!elecNet || !pneuNet || !mechSys) {
    if (enable_debug_logging_) {
      std::cout << "[Physics] Cannot sync to application: one or more networks "
                   "are null"
                << std::endl;
    }
    return;
  }

  (void)mechSys;  // Mechanical system synchronization not yet implemented

  // 1. 전기 네트워크 결과를 m_portVoltages에 동기화
  if (elecNet && elecNet->nodeCount > 0) {
    port_voltages_.clear();

    for (int n = 0; n < elecNet->nodeCount; n++) {
      const ElectricalNode& node = elecNet->nodes[n];
      if (node.base.isActive) {
        std::pair<int, int> portKey =
            std::make_pair(node.base.componentId, node.base.portId);
        port_voltages_[portKey] = node.voltage;
      }
    }
  }

  // 2. 공압 네트워크 결과를 m_portPressures에 동기화
  if (pneuNet && pneuNet->nodeCount > 0) {
    port_pressures_.clear();

    for (int n = 0; n < pneuNet->nodeCount; n++) {
      const PneumaticNode& node = pneuNet->nodes[n];
      if (node.base.isActive) {
        std::pair<int, int> portKey =
            std::make_pair(node.base.componentId, node.base.portId);
        // Pa → bar 변환
        port_pressures_[portKey] = node.gaugePressure / 100000.0f;
      }
    }
  }

  // Component physics state synchronization to internalStates with bounds
  // checking
  for (auto& comp : placed_components_) {
    int physicsIndex = -1;
    if (comp.instanceId >= 0 &&
        comp.instanceId < physics_engine_->maxComponents &&
        physics_engine_->componentIdToPhysicsIndex != nullptr) {
      physicsIndex =
          physics_engine_->componentIdToPhysicsIndex[comp.instanceId];
    }

    if (physicsIndex >= 0 && physicsIndex < physics_engine_->activeComponents &&
        physics_engine_->componentPhysicsStates != nullptr) {
      const TypedPhysicsState& physState =
          physics_engine_->componentPhysicsStates[physicsIndex];

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
          if (cylState.isExtending)
            status = 1.0f;
          else if (cylState.isRetracting)
            status = -1.0f;
          comp.internalStates["status"] = status;

          // 힘 정보 동기화
          comp.internalStates["force"] = cylState.totalForce;
          comp.internalStates["pneumatic_force_a"] = cylState.pneumaticForceA;
          comp.internalStates["pneumatic_force_b"] = cylState.pneumaticForceB;
          comp.internalStates["friction_force"] = cylState.frictionForce;

          break;
        }

        case PHYSICS_STATE_LIMIT_SWITCH: {
          const LimitSwitchPhysicsState& switchState =
              physState.state.limitSwitch;

          // 접촉 상태 동기화
          comp.internalStates["is_pressed"] =
              switchState.isPhysicallyPressed ? 1.0f : 0.0f;
          comp.internalStates["contact_force"] = switchState.contactForce;
          comp.internalStates["displacement"] = switchState.displacement;

          // Synchronize electrical characteristics
          comp.internalStates["contact_resistance_no"] =
              switchState.contactResistanceNO;
          comp.internalStates["contact_resistance_nc"] =
              switchState.contactResistanceNC;
          comp.internalStates["contact_voltage"] = switchState.contactVoltage;

          break;
        }

        case PHYSICS_STATE_SENSOR: {
          const SensorPhysicsState& sensorState = physState.state.sensor;

          // 감지 상태 동기화
          bool prefer_basic_sensor =
              (comp.type == ComponentType::SENSOR ||
               comp.type == ComponentType::INDUCTIVE_SENSOR ||
               comp.type == ComponentType::RING_SENSOR);
          if (!prefer_basic_sensor) {
            comp.internalStates[state_keys::kIsDetected] =
                sensorState.detectionState ? 1.0f : 0.0f;
            comp.internalStates[state_keys::kIsPowered] =
                (sensorState.powerConsumption > 0.1f) ? 1.0f : 0.0f;
          } else {
            bool is_detected =
                comp.internalStates.count(state_keys::kIsDetected) &&
                comp.internalStates.at(state_keys::kIsDetected) > 0.5f;
            bool is_powered =
                comp.internalStates.count(state_keys::kIsPowered) &&
                comp.internalStates.at(state_keys::kIsPowered) > 0.5f;
            comp.internalStates["output_voltage"] =
                (is_detected && is_powered) ? 24.0f : 0.0f;
          }
          comp.internalStates["target_distance"] = sensorState.targetDistance;
          if (!prefer_basic_sensor) {
            comp.internalStates["output_voltage"] = sensorState.outputVoltage;
          }

          // 성능 특성 동기화
          comp.internalStates["sensitivity"] = sensorState.sensitivity;
          comp.internalStates["response_time"] = sensorState.responseTime;

          break;
        }

        case PHYSICS_STATE_VALVE_SINGLE: {
          const ValveSinglePhysicsState& valveState =
              physState.state.valveSingle;

          // 솔레노이드 상태 동기화
          comp.internalStates["solenoid_a_active"] =
              valveState.solenoidEnergized ? 1.0f : 0.0f;
          comp.internalStates["solenoid_voltage"] = valveState.solenoidVoltage;
          comp.internalStates["solenoid_current"] = valveState.solenoidCurrent;

          // 밸브 위치 및 유량 동기화
          comp.internalStates["valve_position"] = valveState.valvePosition;
          comp.internalStates["flow_coefficient_pa"] =
              valveState.flowCoefficientPA;
          comp.internalStates["flow_coefficient_ar"] =
              valveState.flowCoefficientAR;

          break;
        }

        case PHYSICS_STATE_VALVE_DOUBLE: {
          const ValveDoublePhysicsState& valveState =
              physState.state.valveDouble;

          // 양방향 솔레노이드 상태 동기화
          comp.internalStates["solenoid_a_active"] =
              valveState.solenoidEnergizedA ? 1.0f : 0.0f;
          comp.internalStates["solenoid_b_active"] =
              valveState.solenoidEnergizedB ? 1.0f : 0.0f;
          comp.internalStates["last_active_solenoid"] =
              valveState.lastActivePosition;

          // Synchronize electrical characteristics
          comp.internalStates["solenoid_voltage_a"] =
              valveState.solenoidVoltageA;
          comp.internalStates["solenoid_voltage_b"] =
              valveState.solenoidVoltageB;
          comp.internalStates["solenoid_current_a"] =
              valveState.solenoidCurrentA;
          comp.internalStates["solenoid_current_b"] =
              valveState.solenoidCurrentB;

          // 밸브 위치 동기화
          comp.internalStates["valve_position"] = valveState.valvePosition;

          break;
        }

        case PHYSICS_STATE_BUTTON_UNIT: {
          const ButtonUnitPhysicsState& buttonState =
              physState.state.buttonUnit;

          // Synchronize button states
          for (int i = 0; i < 3; i++) {
            comp.internalStates["is_pressed_" + std::to_string(i)] =
                buttonState.buttonStates[i] ? 1.0f : 0.0f;
            comp.internalStates["lamp_on_" + std::to_string(i)] =
                buttonState.ledStates[i] ? 1.0f : 0.0f;
            comp.internalStates["button_voltage_" + std::to_string(i)] =
                buttonState.buttonVoltages[i];
            comp.internalStates["led_brightness_" + std::to_string(i)] =
                buttonState.ledBrightness[i];
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
  bool plc_error = false;
  if (programming_mode_) {
    if (!programming_mode_->GetLastCompileError().empty() ||
        !programming_mode_->GetLastScanError().empty()) {
      plc_error = true;
    }
    if (is_plc_running_ && !programming_mode_->GetLastScanSuccess()) {
      plc_error = true;
    }
  }

  for (auto& comp : placed_components_) {
    if (comp.type == ComponentType::PLC) {
      std::map<std::string, bool>
          xInputs;  // 신규: ProgrammingMode로 보낼 X 입력 버퍼
      for (int x = 0; x < 16; x++) {
        std::string xAddress = "X" + std::to_string(x);
        auto portKey = std::make_pair(comp.instanceId, x);

        if (port_voltages_.count(portKey)) {
          float voltage = port_voltages_.at(portKey);
          bool state = (voltage > 12.0f);  // 12V 임계값
          // 기존 경로 유지 (Application 내부 상태/Executor 동기화)
          SetPlcDeviceState(xAddress, state);
          // 신규 경로: ProgrammingMode에 입력 전달(단일 진입점)
          xInputs[xAddress] = state;
        }
      }
      // 실제 동기화 호출
      if (programming_mode_ && !xInputs.empty()) {
        programming_mode_->UpdateInputsFromSystem(xInputs);
      }
      comp.internalStates[state_keys::kPlcRunning] =
          is_plc_running_ ? 1.0f : 0.0f;
      comp.internalStates[state_keys::kPlcError] = plc_error ? 1.0f : 0.0f;
      break;
    }
  }
}

// Update physics engine performance statistics
// Collect statistics for real-time performance monitoring
void Application::UpdatePhysicsPerformanceStats() {
  if (!physics_engine_ || !physics_engine_->isInitialized)
    return;

  PhysicsPerformanceStats& stats = physics_engine_->performanceStats;

  // 네트워크 크기 통계
  stats.electricalNodes = physics_engine_->electricalNetwork->nodeCount;
  stats.electricalEdges = physics_engine_->electricalNetwork->edgeCount;
  stats.pneumaticNodes = physics_engine_->pneumaticNetwork->nodeCount;
  stats.pneumaticEdges = physics_engine_->pneumaticNetwork->edgeCount;
  stats.mechanicalNodes = physics_engine_->mechanicalSystem->nodeCount;
  stats.mechanicalEdges = physics_engine_->mechanicalSystem->edgeCount;

  // 수렴성 통계
  stats.electricalConverged = physics_engine_->electricalNetwork->isConverged;
  stats.pneumaticConverged = physics_engine_->pneumaticNetwork->isConverged;
  stats.mechanicalStable = physics_engine_->mechanicalSystem->isStable;
  stats.electricalIterations =
      physics_engine_->electricalNetwork->currentIteration;
  stats.pneumaticIterations =
      physics_engine_->pneumaticNetwork->currentIteration;

  // 품질 지표
  ElectricalNetwork* elec = physics_engine_->electricalNetwork;
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

  PneumaticNetwork* pneu = physics_engine_->pneumaticNetwork;
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

  MechanicalSystem* mech = physics_engine_->mechanicalSystem;
  if (mech && mech->nodeCount > 0) {
    // 기계 시스템 총 에너지 계산
    float totalEnergy = 0.0f;
    for (int i = 0; i < mech->nodeCount; i++) {
      const MechanicalNode& node = mech->nodes[i];
      // 운동에너지: ½mv²
      float kineticEnergy = 0.5f * node.mass *
                            (node.velocity[0] * node.velocity[0] +
                             node.velocity[1] * node.velocity[1] +
                             node.velocity[2] * node.velocity[2]);
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
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      currentTime - lastUpdateTime);

  if (elapsed.count() > 0) {
    float deltaSeconds = elapsed.count() / 1000000.0f;
    stats.simulationFPS = 1.0f / deltaSeconds;
    stats.realTimeRatio = (physics_engine_->deltaTime) / deltaSeconds;

    lastUpdateTime = currentTime;
  }

  // 메모리 사용량 추정 (단순화)
  int totalMemory = 0;
  totalMemory += elec ? (elec->nodeCount * sizeof(ElectricalNode) +
                         elec->edgeCount * sizeof(ElectricalEdge))
                      : 0;
  totalMemory += pneu ? (pneu->nodeCount * sizeof(PneumaticNode) +
                         pneu->edgeCount * sizeof(PneumaticEdge))
                      : 0;
  totalMemory += mech ? (mech->nodeCount * sizeof(MechanicalNode) +
                         mech->edgeCount * sizeof(MechanicalEdge))
                      : 0;
  totalMemory += physics_engine_->maxComponents * sizeof(TypedPhysicsState);

  stats.memoryUsage = totalMemory / (1024.0f * 1024.0f);  // bytes → MB

  // 디버깅 출력 (선택적)
  if (physics_engine_->enableLogging && physics_engine_->logLevel >= 3) {
    static int debugCounter = 0;
    if (++debugCounter % 180 == 0) {  // 3초마다 (60FPS 기준)
      std::cout << "[Physics Stats] FPS:" << stats.simulationFPS
                << " Power:" << stats.powerConsumption << "W"
                << " Air:" << stats.airConsumption << "L/min"
                << " Energy:" << stats.mechanicalEnergy << "J"
                << " Memory:" << stats.memoryUsage << "MB" << std::endl;
    }
  }
}

std::vector<std::pair<int, bool>> Application::GetPortsForComponent(
    const PlacedComponent& comp) {
  std::vector<std::pair<int, bool>> ports;
  int max_ports = 0;
  switch (comp.type) {
    case ComponentType::PLC:
      max_ports = 32;
      break;
    case ComponentType::FRL:
      max_ports = 1;
      break;
    case ComponentType::MANIFOLD:
      max_ports = 5;
      break;
    case ComponentType::LIMIT_SWITCH:
      max_ports = 3;
      break;
    case ComponentType::SENSOR:
      max_ports = 3;
      break;
    case ComponentType::CYLINDER:
      max_ports = 2;
      break;
    case ComponentType::VALVE_SINGLE:
      max_ports = 5;
      break;
    case ComponentType::VALVE_DOUBLE:
      max_ports = 7;
      break;
    case ComponentType::BUTTON_UNIT:
      max_ports = 15;
      break;
    case ComponentType::POWER_SUPPLY:
      max_ports = 2;
      break;
    case ComponentType::WORKPIECE_METAL:
    case ComponentType::WORKPIECE_NONMETAL:
      max_ports = 0;
      break;
    case ComponentType::RING_SENSOR:
      max_ports = 3;
      break;
    case ComponentType::METER_VALVE:
      max_ports = 2;
      break;
    case ComponentType::INDUCTIVE_SENSOR:
      max_ports = 3;
      break;
    case ComponentType::CONVEYOR:
      max_ports = 2;
      break;
    case ComponentType::PROCESSING_CYLINDER:
      max_ports = 7;
      break;
    case ComponentType::BOX:
      max_ports = 0;
      break;
    case ComponentType::TOWER_LAMP:
      max_ports = 4;
      break;
    case ComponentType::EMERGENCY_STOP:
      max_ports = 4;
      break;
    default:
      max_ports = 0;
      break;
  }
  for (int i = 0; i < max_ports; ++i) {
    ports.push_back({i, true});
  }
  return ports;
}
}  // namespace plc


