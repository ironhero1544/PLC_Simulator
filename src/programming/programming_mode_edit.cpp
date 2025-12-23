// programming_mode_edit.cpp
// Copyright 2024 PLC Emulator Project
//
// Ladder editing functions.

#include "plc_emulator/programming/programming_mode.h"

#include <algorithm>
#include <cctype>
#include <vector>  // [NEW] std::vector 사용을 위해 추가

namespace plc {

void to_upper(char* str) {
  while (*str) {
    *str = static_cast<char>(std::toupper(static_cast<unsigned char>(*str)));
    str++;
  }
}

void ProgrammingMode::HandleKeyboardInput(int key) {
  if (show_address_popup_ || show_vertical_dialog_)
    return;

  bool onEndRung =
      (selected_rung_ == static_cast<int>(ladder_program_.rungs.size()) - 1);
  bool shiftPressed = ImGui::GetIO().KeyShift;

  if (!is_monitor_mode_) {
    switch (key) {
      case ImGuiKey_F5:
      case ImGuiKey_F6:
      case ImGuiKey_F7:
        if (onEndRung) {
          pending_action_.type = PendingActionType::ADD_NEW_RUNG;
        } else {
          pending_action_.type = PendingActionType::ADD_INSTRUCTION;
          pending_action_.instructionType =
              (key == ImGuiKey_F5)   ? LadderInstructionType::XIC
              : (key == ImGuiKey_F6) ? LadderInstructionType::XIO
                                     : LadderInstructionType::OTE;
        }
        return;
      case ImGuiKey_F9:
        if (shiftPressed) {
          if (!onEndRung && selected_cell_ > 0) {
            AddVerticalConnection();
          }
        } else {
          if (onEndRung) {
            pending_action_.type = PendingActionType::ADD_NEW_RUNG;
          } else {
            pending_action_.type = PendingActionType::ADD_INSTRUCTION;
            pending_action_.instructionType = LadderInstructionType::HLINE;
          }
        }
        return;
      case ImGuiKey_Delete:
        if (!onEndRung)
          pending_action_.type = PendingActionType::DELETE_INSTRUCTION;
        return;
      case ImGuiKey_Insert:
        pending_action_.type = PendingActionType::ADD_NEW_RUNG;
        return;
      case ImGuiKey_Enter:
        if (!onEndRung) {
          EditCurrentInstruction();
        }
        return;
    }
  }

  switch (key) {
    case ImGuiKey_LeftArrow:
      if (selected_cell_ > 0)
        selected_cell_--;
      break;
    case ImGuiKey_RightArrow:
      if (selected_cell_ < 11)
        selected_cell_++;
      break;
    case ImGuiKey_UpArrow:
      if (selected_rung_ > 0)
        selected_rung_--;
      break;
    case ImGuiKey_DownArrow:
      if (selected_rung_ < static_cast<int>(ladder_program_.rungs.size()) - 1)
        selected_rung_++;
      break;
    case ImGuiKey_F2:
      is_monitor_mode_ = true;
      break;
    case ImGuiKey_F3:
      is_monitor_mode_ = false;
      break;
  }
}

void ProgrammingMode::HandleEditAction(LadderInstructionType type) {
  if (type == LadderInstructionType::HLINE) {
    InsertHorizontalLine();
  } else {
    pending_instruction_type_ = type;
    temp_address_buffer_[0] = '\0';
    show_address_popup_ = true;
  }
}

void ProgrammingMode::ConfirmInstruction() {
  if (selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()))
    return;

  auto& rung = ladder_program_.rungs[selected_rung_];

  to_upper(temp_address_buffer_);

  LadderInstruction newInstruction;
  newInstruction.type = pending_instruction_type_;
  newInstruction.address = temp_address_buffer_;

  int deviceNum = 0, presetNum = 0;
  char deviceTypeBuffer[64] = {0};

