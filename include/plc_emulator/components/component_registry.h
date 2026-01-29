#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_REGISTRY_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_REGISTRY_H_

#include "plc_emulator/components/component_definition.h"

namespace plc {

const ComponentDefinition* GetComponentDefinition(ComponentType type);
int GetComponentDefinitionCount();
const ComponentDefinition* GetComponentDefinitionByIndex(int index);

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_REGISTRY_H_
