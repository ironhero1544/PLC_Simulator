/*
 * rtl_module_def.h
 *
 * Generic render definition for dynamically analyzed RTL modules.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_RTL_MODULE_DEF_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_RTL_MODULE_DEF_H_

#include "plc_emulator/components/component_definition.h"

namespace plc {

const ComponentDefinition* GetRtlModuleDefinition();

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_RTL_MODULE_DEF_H_