  if (pending_instruction_type_ == LadderInstructionType::OTE) {
    if (sscanf(temp_address_buffer_, "T%d K%d", &deviceNum, &presetNum) == 2) {
      newInstruction.type = LadderInstructionType::TON;
      newInstruction.address = "T" + std::to_string(deviceNum);
      newInstruction.preset = "K" + std::to_string(presetNum);
      timer_states_[newInstruction.address].preset = presetNum;
    } else if (sscanf(temp_address_buffer_, "C%d K%d", &deviceNum, &presetNum) ==
               2) {
      newInstruction.type = LadderInstructionType::CTU;
      newInstruction.address = "C" + std::to_string(deviceNum);
      newInstruction.preset = "K" + std::to_string(presetNum);
      counter_states_[newInstruction.address].preset = presetNum;
    } else if (sscanf(temp_address_buffer_, "SET %s", deviceTypeBuffer) == 1) {
      newInstruction.type = LadderInstructionType::SET;
      newInstruction.address = deviceTypeBuffer;
    } else if (sscanf(temp_address_buffer_, "RST %s", deviceTypeBuffer) == 1) {
      if (deviceTypeBuffer[0] == 'T' || deviceTypeBuffer[0] == 'C') {
        newInstruction.type = LadderInstructionType::RST_TMR_CTR;
      } else {
        newInstruction.type = LadderInstructionType::RST;
      }
      newInstruction.address = deviceTypeBuffer;
    }
  }

  bool isOutputInstruction =
      (newInstruction.type == LadderInstructionType::OTE ||
       newInstruction.type == LadderInstructionType::SET ||
       newInstruction.type == LadderInstructionType::RST ||
       newInstruction.type == LadderInstructionType::TON ||
       newInstruction.type == LadderInstructionType::CTU ||
       newInstruction.type == LadderInstructionType::RST_TMR_CTR);

  int targetCell = isOutputInstruction ? 11 : selected_cell_;
  if (targetCell < 0 || targetCell >= 12)
    return;

  rung.cells[targetCell] = newInstruction;
  UpdateHorizontalLines(selected_rung_);
  MarkDirty();
}

void ProgrammingMode::DeleteCurrentInstruction() {
  if (selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()) ||
      selected_cell_ < 0 || selected_cell_ >= 12)
    return;

  ladder_program_.rungs[selected_rung_].cells[selected_cell_] =
      LadderInstruction();
  UpdateHorizontalLines(selected_rung_);
  MarkDirty();
}

void ProgrammingMode::EditCurrentInstruction() {
  if (selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()) ||
      selected_cell_ < 0 || selected_cell_ >= 12)
    return;

  const auto& instruction =
      ladder_program_.rungs[selected_rung_].cells[selected_cell_];

  if (instruction.type == LadderInstructionType::EMPTY ||
      instruction.type == LadderInstructionType::HLINE) {
    return;
  }

  std::string full_command = instruction.address;
  if (!instruction.preset.empty()) {
    full_command += " " + instruction.preset;
  }
  strncpy(temp_address_buffer_, full_command.c_str(),
          sizeof(temp_address_buffer_) - 1);
  temp_address_buffer_[sizeof(temp_address_buffer_) - 1] = '\0';

  pending_instruction_type_ = instruction.type;
  if (pending_instruction_type_ != LadderInstructionType::XIC &&
      pending_instruction_type_ != LadderInstructionType::XIO) {
    pending_instruction_type_ = LadderInstructionType::OTE;
  }

  show_address_popup_ = true;
}

void ProgrammingMode::AddNewRung() {
  int pos = ladder_program_.rungs.size() - 1;
  ladder_program_.rungs.insert(ladder_program_.rungs.begin() + pos, Rung());
  selected_rung_ = pos;
  selected_cell_ = 0;
  for (size_t i = 0; i < ladder_program_.rungs.size() - 1; ++i) {
    ladder_program_.rungs[i].number = i;
  }
  MarkDirty();
}

