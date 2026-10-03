#include "plc_emulator/core/application.h"

#include "plc_emulator/physics/pneumatic_simulation.h"

namespace plc {

void Application::SimulatePneumaticImpl() {
  SimulatePneumaticNetwork(wires_, placed_components_, &port_pressures_,
                           &port_supply_reachable_, &port_exhaust_quality_,
                           &port_pneumatic_connected_);
}

}  // namespace plc
