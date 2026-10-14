#pragma once

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/component_physics_adapter.h"

#include <cstdint>
#include <map>
#include <vector>

namespace plc_emulator {
struct PulseSource {
  int component_id = -1;
  int output = -1;
  uint64_t forward = 0;
  uint64_t reverse = 0;
  uint64_t epoch = 0;
  int direction_output = -1;
};
void StepServoNetwork(std::vector<plc::PlacedComponent>* components,
                      const std::vector<plc::Wire>& wires,
                      const std::map<plc::PortRef, float>& voltages,
                      const std::vector<PulseSource>& pulses, double delta_time,
                      bool active_low);
bool IsServoSignalRole(const std::string& role);
}  // namespace plc_emulator
