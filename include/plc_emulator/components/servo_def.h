#pragma once

#include "plc_emulator/components/component_definition.h"

namespace plc_emulator {
const plc::ComponentDefinition* GetDcMotorDefinition();
const plc::ComponentDefinition* GetDcMotorDriverDefinition();
const plc::ComponentDefinition* GetServoMotorDefinition();
const plc::ComponentDefinition* GetServoAmplifierDefinition();
const plc::ComponentDefinition* GetServoLiftDefinition();
const plc::ComponentDefinition* GetLiftSuctionDefinition();
const plc::ComponentDefinition* GetAcSupplyDefinition();
void RenderServoFront(ImDrawList* list, const plc::PlacedComponent& component,
                      ImVec2 origin, float zoom);
}  // namespace plc_emulator
