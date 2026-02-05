// physics_update.cpp
//
// Main physics update loop and advanced engine integration.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/application_physics/physics_helpers.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

extern "C" {
int SolveElectricalNetworkC(plc::ElectricalNetwork* network, float deltaTime);
int SolvePneumaticNetworkC(plc::PneumaticNetwork* network, float deltaTime);
int SolveMechanicalSystemC(plc::MechanicalSystem* system, float deltaTime);
}

namespace {

void HashCombine(uint64_t* seed, uint64_t value) {
  *seed ^= value + 0x9e3779b97f4a7c15ULL + (*seed << 6) + (*seed >> 2);
}

uint64_t ComputeAdvancedTopologyHash(
    const std::vector<plc::PlacedComponent>& components,
    const std::vector<plc::Wire>& wires) {
  uint64_t seed = 0;
  for (const auto& comp : components) {
    if (plc::IsWorkpiece(comp.type)) {
      continue;
    }
    HashCombine(&seed, static_cast<uint64_t>(comp.instanceId));
    HashCombine(&seed, static_cast<uint64_t>(comp.type));
  }
  for (const auto& wire : wires) {
    HashCombine(&seed, static_cast<uint64_t>(wire.id));
    HashCombine(&seed, static_cast<uint64_t>(wire.fromComponentId));
    HashCombine(&seed, static_cast<uint64_t>(wire.fromPortId));
    HashCombine(&seed, static_cast<uint64_t>(wire.toComponentId));
    HashCombine(&seed, static_cast<uint64_t>(wire.toPortId));
    HashCombine(&seed, wire.isElectric ? 1U : 0U);
  }
  return seed;
}

}  // namespace

