#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_COMPONENT_PHYSICS_ADAPTER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_COMPONENT_PHYSICS_ADAPTER_H_

#include <map>
#include <utility>

#include "plc_emulator/core/data_types.h"

namespace plc {

using PortRef = std::pair<int, int>;

struct ComponentPhysicsAdapter {
  void (*UpdateElectrical)(PlacedComponent* comp,
                           const std::map<PortRef, float>& voltages);
  void (*UpdateValveLogic)(PlacedComponent* comp);
  void (*UpdateCylinder)(PlacedComponent* comp,
                         const std::map<PortRef, float>& pressures,
                         const std::map<PortRef, float>& exhaust_quality,
                         const std::map<PortRef, bool>& pneumatic_connected,
                         float delta_time,
                         float piston_mass,
                         float friction_coeff,
                         float activation_threshold,
                         float force_scale);
};

const ComponentPhysicsAdapter* GetComponentPhysicsAdapter(ComponentType type);

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_COMPONENT_PHYSICS_ADAPTER_H_
