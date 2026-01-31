// programming_mode_edit.cpp
//
// Ladder editing functions.

#include "plc_emulator/programming/programming_mode.h"

#include <algorithm>
#include <map>
#include <set>
#include <cctype>
#include <vector>  // [NEW] std::vector ?ъ슜???꾪빐 異붽?

namespace plc {

namespace {
bool TryParseDevicePreset(const char* text,
                          char devicePrefix,
                          int* deviceNum,
                          int* presetNum) {
  if (!text || !deviceNum || !presetNum) {
    return false;
  }

  std::string input(text);
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] != devicePrefix) {
      continue;
    }

    size_t j = i + 1;
    while (j < input.size() &&
           !std::isdigit(static_cast<unsigned char>(input[j]))) {
      ++j;
    }

    if (j >= input.size()) {
      continue;
    }

    size_t deviceStart = j;
    while (j < input.size() &&
           std::isdigit(static_cast<unsigned char>(input[j]))) {
      ++j;
    }

    std::string deviceDigits = input.substr(deviceStart, j - deviceStart);
    if (deviceDigits.empty()) {
      continue;
    }

    size_t kPos = input.find_first_of("Kk", j);
    if (kPos == std::string::npos) {
      continue;
    }

    size_t p = kPos + 1;
    while (p < input.size() &&
           !std::isdigit(static_cast<unsigned char>(input[p]))) {
      ++p;
    }

    if (p >= input.size()) {
      continue;
    }

    size_t presetStart = p;
    while (p < input.size() &&
           std::isdigit(static_cast<unsigned char>(input[p]))) {
      ++p;
    }

    std::string presetDigits = input.substr(presetStart, p - presetStart);
    if (presetDigits.empty()) {
      continue;
    }

