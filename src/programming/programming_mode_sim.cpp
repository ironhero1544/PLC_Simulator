#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/openplc_compiler_integration.h"

#include <fstream>
#include <iostream>

namespace plc {

void ProgrammingMode::SimulateLadderProgram() {
  if (use_compiled_engine_ && plc_executor_) {
    ExecuteWithOpenPLCEngine();
  } else {
    ExecuteWithLegacySimulation();
  }
}

void ProgrammingMode::ExecuteWithOpenPLCEngine() {
  try {
    // STEP 1: Recompilation check with UI structure protection
    if (NeedsRecompilation()) {
      LadderProgram programCopy = DeepCopyLadderProgram(ladder_program_);
      (void)programCopy;  // 구조 보호 목적의 딥카피 (현 시점에서 미사용)

      if (!CompileLadderToOpenPLC()) {
        std::cerr << "[PLC ERROR] OpenPLC compilation failed, switching to "
                     "legacy simulation"
                  << std::endl;
        use_compiled_engine_ = false;
        ExecuteWithLegacySimulation();
        return;
      }
    }

    // STEP 2: Input synchronization with error detection
    SyncPhysicsToOpenPLC();

    // STEP 3: Execute OpenPLC scan cycle with timeout protection
    auto result = plc_executor_->ExecuteScanCycle();

    // === Phase 4: 스캔 결과 캐시 ===
    last_scan_success_ = result.success;
    last_cycle_time_us_ = result.cycleTime_us;
    last_instruction_count_ = result.instructionCount;
    if (!result.success) {
      last_scan_error_ = result.errorMessage;
    } else {
      last_scan_error_.clear();
    }

    if (result.success) {
      // STEP 4: Output synchronization with state validation
      SyncOpenPLCToDevices();

      SimulatorState currentState = GetCurrentStateSnapshot();
      UpdateUIFromSimulatorState(currentState);

      static int successCount = 0;
      if (successCount % 300 == 0) {
        std::cout << "[OpenPLC] Scan cycle success: " << result.cycleTime_us
                  << "us" << std::endl;
      }
      successCount++;

    } else {
      std::cerr << "[PLC ERROR] OpenPLC scan cycle failed: "
                << result.errorMessage << std::endl;
      std::cerr << "[PLC RECOVERY] Switching to legacy simulation to maintain "
                   "operation"
                << std::endl;
      use_compiled_engine_ = false;
      ExecuteWithLegacySimulation();
    }

  } catch (const std::exception& e) {
    last_scan_success_ = false;
    last_scan_error_ = e.what();
    std::cerr << "[PLC CRITICAL ERROR] OpenPLC engine exception: " << e.what()
              << std::endl;
    std::cerr << "[PLC RECOVERY] Emergency fallback to legacy simulation"
              << std::endl;
    use_compiled_engine_ = false;
    ExecuteWithLegacySimulation();
  }
}

void ProgrammingMode::ExecuteWithLegacySimulation() {
  // GXWorks2 정규화 적용본으로 시뮬레이션 (UI 구조는 불변)
  LadderProgram programCopy = gx2_normalization_enabled_
                                  ? NormalizeLadderGX2(ladder_program_)
                                  : DeepCopyLadderProgram(ladder_program_);

  for (auto& pair : device_states_) {
    if (pair.first[0] == 'Y')
      pair.second = false;
  }

  std::vector<std::vector<bool>> rungPowerFlow(programCopy.rungs.size());

  for (size_t rungIndex = 0; rungIndex < programCopy.rungs.size();
       ++rungIndex) {
    auto& rung = programCopy.rungs[rungIndex];
    if (rung.isEndRung)
      break;

    rungPowerFlow[rungIndex].resize(12, false);
    bool leftPowerRail = true;
    bool powerFlow = leftPowerRail;

    for (size_t cellIndex = 0; cellIndex < rung.cells.size(); ++cellIndex) {
      auto& cell = rung.cells[cellIndex];

      bool verticalPowerInput = false;
      for (const auto& connection : programCopy.verticalConnections) {
        if (connection.x == static_cast<int>(cellIndex)) {
          for (int connectedRung : connection.rungs) {
            if (connectedRung >= 0 &&
                connectedRung < static_cast<int>(rungIndex) &&
                connectedRung < static_cast<int>(rungPowerFlow.size()) &&
                !rungPowerFlow[connectedRung].empty() &&
                cellIndex < rungPowerFlow[connectedRung].size()) {
              if (rungPowerFlow[connectedRung][cellIndex]) {
                verticalPowerInput = true;
                break;
              }
            }
          }
          if (verticalPowerInput)
            break;
        }
      }

      powerFlow = powerFlow || verticalPowerInput;

      if (cell.type == LadderInstructionType::EMPTY) {
        rungPowerFlow[rungIndex][cellIndex] = powerFlow;
        cell.isActive = false;
        continue;
      }

      bool inputPower = powerFlow;
      bool outputPower = false;

      switch (cell.type) {
        case LadderInstructionType::XIC:
          outputPower = inputPower && GetDeviceState(cell.address);
          cell.isActive = inputPower && GetDeviceState(cell.address);
          break;
        case LadderInstructionType::XIO:
          outputPower = inputPower && !GetDeviceState(cell.address);
          cell.isActive = inputPower && !GetDeviceState(cell.address);
          break;
        case LadderInstructionType::OTE:
          if (inputPower)
            SetDeviceState(cell.address, true);
          cell.isActive = inputPower;
          outputPower = false;
          break;
        case LadderInstructionType::SET:
          if (inputPower)
            SetDeviceState(cell.address, true);
          cell.isActive = inputPower;
          outputPower = false;
          break;
        case LadderInstructionType::RST:
          if (inputPower)
            SetDeviceState(cell.address, false);
          cell.isActive = inputPower;
          outputPower = false;
          break;
        case LadderInstructionType::TON: {
          auto it = timer_states_.find(cell.address);
          if (it != timer_states_.end()) {
            TimerState& timer = it->second;
            if (inputPower) {
              if (!timer.enabled) {
                timer.enabled = true;
                timer.value = 0;
              }
              if (timer.enabled && !timer.done) {
                timer.value++;
                if (timer.value >= timer.preset) {
                  timer.done = true;
                }
              }
            } else {
              timer.enabled = false;
              timer.value = 0;
              timer.done = false;
            }
          }
          cell.isActive = inputPower;
          outputPower = false;
          break;
        }
        case LadderInstructionType::CTU: {
          auto it = counter_states_.find(cell.address);
          if (it != counter_states_.end()) {
            CounterState& counter = it->second;
            if (inputPower && !counter.lastPower) {
              if (!counter.done) {
                counter.value++;
                if (counter.value >= counter.preset) {
                  counter.done = true;
                }
              }
            }
            counter.lastPower = inputPower;
          }
          cell.isActive = inputPower;
          outputPower = false;
          break;
        }
        case LadderInstructionType::RST_TMR_CTR: {
          if (inputPower) {
            if (!cell.address.empty() && cell.address[0] == 'T') {
              auto it = timer_states_.find(cell.address);
              if (it != timer_states_.end()) {
                it->second.value = 0;
                it->second.done = false;
                it->second.enabled = false;
              }
            } else if (!cell.address.empty() && cell.address[0] == 'C') {
              auto it = counter_states_.find(cell.address);
              if (it != counter_states_.end()) {
                it->second.value = 0;
                it->second.done = false;
                it->second.lastPower = false;
              }
            }
          }
          cell.isActive = inputPower;
          outputPower = false;
          break;
        }
        case LadderInstructionType::HLINE:
          outputPower = inputPower;
          cell.isActive = inputPower;
          break;
        default:
          outputPower = inputPower;
          cell.isActive = inputPower;
          break;
      }

      powerFlow = outputPower;
      rungPowerFlow[rungIndex][cellIndex] = powerFlow;
    }
  }
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
  }
}

