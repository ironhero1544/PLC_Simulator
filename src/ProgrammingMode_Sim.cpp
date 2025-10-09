#include "ProgrammingMode.h"
#include "CompiledPLCExecutor.h"
#include "LadderToLDConverter.h"
#include "OpenPLCCompilerIntegration.h"
#include <fstream>
#include <iostream>

namespace plc {

void ProgrammingMode::SimulateLadderProgram() {
    if (m_useCompiledEngine && m_plcExecutor) {
        ExecuteWithOpenPLCEngine();
    } else {
        ExecuteWithLegacySimulation();
    }
}

void ProgrammingMode::ExecuteWithOpenPLCEngine() {
    try {
        // STEP 1: Recompilation check with UI structure protection
        if (NeedsRecompilation()) {
            LadderProgram programCopy = DeepCopyLadderProgram(m_ladderProgram);
            (void)programCopy; // 구조 보호 목적의 딥카피 (현 시점에서 미사용)

            if (!CompileLadderToOpenPLC()) {
                std::cerr << "[PLC ERROR] OpenPLC compilation failed, switching to legacy simulation" << std::endl;
                m_useCompiledEngine = false;
                ExecuteWithLegacySimulation();
                return;
            }
        }
        
        // STEP 2: Input synchronization with error detection
        SyncPhysicsToOpenPLC();

        // STEP 3: Execute OpenPLC scan cycle with timeout protection
        auto result = m_plcExecutor->ExecuteScanCycle();
        
        // === Phase 4: 스캔 결과 캐시 ===
        m_lastScanSuccess = result.success;
        m_lastCycleTimeUs = result.cycleTime_us;
        m_lastInstructionCount = result.instructionCount;
        if (!result.success) {
            m_lastScanError = result.errorMessage;
        } else {
            m_lastScanError.clear();
        }

        if (result.success) {
            // STEP 4: Output synchronization with state validation
            SyncOpenPLCToDevices();

            SimulatorState currentState = GetCurrentStateSnapshot();
            UpdateUIFromSimulatorState(currentState);
            
            static int successCount = 0;
            if (successCount % 300 == 0) {
                std::cout << "[OpenPLC] Scan cycle success: " << result.cycleTime_us << "us" << std::endl;
            }
            successCount++;
            
        } else {
            std::cerr << "[PLC ERROR] OpenPLC scan cycle failed: " << result.errorMessage << std::endl;
            std::cerr << "[PLC RECOVERY] Switching to legacy simulation to maintain operation" << std::endl;
            m_useCompiledEngine = false;
            ExecuteWithLegacySimulation();
        }
        
    } catch (const std::exception& e) {
        m_lastScanSuccess = false;
        m_lastScanError = e.what();
        std::cerr << "[PLC CRITICAL ERROR] OpenPLC engine exception: " << e.what() << std::endl;
        std::cerr << "[PLC RECOVERY] Emergency fallback to legacy simulation" << std::endl;
        m_useCompiledEngine = false;
        ExecuteWithLegacySimulation();
    }
}

void ProgrammingMode::ExecuteWithLegacySimulation() {
    // GXWorks2 정규화 적용본으로 시뮬레이션 (UI 구조는 불변)
    LadderProgram programCopy = m_gx2NormalizationEnabled ? NormalizeLadderGX2(m_ladderProgram)
                                                          : DeepCopyLadderProgram(m_ladderProgram);

    for (auto& pair : m_deviceStates) {
        if (pair.first[0] == 'Y') pair.second = false;
    }

    std::vector<std::vector<bool>> rungPowerFlow(programCopy.rungs.size());
    
    for (size_t rungIndex = 0; rungIndex < programCopy.rungs.size(); ++rungIndex) {
        auto& rung = programCopy.rungs[rungIndex];
        if (rung.isEndRung) break;

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
                    if (verticalPowerInput) break;
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
                    if (inputPower) SetDeviceState(cell.address, true);
                    cell.isActive = inputPower;
                    outputPower = false;
                    break;
                case LadderInstructionType::SET:
                    if (inputPower) SetDeviceState(cell.address, true);
                    cell.isActive = inputPower;
                    outputPower = false;
                    break;
                case LadderInstructionType::RST:
                    if (inputPower) SetDeviceState(cell.address, false);
                    cell.isActive = inputPower;
                    outputPower = false;
                    break;
                case LadderInstructionType::TON: {
                    auto it = m_timerStates.find(cell.address);
                    if (it != m_timerStates.end()) {
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
                    auto it = m_counterStates.find(cell.address);
                    if (it != m_counterStates.end()) {
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
                            auto it = m_timerStates.find(cell.address);
                            if (it != m_timerStates.end()) {
                                it->second.value = 0;
                                it->second.done = false;
                                it->second.enabled = false;
                            }
                        } else if (!cell.address.empty() && cell.address[0] == 'C') {
                            auto it = m_counterStates.find(cell.address);
                            if (it != m_counterStates.end()) {
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
    if (address.empty()) return false;

    switch (address[0]) {
        case 'T': {
            auto it = m_timerStates.find(address);
            return (it != m_timerStates.end()) ? it->second.done : false;
        }
        case 'C': {
            auto it = m_counterStates.find(address);
            return (it != m_counterStates.end()) ? it->second.done : false;
        }
        case 'X':
        case 'Y':
        case 'M':
        default: {
            auto it = m_deviceStates.find(address);
            return (it != m_deviceStates.end()) ? it->second : false;
        }
    }
}

void ProgrammingMode::SetDeviceState(const std::string& address, bool state) {
    if (!address.empty()) {
        m_deviceStates[address] = state;
    }
}

bool ProgrammingMode::CompileLadderToOpenPLC() {
    try {
        // GXWorks2 정규화 적용본으로 LD 생성
        const LadderProgram& srcProg = m_gx2NormalizationEnabled ? NormalizeLadderGX2(m_ladderProgram)
                                                                 : m_ladderProgram;
        std::string ldCode = m_ldConverter->ConvertToLDString(srcProg);
        if (ldCode.empty()) {
            m_lastCompileError = "Ladder to LD conversion failed";
            std::cerr << "[COMPILE ERROR] Ladder to LD conversion failed" << std::endl;
            return false;
        }

        std::ofstream ldFile("temp_ladder_program.ld");
        if (!ldFile) {
            m_lastCompileError = "Failed to create temporary LD file";
            std::cerr << "[COMPILE ERROR] Failed to create temporary LD file" << std::endl;
            return false;
        }
        ldFile << ldCode;
        ldFile.close();
        
        OpenPLCCompilerIntegration compiler;
        auto result = compiler.CompileLDFile("temp_ladder_program.ld");
        
        if (!result.success) {
            m_lastCompileError = result.errorMessage;
            std::cerr << "[COMPILE ERROR] OpenPLC compilation failed: " << result.errorMessage << std::endl;
            return false;
        }
        
        if (!m_plcExecutor->LoadCompiledCode(result.generatedCode)) {
            m_lastCompileError = "Failed to load compiled code into OpenPLC engine";
            std::cerr << "[COMPILE ERROR] Failed to load compiled code into OpenPLC engine" << std::endl;
            return false;
        }
        
        // 상태 갱신: 컴파일된 코드/해시/더티 플래그
        m_currentCompiledCode = result.generatedCode;
        m_lastCompiledHash = ComputeProgramHash(m_ladderProgram);
        m_isDirty = false;
        m_lastCompileError.clear();

        std::cout << "[COMPILE] OpenPLC engine loaded successfully (" << result.generatedCode.length() << " chars)" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        m_lastCompileError = e.what();
        std::cerr << "[COMPILE] Exception occurred: " << e.what() << std::endl;
        return false;
    }
}

void ProgrammingMode::SyncPhysicsToOpenPLC() {
    for (const auto& pair : m_deviceStates) {
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
                m_plcExecutor->SetDeviceState(address, state);
            }
        }
    }
}

void ProgrammingMode::SyncOpenPLCToDevices() {
    // Sync Y0-Y15
    for (int i = 0; i < 16; i++) {
        std::string yAddr = "Y" + std::to_string(i);
        bool yState = m_plcExecutor->GetDeviceState(yAddr);
        auto itY = m_deviceStates.find(yAddr);
        if (itY == m_deviceStates.end() || itY->second != yState) {
            m_deviceStates[yAddr] = yState;
        }
    }

    // Sync M0..M999
    for (int i = 0; i < 1000; i++) {
        std::string mAddr = "M" + std::to_string(i);
        bool mState = m_plcExecutor->GetDeviceState(mAddr);
        auto itM = m_deviceStates.find(mAddr);
        if (itM == m_deviceStates.end() || itM->second != mState) {
            m_deviceStates[mAddr] = mState;
        }
    }
}

void ProgrammingMode::UpdateVisualActiveStates() {
    for (auto& rung : m_ladderProgram.rungs) {
        for (auto& cell : rung.cells) {
            if (cell.type == LadderInstructionType::EMPTY) {
                cell.isActive = false;
            } else {
                switch (cell.type) {
                    case LadderInstructionType::XIC:
                        cell.isActive = m_plcExecutor->GetDeviceState(cell.address);
                        break;
                    case LadderInstructionType::XIO:
                        cell.isActive = !m_plcExecutor->GetDeviceState(cell.address);
                        break;
                    case LadderInstructionType::OTE:
                    case LadderInstructionType::SET:
                    case LadderInstructionType::RST:
                        cell.isActive = m_plcExecutor->GetDeviceState(cell.address);
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
    if (m_isDirty || m_currentCompiledCode.empty()) return true;
    size_t currentHash = ComputeProgramHash(m_ladderProgram);
    return currentHash != m_lastCompiledHash;
}

}