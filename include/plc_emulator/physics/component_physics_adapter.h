/*
 * component_physics_adapter.h
 *
 * 컴포넌트 물리 업데이트 함수 포인터 선언.
 * Declarations for component physics update callbacks.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_COMPONENT_PHYSICS_ADAPTER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_COMPONENT_PHYSICS_ADAPTER_H_

#include <map>
#include <utility>

#include "plc_emulator/core/data_types.h"

namespace plc {

using PortRef = std::pair<int, int>;

/*
 * 컴포넌트별 물리 업데이트 훅을 묶습니다.
 * Bundles per-component physics update hooks.
 */
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

/*
 * 컴포넌트 타입에 맞는 물리 어댑터를 반환합니다.
 * Returns the physics adapter for the given component type.
 */
const ComponentPhysicsAdapter* GetComponentPhysicsAdapter(ComponentType type);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PHYSICS_COMPONENT_PHYSICS_ADAPTER_H_ */