// [MODIFIED] 룽 삭제 시 연결된 세로선을 함께 제거하고, 나머지 세로선의 인덱스를
// 업데이트하는 로직 추가
void ProgrammingMode::DeleteRung(int rungIndexToDelete) {
  // END 룽이나 존재하지 않는 룽은 삭제할 수 없음
  if (rungIndexToDelete < 0 ||
      rungIndexToDelete >= static_cast<int>(ladder_program_.rungs.size()) - 1) {
    return;
  }

  // 1. 세로선 연결 정보 업데이트
  std::vector<VerticalConnection> updatedConnections;
  for (const auto& conn : ladder_program_.verticalConnections) {
    // 현재 연결이 삭제될 룽을 포함하는지 확인
    auto it =
        std::find(conn.rungs.begin(), conn.rungs.end(), rungIndexToDelete);

    if (it == conn.rungs.end()) {
      // 삭제될 룽을 포함하지 않으면, 이 연결은 유지. 단, 인덱스 조정이 필요.
      VerticalConnection newConn;
      newConn.x = conn.x;
      for (int r_idx : conn.rungs) {
        if (r_idx > rungIndexToDelete) {
          newConn.rungs.push_back(r_idx - 1);  // 인덱스 1 감소
        } else {
          newConn.rungs.push_back(r_idx);
        }
      }
      // 업데이트 후에도 여전히 2개 이상의 룽을 연결하는 경우에만 추가
      if (newConn.rungs.size() >= 2) {
        updatedConnections.push_back(newConn);
      }
    }
    // else: 삭제될 룽을 포함하는 연결은 updatedConnections에 추가하지 않아
    // 자동으로 삭제됨
  }
  // 업데이트된 연결 목록으로 교체
  ladder_program_.verticalConnections = updatedConnections;

  // 2. 룽 삭제
  if (ladder_program_.rungs.size() > 2) {  // 최소 1개의 룽과 END 룽은 유지
    ladder_program_.rungs.erase(ladder_program_.rungs.begin() +
                                rungIndexToDelete);

    // 커서 위치 조정
    if (selected_rung_ >= rungIndexToDelete && selected_rung_ > 0) {
      selected_rung_--;
    }

    // 3. 나머지 룽 번호 재정렬
    for (size_t i = 0; i < ladder_program_.rungs.size() - 1; ++i) {
      ladder_program_.rungs[i].number = i;
    }
  }
  MarkDirty();
}

void ProgrammingMode::InsertHorizontalLine() {
  ladder_program_.rungs[selected_rung_].cells[selected_cell_].type =
      LadderInstructionType::HLINE;
  MarkDirty();
}

void ProgrammingMode::UpdateHorizontalLines(int rungIndex) {
  if (rungIndex < 0 ||
      rungIndex >= static_cast<int>(ladder_program_.rungs.size()))
    return;

  auto& cells = ladder_program_.rungs[rungIndex].cells;
  int coil_pos = -1;
  int first_instruction = -1;

  for (int i = 11; i >= 0; --i) {
    if (cells[i].type != LadderInstructionType::EMPTY &&
        cells[i].type != LadderInstructionType::HLINE) {
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
    if (cells[i].type != LadderInstructionType::EMPTY &&
        cells[i].type != LadderInstructionType::HLINE) {
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
  if (selected_cell_ == 0)
    return;
  if (selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()) - 1)
    return;

  vertical_line_count_ = 1;
  show_vertical_dialog_ = true;
}

void ProgrammingMode::ConfirmVerticalConnection() {
  if (vertical_line_count_ < 1)
    return;

  int startRung = selected_rung_;
  int endRung = startRung + vertical_line_count_;
  int maxRung = ladder_program_.rungs.size() - 2;

  if (endRung > maxRung)
    return;

  VerticalConnection newConnection(selected_cell_, startRung, endRung);
  ladder_program_.verticalConnections.push_back(newConnection);

  show_vertical_dialog_ = false;
  MarkDirty();
}

}  // namespace plc
