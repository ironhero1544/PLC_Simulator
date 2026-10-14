#pragma once

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/component_physics_adapter.h"

#include <map>
#include <vector>

namespace plc_emulator {
bool IsStorageSensorPart(plc::ComponentType type);
bool IsValidFiberSettings(const plc::FiberSettings& settings);
bool IsValidHmiSettings(const plc::HmiSettings& settings);
int FindHmiPlc(const plc::PlacedComponent& panel,
               const std::vector<plc::PlacedComponent>& parts,
               const std::map<plc::PortRef, float>& voltages);
float WarehouseSlotHeight(const plc::PlacedComponent& warehouse, int slot);
bool StoreWorkpieceInBox(std::vector<plc::PlacedComponent>* parts,
                         const plc::PlacedComponent& box,
                         plc::PlacedComponent* workpiece);
// Editing/loading restores existing slots without capturing free workpieces.
void UpdateWarehouseWorkpieces(std::vector<plc::PlacedComponent>* parts,
                               bool capture_free_workpieces = true);
void UpdateFiberSensors(std::vector<plc::PlacedComponent>* parts,
                        const std::vector<plc::Wire>& wires,
                        const std::map<plc::PortRef, float>& voltages);
}  // namespace plc_emulator
