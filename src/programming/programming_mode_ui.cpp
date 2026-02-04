// programming_mode_ui.cpp
//
// Programming mode UI rendering.

#include "plc_emulator/core/application.h"  // SaveProject ?????????????????
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/programming/programming_mode.h"

#include <algorithm>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace plc {

void ProgrammingMode::RenderProgrammingModeUI(bool isPlcRunning) {
  RenderProgrammingToolbar(isPlcRunning);
  RenderProgrammingMainArea();
  RenderStatusBar(isPlcRunning);

  RenderAddressPopup();
  RenderVerticalDialog();
}

void ProgrammingMode::RenderProgrammingHeader() {
  // Application ???????????????????????????????????????????????????????????????????????????????????
}

void ProgrammingMode::RenderProgrammingToolbar(bool isPlcRunning) {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
  if (ImGui::BeginChild("ProgrammingToolbar", ImVec2(0, 50 * layout_scale),
                        true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPos(ImVec2(20 * layout_scale, 10 * layout_scale));

    if (isPlcRunning) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button("Edit (F3)", ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            !is_monitor_mode_
                                ? ImVec4(0.4f, 0.6f, 0.8f, 1.0f)
                                : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
      if (ImGui::Button("Edit (F3)",
                        ImVec2(100 * layout_scale, 30 * layout_scale))) {
        is_monitor_mode_ = false;
      }
      ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,
                          is_monitor_mode_ ? ImVec4(0.4f, 0.6f, 0.8f, 1.0f)
                                          : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::Button("Monitor (F2)",
                      ImVec2(100 * layout_scale, 30 * layout_scale))) {
      if (!is_monitor_mode_) {
        is_monitor_mode_ = true;
        scan_time_initialized_ = false;
        scan_accumulator_ = 0.0;
        InitializeTimersAndCountersFromProgram();
      }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    if (isPlcRunning || is_monitor_mode_) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button("Save", ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      if (ImGui::Button("Save",
                        ImVec2(100 * layout_scale, 30 * layout_scale))) {

        // ???**NEW**: .csv ???????????????????????????????????????????????????
        if (application_) {
          std::cout << "[DEBUG] application_ available" << std::endl;
#ifdef _WIN32
          OPENFILENAMEA ofn;
          CHAR szFile[260] = "project.csv";
          ZeroMemory(&ofn, sizeof(ofn));
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = NULL;
          ofn.lpstrFile = szFile;
          ofn.nMaxFile = sizeof(szFile);
          ofn.lpstrFilter =
              "PLC Ladder CSV (*.csv)\0*.csv\0All Files "
              "(*.*)\0*.*\0";
          ofn.nFilterIndex = 1;
          ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
          std::cout << "[DEBUG] Save dialog opened" << std::endl;
          if (GetSaveFileNameA(&ofn) == TRUE) {
            std::cout << "[DEBUG] Save dialog confirmed" << std::endl;
            std::string savePath = ofn.lpstrFile;
            if (savePath.find(".csv") == std::string::npos) {
              savePath += ".csv";
            }

            // ??????????????????????????????????????????????????????(XML + ZIP ???????????????????
            std::cout << "[DEBUG] Save path: " << savePath << std::endl;
            std::cout << "[DEBUG] application_: " << (application_ ? "OK" : "NULL") << std::endl;


            bool success = application_->SaveProject(savePath, "PLC_Project");
            if (success) {
              std::cout << "[INFO] Project saved: " << savePath << std::endl;


            } else {
              std::cout << "[ERROR] Project save failed: " << savePath << std::endl;
              std::cout << "[TIP] Try a simple path like C:\\temp\\project.csv" << std::endl;


            }
          } else {
            std::cout << "[DEBUG] Save dialog cancelled" << std::endl;
          }
#else
          // ?????????????????????????????????????????????????????
            std::cout << "[DEBUG] Save path: " << savePath << std::endl;
            std::cout << "[DEBUG] Save path: " << savePath << std::endl;
                    << (application_ ? "OK" : "NULL") << std::endl;

          bool success =
              application_->SaveProject("project.csv", "PLC_Project");
          if (success) {
            std::cout << "[INFO] Project saved: project.csv" << std::endl;

          } else {
            std::cout << "[ERROR] Project save failed: project.csv" << std::endl;
            std::cout << "[TIP] Try a simple path like C:\\temp\\project.csv" << std::endl;

          }
#endif
        } else {
          std::cout << "[DEBUG] application_ is NULL" << std::endl;
        }
      }
    }

    ImGui::SameLine();

    if (isPlcRunning || is_monitor_mode_) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button(TR("ui.programming.load", "Load"),
                    ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      if (ImGui::Button(TR("ui.programming.load", "Load"),
                        ImVec2(100 * layout_scale, 30 * layout_scale))) {
        if (application_) {
#ifdef _WIN32
          OPENFILENAMEA ofn;
          CHAR szFile[260] = {0};
          ZeroMemory(&ofn, sizeof(ofn));
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = NULL;
          ofn.lpstrFile = szFile;
          ofn.nMaxFile = sizeof(szFile);
          ofn.lpstrFilter =
              "PLC Ladder CSV (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
          ofn.nFilterIndex = 1;
          ofn.lpstrFileTitle = NULL;
          ofn.nMaxFileTitle = 0;
          ofn.lpstrInitialDir = NULL;
          ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
          if (GetOpenFileNameA(&ofn) == TRUE) {
            bool success = application_->LoadProject(ofn.lpstrFile);
            if (success) {
              std::cout << "[INFO] Project loaded: " << ofn.lpstrFile
                        << std::endl;
            } else {
              std::cout << "[ERROR] Project load failed: " << ofn.lpstrFile
                        << std::endl;
            }
          }
#else
          bool success = application_->LoadProject("project.csv");
          if (success) {
            std::cout << "[INFO] Project loaded: project.csv" << std::endl;
          } else {
            std::cout << "[ERROR] Project load failed: project.csv"
                      << std::endl;
          }
#endif
        }
      }
    }

    ImGui::SameLine();

    if (isPlcRunning || is_monitor_mode_) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button("Compile",
                    ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      if (ImGui::Button("Compile",
                        ImVec2(100 * layout_scale, 30 * layout_scale))) {
        CompileLadderToOpenPLC();
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void ProgrammingMode::RenderProgrammingMainArea() {
  const float layout_scale = GetLayoutScale();
  ImGui::Columns(2, "MainArea", false);
  float windowWidth = ImGui::GetWindowWidth();
  float rightPanelWidth = 320.0f * layout_scale;
  ImGui::SetColumnWidth(0, windowWidth - rightPanelWidth);
  RenderLadderEditor();
  ImGui::NextColumn();
  RenderDeviceMonitor();
  ImGui::Columns(1);
}

float ProgrammingMode::GetLayoutScale() const {
  return application_ ? application_->GetLayoutScale() : 1.0f;
}

void ProgrammingMode::RenderLadderEditor() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

  float availableHeight = ImGui::GetContentRegionAvail().y -
                          (35.0f * layout_scale);
  if (ImGui::BeginChild("LadderEditor", ImVec2(0, availableHeight), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    RenderColumnHeader();
    RenderLadderDiagram();
    // ???????????????????????????????? ????????????????????????????????????????????????????????????????????????????????????????????????????
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}

void ProgrammingMode::RenderColumnHeader() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
  if (ImGui::BeginChild("ColumnHeader", ImVec2(0, 50 * layout_scale), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f * layout_scale;
    float railWidth = 22.0f * layout_scale;
    float deleteButtonWidth =
        is_monitor_mode_ ? 0.0f : 40.0f * layout_scale;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;
    float cellWidth = cellAreaWidth / 12.0f;

    ImGui::Dummy(ImVec2(rungNumberWidth, 0));
    ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(railWidth, 0));
    ImGui::SameLine(0, 0);

    for (int i = 0; i < 12; i++) {
      ImGui::BeginGroup();
      ImGui::Dummy(ImVec2(cellWidth, 35 * layout_scale));
      ImVec2 cellMin = ImGui::GetItemRectMin();
      char numStr[3];
      snprintf(numStr, 3, "%d", i);
      ImVec2 textSize = ImGui::CalcTextSize(numStr);
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(cellMin.x + (cellWidth - textSize.x) * 0.5f,
                 cellMin.y + (35 - textSize.y) * 0.5f),
          IM_COL32(80, 80, 80, 255), numStr);
      ImGui::EndGroup();

      if (i < 11) {
        ImGui::SameLine(0, 1);
      }
    }

    ImGui::SameLine(0, 5);
    ImGui::Dummy(ImVec2(railWidth, 0));

    if (!is_monitor_mode_) {
      ImGui::SameLine(0, 0);
      ImGui::Dummy(ImVec2(deleteButtonWidth, 0));
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}

void ProgrammingMode::RenderLadderDiagram() {
  for (size_t i = 0; i < ladder_program_.rungs.size(); ++i) {
    if (ladder_program_.rungs[i].isEndRung) {
      RenderEndRung(i);
    } else {
      RenderRung(i);
    }
  }
}

void ProgrammingMode::RenderVerticalConnections() {
  // [DEPRECATED] ??????????????????????? ??????????????????
  // ???????????????????????????????? ?????????????????????????????????????????????????????????????????????????????????????????????
}

// [PPT: ????????????????????????1 - ??????????????????????????????????????????????
// ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????? ????????????????????
// ????????????????????????
void ProgrammingMode::RenderVerticalConnectionsForRung(int rungIndex,
                                                       float cellAreaWidth) {
  if (ladder_program_.verticalConnections.empty())
    return;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 windowPos = ImGui::GetWindowPos();
  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();

  float rungNumberWidth = 80.0f;
  float railWidth = 22.0f;

  // [PPT: ????????????????????????????? for????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
  for (const auto& connection : ladder_program_.verticalConnections) {
    // ??????????????????????????????????????????????????????????????????????????????????????????????????? ??????????????
    bool isThisRungInConnection = false;
    for (int connectedRung : connection.rungs) {
      if (connectedRung == rungIndex) {
        isThisRungInConnection = true;
        break;
      }
    }

    if (!isThisRungInConnection)
      continue;

    // [PPT: ????????????????????????2 - React ???????????????????????????????????????????????????? ???????????????????????????????
    // ??????????? ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
    float lineX = windowPos.x + contentMin.x + rungNumberWidth + railWidth +
                  (connection.x / 12.0f) * cellAreaWidth;

    float rungCenterY = windowPos.y + contentMin.y + 25.0f;  // ?????????????????????Y ?????????????

    // [PPT: ????????????????????????????? if-else ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
    ImU32 lineColor = is_monitor_mode_ ? IM_COL32(107, 114, 128, 255)
                                      :                  // React??#6B7280
                          IM_COL32(156, 163, 175, 255);  // React??#9CA3AF

    // ?????????????????????????????????????????????????????????????????????????????????????????????????????
    // ????????????????????????????????????????????????????????????????????????????? ??????????????
    std::vector<int> sortedRungs = connection.rungs;
    std::sort(sortedRungs.begin(), sortedRungs.end());

    int minRung = sortedRungs.front();
    int maxRung = sortedRungs.back();

    float lineStartY = rungCenterY;
    float lineEndY = rungCenterY;

    // ????????????????????????????????????????????????????????? ??????????????
    if (rungIndex > minRung) {
      lineStartY = windowPos.y + contentMin.y - 2.5f;  // ??????????????????????????
    }

    // ????????????????????????????????????????????????????????????? ??????????????
    if (rungIndex < maxRung) {
      lineEndY =
          windowPos.y + contentMin.y + 52.5f;  // ??????????????????????????(50px + 2.5px)
    }

    // ???????????????????????????????????????????????
    draw_list->AddLine(ImVec2(lineX, lineStartY), ImVec2(lineX, lineEndY),
                       lineColor, 2.0f);

    // ?????????????????????????????????????????????????????????????????????
    draw_list->AddCircleFilled(ImVec2(lineX, rungCenterY), 2.0f, lineColor);
  }
}

void ProgrammingMode::RenderRung(int rungIndex) {
  const float layout_scale = GetLayoutScale();
  ImGui::PushID(rungIndex);
  bool isSelected = (selected_rung_ == rungIndex);
  bool hasCompileError = IsRungCompileError(rungIndex);
  ImVec4 bgColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  if (hasCompileError) {
    bgColor = ImVec4(1.0f, 0.88f, 0.88f, 1.0f);
  }
  if (isSelected) {
    bgColor = hasCompileError ? ImVec4(1.0f, 0.82f, 0.7f, 1.0f)
                              : ImVec4(1.0f, 1.0f, 0.8f, 1.0f);
  }
  ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);

  if (ImGui::BeginChild("Rung", ImVec2(0, 50 * layout_scale), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f * layout_scale;
    float railWidth = 22.0f * layout_scale;
    float deleteButtonWidth =
        is_monitor_mode_ ? 0.0f : 40.0f * layout_scale;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;
    float cellWidth = cellAreaWidth / 12.0f;

    RenderVerticalConnectionsForRung(rungIndex, cellAreaWidth);

    ImGui::SetCursorPos(ImVec2(10 * layout_scale, 15 * layout_scale));
    char num[5];
    snprintf(num, 5, "%04d", ladder_program_.rungs[rungIndex].number);
    ImGui::TextUnformatted(num);
    if (hasCompileError) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1.0f), "!");
    }
    ImGui::SameLine(80 * layout_scale);
    ImGui::TextUnformatted("|");
    ImGui::SameLine(102 * layout_scale);

    for (int i = 0; i < 12; ++i) {
      RenderLadderCell(rungIndex, i, cellWidth);
      if (i < 11)
        ImGui::SameLine(0, 1 * layout_scale);
    }
    ImGui::SameLine(0, 5 * layout_scale);
    ImGui::TextUnformatted("|");
    if (!is_monitor_mode_) {
      ImGui::SameLine();
      if (ImGui::Button("X",
                        ImVec2(28 * layout_scale, 35 * layout_scale))) {
        pending_action_.type = PendingActionType::DELETE_RUNG;
        pending_action_.rungIndex = rungIndex;
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::Separator();
  ImGui::PopID();
}

void ProgrammingMode::RenderEndRung(int rungIndex) {
  const float layout_scale = GetLayoutScale();
  ImGui::PushID(rungIndex);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        (selected_rung_ == rungIndex)
                            ? ImVec4(1.0f, 1.0f, 0.8f, 1.0f)
                            : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));

  if (ImGui::BeginChild("EndRung", ImVec2(0, 50 * layout_scale), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f * layout_scale;
    float railWidth = 22.0f * layout_scale;
    float deleteButtonWidth =
        is_monitor_mode_ ? 0.0f : 40.0f * layout_scale;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetWindowPos();

    if (ImGui::InvisibleButton("##EndSelect", ImVec2(-1, -1))) {
      selected_rung_ = rungIndex;
      selected_cell_ = 0;
    }

    ImGui::SetCursorPos(ImVec2(10 * layout_scale, 15 * layout_scale));
    ImGui::TextUnformatted("END");

    ImGui::SetCursorPos(ImVec2(80 * layout_scale, 15 * layout_scale));
    ImGui::TextUnformatted("|");

    float line_y = p_min.y + 25.0f * layout_scale;
    float line_start_x = p_min.x + 102.0f * layout_scale;
    float line_end_x = line_start_x + cellAreaWidth;
    draw_list->AddLine(ImVec2(line_start_x, line_y), ImVec2(line_end_x, line_y),
                       IM_COL32(100, 100, 100, 255), 2.0f);

    const char* endText = "[END]";
    ImVec2 endTextSize = ImGui::CalcTextSize(endText);
    ImVec2 textPos =
        ImVec2(line_start_x + (cellAreaWidth - endTextSize.x) * 0.5f,
               line_y - endTextSize.y * 0.5f);

    draw_list->AddRectFilled(
        textPos, ImVec2(textPos.x + endTextSize.x, textPos.y + endTextSize.y),
        ImGui::GetColorU32(ImGuiCol_ChildBg));
    draw_list->AddText(textPos, IM_COL32_BLACK, endText);

    ImGui::SetCursorPos(ImVec2(line_end_x + 5 * layout_scale,
                               15 * layout_scale));
    ImGui::TextUnformatted("|");

    if (!is_monitor_mode_) {
      ImGui::SameLine();
      ImGui::Dummy(
          ImVec2(deleteButtonWidth, 35 * layout_scale));
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopID();
}

void ProgrammingMode::RenderLadderCell(int rungIndex, int cellIndex,
                                       float cellWidth) {
  const float layout_scale = GetLayoutScale();
  ImGui::PushID(cellIndex);
  const auto& instruction = ladder_program_.rungs[rungIndex].cells[cellIndex];
  bool isSelected =
      (selected_rung_ == rungIndex && selected_cell_ == cellIndex);

  ImGui::PushStyleColor(ImGuiCol_Button,
                        GetInstructionColor(isSelected, instruction.isActive));

  if (ImGui::Button("##cell",
                    ImVec2(cellWidth, 35 * layout_scale))) {
    selected_rung_ = rungIndex;
    selected_cell_ = cellIndex;
  }

  if (ImGui::IsItemVisible()) {
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    bool isTimerCounter = (instruction.type == LadderInstructionType::TON ||
                           instruction.type == LadderInstructionType::CTU ||
                           instruction.type == LadderInstructionType::RST_TMR_CTR);

    if (instruction.type == LadderInstructionType::HLINE) {
      float centerY = p_min.y + 17.5f * layout_scale;
      ImU32 lineColor = instruction.isActive ? IM_COL32(100, 100, 100, 255)
                                             : IM_COL32(50, 50, 50, 255);
      draw_list->AddLine(ImVec2(p_min.x, centerY), ImVec2(p_max.x, centerY),
                         lineColor, 2.0f);
    } else if (isTimerCounter) {
      const char* symbol = GetInstructionSymbol(instruction.type);
      float fontSize = ImGui::GetFontSize();
      float topY = p_min.y + 1.0f * layout_scale;
      float midY = p_min.y + (35.0f * layout_scale - fontSize) * 0.5f;
      float bottomY =
          p_min.y + 35.0f * layout_scale - fontSize - 1.0f * layout_scale;

      draw_list->AddText(ImVec2(p_min.x + 2.0f, topY), IM_COL32_BLACK, symbol);

      if (!instruction.address.empty()) {
        ImVec2 addr_size = ImGui::CalcTextSize(instruction.address.c_str());
        draw_list->AddText(
            ImVec2(p_min.x + (cellWidth - addr_size.x) * 0.5f, midY),
            IM_COL32(20, 20, 200, 255), instruction.address.c_str());
      }

      if (!instruction.preset.empty()) {
        ImVec2 preset_size = ImGui::CalcTextSize(instruction.preset.c_str());
        draw_list->AddText(
            ImVec2(p_min.x + (cellWidth - preset_size.x) * 0.5f, bottomY),
            IM_COL32(20, 20, 200, 255), instruction.preset.c_str());
      }
    } else {
      const char* symbol = GetInstructionSymbol(instruction.type);
      ImVec2 text_size = ImGui::CalcTextSize(symbol);
      draw_list->AddText(
          ImVec2(p_min.x + (cellWidth - text_size.x) * 0.5f,
                 p_min.y + (35 * layout_scale - text_size.y) * 0.5f),
                         IM_COL32_BLACK, symbol);
    }

    if (!isTimerCounter) {
      if (!instruction.address.empty()) {
        ImVec2 addr_size = ImGui::CalcTextSize(instruction.address.c_str());
        draw_list->AddText(
            ImVec2(p_min.x + (cellWidth - addr_size.x) * 0.5f,
                   p_min.y - 2 * layout_scale),
            IM_COL32(20, 20, 200, 255), instruction.address.c_str());
      }

      if (!instruction.preset.empty()) {
        ImVec2 preset_size = ImGui::CalcTextSize(instruction.preset.c_str());
        draw_list->AddText(
            ImVec2(p_min.x + (cellWidth - preset_size.x) * 0.5f,
                   p_min.y + 12 * layout_scale),
            IM_COL32(20, 20, 200, 255), instruction.preset.c_str());
      }
    }
  }
  ImGui::PopStyleColor();
  ImGui::PopID();
}

void ProgrammingMode::RenderDeviceMonitor() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
  if (ImGui::BeginChild("DeviceMonitor", ImVec2(0, -35 * layout_scale), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (!is_monitor_mode_) {
      RenderKeyboardHelp();
      ImGui::Separator();
      RenderCursorInfo();
    } else {
      std::vector<std::string> usedM, usedT, usedC;
      GetUsedDevices(usedM, usedT, usedC);

      if (!usedT.empty() || !usedC.empty()) {
        ImGui::Text("%s", TR("ui.programming.timers_counters", "Timers / Counters"));
        ImGui::Separator();

        for (const auto& addr : usedT) {
          auto it = timer_states_.find(addr);
          if (it == timer_states_.end())
            continue;
          const TimerState& timer = it->second;

          ImGui::PushID(addr.c_str());
          ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                ImVec4(0.92f, 0.92f, 0.98f, 1.0f));
          if (ImGui::BeginChild(addr.c_str(),
                                ImVec2(0, 80.0f * layout_scale), true,
                                ImGuiWindowFlags_NoScrollbar)) {
            int timerValueK = timer.value / 100;
            char value_buf[128] = {0};
            FormatString(value_buf, sizeof(value_buf),
                         "ui.programming.timer_value_fmt", "%s  K%d/%d",
                         addr.c_str(), timerValueK, timer.preset);
            ImGui::TextUnformatted(value_buf);

            float progress =
                (timer.preset > 0) ? static_cast<float>(timer.value) / (timer.preset * 100) : 0.0f;
            if (progress > 1.0f)
              progress = 1.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
          }
          ImGui::EndChild();
          ImGui::PopStyleColor();
          ImGui::PopID();
          ImGui::Spacing();
        }

        for (const auto& addr : usedC) {
          auto it = counter_states_.find(addr);
          if (it == counter_states_.end())
            continue;
          const CounterState& counter = it->second;

          ImGui::PushID(addr.c_str());
          ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
          if (ImGui::BeginChild(addr.c_str(),
                                ImVec2(0, 80.0f * layout_scale), true,
                                ImGuiWindowFlags_NoScrollbar)) {
            char value_buf[128] = {0};
            FormatString(value_buf, sizeof(value_buf),
                         "ui.programming.counter_value_fmt", "%s  K%d/%d",
                         addr.c_str(), counter.value, counter.preset);
            ImGui::TextUnformatted(value_buf);

            float progress = (counter.preset > 0)
                                 ? static_cast<float>(counter.value) / counter.preset
                                 : 0.0f;
            if (progress > 1.0f)
              progress = 1.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
          }
          ImGui::EndChild();
          ImGui::PopStyleColor();
          ImGui::PopID();
          ImGui::Spacing();
        }
        ImGui::Spacing();
      }

      if (!usedM.empty()) {
        ImGui::Text("%s", TR("ui.programming.memory_m", "Memory (M)"));
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(5 * layout_scale, 5 * layout_scale));
        for (size_t i = 0; i < usedM.size(); ++i) {
          const std::string& address = usedM[i];
          bool state = GetDeviceState(address);
          ImGui::PushStyleColor(ImGuiCol_Button,
                                state ? ImVec4(0.8f, 0.6f, 0.2f, 1.0f)
                                      : ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
          ImGui::Button(address.c_str(),
                        ImVec2(55 * layout_scale, 35 * layout_scale));
          ImGui::PopStyleColor();
          if ((i + 1) % 4 != 0 && i + 1 < usedM.size())
            ImGui::SameLine();
        }
        ImGui::PopStyleVar();
        ImGui::Spacing();
      }

      RenderSimulationControl();
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}
void ProgrammingMode::RenderKeyboardHelp() {
  ImGui::Text("%s", TR("ui.programming.shortcuts", "Keyboard Shortcuts"));
  ImGui::BulletText("F5: XIC (NO)");
  ImGui::BulletText("F6: XIO (NC)");
  ImGui::BulletText("F7: Coil (OUT)");
  ImGui::BulletText("F9: Add vertical");
  ImGui::BulletText("Shift+F9: Remove vertical");
  ImGui::BulletText("Del: Delete");
  ImGui::BulletText("Insert: Add");
  ImGui::BulletText("Ctrl+Arrow: Toggle line path");
  ImGui::BulletText("Ctrl+Z: Undo");
  ImGui::BulletText("Ctrl+Y / Ctrl+Shift+Z: Redo");
}

void ProgrammingMode::RenderCursorInfo() {
  ImGui::Text("%s", TR("ui.programming.cursor_info", "Cursor Info"));
  char cursor_buf[128] = {0};
  FormatString(cursor_buf, sizeof(cursor_buf),
               "ui.programming.cursor_pos_fmt", "Rung %04d, Cell: %d",
               selected_rung_, selected_cell_);
  ImGui::TextUnformatted(cursor_buf);
}

void ProgrammingMode::RenderSimulationControl() {
  const float layout_scale = GetLayoutScale();
  ImGui::Text("%s", TR("ui.programming.input_controls", "Input Controls"));
  ImGui::Text("%s", TR("ui.programming.inputs_label", "Inputs (X0-X15)"));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(5 * layout_scale, 5 * layout_scale));
  for (int i = 0; i < 16; i++) {
    std::string address = "X" + std::to_string(i);
    bool state = GetDeviceState(address);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          state ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                                : ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    if (ImGui::Button(address.c_str(),
                      ImVec2(55 * layout_scale, 35 * layout_scale))) {
      SetDeviceState(address, !state);
    }
    ImGui::PopStyleColor();
    if ((i + 1) % 4 != 0)
      ImGui::SameLine();
  }
  ImGui::PopStyleVar();
  ImGui::Spacing();
  ImGui::Text("%s", TR("ui.programming.outputs_label", "Outputs (Y0-Y15)"));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(5 * layout_scale, 5 * layout_scale));
  for (int i = 0; i < 16; i++) {
    std::string address = "Y" + std::to_string(i);
    bool state = GetDeviceState(address);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          state ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
                                : ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Button(address.c_str(),
                  ImVec2(55 * layout_scale, 35 * layout_scale));
    ImGui::PopStyleColor();
    if ((i + 1) % 4 != 0)
      ImGui::SameLine();
  }
  ImGui::PopStyleVar();
}

void ProgrammingMode::RenderStatusBar(bool isPlcRunning) {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  if (ImGui::BeginChild("StatusBar", ImVec2(0, 0), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPosY(5 * layout_scale);
    ImGui::Columns(5, "StatusColumns", false);

    char status_buf[128] = {0};
    FormatString(status_buf, sizeof(status_buf),
                 "ui.programming.mode_fmt", "Mode: %s",
                 is_monitor_mode_
                     ? TR("ui.programming.mode_monitor", "Monitor")
                     : TR("ui.programming.mode_edit", "Edit"));
    ImGui::TextUnformatted(status_buf);
    ImGui::NextColumn();

    if (!is_monitor_mode_) {
      FormatString(status_buf, sizeof(status_buf),
                   "ui.programming.cursor_fmt", "Cursor: %04d: X%d",
                   selected_rung_, selected_cell_);
      ImGui::TextUnformatted(status_buf);
    } else {
      FormatString(status_buf, sizeof(status_buf),
                   "ui.programming.steps_fmt", "Steps: %zu",
                   ladder_program_.rungs.size() > 0
                       ? ladder_program_.rungs.size() - 1
                       : 0);
      ImGui::TextUnformatted(status_buf);
    }
    ImGui::NextColumn();

      if (!ladder_program_.verticalConnections.empty()) {
          const char* prefix =
              TR("ui.programming.vertical_prefix", "Vertical: ");
          const char* separator = TR("ui.programming.vertical_sep", ", ");
          std::string verticalInfo = prefix;
          for (size_t i = 0; i < ladder_program_.verticalConnections.size();
               i++) {
            const auto& conn = ladder_program_.verticalConnections[i];
          if (i > 0) {
            verticalInfo += separator;
          }
            char connInfo[64] = {0};
            FormatString(connInfo, sizeof(connInfo),
                         "ui.programming.vertical_item_fmt",
                         "X%d (R%04d-%04d)",
                         conn.x, conn.startRung(), conn.endRung());
            verticalInfo += connInfo;
          }
        ImGui::Text("%s", verticalInfo.c_str());
    } else {
      ImGui::Text("%s", TR("ui.programming.no_vertical", "No vertical connections"));
    }
    ImGui::NextColumn();

    if (compile_failed_) {
      ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1.0f), "%s", TR("ui.programming.compile_fail", "Compile: FAIL"));
      if (ImGui::IsItemHovered() && !last_compile_error_.empty()) {
        ImGui::SetTooltip("%s", last_compile_error_.c_str());
      }
    } else {
      ImGui::Text("%s", TR("ui.programming.compile_ok", "Compile: OK"));
    }
    ImGui::NextColumn();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() -
                         100 * layout_scale);
    FormatString(status_buf, sizeof(status_buf),
                 "ui.programming.plc_status_fmt", "PLC: %s",
                 isPlcRunning ? TR("ui.common.run", "RUN")
                              : TR("ui.common.stop", "STOP"));
    ImGui::TextUnformatted(status_buf);

    ImGui::Columns(1);
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

void ProgrammingMode::RenderAddressPopup() {
  if (show_address_popup_ && !ImGui::IsPopupOpen("Address Input")) {
    ImGui::OpenPopup("Address Input");
  }

  if (ImGui::BeginPopupModal("Address Input", &show_address_popup_,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    const float layout_scale = GetLayoutScale();
    char prompt_buf[128] = {0};
    FormatString(prompt_buf, sizeof(prompt_buf),
                 "ui.programming.enter_device_fmt",
                 "Enter device address for %s:",
                 GetInstructionSymbol(pending_instruction_type_));
    ImGui::TextUnformatted(prompt_buf);
    ImGui::Spacing();

    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }

    if (ImGui::InputText("Address", temp_address_buffer_,
                         sizeof(temp_address_buffer_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      ConfirmInstruction();
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }

    if (pending_instruction_type_ == LadderInstructionType::OTE) {
      ImGui::TextDisabled("%s", TR("ui.programming.device_example", "Example: Y0, T1 K10, C2 K5, SET M0, RST Y0"));
    }

    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120 * layout_scale, 0))) {
      ConfirmInstruction();
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120 * layout_scale, 0))) {
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }
    ImGui::EndPopup();
  }
}
void ProgrammingMode::RenderVerticalDialog() {
  if (show_vertical_dialog_ && !ImGui::IsPopupOpen("Vertical Connection")) {
    ImGui::OpenPopup("Vertical Connection");
  }

  if (ImGui::BeginPopupModal("Vertical Connection", &show_vertical_dialog_,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    const float layout_scale = GetLayoutScale();
    char vertical_buf[128] = {0};
    FormatString(vertical_buf, sizeof(vertical_buf),
                 "ui.programming.vertical_conn_fmt",
                 "Vertical connection (X%d to X%d)",
                 selected_cell_ - 1, selected_cell_);
    ImGui::TextUnformatted(vertical_buf);
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
    if (ImGui::BeginChild("PositionInfo",
                          ImVec2(350 * layout_scale, 60 * layout_scale),
                          true)) {
      FormatString(vertical_buf, sizeof(vertical_buf),
                   "ui.programming.start_rung_fmt", "Start rung: %04d",
                   selected_rung_);
      ImGui::TextUnformatted(vertical_buf);
      FormatString(vertical_buf, sizeof(vertical_buf),
                   "ui.programming.vertical_conn_fmt",
                   "Vertical connection (X%d to X%d)",
                   selected_cell_ - 1, selected_cell_);
      ImGui::TextUnformatted(vertical_buf);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("%s", TR("ui.programming.vertical_info", "Vertical connection info"));

    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }

    int maxLines = (ladder_program_.rungs.size() - 2) - selected_rung_;

    if (ImGui::InputInt("##linecount", &vertical_line_count_)) {
      if (vertical_line_count_ < 1)
        vertical_line_count_ = 1;
      if (vertical_line_count_ > maxLines)
        vertical_line_count_ = maxLines;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
      ConfirmVerticalConnection();
      ImGui::CloseCurrentPopup();
      show_vertical_dialog_ = false;
    }

    ImGui::Spacing();
    FormatString(vertical_buf, sizeof(vertical_buf),
                 "ui.programming.end_rung_fmt", "End rung: %04d",
                 selected_rung_ + vertical_line_count_);
    ImGui::TextUnformatted(vertical_buf);

    ImGui::Spacing();
    if (ImGui::Button("Confirm", ImVec2(120 * layout_scale, 0))) {
      ConfirmVerticalConnection();
      ImGui::CloseCurrentPopup();
      show_vertical_dialog_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120 * layout_scale, 0))) {
      ImGui::CloseCurrentPopup();
      show_vertical_dialog_ = false;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      ImGui::CloseCurrentPopup();
      show_vertical_dialog_ = false;
    }

    ImGui::EndPopup();
  }
}

const char* ProgrammingMode::GetInstructionSymbol(
    LadderInstructionType type) const {
  switch (type) {
    case LadderInstructionType::XIC:
      return "LD";
    case LadderInstructionType::XIO:
      return "LDN";
    case LadderInstructionType::OTE:
      return "( )";
    case LadderInstructionType::HLINE:
      return "---";
    case LadderInstructionType::SET:
      return "(S)";
    case LadderInstructionType::RST:
      return "(R)";
    case LadderInstructionType::TON:
      return "[TON]";
    case LadderInstructionType::CTU:
      return "[CTU]";
    case LadderInstructionType::RST_TMR_CTR:
      return "[RST]";
    default:
      return "";
  }
}
ImVec4 ProgrammingMode::GetInstructionColor(bool isSelected,
                                            bool isActive) const {
  if (isSelected && !is_monitor_mode_)
    return ImVec4(1.0f, 1.0f, 0.6f, 1.0f);
  if (is_monitor_mode_ && isActive)
    return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
  return ImVec4(1.0f, 1.0f, 1.0f, 0.0f);
}

}  // namespace plc