namespace plc {

void Application::UpdatePhysicsImpl() {
  using clock = std::chrono::steady_clock;
  constexpr double kPhysicsStep = 1.0 / 240.0;
  constexpr double kPlcStep = kPlcScanStepSeconds;
  constexpr double kMaxFrameTime = 0.25;
  constexpr double kAdvancedPhysicsBudgetMs = 4.0;
  constexpr double kAdvancedPhysicsStepFine = 1.0 / 120.0;
  constexpr double kAdvancedPhysicsStepMedium = 1.0 / 60.0;
  constexpr double kAdvancedPhysicsStepCoarse = 1.0 / 30.0;

  auto frame_start = clock::now();
  auto elapsed_ms =
      [](clock::time_point start, clock::time_point end) -> double {
    return std::chrono::duration<double, std::milli>(end - start).count();
  };
  double electrical_ms = 0.0;
  double component_ms = 0.0;
  double pneumatic_ms = 0.0;
  double actuator_ms = 0.0;
  double plc_ms = 0.0;
  double advanced_build_ms = 0.0;
  double advanced_solve_ms = 0.0;
  double advanced_sync_ms = 0.0;
  double advanced_electrical_ms = 0.0;
  double advanced_pneumatic_ms = 0.0;
  double advanced_mechanical_ms = 0.0;
  int physics_steps = 0;
  int advanced_steps = 0;

  auto now = clock::now();
  if (!physics_time_initialized_) {
    last_physics_time_ = now;
    physics_time_initialized_ = true;
  }

  double frame_dt =
      std::chrono::duration<double>(now - last_physics_time_).count();
  last_physics_time_ = now;
  if (frame_dt < 0.0) {
    frame_dt = 0.0;
  } else if (frame_dt > kMaxFrameTime) {
    frame_dt = kMaxFrameTime;
  }

  physics_accumulator_ += frame_dt;
  if (physics_accumulator_ > kMaxFrameTime) {
    physics_accumulator_ = kMaxFrameTime;
  }

  if (!is_plc_running_) {
    plc_accumulator_ = 0.0;
    advanced_last_input_hash_ = 0;
    advanced_inputs_dirty_ = true;
  }

  double advanced_step = kAdvancedPhysicsStepCoarse;
  switch (physics_lod_state_.tier) {
    case PhysicsLodTier::kHigh:
      advanced_step = kAdvancedPhysicsStepFine;
      break;
    case PhysicsLodTier::kMedium:
      advanced_step = kAdvancedPhysicsStepMedium;
      break;
    case PhysicsLodTier::kLow:
    default:
      advanced_step = kAdvancedPhysicsStepCoarse;
      break;
  }

  auto compute_advanced_input_hash = [&]() -> uint64_t {
    uint32_t output_mask = 0;
    for (int i = 0; i < 16; ++i) {
      if (GetPlcDeviceState("Y" + std::to_string(i))) {
        output_mask |= (1u << i);
      }
    }
    uint64_t seed = static_cast<uint64_t>(output_mask);
    HashCombine(&seed, static_cast<uint64_t>(is_plc_running_ ? 1U : 0U));
    return seed;
  };

  size_t network_component_count = 0;
  for (const auto& comp : placed_components_) {
    if (IsWorkpiece(comp.type)) {
      continue;
    }
    ++network_component_count;
  }
  uint64_t topology_hash =
      ComputeAdvancedTopologyHash(placed_components_, wires_);

  bool did_advanced_sync = false;
  bool advanced_ran_this_frame = false;
  while (physics_accumulator_ >= kPhysicsStep) {
    physics_accumulator_ -= kPhysicsStep;
    physics_steps++;

    if (!is_plc_running_) {
      advanced_accumulator_ = 0.0;
    } else {
      advanced_accumulator_ += kPhysicsStep;
      if (advanced_ran_this_frame && advanced_accumulator_ > advanced_step) {
        advanced_accumulator_ = advanced_step;
      }
    }
    bool run_advanced = advanced_accumulator_ >= advanced_step;
    bool topology_changed =
        !advanced_network_built_ || topology_hash != advanced_topology_hash_;
    if (topology_changed) {
      advanced_inputs_dirty_ = true;
    }

    if (advanced_physics_disabled_ &&
        topology_hash != advanced_disable_topology_hash_) {
      advanced_physics_disabled_ = false;
    }

    bool advanced_ready =
        run_advanced && !advanced_ran_this_frame && !advanced_physics_disabled_ &&
        advanced_inputs_dirty_ &&
        is_plc_running_ &&
        physics_engine_ &&
        physics_engine_->isInitialized &&
        physics_engine_->networking.BuildNetworksFromWiring &&
        physics_engine_->IsSafeToRunSimulation &&
        physics_engine_->IsSafeToRunSimulation(physics_engine_);

    // Basic physics simulation (always active)
    // Physical laws operate independently of PLC control state.
    auto stage_start = clock::now();
    SimulateElectrical();
    electrical_ms += elapsed_ms(stage_start, clock::now());
    stage_start = clock::now();
    UpdateComponentLogic();
    component_ms += elapsed_ms(stage_start, clock::now());
    stage_start = clock::now();
    SimulatePneumatic();
    pneumatic_ms += elapsed_ms(stage_start, clock::now());
    stage_start = clock::now();
    UpdateActuators(static_cast<float>(kPhysicsStep), advanced_ready);
    actuator_ms += elapsed_ms(stage_start, clock::now());

    if (is_plc_running_) {
      auto plc_start = clock::now();
      plc_accumulator_ += kPhysicsStep;
      while (plc_accumulator_ >= kPlcStep) {
        SimulateLoadedLadder();
        plc_accumulator_ -= kPlcStep;
      }
      plc_ms += elapsed_ms(plc_start, clock::now());
    }

    uint64_t input_hash = advanced_last_input_hash_;
    if (is_plc_running_) {
      input_hash = compute_advanced_input_hash();
      if (input_hash != advanced_last_input_hash_) {
        advanced_inputs_dirty_ = true;
      }
    }

    // Advanced physics engine (optional enhancement).
    if (advanced_ready) {
      advanced_steps++;
      advanced_accumulator_ -= advanced_step;
      advanced_ran_this_frame = true;
      auto advanced_start = clock::now();
      // Network reconstruction when topology changes.
      if (!advanced_network_built_ ||
          topology_hash != advanced_topology_hash_) {
        auto build_start = clock::now();
        int result = physics_engine_->networking.BuildNetworksFromWiring(
            physics_engine_, wires_.data(), static_cast<int>(wires_.size()),
            placed_components_.data(),
            static_cast<int>(placed_components_.size()));

        if (result == PHYSICS_ENGINE_SUCCESS) {
          physics_engine_->networking.UpdateNetworkTopology(physics_engine_);
          advanced_network_built_ = true;
          advanced_topology_hash_ = topology_hash;
          advanced_last_component_count_ = network_component_count;
          advanced_last_wire_count_ = wires_.size();
        } else {
          advanced_physics_disabled_ = true;
          advanced_disable_components_ = network_component_count;
          advanced_disable_wires_ = wires_.size();
          advanced_disable_topology_hash_ = topology_hash;
          advanced_build_ms += elapsed_ms(build_start, clock::now());
          continue;
        }
        advanced_build_ms += elapsed_ms(build_start, clock::now());
      }

      // Synchronize PLC outputs to physics engine.
      auto sync_start = clock::now();
      SyncPLCOutputsToPhysicsEngine();
      advanced_sync_ms += elapsed_ms(sync_start, clock::now());
      physics_engine_->deltaTime = static_cast<float>(kPhysicsStep);

      bool advanced_solved = false;
      try {
        if (physics_engine_->electricalNetwork &&
            physics_engine_->electricalNetwork->nodeCount > 0) {
          auto solve_start = clock::now();
          SolveElectricalNetworkC(physics_engine_->electricalNetwork,
                                  static_cast<float>(kPhysicsStep));
          double elapsed = elapsed_ms(solve_start, clock::now());
          advanced_electrical_ms += elapsed;
          advanced_solve_ms += elapsed;
        }

        if (physics_engine_->pneumaticNetwork &&
            physics_engine_->pneumaticNetwork->nodeCount > 0) {
          auto solve_start = clock::now();
          SolvePneumaticNetworkC(physics_engine_->pneumaticNetwork,
                                 static_cast<float>(kPhysicsStep));
          double elapsed = elapsed_ms(solve_start, clock::now());
          advanced_pneumatic_ms += elapsed;
          advanced_solve_ms += elapsed;
        }

        if (physics_engine_->mechanicalSystem &&
            physics_engine_->mechanicalSystem->nodeCount > 0) {
          auto solve_start = clock::now();
          SolveMechanicalSystemC(physics_engine_->mechanicalSystem,
                                 static_cast<float>(kPhysicsStep));
          double elapsed = elapsed_ms(solve_start, clock::now());
          advanced_mechanical_ms += elapsed;
          advanced_solve_ms += elapsed;
        }

        sync_start = clock::now();
        SyncPhysicsEngineToApplication();
        advanced_sync_ms += elapsed_ms(sync_start, clock::now());
        did_advanced_sync = true;
        advanced_solved = true;

      } catch (const std::exception& e) {
        if (enable_debug_logging_) {
          std::cout << "[PHYSICS ERROR] Advanced engine exception: " << e.what()
                    << std::endl;
        }
      }

      if (advanced_solved) {
        advanced_last_input_hash_ = input_hash;
        advanced_inputs_dirty_ = false;
      }

      auto advanced_elapsed =
          std::chrono::duration<double, std::milli>(clock::now() -
                                                    advanced_start)
              .count();
      if (advanced_elapsed > kAdvancedPhysicsBudgetMs) {
        advanced_physics_disabled_ = true;
        advanced_disable_components_ = network_component_count;
        advanced_disable_wires_ = wires_.size();
        advanced_disable_topology_hash_ = topology_hash;
      }
    }
  }

  physics_stage_total_ms_ = elapsed_ms(frame_start, clock::now());
  physics_stage_electrical_ms_ = electrical_ms;
  physics_stage_component_ms_ = component_ms;
  physics_stage_pneumatic_ms_ = pneumatic_ms;
  physics_stage_actuator_ms_ = actuator_ms;
  physics_stage_plc_ms_ = plc_ms;
  physics_stage_advanced_build_ms_ = advanced_build_ms;
  physics_stage_advanced_solve_ms_ = advanced_solve_ms;
  physics_stage_advanced_sync_ms_ = advanced_sync_ms;
  physics_stage_advanced_electrical_ms_ = advanced_electrical_ms;
  physics_stage_advanced_pneumatic_ms_ = advanced_pneumatic_ms;
  physics_stage_advanced_mechanical_ms_ = advanced_mechanical_ms;
  physics_stage_steps_ = physics_steps;
  physics_stage_advanced_steps_ = advanced_steps;

  if (!did_advanced_sync) {
    bool plc_error = false;
    if (programming_mode_ && programming_mode_->HasCompileAttempted()) {
      if (!programming_mode_->GetLastCompileError().empty() ||
          !programming_mode_->GetLastScanError().empty()) {
        plc_error = true;
      }
      if (is_plc_running_ && !programming_mode_->GetLastScanSuccess() &&
          !programming_mode_->GetLastScanError().empty()) {
        plc_error = true;
      }
    }

    for (auto& comp : placed_components_) {
      if (comp.type != ComponentType::PLC) {
        continue;
      }
      for (int i = 0; i < 16; ++i) {
        std::string x_key = std::string(state_keys::kPlcXPrefix) +
                            std::to_string(i);
        std::string y_key = std::string(state_keys::kPlcYPrefix) +
                            std::to_string(i);
        bool x_active = false;
        auto port_key = std::make_pair(comp.instanceId, i);
        auto voltage_it = port_voltages_.find(port_key);
        if (voltage_it != port_voltages_.end()) {
          x_active = voltage_it->second > 12.0f;
        } else {
          x_active = GetPlcDeviceState("X" + std::to_string(i));
        }
        bool y_active = GetPlcDeviceState("Y" + std::to_string(i));
        comp.internalStates[x_key] = x_active ? 1.0f : 0.0f;
        comp.internalStates[y_key] = y_active ? 1.0f : 0.0f;
      }
      comp.internalStates[state_keys::kPlcRunning] =
          is_plc_running_ ? 1.0f : 0.0f;
      comp.internalStates[state_keys::kPlcError] = plc_error ? 1.0f : 0.0f;
      break;
    }
  }
}

void Application::PrewarmAdvancedPhysicsNetworks() {
  if (!physics_engine_ || !physics_engine_->isInitialized) {
    return;
  }
  if (!physics_engine_->networking.BuildNetworksFromWiring ||
      !physics_engine_->networking.UpdateNetworkTopology) {
    return;
  }
  if (physics_engine_->IsSafeToRunSimulation &&
      !physics_engine_->IsSafeToRunSimulation(physics_engine_)) {
    return;
  }

  size_t network_component_count = 0;
  for (const auto& comp : placed_components_) {
    if (IsWorkpiece(comp.type)) {
      continue;
    }
    ++network_component_count;
  }

  uint64_t topology_hash =
      ComputeAdvancedTopologyHash(placed_components_, wires_);
  if (advanced_network_built_ &&
      advanced_topology_hash_ == topology_hash) {
    return;
  }

  if (placed_components_.empty()) {
    return;
  }

  int result = physics_engine_->networking.BuildNetworksFromWiring(
      physics_engine_, wires_.data(), static_cast<int>(wires_.size()),
      placed_components_.data(), static_cast<int>(placed_components_.size()));
  if (result == PHYSICS_ENGINE_SUCCESS) {
    physics_engine_->networking.UpdateNetworkTopology(physics_engine_);
    advanced_network_built_ = true;
    advanced_topology_hash_ = topology_hash;
    advanced_last_component_count_ = network_component_count;
    advanced_last_wire_count_ = wires_.size();
    advanced_physics_disabled_ = false;
    advanced_inputs_dirty_ = true;
  } else {
    advanced_physics_disabled_ = true;
    advanced_disable_components_ = network_component_count;
    advanced_disable_wires_ = wires_.size();
    advanced_disable_topology_hash_ = topology_hash;
  }
}

}  // namespace plc
