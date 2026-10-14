#include "plc_emulator/project/instruction_codec.h"
// programming_mode_edit.cpp
//
// Ladder editing functions.

#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/programming/ladder_program_utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <cctype>
#include <vector>

namespace plc {

namespace {
LadderInstructionType InstructionShortcut(int key, bool shift) {
  if (key == ImGuiKey_F5) return LadderInstructionType::XIC;
  if (key == ImGuiKey_F6) return LadderInstructionType::XIO;
  if (key == ImGuiKey_F7) {
    return shift ? LadderInstructionType::RISING_CONTACT
                 : LadderInstructionType::OTE;
  }
  return shift ? LadderInstructionType::FALLING_CONTACT
               : LadderInstructionType::APPLICATION;
}

void AppendVerticalConnectionComponents(int x,
                                        const std::vector<int>& source_rungs,
                                        int rung_offset,
                                        int x_offset,
                                        std::vector<VerticalConnection>* out) {
  if (!out) {
    return;
  }

  std::vector<int> rungs = source_rungs;
  std::sort(rungs.begin(), rungs.end());
  rungs.erase(std::unique(rungs.begin(), rungs.end()), rungs.end());

  std::vector<int> component;
  auto flush_component = [&]() {
    if (component.size() < 2) {
      component.clear();
      return;
    }

    VerticalConnection connection;
    connection.x = x + x_offset;
    connection.rungs.reserve(component.size());
    for (int rung : component) {
      connection.rungs.push_back(rung + rung_offset);
    }
    out->push_back(connection);
    component.clear();
  };

  for (int rung : rungs) {
    if (!component.empty() && rung != component.back() + 1) {
      flush_component();
    }
    component.push_back(rung);
  }
  flush_component();
}

}  // namespace

void to_upper(char* str) {
  while (*str) {
    *str = static_cast<char>(std::toupper(static_cast<unsigned char>(*str)));
    str++;
  }
}

int ProgrammingMode::InstructionColumns(
    const LadderInstruction& instruction) const {
  using Type = LadderInstructionType;
  if (instruction.type == Type::EMPTY || instruction.type == Type::HLINE ||
      instruction.type == Type::kWrappingSource ||
      instruction.type == Type::kWrappingDestination) return 1;
  int minimum = 1;
  const bool contact = instruction.type == Type::XIC ||
      instruction.type == Type::XIO || instruction.type == Type::RISING_CONTACT ||
      instruction.type == Type::FALLING_CONTACT;
  if (instruction.type == Type::TON || instruction.type == Type::CTU)
    minimum = 2;
  if (instruction.type == Type::APPLICATION || instruction.type == Type::SET ||
      instruction.type == Type::RST || instruction.type == Type::COMPARISON ||
      instruction.type == Type::RST_TMR_CTR || instruction.type == Type::BKRST) {
    minimum = 2;
  }
  std::string label = instruction.address;
  if (minimum > 1 && !contact)
    label = plc_emulator::programming::FormatCellInput(instruction);
  const float font_size = ImGui::GetCurrentContext() && ImGui::GetFont()
      ? std::max(20.0f, ImGui::GetFontSize() * 1.35f) : 20.0f;
  float text_width = static_cast<float>(label.size()) * font_size * 0.6f;
  if (ImGui::GetCurrentContext() && ImGui::GetFont())
    text_width = ImGui::GetFont()->CalcTextSizeA(
        font_size,
        std::numeric_limits<float>::max(), 0, label.c_str()).x;
  const int measured = static_cast<int>(std::ceil(
      (text_width + (minimum > 1 ? font_size * 3.0f
                                 : 32.0f * GetLayoutScale())) /
      std::max(1.0f, ladder_column_width_)));
  return std::max(minimum, measured);
}

ProgrammingMode::InstructionSpan ProgrammingMode::GetInstructionSpan(
    int rung, int anchor) const {
  InstructionSpan span{anchor, anchor, anchor};
  if (rung < 0 || rung >= static_cast<int>(ladder_program_.rungs.size()))
    return span;
  const auto& cells = ladder_program_.rungs[rung].cells;
  if (anchor < 0 || anchor >= static_cast<int>(cells.size())) return span;
  const int columns = InstructionColumns(cells[anchor]);
  const bool output = IsLadderOutput(cells[anchor].type);
  const int direction = output ? -1 : 1;
  for (int offset = 1; offset < columns; ++offset) {
    const int next = anchor + offset * direction;
    if (next < 0 || next >= static_cast<int>(cells.size())) break;
    if (cells[next].type != LadderInstructionType::EMPTY &&
        cells[next].type != LadderInstructionType::HLINE) break;
    const int boundary = output ? next + 1 : next;
    bool blocked = false;
    for (const auto& connection : ladder_program_.verticalConnections) {
      if (connection.x == boundary &&
          std::find(connection.rungs.begin(), connection.rungs.end(), rung) !=
              connection.rungs.end()) blocked = true;
    }
    if (output) {
      for (int prior = 0; prior < next; ++prior) {
        if (cells[prior].type != LadderInstructionType::EMPTY &&
            cells[prior].type != LadderInstructionType::HLINE &&
            !IsLadderOutput(cells[prior].type) &&
            GetInstructionSpan(rung, prior).last >= next) blocked = true;
      }
    }
    if (blocked) break;
    span.first = std::min(span.first, next);
    span.last = std::max(span.last, next);
  }
  return span;
}

ProgrammingMode::InstructionSpan ProgrammingMode::GetCellSpan(
    int rung, int column) const {
  if (rung >= 0 && rung < static_cast<int>(ladder_program_.rungs.size()) &&
      !ladder_program_.rungs[rung].isEndRung) {
    const auto& cells = ladder_program_.rungs[rung].cells;
    for (int anchor = 0; anchor < static_cast<int>(cells.size()); ++anchor) {
      if (cells[anchor].type == LadderInstructionType::EMPTY ||
          cells[anchor].type == LadderInstructionType::HLINE) continue;
      const InstructionSpan span = GetInstructionSpan(rung, anchor);
      if (column >= span.first && column <= span.last) return span;
    }
  }
  return {column, column, column};
}

void ProgrammingMode::HandleKeyboardInput(int key) {
  if (key == ImGuiKey_F4 && !is_monitor_mode_) {
    CompileLadderToOpenPLC();
    return;
  }
  const bool any_popup_open =
      show_address_popup_ || show_rung_memo_popup_ || show_vertical_dialog_;
  const bool is_instruction_shortcut =
      key >= ImGuiKey_F5 && key <= ImGuiKey_F8;
  if (any_popup_open) {
    if (!is_monitor_mode_ && show_address_popup_ && is_instruction_shortcut) {
      pending_instruction_type_ = InstructionShortcut(key, ImGui::GetIO().KeyShift);
      if (pending_instruction_type_ == LadderInstructionType::OTE) {
        SelectSingleCell(selected_rung_, GetColumnCount() - 1, false);
      }
    }
    return;
  }

  bool onEndRung =
      (selected_rung_ == static_cast<int>(ladder_program_.rungs.size()) - 1);
  ImGuiIO& io = ImGui::GetIO();
  bool shiftPressed = io.KeyShift;
  bool ctrlPressed = io.KeyCtrl;

  if (!is_monitor_mode_ && key == ImGuiKey_GraveAccent) {
    if (!ladder_program_.rungs.empty()) {
      int targetRung = selected_rung_;
      if (targetRung < 0 ||
          targetRung >= static_cast<int>(ladder_program_.rungs.size())) {
        targetRung = 0;
      }
      if (ladder_program_.rungs[static_cast<size_t>(targetRung)].isEndRung &&
          ladder_program_.rungs.size() > 1) {
        targetRung = static_cast<int>(ladder_program_.rungs.size()) - 2;
      }
      if (targetRung >= 0 &&
          targetRung < static_cast<int>(ladder_program_.rungs.size()) &&
          !ladder_program_.rungs[static_cast<size_t>(targetRung)].isEndRung) {
        selected_rung_ = targetRung;
        rung_memo_popup_target_rung_ = targetRung;
        std::strncpy(rung_memo_popup_buffer_,
                     ladder_program_.rungs[static_cast<size_t>(targetRung)]
                         .memo.c_str(),
                     sizeof(rung_memo_popup_buffer_) - 1);
        rung_memo_popup_buffer_[sizeof(rung_memo_popup_buffer_) - 1] = '\0';
        show_rung_memo_popup_ = true;
        memo_edit_session_active_ = false;
      }
    }
    return;
  }

  if (io.WantTextInput) {
    return;
  }

  if (ctrlPressed) {
    if (!is_monitor_mode_ && key == ImGuiKey_C) {
      if (rung_selection_mode_) {
        CopySelectedRungsToClipboard();
      } else {
        CopySelectedCellsToClipboard();
      }
      return;
    }
    if (!is_monitor_mode_ && key == ImGuiKey_V) {
      if (rung_selection_mode_) {
        PasteRungClipboard();
      } else {
        PasteCellClipboard();
      }
      return;
    }
    if (key == ImGuiKey_Z) {
      if (shiftPressed) {
        RedoProgrammingState();
      } else {
        UndoProgrammingState();
      }
      return;
    }
    if (key == ImGuiKey_Y) {
      RedoProgrammingState();
      return;
    }
  }

  if (!is_monitor_mode_) {
    switch (key) {
      case ImGuiKey_F5:
      case ImGuiKey_F6:
      case ImGuiKey_F7:
      case ImGuiKey_F8:
        pending_action_.type = PendingActionType::ADD_INSTRUCTION;
        pending_action_.instructionType = InstructionShortcut(key, shiftPressed);
        return;
      case ImGuiKey_F9:
        if (shiftPressed) {
          if (!onEndRung && selected_cell_ > 0) {
            AddVerticalConnection();
          }
        } else {
          pending_action_.type = PendingActionType::ADD_INSTRUCTION;
          pending_action_.instructionType = LadderInstructionType::HLINE;
        }
        return;
      case ImGuiKey_Delete:
        if (!onEndRung) {
          if (!rung_selection_mode_ && HasCellSelection()) {
            DeleteSelectedCells();
          } else if ((rung_selection_mode_ &&
               HasMultiSelectedRung(selected_rung_)) ||
              GetSortedSelectedRungs().size() > 1) {
            pending_action_.type = PendingActionType::DELETE_RUNG;
            pending_action_.rungIndex = selected_rung_;
          } else {
            pending_action_.type = PendingActionType::DELETE_INSTRUCTION;
          }
        }
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
  const int maxSelectableRung =
      std::max(0, static_cast<int>(ladder_program_.rungs.size()) - 2);
  const bool is_arrow = key == ImGuiKey_LeftArrow ||
                        key == ImGuiKey_RightArrow ||
                        key == ImGuiKey_UpArrow ||
                        key == ImGuiKey_DownArrow;

  switch (key) {
    case ImGuiKey_LeftArrow:
      selected_cell_ = std::max(0,
          GetCellSpan(selected_rung_, selected_cell_).first - 1);
      rung_selection_mode_ = false;
      break;
    case ImGuiKey_RightArrow:
      selected_cell_ = std::min(GetColumnCount() - 1,
          GetCellSpan(selected_rung_, selected_cell_).last + 1);
      rung_selection_mode_ = false;
      break;
    case ImGuiKey_UpArrow:
      if (selected_rung_ > 0)
        selected_rung_--;
      break;
    case ImGuiKey_DownArrow:
      if (selected_rung_ < (shiftPressed ? maxSelectableRung :
          static_cast<int>(ladder_program_.rungs.size()) - 1)) {
        selected_rung_++;
      }
      break;
    case ImGuiKey_F2:
      SetMonitorMode(true);
      break;
    case ImGuiKey_F3:
      SetMonitorMode(false);
      break;
  }

  if (is_arrow && !shiftPressed) {
    SelectSingleCell(std::clamp(selected_rung_, 0,
        static_cast<int>(ladder_program_.rungs.size()) - 1),
                     selected_cell_, false);
  }

  if (shiftPressed && prev_rung != selected_rung_ &&
      selected_rung_ >= 0 && selected_rung_ <= maxSelectableRung) {
    ExtendRungSelectionTo(selected_rung_);
  } else if (!ctrlPressed) {
    if (!rung_selection_mode_ &&
        (prev_rung != selected_rung_ || prev_cell != selected_cell_) &&
        selected_rung_ >= 0 && selected_rung_ <= maxSelectableRung &&
        selected_cell_ >= 0 && selected_cell_ < GetColumnCount()) {
      SelectSingleCell(selected_rung_, selected_cell_, false);
    }
    selected_rungs_.clear();
    if (selected_rung_ >= 0 &&
        selected_rung_ < static_cast<int>(ladder_program_.rungs.size()) - 1) {
      rung_selection_anchor_ = selected_rung_;
      selected_rungs_.insert(selected_rung_);
    }
  }

  if (ctrlPressed && !is_monitor_mode_ &&
      (prev_rung != selected_rung_ || prev_cell != selected_cell_)) {
    PushProgrammingUndoState();
    if (prev_rung == selected_rung_) {
      int step = (selected_cell_ > prev_cell) ? 1 : -1;
      for (int c = prev_cell; c != selected_cell_; c += step) {
        int next = c + step;
        if (next < 0 || next >= GetColumnCount())
          continue;
        if (prev_rung >= static_cast<int>(ladder_program_.rungs.size()) - 1)
          continue;

        if (next != (GetColumnCount() - 1) &&
            GetCellSpan(prev_rung, next).anchor == next) {
          auto& dest = ladder_program_.rungs[prev_rung].cells[next];
          if (dest.type == LadderInstructionType::HLINE) {
            dest = LadderInstruction();
            MarkDirty();
            continue;
          }
        }

        if (c != (GetColumnCount() - 1) &&
            GetCellSpan(prev_rung, c).anchor == c) {
          auto& current = ladder_program_.rungs[prev_rung].cells[c];
          if (current.type == LadderInstructionType::EMPTY) {
            current.type = LadderInstructionType::HLINE;
            MarkDirty();
          }
        }
      }
    } else if (prev_cell == selected_cell_) {
      if (selected_cell_ == (GetColumnCount() - 1)) {
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

void ProgrammingMode::SelectSingleRung(int rungIndex) {
  if (rungIndex < 0 ||
      rungIndex >= static_cast<int>(ladder_program_.rungs.size()) - 1) {
    return;
  }

  ClearCellSelection();
  selected_rung_ = rungIndex;
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  rung_selection_anchor_ = rungIndex;
  selected_rungs_.clear();
  selected_rungs_.insert(rungIndex);
}

void ProgrammingMode::ExtendRungSelectionTo(int rungIndex) {
  if (rungIndex < 0 ||
      rungIndex >= static_cast<int>(ladder_program_.rungs.size()) - 1) {
    return;
  }

  ClearCellSelection();
  const int anchor = std::max(
      0, std::min(rung_selection_anchor_,
                  static_cast<int>(ladder_program_.rungs.size()) - 2));
  const int first = std::min(anchor, rungIndex);
  const int last = std::max(anchor, rungIndex);

  selected_rung_ = rungIndex;
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  selected_rungs_.clear();
  for (int index = first; index <= last; ++index) {
    selected_rungs_.insert(index);
  }
}

void ProgrammingMode::ToggleRungSelection(int rungIndex) {
  if (rungIndex < 0 ||
      rungIndex >= static_cast<int>(ladder_program_.rungs.size()) - 1) {
    return;
  }

  ClearCellSelection();
  selected_rung_ = rungIndex;
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  rung_selection_anchor_ = rungIndex;

  const auto existing = selected_rungs_.find(rungIndex);
  if (existing == selected_rungs_.end()) {
    selected_rungs_.insert(rungIndex);
    return;
  }

  if (selected_rungs_.size() == 1) {
    return;
  }

  selected_rungs_.erase(existing);
  if (!selected_rungs_.empty()) {
    selected_rung_ = *selected_rungs_.begin();
  }
}

void ProgrammingMode::SelectSingleCell(int rungIndex,
                                       int cellIndex,
                                       bool startDrag) {
  if (rungIndex < 0 ||
      rungIndex >= static_cast<int>(ladder_program_.rungs.size()) ||
      cellIndex < 0 || cellIndex >= GetColumnCount()) {
    return;
  }

  selected_rung_ = rungIndex;
  cellIndex = GetCellSpan(rungIndex, cellIndex).anchor;
  selected_cell_ = cellIndex;
  rung_selection_mode_ = false;
  rung_selection_drag_active_ = false;
  cell_selection_active_ = true;
  cell_selection_anchor_rung_ = rungIndex;
  cell_selection_anchor_cell_ = cellIndex;
  cell_selection_end_rung_ = rungIndex;
  cell_selection_end_cell_ = cellIndex;
  cell_selection_drag_active_ = startDrag;
  selected_rungs_.clear();
  selected_rungs_.insert(rungIndex);
}

void ProgrammingMode::UpdateCellSelection(int rungIndex, int cellIndex) {
  if (!cell_selection_active_ ||
      rungIndex < 0 ||
      rungIndex >= static_cast<int>(ladder_program_.rungs.size()) - 1 ||
      cellIndex < 0 || cellIndex >= GetColumnCount()) {
    return;
  }

  cellIndex = GetCellSpan(rungIndex, cellIndex).anchor;
  selected_rung_ = rungIndex;
  selected_cell_ = cellIndex;
  rung_selection_mode_ = false;
  cell_selection_end_rung_ = rungIndex;
  cell_selection_end_cell_ = cellIndex;
  selected_rungs_.clear();

  CellSelectionBounds bounds;
  if (!GetCellSelectionBounds(&bounds)) {
    return;
  }
  for (int row = bounds.minRung; row <= bounds.maxRung; ++row) {
    selected_rungs_.insert(row);
  }
}

void ProgrammingMode::ClearCellSelection() {
  cell_selection_active_ = false;
  cell_selection_drag_active_ = false;
  cell_selection_anchor_rung_ = selected_rung_;
  cell_selection_anchor_cell_ = selected_cell_;
  cell_selection_end_rung_ = selected_rung_;
  cell_selection_end_cell_ = selected_cell_;
}

bool ProgrammingMode::HasCellSelection() const {
  CellSelectionBounds bounds;
  return GetCellSelectionBounds(&bounds);
}

bool ProgrammingMode::GetCellSelectionBounds(CellSelectionBounds* bounds) const {
  if (!bounds || !cell_selection_active_ || rung_selection_mode_ ||
      ladder_program_.rungs.size() <= 1) {
    return false;
  }

  const int maxRung =
      std::max(0, static_cast<int>(ladder_program_.rungs.size()) - 2);
  if (cell_selection_anchor_rung_ > maxRung &&
      cell_selection_end_rung_ > maxRung) {
    return false;
  }
  bounds->minRung = std::max(
      0, std::min(std::min(cell_selection_anchor_rung_, cell_selection_end_rung_),
                  maxRung));
  bounds->maxRung = std::max(
      0, std::min(std::max(cell_selection_anchor_rung_, cell_selection_end_rung_),
                  maxRung));
  bounds->minCell =
      std::max(0, std::min(cell_selection_anchor_cell_, cell_selection_end_cell_));
  bounds->maxCell =
      std::max(0, std::min(std::max(cell_selection_anchor_cell_, cell_selection_end_cell_), (GetColumnCount() - 1)));
  for (int pass = 0; pass < GetColumnCount(); ++pass) {
    const int first = bounds->minCell;
    const int last = bounds->maxCell;
    for (int row = bounds->minRung; row <= bounds->maxRung; ++row) {
      for (const int column : {first, last}) {
        const InstructionSpan span = GetCellSpan(row, column);
        bounds->minCell = std::min(bounds->minCell, span.first);
        bounds->maxCell = std::max(bounds->maxCell, span.last);
      }
    }
    if (first == bounds->minCell && last == bounds->maxCell) break;
  }
  return bounds->minRung <= bounds->maxRung &&
         bounds->minCell <= bounds->maxCell;
}

bool ProgrammingMode::IsCellInSelection(int rungIndex, int cellIndex) const {
  CellSelectionBounds bounds;
  if (!GetCellSelectionBounds(&bounds)) {
    return selected_rung_ == rungIndex && selected_cell_ == cellIndex &&
           !rung_selection_mode_;
  }

  return rungIndex >= bounds.minRung && rungIndex <= bounds.maxRung &&
         cellIndex >= bounds.minCell && cellIndex <= bounds.maxCell;
}

bool ProgrammingMode::HasMultiSelectedRung(int rungIndex) const {
  return rungIndex >= 0 &&
         rungIndex < static_cast<int>(ladder_program_.rungs.size()) - 1 &&
         selected_rungs_.count(rungIndex) > 0;
}

bool ProgrammingMode::HasAnySelectedRungs() const {
  return !GetSortedSelectedRungs().empty();
}

std::vector<int> ProgrammingMode::GetSortedSelectedRungs() const {
  std::vector<int> result;
  const int maxRung = static_cast<int>(ladder_program_.rungs.size()) - 1;
  for (int rungIndex : selected_rungs_) {
    if (rungIndex >= 0 && rungIndex < maxRung) {
      result.push_back(rungIndex);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

void ProgrammingMode::CopySelectedCellsToClipboard() {
  CellSelectionBounds bounds;
  if (!GetCellSelectionBounds(&bounds)) {
    return;
  }

  const int height = bounds.maxRung - bounds.minRung + 1;
  const int width = bounds.maxCell - bounds.minCell + 1;
  cell_clipboard_.assign(
      static_cast<size_t>(height),
      std::vector<LadderInstruction>(static_cast<size_t>(width)));
  cell_clipboard_connections_.clear();

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      cell_clipboard_[static_cast<size_t>(row)][static_cast<size_t>(col)] =
          ladder_program_.rungs[static_cast<size_t>(bounds.minRung + row)]
              .cells[static_cast<size_t>(bounds.minCell + col)];
    }
  }

  for (const auto& connection : ladder_program_.verticalConnections) {
    if (connection.x < bounds.minCell || connection.x > bounds.maxCell) {
      continue;
    }

    std::vector<int> copiedRungs;
    for (int rungIndex : connection.rungs) {
      if (rungIndex >= bounds.minRung && rungIndex <= bounds.maxRung) {
        copiedRungs.push_back(rungIndex);
      }
    }

    AppendVerticalConnectionComponents(connection.x, copiedRungs, -bounds.minRung,
                                       -bounds.minCell,
                                       &cell_clipboard_connections_);
  }
}

void ProgrammingMode::PasteCellClipboard() {
  if (is_monitor_mode_ || rung_selection_mode_ || cell_clipboard_.empty() ||
      cell_clipboard_.front().empty() || ladder_program_.rungs.empty()) {
    return;
  }

  int targetRung = selected_rung_;
  int targetCell = selected_cell_;
  CellSelectionBounds targetBounds;
  if (GetCellSelectionBounds(&targetBounds)) {
    targetRung = targetBounds.minRung;
    targetCell = targetBounds.minCell;
  }

  const int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  if (targetRung < 0 || targetRung >= endIndex || targetCell < 0 ||
      targetCell >= GetColumnCount()) {
    return;
  }

  const int height = static_cast<int>(cell_clipboard_.size());
  const int width = static_cast<int>(cell_clipboard_.front().size());
  if (targetCell + width > GetColumnCount()) {
    std::cout << "[WARN] Cell clipboard paste exceeds ladder width" << std::endl;
    return;
  }
  for (int row = targetRung; row < std::min(targetRung + height, endIndex); ++row) {
    for (int column = targetCell; column < targetCell + width; ++column) {
      const InstructionSpan span = GetCellSpan(row, column);
      if (span.first < targetCell || span.last >= targetCell + width) {
        inline_input_error_ = "Paste would split an existing instruction";
        return;
      }
    }
  }

  PushProgrammingUndoState();

  while (targetRung + height >
         static_cast<int>(ladder_program_.rungs.size()) - 1) {
    ladder_program_.rungs.insert(ladder_program_.rungs.end() - 1, Rung());
  }

  ClearVerticalConnectionsInRange(targetRung, targetRung + height - 1,
                                  targetCell, targetCell + width - 1);

  for (int row = 0; row < height; ++row) {
    auto& destinationRung =
        ladder_program_.rungs[static_cast<size_t>(targetRung + row)];
    for (int col = 0; col < width; ++col) {
      destinationRung.cells[static_cast<size_t>(targetCell + col)] =
          cell_clipboard_[static_cast<size_t>(row)][static_cast<size_t>(col)];
    }
    UpdateHorizontalLines(targetRung + row);
  }

  for (const auto& connection : cell_clipboard_connections_) {
    VerticalConnection pasted = connection;
    pasted.x += targetCell;
    for (int& rungIndex : pasted.rungs) {
      rungIndex += targetRung;
    }
    ladder_program_.verticalConnections.push_back(pasted);
  }

  CanonicalizeLadderProgram(&ladder_program_);

  selected_rung_ = targetRung;
  selected_cell_ = targetCell;
  cell_selection_active_ = true;
  cell_selection_anchor_rung_ = targetRung;
  cell_selection_anchor_cell_ = targetCell;
  cell_selection_end_rung_ = targetRung + height - 1;
  cell_selection_end_cell_ = targetCell + width - 1;
  cell_selection_drag_active_ = false;
  selected_rungs_.clear();
  for (int row = cell_selection_anchor_rung_; row <= cell_selection_end_rung_;
       ++row) {
    selected_rungs_.insert(row);
  }
  MarkDirty();
}

void ProgrammingMode::CopySelectedRungsToClipboard() {
  if (!rung_selection_mode_) {
    return;
  }

  std::vector<int> selected = GetSortedSelectedRungs();
  const int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  if (selected.empty() && selected_rung_ >= 0 && selected_rung_ < endIndex) {
    selected.push_back(selected_rung_);
  }
  if (selected.empty()) {
    return;
  }

  rung_clipboard_.clear();
  rung_clipboard_connections_.clear();

  std::map<int, int> remap;
  for (size_t i = 0; i < selected.size(); ++i) {
    remap[selected[i]] = static_cast<int>(i);
    rung_clipboard_.push_back(ladder_program_.rungs[selected[i]]);
  }

  for (const auto& connection : ladder_program_.verticalConnections) {
    VerticalConnection copied;
    copied.x = connection.x;
    for (int rungIndex : connection.rungs) {
      const auto it = remap.find(rungIndex);
      if (it != remap.end()) {
        copied.rungs.push_back(it->second);
      }
    }
    if (copied.rungs.size() >= 2) {
      std::sort(copied.rungs.begin(), copied.rungs.end());
      copied.rungs.erase(
          std::unique(copied.rungs.begin(), copied.rungs.end()),
          copied.rungs.end());
      rung_clipboard_connections_.push_back(copied);
    }
  }
}

void ProgrammingMode::PasteRungClipboard() {
  if (is_monitor_mode_ || !rung_selection_mode_ || rung_clipboard_.empty() ||
      ladder_program_.rungs.empty()) {
    return;
  }

  const int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  int insertIndex = endIndex;
  if (selected_rung_ >= 0 && selected_rung_ < endIndex) {
    insertIndex = selected_rung_ + 1;
  }

  PushProgrammingUndoState();

  const int addedCount = static_cast<int>(rung_clipboard_.size());
  for (auto& connection : ladder_program_.verticalConnections) {
    for (int& rungIndex : connection.rungs) {
      if (rungIndex >= insertIndex) {
        rungIndex += addedCount;
      }
    }
  }

  ladder_program_.rungs.insert(ladder_program_.rungs.begin() + insertIndex,
                               rung_clipboard_.begin(), rung_clipboard_.end());

  for (const auto& clipboardConnection : rung_clipboard_connections_) {
    VerticalConnection inserted = clipboardConnection;
    for (int& rungIndex : inserted.rungs) {
      rungIndex += insertIndex;
    }
    ladder_program_.verticalConnections.push_back(inserted);
  }

  CanonicalizeLadderProgram(&ladder_program_);

  ClearCellSelection();
  selected_rung_ = insertIndex;
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  rung_selection_anchor_ = insertIndex;
  selected_rungs_.clear();
  for (int i = 0; i < addedCount; ++i) {
    selected_rungs_.insert(insertIndex + i);
  }
  MarkDirty();
}

void ProgrammingMode::MoveSelectedRungsBefore(int targetRungIndex) {
  if (is_monitor_mode_ || ladder_program_.rungs.size() <= 1) {
    return;
  }

  std::vector<int> selected = GetSortedSelectedRungs();
  const int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  if (selected.empty() && selected_rung_ >= 0 && selected_rung_ < endIndex) {
    selected.push_back(selected_rung_);
  }
  if (selected.empty()) {
    return;
  }

  if (targetRungIndex < 0) {
    targetRungIndex = 0;
  }
  if (targetRungIndex > endIndex) {
    targetRungIndex = endIndex;
  }
  if (std::find(selected.begin(), selected.end(), targetRungIndex) !=
      selected.end()) {
    return;
  }

  std::vector<int> remaining;
  remaining.reserve(endIndex - static_cast<int>(selected.size()));
  for (int rungIndex = 0; rungIndex < endIndex; ++rungIndex) {
    if (!std::binary_search(selected.begin(), selected.end(), rungIndex)) {
      remaining.push_back(rungIndex);
    }
  }

  int insertionPos = 0;
  if (targetRungIndex >= endIndex) {
    insertionPos = static_cast<int>(remaining.size());
  } else {
    insertionPos = static_cast<int>(std::count_if(
        remaining.begin(), remaining.end(),
        [targetRungIndex](int rungIndex) { return rungIndex < targetRungIndex; }));
  }

  std::vector<int> newOrder = remaining;
  newOrder.insert(newOrder.begin() + insertionPos, selected.begin(),
                  selected.end());

  std::map<int, int> remap;
  for (size_t newIndex = 0; newIndex < newOrder.size(); ++newIndex) {
    remap[newOrder[newIndex]] = static_cast<int>(newIndex);
  }

  PushProgrammingUndoState();

  std::vector<Rung> reordered;
  reordered.reserve(ladder_program_.rungs.size());
  for (int originalIndex : newOrder) {
    reordered.push_back(ladder_program_.rungs[originalIndex]);
  }
  reordered.push_back(ladder_program_.rungs.back());
  ladder_program_.rungs.swap(reordered);

  std::vector<VerticalConnection> remappedConnections;
  remappedConnections.reserve(ladder_program_.verticalConnections.size());
  for (const auto& connection : ladder_program_.verticalConnections) {
    VerticalConnection remappedConnection;
    remappedConnection.x = connection.x;
    for (int rungIndex : connection.rungs) {
      const auto it = remap.find(rungIndex);
      if (it != remap.end()) {
        remappedConnection.rungs.push_back(it->second);
      }
    }
    if (remappedConnection.rungs.size() >= 2) {
      std::sort(remappedConnection.rungs.begin(),
                remappedConnection.rungs.end());
      remappedConnection.rungs.erase(
          std::unique(remappedConnection.rungs.begin(),
                      remappedConnection.rungs.end()),
          remappedConnection.rungs.end());
      remappedConnections.push_back(remappedConnection);
    }
  }
  ladder_program_.verticalConnections.swap(remappedConnections);
  CanonicalizeLadderProgram(&ladder_program_);

  ClearCellSelection();
  selected_rung_ = insertionPos;
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  rung_selection_anchor_ = insertionPos;
  selected_rungs_.clear();
  for (size_t i = 0; i < selected.size(); ++i) {
    selected_rungs_.insert(insertionPos + static_cast<int>(i));
  }
  MarkDirty();
}

void ProgrammingMode::ClearVerticalConnectionsInRange(int minRung,
                                                      int maxRung,
                                                      int minCell,
                                                      int maxCell) {
  std::vector<VerticalConnection> updatedConnections;
  updatedConnections.reserve(ladder_program_.verticalConnections.size());

  for (const auto& connection : ladder_program_.verticalConnections) {
    if (connection.x < minCell || connection.x > maxCell) {
      updatedConnections.push_back(connection);
      continue;
    }

    std::vector<int> remainingRungs;
    for (int rungIndex : connection.rungs) {
      if (rungIndex < minRung || rungIndex > maxRung) {
        remainingRungs.push_back(rungIndex);
      }
    }

    AppendVerticalConnectionComponents(connection.x, remainingRungs, 0, 0,
                                       &updatedConnections);
  }

  ladder_program_.verticalConnections.swap(updatedConnections);
}

void ProgrammingMode::DeleteSelectedCells() {
  CellSelectionBounds bounds;
  if (!GetCellSelectionBounds(&bounds)) {
    DeleteCurrentInstruction();
    return;
  }

  PushProgrammingUndoState();

  for (int rungIndex = bounds.minRung; rungIndex <= bounds.maxRung; ++rungIndex) {
    auto& cells = ladder_program_.rungs[static_cast<size_t>(rungIndex)].cells;
    for (int cellIndex = bounds.minCell; cellIndex <= bounds.maxCell; ++cellIndex) {
      cells[static_cast<size_t>(cellIndex)] = LadderInstruction();
    }
    UpdateHorizontalLines(rungIndex);
  }

  ClearVerticalConnectionsInRange(bounds.minRung, bounds.maxRung, bounds.minCell,
                                  bounds.maxCell);
  CanonicalizeLadderProgram(&ladder_program_);

  selected_rung_ = bounds.minRung;
  selected_cell_ = bounds.minCell;
  rung_selection_mode_ = false;
  rung_selection_drag_active_ = false;
  cell_selection_active_ = true;
  cell_selection_anchor_rung_ = bounds.minRung;
  cell_selection_anchor_cell_ = bounds.minCell;
  cell_selection_end_rung_ = bounds.maxRung;
  cell_selection_end_cell_ = bounds.maxCell;
  cell_selection_drag_active_ = false;
  selected_rungs_.clear();
  for (int rungIndex = bounds.minRung; rungIndex <= bounds.maxRung; ++rungIndex) {
    selected_rungs_.insert(rungIndex);
  }
  MarkDirty();
}

void ProgrammingMode::DeleteSelectedRungs() {
  std::vector<int> selected = GetSortedSelectedRungs();
  if (selected.empty()) {
    if (selected_rung_ >= 0 &&
        selected_rung_ < static_cast<int>(ladder_program_.rungs.size()) - 1) {
      DeleteRung(selected_rung_);
    }
    return;
  }
  if (selected.size() == 1) {
    DeleteRung(selected.front());
    return;
  }

  const int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  if (endIndex <= 0) {
    return;
  }

  PushProgrammingUndoState();

  std::set<int> selectedSet(selected.begin(), selected.end());
  std::vector<Rung> remaining;
  remaining.reserve(ladder_program_.rungs.size());
  std::map<int, int> remap;

  int nextIndex = 0;
  for (int rungIndex = 0; rungIndex < endIndex; ++rungIndex) {
    if (selectedSet.count(rungIndex) > 0) {
      continue;
    }
    remap[rungIndex] = nextIndex++;
    remaining.push_back(ladder_program_.rungs[static_cast<size_t>(rungIndex)]);
  }
  remaining.push_back(ladder_program_.rungs.back());
  ladder_program_.rungs.swap(remaining);

  std::vector<VerticalConnection> updatedConnections;
  updatedConnections.reserve(ladder_program_.verticalConnections.size());
  for (const auto& connection : ladder_program_.verticalConnections) {
    VerticalConnection updated;
    updated.x = connection.x;
    for (int rungIndex : connection.rungs) {
      const auto it = remap.find(rungIndex);
      if (it != remap.end()) {
        updated.rungs.push_back(it->second);
      }
    }
    if (updated.rungs.size() >= 2) {
      std::sort(updated.rungs.begin(), updated.rungs.end());
      updated.rungs.erase(
          std::unique(updated.rungs.begin(), updated.rungs.end()),
          updated.rungs.end());
      updatedConnections.push_back(updated);
    }
  }
  ladder_program_.verticalConnections.swap(updatedConnections);
  CanonicalizeLadderProgram(&ladder_program_);

  ClearCellSelection();
  selected_rung_ = std::max(
      0, std::min(selected.front(),
                  static_cast<int>(ladder_program_.rungs.size()) - 2));
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  rung_selection_anchor_ = std::max(0, selected_rung_);
  selected_rungs_.clear();
  if (selected_rung_ >= 0 &&
      selected_rung_ < static_cast<int>(ladder_program_.rungs.size()) - 1) {
    selected_rungs_.insert(selected_rung_);
  }
  MarkDirty();
}

void ProgrammingMode::HandleEditAction(LadderInstructionType type) {
  if (type == LadderInstructionType::OTE) {
    SelectSingleCell(selected_rung_, GetColumnCount() - 1, false);
  }
  pending_instruction_type_ = type;
  temp_address_buffer_[0] = '\0';
  if (type == LadderInstructionType::HLINE) {
    std::strcpy(temp_address_buffer_, "1");
  }
  show_address_popup_ = true;
  inline_focus_pending_ = true;
  inline_input_error_.clear();
}

void ProgrammingMode::ConfirmInstruction() {
  if (selected_rung_ < 0 || selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()) ||
      ladder_program_.rungs[selected_rung_].isEndRung) return;
  LadderInstruction instruction;
  if (!plc_emulator::programming::ParseCellInput(temp_address_buffer_,
      pending_instruction_type_, &instruction, &inline_input_error_)) return;
  auto& rung = ladder_program_.rungs[selected_rung_];
  if (instruction.type == LadderInstructionType::HLINE) {
    const auto length = plc_emulator::programming::ParseOperand(instruction.preset);
    const int count = length ? length->immediate : 0;
    const int64_t destination = static_cast<int64_t>(selected_cell_) + count;
    if (count == 0 || destination < 0 ||
        destination > static_cast<int64_t>(rung.cells.size())) {
      inline_input_error_ = "Line length exceeds this row";
      return;
    }
    const int end = static_cast<int>(destination);
    const int begin = count > 0 ? selected_cell_ : end;
    const int finish = count > 0 ? end : selected_cell_;
    for (int column = begin; column < finish; ++column) {
      if (rung.cells[column].type != LadderInstructionType::EMPTY &&
          rung.cells[column].type != LadderInstructionType::HLINE) {
        inline_input_error_ = "Line would overwrite an instruction";
        return;
      }
    }
    PushProgrammingUndoState();
    for (int column = begin; column < finish; ++column) {
      rung.cells[column].type = LadderInstructionType::HLINE;
    }
    MarkDirty();
    SelectSingleCell(selected_rung_, std::min(end, GetColumnCount() - 1), false);
    show_address_popup_ = false;
    inline_input_error_.clear();
    return;
  }
  if (instruction.type == LadderInstructionType::kWrappingSource &&
      selected_cell_ == 0) {
    instruction.type = LadderInstructionType::kWrappingDestination;
  }
  const bool output = IsLadderOutput(instruction.type);
  const int target = output ? static_cast<int>(rung.cells.size()) - 1 : selected_cell_;
  if (target < 0 || target >= static_cast<int>(rung.cells.size())) return;
  const LadderInstruction previous = rung.cells[target];
  rung.cells[target] = instruction;
  const InstructionSpan span = GetInstructionSpan(selected_rung_, target);
  rung.cells[target] = previous;
  if (span.last - span.first + 1 < InstructionColumns(instruction)) {
    inline_input_error_ = "Instruction needs more free columns; use K wrapping";
    return;
  }
  const bool had_source =
      rung.cells[target].type == LadderInstructionType::kWrappingSource;
  PushProgrammingUndoState();
  rung.cells[target] = std::move(instruction);
  UpdateHorizontalLines(selected_rung_);
  MarkDirty();
  if (rung.cells[target].type == LadderInstructionType::kWrappingSource) {
    if (!had_source) AddNewRung(false);
    SelectSingleCell(had_source ? selected_rung_ + 1 : selected_rung_, 0, false);
    show_address_popup_ = false;
    inline_input_error_.clear();
    return;
  }
  const int next_cell = output ? target : std::min(
      GetInstructionSpan(selected_rung_, target).last + 1,
      static_cast<int>(rung.cells.size()) - 1);
  SelectSingleCell(selected_rung_, next_cell, false);
  show_address_popup_ = false;
  inline_input_error_.clear();
}

void ProgrammingMode::DeleteCurrentInstruction() {
  if (selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()) ||
      selected_cell_ < 0 || selected_cell_ >= GetColumnCount())
    return;

  PushProgrammingUndoState();
  ladder_program_.rungs[selected_rung_].cells[selected_cell_] =
      LadderInstruction();
  UpdateHorizontalLines(selected_rung_);
  MarkDirty();
}

void ProgrammingMode::EditCurrentInstruction() {
  if (selected_rung_ >= static_cast<int>(ladder_program_.rungs.size()) ||
      selected_cell_ < 0 || selected_cell_ >= GetColumnCount())
    return;

  const auto& instruction =
      ladder_program_.rungs[selected_rung_].cells[selected_cell_];

  if (instruction.type == LadderInstructionType::EMPTY ||
      instruction.type == LadderInstructionType::HLINE) {
    return;
  }

  const std::string full_command =
      plc_emulator::programming::FormatCellInput(instruction);
  strncpy(temp_address_buffer_, full_command.c_str(),
          sizeof(temp_address_buffer_) - 1);
  temp_address_buffer_[sizeof(temp_address_buffer_) - 1] = '\0';

  pending_instruction_type_ = instruction.type;
  if (pending_instruction_type_ != LadderInstructionType::XIC &&
      pending_instruction_type_ != LadderInstructionType::XIO &&
      pending_instruction_type_ != LadderInstructionType::RISING_CONTACT &&
      pending_instruction_type_ != LadderInstructionType::FALLING_CONTACT &&
      pending_instruction_type_ != LadderInstructionType::HLINE &&
      pending_instruction_type_ != LadderInstructionType::kWrappingSource &&
      pending_instruction_type_ != LadderInstructionType::kWrappingDestination) {
    pending_instruction_type_ = LadderInstructionType::OTE;
  }

  show_address_popup_ = true;
  inline_focus_pending_ = true;
  inline_input_error_.clear();
}

void ProgrammingMode::AddNewRung(bool record_undo) {
  if (ladder_program_.rungs.empty()) {
    if (record_undo) PushProgrammingUndoState();
    ladder_program_.rungs.push_back(Rung());
    CanonicalizeLadderProgram(&ladder_program_);
    selected_rung_ = 0;
    selected_cell_ = 0;
    rung_selection_mode_ = true;
    rung_selection_anchor_ = 0;
    selected_rungs_.clear();
    selected_rungs_.insert(0);
    MarkDirty();
    return;
  }

  int endIndex = static_cast<int>(ladder_program_.rungs.size()) - 1;
  int insertIndex = endIndex;
  if (selected_rung_ < endIndex) {
    insertIndex = selected_rung_ + 1;
  }

  if (record_undo) PushProgrammingUndoState();
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
  CanonicalizeLadderProgram(&ladder_program_);
  selected_rung_ = insertIndex;
  selected_cell_ = 0;
  rung_selection_mode_ = true;
  rung_selection_anchor_ = insertIndex;
  selected_rungs_.clear();
  selected_rungs_.insert(insertIndex);
  MarkDirty();
}

void ProgrammingMode::DeleteRung(int rungIndexToDelete) {
  // Keep the end rung intact.
  if (rungIndexToDelete < 0 ||
      rungIndexToDelete >= static_cast<int>(ladder_program_.rungs.size()) - 1) {
    return;
  }

  PushProgrammingUndoState();
  // Rebuild vertical connections with shifted indices.
  std::vector<VerticalConnection> updatedConnections;
  for (const auto& conn : ladder_program_.verticalConnections) {
    VerticalConnection newConn;
    newConn.x = conn.x;
    for (int r_idx : conn.rungs) {
      if (r_idx == rungIndexToDelete) {
        continue;
      }
      if (r_idx > rungIndexToDelete) {
        newConn.rungs.push_back(r_idx - 1);  // Shift index after deletion.
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
  // Remove the rung.
  ladder_program_.rungs.erase(ladder_program_.rungs.begin() +
                              rungIndexToDelete);
  ladder_program_.verticalConnections = updatedConnections;
  CanonicalizeLadderProgram(&ladder_program_);

  // Adjust selection index.
  if (selected_rung_ >= rungIndexToDelete && selected_rung_ > 0) {
    selected_rung_--;
  }
  rung_selection_mode_ = true;
  rung_selection_anchor_ = std::max(0, selected_rung_);
  selected_rungs_.clear();
  if (selected_rung_ >= 0 &&
      selected_rung_ < static_cast<int>(ladder_program_.rungs.size()) - 1) {
    selected_rungs_.insert(selected_rung_);
  }

  MarkDirty();
}

void ProgrammingMode::InsertHorizontalLine() {
  PushProgrammingUndoState();
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

  for (int i = (GetColumnCount() - 1); i >= 0; --i) {
    if (cells[i].type != LadderInstructionType::EMPTY &&
        cells[i].type != LadderInstructionType::HLINE) {
      bool isOutput = (cells[i].type == LadderInstructionType::OTE ||
                       cells[i].type == LadderInstructionType::SET ||
                       cells[i].type == LadderInstructionType::RST ||
                       cells[i].type == LadderInstructionType::TON ||
                       cells[i].type == LadderInstructionType::CTU ||
                       cells[i].type == LadderInstructionType::RST_TMR_CTR ||
                       cells[i].type == LadderInstructionType::BKRST ||
                       cells[i].type == LadderInstructionType::APPLICATION ||
                       cells[i].type == LadderInstructionType::kWrappingSource);
      if (isOutput) {
        coil_pos = i;
        break;
      }
    }
  }

  for (int i = 0; i < GetColumnCount(); ++i) {
    if (cells[i].type != LadderInstructionType::EMPTY &&
        cells[i].type != LadderInstructionType::HLINE) {
      first_instruction = i;
      break;
    }
  }

  for (int i = 0; i < GetColumnCount(); ++i) {
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
  int maxRung = std::max(0, static_cast<int>(ladder_program_.rungs.size()) - 2);

  if (endRung > maxRung)
    return;

  PushProgrammingUndoState();
  VerticalConnection newConnection(selected_cell_, startRung, endRung);
  ladder_program_.verticalConnections.push_back(newConnection);
  CanonicalizeLadderProgram(&ladder_program_);

  show_vertical_dialog_ = false;
  MarkDirty();
}

}  // namespace plc
