#pragma once

#include "plc_emulator/components/component_definition.h"

namespace plc_emulator {

const plc::ComponentDefinition* GetSensorCylinderDefinition();
const plc::ComponentDefinition* GetSuctionCylinderDefinition();
const plc::ComponentDefinition* GetStopperCylinderDefinition();
const plc::ComponentDefinition* GetVacuumGeneratorDefinition();
void RenderPneumaticFront(ImDrawList* draw_list,
                         const plc::PlacedComponent& component,
                         ImVec2 position, float zoom);

}  // namespace plc_emulator
