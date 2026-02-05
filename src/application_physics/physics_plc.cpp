// physics_plc.cpp
//
// PLC execution helpers for physics update loop.

#include "plc_emulator/core/application.h"
#include "plc_emulator/components/state_keys.h"

#include <iostream>
#include <string>

namespace plc {

void Application::SimulateLoadedLadder() {
  if (loaded_ladder_program_.rungs.empty()) {
    static bool firstRun = true;
    if (firstRun) {
      SyncLadderProgramFromProgrammingMode();
      if (loaded_ladder_program_.rungs.empty() ||
          (loaded_ladder_program_.rungs.size() <= 1 &&
           loaded_ladder_program_.rungs[0].cells.empty())) {
        CreateDefaultTestLadderProgram();
      }
      firstRun = false;
    }
    return;
  }

  if (!compiled_plc_executor_) {
    return;
  }

  compiled_plc_executor_->SetDebugMode(enable_debug_logging_);
  auto result = compiled_plc_executor_->ExecuteScanCycle();

  if (!result.success && enable_debug_logging_) {
    std::cout << "[PLC ERROR] Scan cycle failed: " << result.errorMessage
              << std::endl;
    return;
  }
}

bool Application::GetPlcDeviceState(const std::string& address) {
  if (address.empty())
    return false;

  // PRIMARY: Use compiled PLC execution engine for optimal performance
  if (compiled_plc_executor_) {
    return compiled_plc_executor_->GetDeviceState(address);
  }

  // FALLBACK: Legacy system for backward compatibility and error recovery
  switch (address[0]) {
    case 'T': {
      auto it = plc_timer_states_.find(address);
      return (it != plc_timer_states_.end()) ? it->second.done : false;
    }
    case 'C': {
      auto it = plc_counter_states_.find(address);
      return (it != plc_counter_states_.end()) ? it->second.done : false;
    }
    default: {
      auto it = plc_device_states_.find(address);
      return (it != plc_device_states_.end()) ? it->second : false;
    }
  }
}

void Application::SetPlcDeviceState(const std::string& address, bool state) {
  if (address.empty())
    return;

  // PRIMARY: Update compiled PLC execution engine
  if (compiled_plc_executor_) {
    compiled_plc_executor_->SetDeviceState(address, state);
  }

  // SYNCHRONIZATION: Update legacy system for consistency
  plc_device_states_[address] = state;
}



}  // namespace plc
