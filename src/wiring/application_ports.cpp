// application_ports.cpp
//
// Port detection and connection logic.

// src/Application_Ports.cpp

#include "plc_emulator/core/application.h"

#include "plc_emulator/components/component_registry.h"

namespace plc {

bool Application::IsValidWireConnection(const Port& fromPort,
                                        const Port& toPort) {
  if (fromPort.type != toPort.type)
    return false;
  return true;
}

Port* Application::FindPortAtPosition(ImVec2 worldPos, int& outComponentId) {
  const float kPortRadius = 12.0f;
  const float kPortRadiusSq = kPortRadius * kPortRadius;
  bool found = false;
  float best_dist_sq = kPortRadiusSq;

  for (const auto& component : placed_components_) {
    if (worldPos.x < component.position.x - kPortRadius ||
        worldPos.x > component.position.x + component.size.width + kPortRadius ||
        worldPos.y < component.position.y - kPortRadius ||
        worldPos.y > component.position.y + component.size.height + kPortRadius) {
      continue;
    }

    const ComponentDefinition* def = GetComponentDefinition(component.type);
    if (!def || !def->ports || def->port_count <= 0) {
      continue;
    }

    for (int i = 0; i < def->port_count; ++i) {
      const ComponentPortDef& port_def = def->ports[i];
      ImVec2 port_world = ImVec2(component.position.x + port_def.rel_pos.x,
                                 component.position.y + port_def.rel_pos.y);
      float dx = worldPos.x - port_world.x;
      float dy = worldPos.y - port_world.y;
      float dist_sq = dx * dx + dy * dy;
      if (dist_sq <= best_dist_sq) {
        best_dist_sq = dist_sq;
        outComponentId = component.instanceId;
        temp_port_buffer_ = {port_def.id, port_def.rel_pos, port_def.color,
                             port_def.is_input, port_def.type,
                             port_def.role ? port_def.role : ""};
        found = true;
      }
    }
  }

  if (!found) {
    outComponentId = -1;
    return nullptr;
  }
  return &temp_port_buffer_;
}

}  // namespace plc
