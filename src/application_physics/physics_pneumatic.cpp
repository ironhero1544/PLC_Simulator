// physics_pneumatic.cpp
//
// Pneumatic simulation helpers.

#include "plc_emulator/core/application.h"
#include "plc_emulator/physics/component_physics_adapter.h"

#include "plc_emulator/application_physics/physics_helpers.h"

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace plc {
namespace {

struct PneumaticLink {
  PortRef to;
  float attenuation = 1.0f;
  bool is_vent = false;
};

using PneumaticAdjacency = std::map<PortRef, std::vector<PneumaticLink>>;

constexpr int kAtmosphereComponentId = -1;
constexpr int kAtmospherePortId = -1;
const PortRef kAtmospherePort = {kAtmosphereComponentId, kAtmospherePortId};

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

}  // namespace

void Application::SimulatePneumaticImpl() {
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
        AddBidirectionalEdge(pneumatic_adjacency, p, b, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, a, kAtmospherePort, 1.0f, true);
      } else {
        AddBidirectionalEdge(pneumatic_adjacency, p, a, 1.0f);
        AddDirectedEdge(pneumatic_adjacency, b, kAtmospherePort, 1.0f, true);
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

}  // namespace plc
