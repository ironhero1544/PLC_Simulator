// programming_mode_ui.cpp
// Copyright 2024 PLC Emulator Project
//
// Programming mode UI rendering.

#include "plc_emulator/core/application.h"  // SaveProject ???????轅붽???????
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
  // Application ??????????�롫?�鴉?��????????곸궔?????�롫?�揶?�???�롫?�繹�?��????????�롫�??????????
}

void ProgrammingMode::RenderProgrammingToolbar(bool isPlcRunning) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
  if (ImGui::BeginChild("ProgrammingToolbar", ImVec2(0, 50), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPos(ImVec2(20, 10));

    if (isPlcRunning) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button("Edit (F3)", ImVec2(100, 30));
      ImGui::PopStyleVar();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            !is_monitor_mode_
                                ? ImVec4(0.4f, 0.6f, 0.8f, 1.0f)
                                : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
      if (ImGui::Button("Edit (F3)", ImVec2(100, 30))) {
        is_monitor_mode_ = false;
      }
      ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,
                          is_monitor_mode_ ? ImVec4(0.4f, 0.6f, 0.8f, 1.0f)
                                          : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::Button("Monitor (F2)", ImVec2(100, 30))) {
      is_monitor_mode_ = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    if (isPlcRunning || is_monitor_mode_) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button("Save", ImVec2(100, 30));
      ImGui::PopStyleVar();
    } else {
      if (ImGui::Button("Save", ImVec2(100, 30))) {

        // ???**NEW**: .csv ??????�롫????�?λ????????�롫???????????�롫????????
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

            // ?????????�롫????�?λ????????�롫???????????�롫????????(XML + ZIP ??轅붽????????�롫???
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
          // ?????�롫�????????????�롫?????�롫??????�롫�???????????
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
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void ProgrammingMode::RenderProgrammingMainArea() {
  ImGui::Columns(2, "MainArea", false);
  float windowWidth = ImGui::GetWindowWidth();
  float rightPanelWidth = 320.0f;
  ImGui::SetColumnWidth(0, windowWidth - rightPanelWidth);
  RenderLadderEditor();
  ImGui::NextColumn();
  RenderDeviceMonitor();
  ImGui::Columns(1);
}

void ProgrammingMode::RenderLadderEditor() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

  float availableHeight = ImGui::GetContentRegionAvail().y - 35;
  if (ImGui::BeginChild("LadderEditor", ImVec2(0, availableHeight), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    RenderColumnHeader();
    RenderLadderDiagram();
    // ??轅붽??????�띿�????????????????? ??????�멥?�揶?????????�늉?????????�롫????�?????????�롫????壤굿??戮㏐�??????�롫???????????�롫????�쐻???
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}

void ProgrammingMode::RenderColumnHeader() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
  if (ImGui::BeginChild("ColumnHeader", ImVec2(0, 50), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f;
    float railWidth = 22.0f;
    float deleteButtonWidth = is_monitor_mode_ ? 0.0f : 40.0f;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;
    float cellWidth = cellAreaWidth / 12.0f;

    ImGui::Dummy(ImVec2(rungNumberWidth, 0));
    ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(railWidth, 0));
    ImGui::SameLine(0, 0);

    for (int i = 0; i < 12; i++) {
      ImGui::BeginGroup();
      ImGui::Dummy(ImVec2(cellWidth, 35));
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
  // [DEPRECATED] ??????????????????????? ????????????�롫???
  // ??轅붽??????�띿�????????????????? ??????�롫??????????�멥?�揶?????????�늉?????????�롫????�?????????�롫????壤굿??戮㏐�????꿔꺂??????
}

// [PPT: ?????????????�롫???�뼹??1 - ????�멥?�揶?????轅붽??????�띿�?????????????????
// ??????�멥?�揶?????????????�롫?????�뉩??????轅붽??????�띿�????????????????�롫????�????�롫�???�쐻??????�롫?????????轅붽????�쐻????�뒩?????????�롫????? ????????????????????
// ????�롫????????꿔꺂??????
void ProgrammingMode::RenderVerticalConnectionsForRung(int rungIndex,
                                                       float cellAreaWidth) {
  if (ladder_program_.verticalConnections.empty())
    return;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 windowPos = ImGui::GetWindowPos();
  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();

  float rungNumberWidth = 80.0f;
  float railWidth = 22.0f;

  // [PPT: ???꿔꺂????????????�롫???�뼹?? for???????�롫�?????????꿔꺂??????�롫�????????�롫????????�멥?�揶????????곸궔????????�롫?????轅붽??????�띿�?????????�롫?�獒??????�롫??????????????�롫???
  for (const auto& connection : ladder_program_.verticalConnections) {
    // ??????�롫????????�멥?�揶???????轅붽??????�띿�?????????�롫?�獒???????????�롫?????�뉩????γ?볥?�???? ??轅붽?????????
    bool isThisRungInConnection = false;
    for (int connectedRung : connection.rungs) {
      if (connectedRung == rungIndex) {
        isThisRungInConnection = true;
        break;
      }
    }

    if (!isThisRungInConnection)
      continue;

    // [PPT: ?????????????�롫???�뼹??2 - React ??????�롫????�?λ???????????�롫???????????�쿋?????? ?????믩틲?????�?��??????�롫�???
    // ??????????? ?????믩틲??????�롫�???????????꿔꺂??????�롫�????轅붽????�쐻????�뒩?????轅붽??????�띿�?????????�롫???????�?��??????�롫�?????꿔꺂??????
    float lineX = windowPos.x + contentMin.x + rungNumberWidth + railWidth +
                  (connection.x / 12.0f) * cellAreaWidth;

    float rungCenterY = windowPos.y + contentMin.y + 25.0f;  // ???????밸쐻????�롫????Y ??????�롫????

    // [PPT: ???꿔꺂????????????�롫???�뼹?? if-else ????�롫??????????�????�????�?��???뤸납????�??룸뜤?????�롫?????????�뇡???????�롫?????????�뇡??????????�롫???????μ?�媛?????�롫??????棺堉?�??????꿔꺂??????
    ImU32 lineColor = is_monitor_mode_ ? IM_COL32(107, 114, 128, 255)
                                      :                  // React??#6B7280
                          IM_COL32(156, 163, 175, 255);  // React??#9CA3AF

    // ??????�롫????????�멥?�揶?????꿔꺂??????�롫?????轅붽??????�띿�??????????????????�롫??????????�롫�???�쐻?
    // ??????�롫???????????????????꿔꺂??????�롫�????????�롫?????�뉩????γ?볥?�???? ??轅붽?????????
    std::vector<int> sortedRungs = connection.rungs;
    std::sort(sortedRungs.begin(), sortedRungs.end());

    int minRung = sortedRungs.front();
    int maxRung = sortedRungs.back();

    float lineStartY = rungCenterY;
    float lineEndY = rungCenterY;

    // ??????�롫?�筌???�롫????????�롫�?????????????????�롫?????? ??轅붽?????????
    if (rungIndex > minRung) {
      lineStartY = windowPos.y + contentMin.y - 2.5f;  // ????????�롫?�筌???�롫?????
    }

    // ??????�롫???????�롫???????�롫?�筌???????????????????�롫?????? ??轅붽?????????
    if (rungIndex < maxRung) {
      lineEndY =
          windowPos.y + contentMin.y + 52.5f;  // ????????�롫???????�롫?????(50px + 2.5px)
    }

    // ??轅붽??????�띿�????????�롫??????????�롫�???�쐻?
    draw_list->AddLine(ImVec2(lineX, lineStartY), ImVec2(lineX, lineEndY),
                       lineColor, 2.0f);

    // ??????�롫????????�멥?�揶????????밸쐻????�롫???????????�롫?�獒?????????
    draw_list->AddCircleFilled(ImVec2(lineX, rungCenterY), 2.0f, lineColor);
  }
}

void ProgrammingMode::RenderRung(int rungIndex) {
  ImGui::PushID(rungIndex);
  if (selected_rung_ == rungIndex)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 0.8f, 1.0f));
  else
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

  if (ImGui::BeginChild("Rung", ImVec2(0, 50), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f;
    float railWidth = 22.0f;
    float deleteButtonWidth = is_monitor_mode_ ? 0.0f : 40.0f;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;
    float cellWidth = cellAreaWidth / 12.0f;

    // [PPT: ?????????????�롫???�뼹??1 - ????�멥?�揶?????轅붽??????�띿�??????????
    // ??????�멥?�揶?????????????�롫?????�뉩??????轅붽??????�띿�????????????????�롫????�????????�늉?????????�롫????�???????????�롫?????????轅붽????�쐻????�뒩?????????�롫????? ????�싲�?��??�엺?????
    // ????�롫????????꿔꺂??????
    RenderVerticalConnectionsForRung(rungIndex, cellAreaWidth);

    ImGui::SetCursorPos(ImVec2(10, 15));
    char num[5];
    snprintf(num, 5, "%04d", ladder_program_.rungs[rungIndex].number);
    ImGui::TextUnformatted(num);
    ImGui::SameLine(80);
    ImGui::TextUnformatted("|");
    ImGui::SameLine(102);

    for (int i = 0; i < 12; ++i) {
      RenderLadderCell(rungIndex, i, cellWidth);
      if (i < 11)
        ImGui::SameLine(0, 1);
    }
    ImGui::SameLine(0, 5);
    ImGui::TextUnformatted("|");
    if (!is_monitor_mode_) {
      ImGui::SameLine();
      if (ImGui::Button("X", ImVec2(28, 35))) {
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
  ImGui::PushID(rungIndex);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        (selected_rung_ == rungIndex)
                            ? ImVec4(1.0f, 1.0f, 0.8f, 1.0f)
                            : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));

  if (ImGui::BeginChild("EndRung", ImVec2(0, 50), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f;
    float railWidth = 22.0f;
    float deleteButtonWidth = is_monitor_mode_ ? 0.0f : 40.0f;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetWindowPos();

    if (ImGui::InvisibleButton("##EndSelect", ImVec2(-1, -1))) {
      selected_rung_ = rungIndex;
      selected_cell_ = 0;
    }

    ImGui::SetCursorPos(ImVec2(10, 15));
    ImGui::TextUnformatted("END");

    ImGui::SetCursorPos(ImVec2(80, 15));
    ImGui::TextUnformatted("|");

    float line_y = p_min.y + 25.0f;
    float line_start_x = p_min.x + 102.0f;
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

    ImGui::SetCursorPos(ImVec2(line_end_x + 5, 15));
    ImGui::TextUnformatted("|");

    if (!is_monitor_mode_) {
      ImGui::SameLine();
      ImGui::Dummy(ImVec2(deleteButtonWidth, 35));
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopID();
}

void ProgrammingMode::RenderLadderCell(int rungIndex, int cellIndex,
                                       float cellWidth) {
  ImGui::PushID(cellIndex);
  const auto& instruction = ladder_program_.rungs[rungIndex].cells[cellIndex];
  bool isSelected =
      (selected_rung_ == rungIndex && selected_cell_ == cellIndex);

  ImGui::PushStyleColor(ImGuiCol_Button,
                        GetInstructionColor(isSelected, instruction.isActive));

  if (ImGui::Button("##cell", ImVec2(cellWidth, 35))) {
    selected_rung_ = rungIndex;
    selected_cell_ = cellIndex;
  }

  if (ImGui::IsItemVisible()) {
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (instruction.type == LadderInstructionType::HLINE) {
      float centerY = p_min.y + 17.5f;
      ImU32 lineColor = instruction.isActive ? IM_COL32(100, 100, 100, 255)
                                             : IM_COL32(50, 50, 50, 255);
      draw_list->AddLine(ImVec2(p_min.x, centerY), ImVec2(p_max.x, centerY),
                         lineColor, 2.0f);
    } else {
      const char* symbol = GetInstructionSymbol(instruction.type);
      ImVec2 text_size = ImGui::CalcTextSize(symbol);
      draw_list->AddText(ImVec2(p_min.x + (cellWidth - text_size.x) * 0.5f,
                                p_min.y + (35 - text_size.y) * 0.5f),
                         IM_COL32_BLACK, symbol);
    }

    if (!instruction.address.empty()) {
      ImVec2 addr_size = ImGui::CalcTextSize(instruction.address.c_str());
      draw_list->AddText(
          ImVec2(p_min.x + (cellWidth - addr_size.x) * 0.5f, p_min.y - 2),
          IM_COL32(20, 20, 200, 255), instruction.address.c_str());
    }

    if (!instruction.preset.empty()) {
      ImVec2 preset_size = ImGui::CalcTextSize(instruction.preset.c_str());
      draw_list->AddText(
          ImVec2(p_min.x + (cellWidth - preset_size.x) * 0.5f, p_min.y + 12),
          IM_COL32(20, 20, 200, 255), instruction.preset.c_str());
    }
  }
  ImGui::PopStyleColor();
  ImGui::PopID();
}

void ProgrammingMode::RenderDeviceMonitor() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
  if (ImGui::BeginChild("DeviceMonitor", ImVec2(0, -35), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (!is_monitor_mode_) {
      RenderKeyboardHelp();
      ImGui::Separator();
      RenderCursorInfo();
    } else {
      std::vector<std::string> usedM, usedT, usedC;
      GetUsedDevices(usedM, usedT, usedC);

      if (!usedT.empty() || !usedC.empty()) {
        ImGui::Text("Timers / Counters");
        ImGui::Separator();

        for (const auto& addr : usedT) {
          auto it = timer_states_.find(addr);
          if (it == timer_states_.end())
            continue;
          const TimerState& timer = it->second;

          ImGui::PushID(addr.c_str());
          ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                ImVec4(0.92f, 0.92f, 0.98f, 1.0f));
          if (ImGui::BeginChild(addr.c_str(), ImVec2(0, 0), true,
                                ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoScrollbar)) {
            ImGui::Columns(2, nullptr, false);
            float buttonColumnWidth = 90.0f;
            ImGui::SetColumnWidth(
                0, ImGui::GetContentRegionAvail().x - buttonColumnWidth);

            ImGui::Text("%s", addr.c_str());
            ImGui::Text("Timer: %d / Preset: %d", timer.value, timer.preset);

            ImGui::NextColumn();
            const char* statusText =
                timer.done ? "DONE" : (timer.enabled ? "RUN" : "STOP");
            ImVec4 statusColor =
                timer.done ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f)
                           : (timer.enabled ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f)
                                            : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, statusColor);
            ImGui::Button(statusText, ImVec2(-1, 40.0f));
            ImGui::PopStyleColor();
            ImGui::Columns(1);

            float progress =
                (timer.preset > 0) ? static_cast<float>(timer.value) / timer.preset : 0.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
          }
          ImGui::EndChild();
          ImGui::PopStyleColor();
          ImGui::PopID();
        }

        for (const auto& addr : usedC) {
          auto it = counter_states_.find(addr);
          if (it == counter_states_.end())
            continue;
          const CounterState& counter = it->second;

          ImGui::PushID(addr.c_str());
          ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
          if (ImGui::BeginChild(addr.c_str(), ImVec2(0, 0), true,
                                ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoScrollbar)) {
            ImGui::Columns(2, nullptr, false);
            float buttonColumnWidth = 90.0f;
            ImGui::SetColumnWidth(
                0, ImGui::GetContentRegionAvail().x - buttonColumnWidth);

            ImGui::Text("%s", addr.c_str());
            ImGui::Text("Counter: %d / Preset: %d", counter.value, counter.preset);

            ImGui::NextColumn();
            const char* statusText = counter.done ? "DONE" : "COUNTING";
            ImVec4 statusColor = counter.done ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f)
                                              : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, statusColor);
            ImGui::Button(statusText, ImVec2(-1, 40.0f));
            ImGui::PopStyleColor();
            ImGui::Columns(1);

            float progress = (counter.preset > 0)
                                 ? static_cast<float>(counter.value) / counter.preset
                                 : 0.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
          }
          ImGui::EndChild();
          ImGui::PopStyleColor();
          ImGui::PopID();
        }
        ImGui::Spacing();
      }

      if (!usedM.empty()) {
        ImGui::Text("Memory (M)");
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
        for (size_t i = 0; i < usedM.size(); ++i) {
          const std::string& address = usedM[i];
          bool state = GetDeviceState(address);
          ImGui::PushStyleColor(ImGuiCol_Button,
                                state ? ImVec4(0.8f, 0.6f, 0.2f, 1.0f)
                                      : ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
          ImGui::Button(address.c_str(), ImVec2(55, 35));
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
  ImGui::Text("Keyboard Shortcuts");
  ImGui::BulletText("F5: XIC (NO)");
  ImGui::BulletText("F6: XIO (NC)");
  ImGui::BulletText("F7: Coil (OUT)");
  ImGui::BulletText("F9: Add vertical");
  ImGui::BulletText("Shift+F9: Remove vertical");
  ImGui::BulletText("Del: Delete");
  ImGui::BulletText("Insert: Add");
}

void ProgrammingMode::RenderCursorInfo() {
  ImGui::Text("Cursor Info");
  ImGui::Text("Rung %04d, Cell: %d", selected_rung_, selected_cell_);
}

void ProgrammingMode::RenderSimulationControl() {
  ImGui::Text("Input Controls");
  ImGui::Text("Inputs (X0-X15)");
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
  for (int i = 0; i < 16; i++) {
    std::string address = "X" + std::to_string(i);
    bool state = GetDeviceState(address);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          state ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                                : ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    if (ImGui::Button(address.c_str(), ImVec2(55, 35))) {
      SetDeviceState(address, !state);
    }
    ImGui::PopStyleColor();
    if ((i + 1) % 4 != 0)
      ImGui::SameLine();
  }
  ImGui::PopStyleVar();
  ImGui::Spacing();
  ImGui::Text("Outputs (Y0-Y15)");
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
  for (int i = 0; i < 16; i++) {
    std::string address = "Y" + std::to_string(i);
    bool state = GetDeviceState(address);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          state ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
                                : ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Button(address.c_str(), ImVec2(55, 35));
    ImGui::PopStyleColor();
    if ((i + 1) % 4 != 0)
      ImGui::SameLine();
  }
  ImGui::PopStyleVar();
}

void ProgrammingMode::RenderStatusBar(bool isPlcRunning) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  if (ImGui::BeginChild("StatusBar", ImVec2(0, 0), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPosY(5);
    ImGui::Columns(4, "StatusColumns", false);

    ImGui::Text("Mode: %s", is_monitor_mode_ ? "Monitor" : "Edit");
    ImGui::NextColumn();

    if (!is_monitor_mode_) {
      ImGui::Text("Cursor: %04d: X%d", selected_rung_, selected_cell_);
    } else {
      ImGui::Text("Steps: %zu", ladder_program_.rungs.size() > 0
                                   ? ladder_program_.rungs.size() - 1
                                   : 0);
    }
    ImGui::NextColumn();

    if (!ladder_program_.verticalConnections.empty()) {
      std::string verticalInfo = "Vertical: ";
      for (size_t i = 0; i < ladder_program_.verticalConnections.size(); i++) {
        const auto& conn = ladder_program_.verticalConnections[i];
        if (i > 0)
          verticalInfo += ", ";
        char connInfo[50];
        snprintf(connInfo, 50, "X%d (R%04d-%04d)", conn.x, conn.startRung(), conn.endRung());

        verticalInfo += connInfo;
      }
      ImGui::Text("%s", verticalInfo.c_str());
    } else {
      ImGui::Text("No vertical connections");
    }
    ImGui::NextColumn();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() -
                         100);
    ImGui::Text("PLC: %s", isPlcRunning ? "RUN" : "STOP");

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
    ImGui::Text("Enter device address for %s:",
                GetInstructionSymbol(pending_instruction_type_));
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

    if (pending_instruction_type_ == LadderInstructionType::OTE) {
      ImGui::TextDisabled("Example: Y0, T1 K10, C2 K5, SET M0, RST Y0");
    }

    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ConfirmInstruction();
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
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
    ImGui::Text("Vertical connection (X%d to X%d)", selected_cell_ - 1, selected_cell_);
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
    if (ImGui::BeginChild("PositionInfo", ImVec2(350, 60), true)) {
      ImGui::Text("Start rung: %04d", selected_rung_);
    ImGui::Text("Vertical connection (X%d to X%d)", selected_cell_ - 1, selected_cell_);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("Vertical connection info");

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
    ImGui::Text("End rung: %04d", selected_rung_ + vertical_line_count_);

    ImGui::Spacing();
    if (ImGui::Button("Confirm", ImVec2(120, 0))) {
      ConfirmVerticalConnection();
      ImGui::CloseCurrentPopup();
      show_vertical_dialog_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
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
