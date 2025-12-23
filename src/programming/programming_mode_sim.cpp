// programming_mode_sim.cpp
// Copyright 2024 PLC Emulator Project
//
// Ladder simulation functions.

#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/openplc_compiler_integration.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <limits>

namespace plc {

namespace {
bool TryParseIndex(const std::string& text, int* value) {
  if (!value || text.empty()) {
    return false;
  }

  char* end = nullptr;
  long parsed = std::strtol(text.c_str(), &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  if (parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }

  *value = static_cast<int>(parsed);
  return true;
}
}  // namespace

void ProgrammingMode::SimulateLadderProgram() {
  if (!plc_executor_) {
    return;
  }

  if (NeedsRecompilation()) {
    use_compiled_engine_ = false;
    if (!CompileLadderToOpenPLC()) {
      return;
    }
  }

  if (!use_compiled_engine_) {
    return;
  }

  ExecuteWithOpenPLCEngine();
}

void ProgrammingMode::ExecuteWithOpenPLCEngine() {
  SyncPhysicsToOpenPLC();

  auto result = plc_executor_->ExecuteScanCycle();

  last_scan_success_ = result.success;
  last_cycle_time_us_ = result.cycleTime_us;
  last_instruction_count_ = result.instructionCount;
  if (!result.success) {
    last_scan_error_ = result.errorMessage;
    use_compiled_engine_ = false;
    return;
  }

  last_scan_error_.clear();

  SyncOpenPLCToDevices();

  SimulatorState currentState = GetCurrentStateSnapshot();
  UpdateUIFromSimulatorState(currentState);
}



bool ProgrammingMode::GetDeviceState(const std::string& address) const {
  if (address.empty())
    return false;

  switch (address[0]) {
    case 'T': {
      auto it = timer_states_.find(address);
      return (it != timer_states_.end()) ? it->second.done : false;
    }
    case 'C': {
      auto it = counter_states_.find(address);
      return (it != counter_states_.end()) ? it->second.done : false;
    }
    case 'X':
    case 'Y':
    case 'M':
    default: {
      auto it = device_states_.find(address);
      return (it != device_states_.end()) ? it->second : false;
    }
  }
}

void ProgrammingMode::SetDeviceState(const std::string& address, bool state) {
  if (!address.empty()) {
    device_states_[address] = state;
    if (!plc_executor_)
      return;

    switch (address[0]) {
      case 'X':
      case 'Y':
      case 'M':
        plc_executor_->SetDeviceState(address, state);
        break;
      case 'T':
        timer_states_[address].done = state;
        break;
      case 'C':
        counter_states_[address].done = state;
        break;
      default:
        break;
    }
  }
}

bool ProgrammingMode::CompileLadderToOpenPLC() {
  use_compiled_engine_ = false;

  const LadderProgram& srcProg = gx2_normalization_enabled_
                                     ? NormalizeLadderGX2(ladder_program_)
                                     : ladder_program_;
  std::string ldCode = ld_converter_->ConvertToLDString(srcProg);
  if (ldCode.empty()) {
    last_compile_error_ = "Ladder to LD conversion failed";
    std::cerr << "[COMPILE ERROR] Ladder to LD conversion failed"
              << std::endl;
    compile_failed_ = true;
    last_failed_hash_ = ComputeProgramHash(ladder_program_);
    current_compiled_code_.clear();
    return false;
  }

  std::ofstream ldFile("temp_ladder_program.ld");
  if (!ldFile) {
    last_compile_error_ = "Failed to create temporary LD file";
    std::cerr << "[COMPILE ERROR] Failed to create temporary LD file"
              << std::endl;
    compile_failed_ = true;
    last_failed_hash_ = ComputeProgramHash(ladder_program_);
    current_compiled_code_.clear();
    return false;
  }
  ldFile << ldCode;
  ldFile.close();

  OpenPLCCompilerIntegration compiler;
  auto result = compiler.CompileLDFile("temp_ladder_program.ld");

  if (!result.success) {
    last_compile_error_ = result.errorMessage;
    std::cerr << "[COMPILE ERROR] OpenPLC compilation failed: "
              << result.errorMessage << std::endl;
    compile_failed_ = true;
    last_failed_hash_ = ComputeProgramHash(ladder_program_);
    current_compiled_code_.clear();
    return false;
  }

  if (!plc_executor_->LoadCompiledCode(result.generatedCode)) {
    last_compile_error_ = "Failed to load compiled code into OpenPLC engine";
    std::cerr
        << "[COMPILE ERROR] Failed to load compiled code into OpenPLC engine"
        << std::endl;
    compile_failed_ = true;
    last_failed_hash_ = ComputeProgramHash(ladder_program_);
    current_compiled_code_.clear();
    return false;
  }

  plc_executor_->ResetMemory();

  current_compiled_code_ = result.generatedCode;
  last_compiled_hash_ = ComputeProgramHash(ladder_program_);
  is_dirty_ = false;
  last_compile_error_.clear();
  compile_failed_ = false;
  last_failed_hash_ = 0;
  use_compiled_engine_ = true;

  std::cout << "[COMPILE] OpenPLC engine loaded successfully ("
            << result.generatedCode.length() << " chars)" << std::endl;
  return true;
}

void ProgrammingMode::SyncPhysicsToOpenPLC() {
  for (const auto& pair : device_states_) {
    const std::string& address = pair.first;
    bool state = pair.second;

    if (!address.empty() && address[0] == 'X') {
      int idx = -1;
      if (TryParseIndex(address.substr(1), &idx) && idx >= 0 && idx < 16) {
        plc_executor_->SetDeviceState(address, state);
      }
    }
  }
}

void ProgrammingMode::SyncOpenPLCToDevices() {
  // Sync Y0-Y15
  for (int i = 0; i < 16; i++) {
    std::string yAddr = "Y" + std::to_string(i);
    bool yState = plc_executor_->GetDeviceState(yAddr);
    auto itY = device_states_.find(yAddr);
    if (itY == device_states_.end() || itY->second != yState) {
      device_states_[yAddr] = yState;
    }
  }

  // Sync M0..M999
  for (int i = 0; i < 1000; i++) {
    std::string mAddr = "M" + std::to_string(i);
    bool mState = plc_executor_->GetDeviceState(mAddr);
    auto itM = device_states_.find(mAddr);
    if (itM == device_states_.end() || itM->second != mState) {
      device_states_[mAddr] = mState;
    }
  }
}

void ProgrammingMode::UpdateVisualActiveStates() {
  for (auto& rung : ladder_program_.rungs) {
    for (auto& cell : rung.cells) {
      if (cell.type == LadderInstructionType::EMPTY) {
        cell.isActive = false;
      } else {
        switch (cell.type) {
          case LadderInstructionType::XIC:
            cell.isActive = plc_executor_->GetDeviceState(cell.address);
            break;
          case LadderInstructionType::XIO:
            cell.isActive = !plc_executor_->GetDeviceState(cell.address);
            break;
          case LadderInstructionType::OTE:
          case LadderInstructionType::SET:
          case LadderInstructionType::RST:
            cell.isActive = plc_executor_->GetDeviceState(cell.address);
            break;
          default:
            cell.isActive = false;
            break;
        }
      }
    }
  }
}

bool ProgrammingMode::NeedsRecompilation() const {
  size_t currentHash = ComputeProgramHash(ladder_program_);
  if (compile_failed_ && current_compiled_code_.empty() &&
      currentHash == last_failed_hash_) {
    return false;
  }
  if (is_dirty_ || current_compiled_code_.empty()) {
    return true;
  }
  return currentHash != last_compiled_hash_;
}

}  // namespace plc