    try {
      *deviceNum = std::stoi(deviceDigits);
      *presetNum = std::stoi(presetDigits);
      return true;
    } catch (...) {
      return false;
    }
  }

  return false;
}
}  // namespace

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
  ImGuiIO& io = ImGui::GetIO();
  bool shiftPressed = io.KeyShift;
  bool ctrlPressed = io.KeyCtrl;

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
        if (selected_cell_ == 11) {
          return;
        }
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

  int prev_rung = selected_rung_;
  int prev_cell = selected_cell_;

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

  if (ctrlPressed && !is_monitor_mode_ &&
      (prev_rung != selected_rung_ || prev_cell != selected_cell_)) {
    if (prev_rung == selected_rung_) {
      int step = (selected_cell_ > prev_cell) ? 1 : -1;
      for (int c = prev_cell; c != selected_cell_; c += step) {
        int next = c + step;
        if (next < 0 || next >= 12)
          continue;
        if (prev_rung >= static_cast<int>(ladder_program_.rungs.size()) - 1)
          continue;

        if (next != 11) {
          auto& dest = ladder_program_.rungs[prev_rung].cells[next];
          if (dest.type == LadderInstructionType::HLINE) {
            dest = LadderInstruction();
            MarkDirty();
            continue;
          }
        }

        if (c != 11) {
          auto& current = ladder_program_.rungs[prev_rung].cells[c];
          if (current.type == LadderInstructionType::EMPTY) {
            current.type = LadderInstructionType::HLINE;
            MarkDirty();
          }
        }
      }
    } else if (prev_cell == selected_cell_) {
      if (selected_cell_ == 11) {
        return;
      }
      int maxRung = static_cast<int>(ladder_program_.rungs.size()) - 2;
      auto segment_key = [](int a, int b) -> long long {
        int lo = std::min(a, b);
        int hi = std::max(a, b);
        return (static_cast<long long>(lo) << 32) | static_cast<unsigned int>(hi);
      };
      std::set<long long> segments;
      for (const auto& conn : ladder_program_.verticalConnections) {
        if (conn.x != selected_cell_)
          continue;
        std::vector<int> rungs = conn.rungs;
        std::sort(rungs.begin(), rungs.end());
        for (size_t i = 0; i + 1 < rungs.size(); ++i) {
          int a = rungs[i];
          int b = rungs[i + 1];
          if (b == a + 1) {
            segments.insert(segment_key(a, b));
          }
        }
      }

      int step = (selected_rung_ > prev_rung) ? 1 : -1;
      for (int r = prev_rung; r != selected_rung_; r += step) {
        int next = r + step;
        if (r < 0 || next < 0 || r > maxRung || next > maxRung)
          continue;
        long long segmentKey = segment_key(r, next);
        if (segments.count(segmentKey)) {
          segments.erase(segmentKey);
        } else {
          segments.insert(segmentKey);
        }
        MarkDirty();
      }

      std::map<int, std::set<int>> adjacency;
      for (long long segmentKey : segments) {
        int a = static_cast<int>(segmentKey >> 32);
        int b = static_cast<int>(segmentKey & 0xffffffff);
        adjacency[a].insert(b);
        adjacency[b].insert(a);
      }

      std::set<int> visited;
      std::vector<VerticalConnection> rebuilt;
      for (const auto& conn : ladder_program_.verticalConnections) {
        if (conn.x != selected_cell_) {
          rebuilt.push_back(conn);
        }
      }

      for (const auto& entry : adjacency) {
        int node = entry.first;
        if (visited.count(node))
          continue;
        std::vector<int> stack;
        std::vector<int> component;
        stack.push_back(node);
        visited.insert(node);
        while (!stack.empty()) {
          int current = stack.back();
          stack.pop_back();
          component.push_back(current);
          for (int neighbor : adjacency[current]) {
            if (visited.insert(neighbor).second) {
              stack.push_back(neighbor);
            }
          }
        }
        std::sort(component.begin(), component.end());
        if (component.size() >= 2) {
          VerticalConnection vc;
          vc.x = selected_cell_;
          vc.rungs = component;
          rebuilt.push_back(vc);
        }
      }

      ladder_program_.verticalConnections = rebuilt;
    }
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
    if (TryParseDevicePreset(temp_address_buffer_, 'T', &deviceNum,
                             &presetNum)) {
      newInstruction.type = LadderInstructionType::TON;
      newInstruction.address = "T" + std::to_string(deviceNum);
      newInstruction.preset = "K" + std::to_string(presetNum);
      timer_states_[newInstruction.address].preset = presetNum;
    } else if (TryParseDevicePreset(temp_address_buffer_, 'C', &deviceNum,
                                    &presetNum)) {
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
  if (ladder_program_.rungs.empty()) {
    ladder_program_.rungs.push_back(Rung());
    selected_rung_ = 0;
    selected_cell_ = 0;
    MarkDirty();
    return;
  }

  int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  int insertIndex = endIndex;
  if (selected_rung_ < endIndex) {
    insertIndex = selected_rung_ + 1;
  }

  std::vector<bool> extendConnections;
  extendConnections.reserve(ladder_program_.verticalConnections.size());
  for (const auto& conn : ladder_program_.verticalConnections) {
    if (conn.rungs.empty()) {
      extendConnections.push_back(false);
      continue;
    }
    std::vector<int> sorted = conn.rungs;
    std::sort(sorted.begin(), sorted.end());
    int minRung = sorted.front();
    int maxRung = sorted.back();
    bool shouldExtend = (minRung < insertIndex && maxRung >= insertIndex);
    extendConnections.push_back(shouldExtend);
  }

  ladder_program_.rungs.insert(ladder_program_.rungs.begin() + insertIndex,
                               Rung());

  for (size_t idx = 0; idx < ladder_program_.verticalConnections.size(); ++idx) {
    auto& conn = ladder_program_.verticalConnections[idx];
    for (int& r_idx : conn.rungs) {
      if (r_idx >= insertIndex) {
        r_idx += 1;
      }
    }
    if (idx < extendConnections.size() && extendConnections[idx]) {
      conn.rungs.push_back(insertIndex);
    }
    if (!conn.rungs.empty()) {
      std::sort(conn.rungs.begin(), conn.rungs.end());
      conn.rungs.erase(std::unique(conn.rungs.begin(), conn.rungs.end()),
                       conn.rungs.end());
    }
  }

  selected_rung_ = insertIndex;
  selected_cell_ = 0;
  for (size_t i = 0; i < ladder_program_.rungs.size() - 1; ++i) {
    ladder_program_.rungs[i].number = i;
  }
  MarkDirty();
}

// [MODIFIED] 猷???젣 ???곌껐???몃줈?좎쓣 ?④퍡 ?쒓굅?섍퀬, ?섎㉧吏 ?몃줈?좎쓽 ?몃뜳?ㅻ?
// ?낅뜲?댄듃?섎뒗 濡쒖쭅 異붽?
void ProgrammingMode::DeleteRung(int rungIndexToDelete) {
  // END 猷쎌씠??議댁옱?섏? ?딅뒗 猷쎌? ??젣?????놁쓬
  if (rungIndexToDelete < 0 ||
      rungIndexToDelete >= static_cast<int>(ladder_program_.rungs.size()) - 1) {
    return;
  }

  // 1. ?몃줈???곌껐 ?뺣낫 ?낅뜲?댄듃
  std::vector<VerticalConnection> updatedConnections;
  for (const auto& conn : ladder_program_.verticalConnections) {
    VerticalConnection newConn;
    newConn.x = conn.x;
    for (int r_idx : conn.rungs) {
      if (r_idx == rungIndexToDelete) {
        continue;
      }
      if (r_idx > rungIndexToDelete) {
        newConn.rungs.push_back(r_idx - 1);  // ?몃뜳??1 媛먯냼
      } else {
        newConn.rungs.push_back(r_idx);
      }
    }
    if (!newConn.rungs.empty()) {
      std::sort(newConn.rungs.begin(), newConn.rungs.end());
      newConn.rungs.erase(std::unique(newConn.rungs.begin(),
                                      newConn.rungs.end()),
                          newConn.rungs.end());
    }
    if (newConn.rungs.size() >= 2) {
      updatedConnections.push_back(newConn);
    }
  }
  // ?낅뜲?댄듃???곌껐 紐⑸줉?쇰줈 援먯껜
  ladder_program_.verticalConnections = updatedConnections;

  // 2. 猷???젣
  ladder_program_.rungs.erase(ladder_program_.rungs.begin() +
                              rungIndexToDelete);

  // 而ㅼ꽌 ?꾩튂 議곗젙
  if (selected_rung_ >= rungIndexToDelete && selected_rung_ > 0) {
    selected_rung_--;
  }

  // 3. ?섎㉧吏 猷?踰덊샇 ?ъ젙??
  if (!ladder_program_.rungs.empty()) {
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
