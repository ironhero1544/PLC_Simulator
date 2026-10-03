#pragma once

#include <map>
#include <vector>

#include "plc_emulator/physics/component_physics_adapter.h"

namespace plc {

// Shared by the application and headless physical integration tests.
void SimulatePneumaticNetwork(
    const std::vector<Wire>& wires,
    const std::vector<PlacedComponent>& components,
    std::map<PortRef, float>* pressures,
    std::map<PortRef, bool>* supply_reachable,
    std::map<PortRef, float>* exhaust_quality,
    std::map<PortRef, bool>* connected);

}  // namespace plc
