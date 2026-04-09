// physics_electrical.cpp
//
// Electrical simulation helpers.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/physics/component_physics_adapter.h"

#include "plc_emulator/application_physics/physics_helpers.h"
#include "plc_emulator/application_physics/physics_task_runner.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace plc {
namespace {

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

}  // namespace

void Application::SimulateElectricalImpl() {
  if (!is_plc_running_) {
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
      if (wire.fromComponentId >= 0 &&
          wire.fromComponentId <= max_component_id &&
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
      int sensor_drive_index =
          get_index(comp.instanceId,
                    plc_input_mode_ == PlcInputMode::SOURCE ? 4 : 0);
      int sensor_fwd_index = get_index(comp.instanceId, 1);
      int sensor_rev_index = get_index(comp.instanceId, 2);
      if (sensor_drive_index >= 0) {
        if (at_down && sensor_fwd_index >= 0) {
          unite(sensor_drive_index, sensor_fwd_index);
        }
        if (at_up && sensor_rev_index >= 0) {
          unite(sensor_drive_index, sensor_rev_index);
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
               comp.type == ComponentType::INDUCTIVE_SENSOR ||
               comp.type == ComponentType::RING_SENSOR) {
      bool is_detected = comp.internalStates.count("is_detected") &&
                         comp.internalStates.at("is_detected") > 0.5f;
      if (is_detected) {
        SensorOutputMode output_mode = GetSensorOutputMode(comp);
        int drive_port = output_mode == SensorOutputMode::NPN
                             ? get_index(comp.instanceId,
                                         comp.type == ComponentType::RING_SENSOR
                                             ? 2
                                             : 1)
                             : get_index(comp.instanceId, 0);
        int signal_port = (comp.type == ComponentType::RING_SENSOR) ? 1 : 2;
        int out_index = get_index(comp.instanceId, signal_port);
        if (drive_port >= 0 && out_index >= 0) {
          unite(drive_port, out_index);
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
          cache.net_voltage[net_id] =
              (GetPlcOutputOnVoltage() > 12.0f)
                  ? static_cast<int>(VoltageType::V24)
                  : static_cast<int>(VoltageType::V0);
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
  RunComponentTasks(placed_components_.size(), UpdateElectricalComponent,
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
      SetPlcDeviceState("X" + std::to_string(i),
                        IsPlcInputVoltageActive(voltage));
    }
  }
}

}  // namespace plc