bool ProgrammingMode::CompileLadderToOpenPLC() {
  try {
    // GXWorks2 정규화 적용본으로 LD 생성
    const LadderProgram& srcProg = gx2_normalization_enabled_
                                       ? NormalizeLadderGX2(ladder_program_)
                                       : ladder_program_;
    std::string ldCode = ld_converter_->ConvertToLDString(srcProg);
    if (ldCode.empty()) {
      last_compile_error_ = "Ladder to LD conversion failed";
      std::cerr << "[COMPILE ERROR] Ladder to LD conversion failed"
                << std::endl;
      return false;
    }

    std::ofstream ldFile("temp_ladder_program.ld");
    if (!ldFile) {
      last_compile_error_ = "Failed to create temporary LD file";
      std::cerr << "[COMPILE ERROR] Failed to create temporary LD file"
                << std::endl;
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
      return false;
    }

    if (!plc_executor_->LoadCompiledCode(result.generatedCode)) {
      last_compile_error_ = "Failed to load compiled code into OpenPLC engine";
      std::cerr
          << "[COMPILE ERROR] Failed to load compiled code into OpenPLC engine"
          << std::endl;
      return false;
    }

    // 상태 갱신: 컴파일된 코드/해시/더티 플래그
    current_compiled_code_ = result.generatedCode;
    last_compiled_hash_ = ComputeProgramHash(ladder_program_);
    is_dirty_ = false;
    last_compile_error_.clear();

    std::cout << "[COMPILE] OpenPLC engine loaded successfully ("
              << result.generatedCode.length() << " chars)" << std::endl;
    return true;

  } catch (const std::exception& e) {
    last_compile_error_ = e.what();
    std::cerr << "[COMPILE] Exception occurred: " << e.what() << std::endl;
    return false;
  }
}

void ProgrammingMode::SyncPhysicsToOpenPLC() {
  for (const auto& pair : device_states_) {
    const std::string& address = pair.first;
    bool state = pair.second;

    if (!address.empty() && address[0] == 'X') {
      // Bounds check: only X0-X15 are valid for current FX3U mapping
      int idx = -1;
      try {
        idx = std::stoi(address.substr(1));
      } catch (...) {
        idx = -1;
      }
      if (idx >= 0 && idx < 16) {
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
  // 더티 플래그, 빈 코드, 해시 변경 모두 고려
  if (is_dirty_ || current_compiled_code_.empty())
    return true;
  size_t currentHash = ComputeProgramHash(ladder_program_);
  return currentHash != last_compiled_hash_;
}

}  // namespace plc