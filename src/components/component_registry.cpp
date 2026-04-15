#include "plc_emulator/components/component_registry.h"

#include "plc_emulator/components/button_unit_def.h"
#include "plc_emulator/components/box_def.h"
#include "plc_emulator/components/conveyor_def.h"
#include "plc_emulator/components/cylinder_def.h"
#include "plc_emulator/components/emergency_stop_def.h"
#include "plc_emulator/components/frl_def.h"
#include "plc_emulator/components/inductive_sensor_def.h"
#include "plc_emulator/components/limit_switch_def.h"
#include "plc_emulator/components/manifold_def.h"
#include "plc_emulator/components/meter_valve_def.h"
#include "plc_emulator/components/plc_def.h"
#include "plc_emulator/components/processing_cylinder_def.h"
#include "plc_emulator/components/power_supply_def.h"
#include "plc_emulator/components/ring_sensor_def.h"
#include "plc_emulator/components/rtl_module_def.h"
#include "plc_emulator/components/sensor_def.h"
#include "plc_emulator/components/tower_lamp_def.h"
#include "plc_emulator/components/valve_double_def.h"
#include "plc_emulator/components/valve_single_def.h"
#include "plc_emulator/components/workpiece_def.h"

namespace plc {

namespace {

const ComponentDefinition* const kDefinitions[] = {
    GetPlcDefinition(),
    GetFrlDefinition(),
    GetManifoldDefinition(),
    GetLimitSwitchDefinition(),
    GetSensorDefinition(),
    GetCylinderDefinition(),
    GetValveSingleDefinition(),
    GetValveDoubleDefinition(),
    GetButtonUnitDefinition(),
    GetPowerSupplyDefinition(),
    GetWorkpieceMetalDefinition(),
    GetWorkpieceNonmetalDefinition(),
    GetRingSensorDefinition(),
    GetMeterValveDefinition(),
    GetInductiveSensorDefinition(),
    GetConveyorDefinition(),
    GetProcessingCylinderDefinition(),
    GetBoxDefinition(),
    GetTowerLampDefinition(),
    GetEmergencyStopDefinition(),
    GetRtlModuleDefinition(),
};

int GetDefinitionCount() {
  return static_cast<int>(sizeof(kDefinitions) / sizeof(kDefinitions[0]));
}

}  // namespace

const ComponentDefinition* GetComponentDefinition(ComponentType type) {
  for (int i = 0; i < GetDefinitionCount(); ++i) {
    const ComponentDefinition* def = kDefinitions[i];
    if (def && def->type == type) {
      return def;
    }
  }
  return nullptr;
}

int GetComponentDefinitionCount() {
  return GetDefinitionCount();
}

const ComponentDefinition* GetComponentDefinitionByIndex(int index) {
  if (index < 0 || index >= GetDefinitionCount()) {
    return nullptr;
  }
  return kDefinitions[index];
}

}  // namespace plc
