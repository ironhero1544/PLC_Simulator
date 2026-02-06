#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_PROCESSING_CYLINDER_DEF_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_PROCESSING_CYLINDER_DEF_H_

#include "plc_emulator/components/component_definition.h"

namespace plc {

const ComponentDefinition* GetProcessingCylinderDefinition();
void RenderProcessingCylinderHead(ImDrawList* draw_list,
                                  const PlacedComponent& comp,
                                  ImVec2 pos,
                                  float zoom);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_PROCESSING_CYLINDER_DEF_H_ */
