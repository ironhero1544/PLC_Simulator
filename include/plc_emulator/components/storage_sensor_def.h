#pragma once

#include "plc_emulator/components/component_definition.h"

#include <vector>

namespace plc_emulator {
constexpr float kWarehouseOverlayScale = 0.45f;
ImVec2 WarehouseOverlayOrigin(const plc::PlacedComponent& warehouse,
                              float camera_zoom);
int FindWarehouseOverlayWorkpiece(
    const std::vector<plc::PlacedComponent>& parts, ImVec2 world_position,
    float camera_zoom);
const plc::ComponentDefinition* GetWarehouseDefinition();
const plc::ComponentDefinition* GetDrivenWarehouseDefinition();
const plc::ComponentDefinition* GetMagazineDefinition();
const plc::ComponentDefinition* GetFiberAmplifierDefinition();
const plc::ComponentDefinition* GetFiberTransmitterDefinition();
const plc::ComponentDefinition* GetFiberReceiverDefinition();
const plc::ComponentDefinition* GetTouchPanelDefinition();
void RenderHmiScreen(ImDrawList* list, const plc::HmiSettings& settings,
                     const plc::HmiState& state, ImVec2 origin, float zoom,
                     bool show_grid = false);
void RenderStorageSensorFront(ImDrawList* list,
                              const plc::PlacedComponent& component,
                              ImVec2 origin, float zoom,
                              const std::vector<plc::PlacedComponent>& parts);
}  // namespace plc_emulator
