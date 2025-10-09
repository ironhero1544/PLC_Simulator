#include "ProgrammingMode.h"
#include <algorithm>
#include <cctype>
#include <vector> // [NEW] std::vector 사용을 위해 추가

namespace plc {

void to_upper(char* str) {
    while (*str) {
        *str = std::toupper(static_cast<unsigned char>(*str));
        str++;
    }
}

void ProgrammingMode::HandleKeyboardInput(int key) {
    if (m_showAddressPopup || m_showVerticalDialog) return;

    bool onEndRung = (m_selectedRung == static_cast<int>(m_ladderProgram.rungs.size()) - 1);
    bool shiftPressed = ImGui::GetIO().KeyShift;

    if (!m_isMonitorMode) {
        switch (key) {
            case ImGuiKey_F5:
            case ImGuiKey_F6:
            case ImGuiKey_F7:
                if (onEndRung) {
                    m_pendingAction.type = PendingActionType::ADD_NEW_RUNG;
                } else {
                    m_pendingAction.type = PendingActionType::ADD_INSTRUCTION;
                    m_pendingAction.instructionType = (key == ImGuiKey_F5) ? LadderInstructionType::XIC :
                                                      (key == ImGuiKey_F6) ? LadderInstructionType::XIO :
                                                      LadderInstructionType::OTE;
                }
                return;
            case ImGuiKey_F9:
                if (shiftPressed) {
                    if (!onEndRung && m_selectedCell > 0) {
                        AddVerticalConnection();
                    }
                } else {
                    if (onEndRung) {
                        m_pendingAction.type = PendingActionType::ADD_NEW_RUNG;
                    } else {
                        m_pendingAction.type = PendingActionType::ADD_INSTRUCTION;
                        m_pendingAction.instructionType = LadderInstructionType::HLINE;
                    }
                }
                return;
            case ImGuiKey_Delete:
                if (!onEndRung) m_pendingAction.type = PendingActionType::DELETE_INSTRUCTION;
                return;
            case ImGuiKey_Insert:
                m_pendingAction.type = PendingActionType::ADD_NEW_RUNG;
                return;
            case ImGuiKey_Enter:
                if (!onEndRung) {
                    EditCurrentInstruction();
                }
                return;
        }
    }

    switch (key) {
        case ImGuiKey_LeftArrow: if (m_selectedCell > 0) m_selectedCell--; break;
        case ImGuiKey_RightArrow: if (m_selectedCell < 11) m_selectedCell++; break;
        case ImGuiKey_UpArrow: if (m_selectedRung > 0) m_selectedRung--; break;
        case ImGuiKey_DownArrow: if (m_selectedRung < static_cast<int>(m_ladderProgram.rungs.size()) - 1) m_selectedRung++; break;
        case ImGuiKey_F2: m_isMonitorMode = true; break;
        case ImGuiKey_F3: m_isMonitorMode = false; break;
    }
}

void ProgrammingMode::HandleEditAction(LadderInstructionType type) {
    if (type == LadderInstructionType::HLINE) {
        InsertHorizontalLine();
    } else {
        m_pendingInstructionType = type;
        m_tempAddressBuffer[0] = '\0';
        m_showAddressPopup = true;
    }
}

void ProgrammingMode::ConfirmInstruction() {
    if (m_selectedRung >= static_cast<int>(m_ladderProgram.rungs.size())) return;

    auto& rung = m_ladderProgram.rungs[m_selectedRung];

    to_upper(m_tempAddressBuffer);

    LadderInstruction newInstruction;
    newInstruction.type = m_pendingInstructionType;
    newInstruction.address = m_tempAddressBuffer;

    int deviceNum = 0, presetNum = 0;
    char deviceTypeBuffer[64] = {0};

    if (m_pendingInstructionType == LadderInstructionType::OTE) {
        if (sscanf(m_tempAddressBuffer, "T%d K%d", &deviceNum, &presetNum) == 2) {
            newInstruction.type = LadderInstructionType::TON;
            newInstruction.address = "T" + std::to_string(deviceNum);
            newInstruction.preset = "K" + std::to_string(presetNum);
            m_timerStates[newInstruction.address].preset = presetNum;
        } else if (sscanf(m_tempAddressBuffer, "C%d K%d", &deviceNum, &presetNum) == 2) {
            newInstruction.type = LadderInstructionType::CTU;
            newInstruction.address = "C" + std::to_string(deviceNum);
            newInstruction.preset = "K" + std::to_string(presetNum);
            m_counterStates[newInstruction.address].preset = presetNum;
        } else if (sscanf(m_tempAddressBuffer, "SET %s", deviceTypeBuffer) == 1) {
            newInstruction.type = LadderInstructionType::SET;
            newInstruction.address = deviceTypeBuffer;
        } else if (sscanf(m_tempAddressBuffer, "RST %s", deviceTypeBuffer) == 1) {
            if (deviceTypeBuffer[0] == 'T' || deviceTypeBuffer[0] == 'C') {
                 newInstruction.type = LadderInstructionType::RST_TMR_CTR;
            } else {
                 newInstruction.type = LadderInstructionType::RST;
            }
            newInstruction.address = deviceTypeBuffer;
        }
    }

    bool isOutputInstruction = (
        newInstruction.type == LadderInstructionType::OTE ||
        newInstruction.type == LadderInstructionType::SET ||
        newInstruction.type == LadderInstructionType::RST ||
        newInstruction.type == LadderInstructionType::TON ||
        newInstruction.type == LadderInstructionType::CTU ||
        newInstruction.type == LadderInstructionType::RST_TMR_CTR);

    int targetCell = isOutputInstruction ? 11 : m_selectedCell;
    if (targetCell < 0 || targetCell >= 12) return;

    rung.cells[targetCell] = newInstruction;
    UpdateHorizontalLines(m_selectedRung);
}

void ProgrammingMode::DeleteCurrentInstruction() {
    if (m_selectedRung >= static_cast<int>(m_ladderProgram.rungs.size()) || m_selectedCell < 0 || m_selectedCell >= 12) return;

    m_ladderProgram.rungs[m_selectedRung].cells[m_selectedCell] = LadderInstruction();
    UpdateHorizontalLines(m_selectedRung);
}

void ProgrammingMode::EditCurrentInstruction() {
    if (m_selectedRung >= static_cast<int>(m_ladderProgram.rungs.size()) || m_selectedCell < 0 || m_selectedCell >= 12) return;

    const auto& instruction = m_ladderProgram.rungs[m_selectedRung].cells[m_selectedCell];

    if (instruction.type == LadderInstructionType::EMPTY || instruction.type == LadderInstructionType::HLINE) {
        return;
    }

    std::string full_command = instruction.address;
    if (!instruction.preset.empty()) {
        full_command += " " + instruction.preset;
    }
    strncpy(m_tempAddressBuffer, full_command.c_str(), sizeof(m_tempAddressBuffer) - 1);
    m_tempAddressBuffer[sizeof(m_tempAddressBuffer) - 1] = '\0';

    m_pendingInstructionType = instruction.type;
    if(m_pendingInstructionType != LadderInstructionType::XIC && m_pendingInstructionType != LadderInstructionType::XIO) {
        m_pendingInstructionType = LadderInstructionType::OTE;
    }

    m_showAddressPopup = true;
}

void ProgrammingMode::AddNewRung() {
    int pos = m_ladderProgram.rungs.size() - 1;
    m_ladderProgram.rungs.insert(m_ladderProgram.rungs.begin() + pos, Rung());
    m_selectedRung = pos;
    m_selectedCell = 0;
    for (size_t i = 0; i < m_ladderProgram.rungs.size() - 1; ++i) {
        m_ladderProgram.rungs[i].number = i;
    }
}

// [MODIFIED] 룽 삭제 시 연결된 세로선을 함께 제거하고, 나머지 세로선의 인덱스를 업데이트하는 로직 추가
void ProgrammingMode::DeleteRung(int rungIndexToDelete) {
    // END 룽이나 존재하지 않는 룽은 삭제할 수 없음
    if (rungIndexToDelete < 0 || rungIndexToDelete >= static_cast<int>(m_ladderProgram.rungs.size()) - 1) {
        return;
    }

    // 1. 세로선 연결 정보 업데이트
    std::vector<VerticalConnection> updatedConnections;
    for (const auto& conn : m_ladderProgram.verticalConnections) {
        // 현재 연결이 삭제될 룽을 포함하는지 확인
        auto it = std::find(conn.rungs.begin(), conn.rungs.end(), rungIndexToDelete);

        if (it == conn.rungs.end()) {
            // 삭제될 룽을 포함하지 않으면, 이 연결은 유지. 단, 인덱스 조정이 필요.
            VerticalConnection newConn;
            newConn.x = conn.x;
            for (int r_idx : conn.rungs) {
                if (r_idx > rungIndexToDelete) {
                    newConn.rungs.push_back(r_idx - 1); // 인덱스 1 감소
                } else {
                    newConn.rungs.push_back(r_idx);
                }
            }
            // 업데이트 후에도 여전히 2개 이상의 룽을 연결하는 경우에만 추가
            if (newConn.rungs.size() >= 2) {
                updatedConnections.push_back(newConn);
            }
        }
        // else: 삭제될 룽을 포함하는 연결은 updatedConnections에 추가하지 않아 자동으로 삭제됨
    }
    // 업데이트된 연결 목록으로 교체
    m_ladderProgram.verticalConnections = updatedConnections;


    // 2. 룽 삭제
    if (m_ladderProgram.rungs.size() > 2) { // 최소 1개의 룽과 END 룽은 유지
        m_ladderProgram.rungs.erase(m_ladderProgram.rungs.begin() + rungIndexToDelete);

        // 커서 위치 조정
        if (m_selectedRung >= rungIndexToDelete && m_selectedRung > 0) {
            m_selectedRung--;
        }

        // 3. 나머지 룽 번호 재정렬
        for (size_t i = 0; i < m_ladderProgram.rungs.size() - 1; ++i) {
            m_ladderProgram.rungs[i].number = i;
        }
    }
}

void ProgrammingMode::InsertHorizontalLine() {
    m_ladderProgram.rungs[m_selectedRung].cells[m_selectedCell].type = LadderInstructionType::HLINE;
}

void ProgrammingMode::UpdateHorizontalLines(int rungIndex) {
    if (rungIndex < 0 || rungIndex >= static_cast<int>(m_ladderProgram.rungs.size())) return;

    auto& cells = m_ladderProgram.rungs[rungIndex].cells;
    int coil_pos = -1;
    int first_instruction = -1;

    for (int i = 11; i >= 0; --i) {
        if (cells[i].type != LadderInstructionType::EMPTY && cells[i].type != LadderInstructionType::HLINE) {
             bool isOutput = (cells[i].type == LadderInstructionType::OTE ||
                              cells[i].type == LadderInstructionType::SET ||
                              cells[i].type == LadderInstructionType::RST ||
                              cells[i].type == LadderInstructionType::TON ||
                              cells[i].type == LadderInstructionType::CTU ||
                              cells[i].type == LadderInstructionType::RST_TMR_CTR);
            if (isOutput) {
                 coil_pos = i;
                 break;
            }
        }
    }

    for (int i = 0; i < 12; ++i) {
        if (cells[i].type != LadderInstructionType::EMPTY && cells[i].type != LadderInstructionType::HLINE) {
            first_instruction = i;
            break;
        }
    }

    for (int i = 0; i < 12; ++i) {
        if (cells[i].type == LadderInstructionType::HLINE) {
            cells[i].type = LadderInstructionType::EMPTY;
        }
    }

    if (coil_pos != -1 && first_instruction != -1) {
        for (int i = first_instruction + 1; i < coil_pos; ++i) {
            if (cells[i].type == LadderInstructionType::EMPTY) {
                cells[i].type = LadderInstructionType::HLINE;
            }
        }
    }
}

void ProgrammingMode::AddVerticalConnection() {
    if (m_selectedCell == 0) return;
    if (m_selectedRung >= static_cast<int>(m_ladderProgram.rungs.size()) - 1) return;

    m_verticalLineCount = 1;
    m_showVerticalDialog = true;
}

void ProgrammingMode::ConfirmVerticalConnection() {
    if (m_verticalLineCount < 1) return;

    int startRung = m_selectedRung;
    int endRung = startRung + m_verticalLineCount;
    int maxRung = m_ladderProgram.rungs.size() - 2;

    if (endRung > maxRung) return;

    VerticalConnection newConnection(m_selectedCell, startRung, endRung);
    m_ladderProgram.verticalConnections.push_back(newConnection);

    m_showVerticalDialog = false;
}

} // namespace plc