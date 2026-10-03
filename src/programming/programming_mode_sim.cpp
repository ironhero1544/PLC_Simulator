#include "plc_emulator/programming/execution_program.h"
// programming_mode_sim.cpp
//
// Ladder simulation functions.

#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/openplc_compiler_integration.h"
#include "plc_emulator/project/instruction_codec.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#endif

namespace plc {



void ProgrammingMode::SimulateLadderProgram() {
  if (!plc_executor_) {
    return;
  }

  if (NeedsRecompilation()) {
    use_compiled_engine_ = false;
    last_scan_success_ = false;
    last_scan_error_ = "Recompile needed";
    return;
  }

  if (!use_compiled_engine_) {
    return;
  }

  auto now = std::chrono::steady_clock::now();
  if (!scan_time_initialized_) {
    last_scan_time_ = now;
    scan_time_initialized_ = true;
    return;
  }

  double delta_seconds =
      std::chrono::duration<double>(now - last_scan_time_).count();
  last_scan_time_ = now;
  if (delta_seconds < 0.0) {
    delta_seconds = 0.0;
  } else if (delta_seconds > kMaxScanCatchupSeconds) {
    delta_seconds = kMaxScanCatchupSeconds;
  }

  scan_accumulator_ += delta_seconds;
  while (scan_accumulator_ >= kDefaultScanStepSeconds) {
    ExecuteWithOpenPLCEngine();
    scan_accumulator_ -= kDefaultScanStepSeconds;
  }
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
  SyncOpenPLCToTimersCounters();

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
  has_compile_attempted_ = true;
  const LadderProgram& source = ladder_program_;
  OpenPLCCompilerIntegration compiler;
  auto result = compiler.CompileLadderProgramWithIR(source);
  const std::string ldCode;
  if (!result.success) {
    last_compile_error_ = result.errorMessage;
    std::cerr << "[COMPILE ERROR] OpenPLC compilation failed: "
              << result.errorMessage << std::endl;
    compile_failed_ = true;
    use_compiled_engine_ = has_loaded_program_;
    last_failed_hash_ = ComputeProgramHash(ladder_program_);
    UpdateCompileErrorRungsOnCompileFailure(ldCode, last_compile_error_);
    return false;
  }

  std::optional<LadderProgram> compacted;
  bool has_wrapping = false;
  bool has_memos = false;
  for (const auto& rung : ladder_program_.rungs) {
    has_memos = has_memos || !rung.memo.empty();
    for (const auto& cell : rung.cells) {
      has_wrapping = has_wrapping ||
          cell.type == LadderInstructionType::kWrappingSource ||
          cell.type == LadderInstructionType::kWrappingDestination;
    }
  }
  if (has_wrapping && !has_memos) {
    LadderProgram compact;
    std::string layout_error;
    if (plc_emulator::programming::MaterializeLadder(
            result.program, &compact, &layout_error)) {
      bool fits = true;
      for (const auto& rung : compact.rungs) {
        fits = fits && static_cast<int>(rung.cells.size()) <= GetColumnCount();
      }
      if (fits) {
        result.program = *compact.canonical_program;
        compacted = std::move(compact);
      }
    }
  }
  const LadderProgram& compiled_source = compacted ? *compacted : source;
  if ((application_ && !application_->LoadProgrammingProgram(
                           result.program, compiled_source)) ||
      !plc_executor_->LoadFromCompilationResult(result)) {
    last_compile_error_ = "Failed to load compiled code into OpenPLC engine";
    std::cerr
        << "[COMPILE ERROR] Failed to load compiled code into OpenPLC engine"
        << std::endl;
    compile_failed_ = true;
    use_compiled_engine_ = has_loaded_program_;
    last_failed_hash_ = ComputeProgramHash(ladder_program_);
    UpdateCompileErrorRungsOnCompileFailure(ldCode, last_compile_error_);
    return false;
  }

  if (compacted) {
    PushProgrammingUndoState();
    ladder_program_ = std::move(*compacted);
    layout_dirty_ = true;
    SelectSingleCell(std::min(selected_rung_,
        static_cast<int>(ladder_program_.rungs.size()) - 2),
        selected_cell_, false);
  }
  has_loaded_program_ = true;
  last_compiled_hash_ = ComputeProgramHash(ladder_program_);
  is_dirty_ = false;
  last_compile_error_.clear();
  compile_failed_ = false;
  compile_error_rungs_.clear();
  compile_error_rung_hashes_.clear();
  last_failed_hash_ = 0;
  use_compiled_engine_ = true;
  InitializeTimersAndCountersFromProgram();

  std::cout << "[COMPILE] OpenPLC engine loaded successfully ("
            << result.program.instructions.size() << " instructions)" << std::endl;
  return true;
}

void ProgrammingMode::SyncPhysicsToOpenPLC() {
  for (const auto& pair : device_states_) {
    const std::string& address = pair.first;
    bool state = pair.second;

    if (!address.empty() && address[0] == 'X') {
      if (plc_executor_->ResolveDeviceAddress(address)) {
        plc_executor_->SetDeviceState(address, state);
      }
    }
  }
}

void ProgrammingMode::SyncOpenPLCToDevices() {
  // Sync Y0-Y15
  for (int i = 0; i < 16; i++) {
    std::string yAddr = plc_emulator::programming::FormatIOAddress('Y', i);
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

void ProgrammingMode::SyncOpenPLCToTimersCounters() {
  if (!plc_executor_) {
    return;
  }

  for (auto& pair : timer_states_) {
    const std::string& address = pair.first;
    if (address.size() < 2)
      continue;
    const auto device = plc_executor_->ResolveDeviceAddress(address);
    if (!device) continue;
    const int idx = static_cast<int>(device->index);
    TimerState& timer = pair.second;
    timer.value = plc_executor_->GetTimerValue(idx);
    timer.enabled = plc_executor_->GetTimerEnabled(idx);
    if (timer.preset > 0) {
      timer.done = plc_executor_->GetDeviceState(address);
    } else {
      timer.done = timer.enabled;
    }
  }

  for (auto& pair : counter_states_) {
    const std::string& address = pair.first;
    if (address.size() < 2)
      continue;
    const auto device = plc_executor_->ResolveDeviceAddress(address);
    if (!device) continue;
    const int idx = static_cast<int>(device->index);
    CounterState& counter = pair.second;
    counter.value = plc_executor_->GetCounterValue(idx);
    counter.lastPower = plc_executor_->GetCounterLastPower(idx);
    if (counter.preset > 0) {
      counter.done = (counter.value >= counter.preset);
    } else {
      counter.done = (counter.value > 0);
    }
  }
}

void ProgrammingMode::UpdateVisualActiveStates() {
  const CompiledPLCExecutor* executor = monitor_external_plc_ && application_
      ? application_->GetCompiledPlcExecutor() : plc_executor_.get();
  const bool observe = executor && !NeedsRecompilation();
  for (size_t row = 0; row < ladder_program_.rungs.size(); ++row) {
    auto& rung = ladder_program_.rungs[row];
    for (size_t column = 0; column < rung.cells.size(); ++column) {
      auto& cell = rung.cells[column];
      cell.isActive = false;
      if (observe) {
        const auto power = executor->GetCellPower(
            static_cast<int>(row), static_cast<int>(column));
        if (power) {
          cell.isActive = *power;
          continue;
        }
      }
      if (cell.type == LadderInstructionType::XIC)
        cell.isActive = GetDeviceState(cell.address);
      else if (cell.type == LadderInstructionType::XIO)
        cell.isActive = !GetDeviceState(cell.address);
    }
  }
}

bool ProgrammingMode::NeedsRecompilation() const {
  size_t currentHash = ComputeProgramHash(ladder_program_);
  if (compile_failed_ &&
      currentHash == last_failed_hash_) {
    return false;
  }
  if (is_dirty_ || !has_loaded_program_) {
    return true;
  }
  return currentHash != last_compiled_hash_;
}

}  // namespace plc
