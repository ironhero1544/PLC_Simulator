// programming_mode_ui.cpp
//
// Programming mode UI rendering.

#include "plc_emulator/core/application.h"  // SaveProject ?????????????????
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/programming/programming_mode.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace plc {
namespace {

constexpr float kLadderRungHeight = 50.0f;
constexpr float kLadderCellHeight = 50.0f;
constexpr int kLadderColumnCount = 12;
constexpr float kRailSeamOverlap = 1.0f;

ImU32 GetLadderRailColor(bool is_monitor_mode) {
  return is_monitor_mode ? IM_COL32(58, 64, 76, 255)
                         : IM_COL32(72, 72, 72, 255);
}

ImU32 GetVerticalConnectionColor(bool is_monitor_mode) {
  (void)is_monitor_mode;
  return IM_COL32(50, 50, 50, 255);
}

ImU32 GetLadderGridColor(bool is_monitor_mode) {
  return is_monitor_mode ? IM_COL32(124, 132, 146, 255)
                         : IM_COL32(146, 146, 146, 255);
}

void DrawContinuousGrid(ImDrawList* draw_list, float start_x, float top_y,
                        float cell_width, float cell_height, int columns,
                        ImU32 color, float thickness) {
  if (!draw_list || columns <= 0 || cell_width <= 0.0f || cell_height <= 0.0f) {
    return;
  }
  const float end_x = start_x + cell_width * static_cast<float>(columns);
  const float bottom_y = top_y + cell_height;
  draw_list->AddLine(ImVec2(start_x, top_y), ImVec2(end_x, top_y), color,
                     thickness);
  draw_list->AddLine(ImVec2(start_x, bottom_y), ImVec2(end_x, bottom_y), color,
                     thickness);
  for (int i = 0; i <= columns; ++i) {
    const float x = start_x + cell_width * static_cast<float>(i);
    draw_list->AddLine(ImVec2(x, top_y), ImVec2(x, bottom_y), color,
                       thickness);
  }
}

void DrawVerticalRailLine(ImDrawList* draw_list, float x, float top_y,
                          float bottom_y, ImU32 color, float thickness) {
  if (!draw_list || bottom_y <= top_y || thickness <= 0.0f) {
    return;
  }
  draw_list->AddLine(ImVec2(x, top_y), ImVec2(x, bottom_y), color, thickness);
}

void DrawLadderContactSymbol(ImDrawList* draw_list,
                             ImVec2 p_min,
                             ImVec2 p_max,
                             LadderInstructionType type,
                             ImU32 color,
                             ImU32 slash_color,
                             float thickness) {
  if (!draw_list || (type != LadderInstructionType::XIC &&
                     type != LadderInstructionType::XIO)) {
    return;
  }
  const float w = p_max.x - p_min.x;
  const float h = p_max.y - p_min.y;
  if (w <= 1.0f || h <= 1.0f || thickness <= 0.0f) {
    return;
  }

  const float center_y = p_min.y + h * 0.5f;
  const float left_bar_x = p_min.x + w * 0.44f;
  const float right_bar_x = p_min.x + w * 0.56f;
  const float bar_half_h = h * 0.20f;
  const float top_y = center_y - bar_half_h;
  const float bottom_y = center_y + bar_half_h;

  draw_list->AddLine(ImVec2(p_min.x, center_y), ImVec2(left_bar_x, center_y),
                     color, thickness);
  draw_list->AddLine(ImVec2(right_bar_x, center_y), ImVec2(p_max.x, center_y),
                     color, thickness);
  draw_list->AddLine(ImVec2(left_bar_x, top_y), ImVec2(left_bar_x, bottom_y),
                     color, thickness);
  draw_list->AddLine(ImVec2(right_bar_x, top_y), ImVec2(right_bar_x, bottom_y),
                     color, thickness);

  if (type == LadderInstructionType::XIO) {
    const float slash_pad = w * 0.09f;
    draw_list->AddLine(ImVec2(left_bar_x - slash_pad, bottom_y),
                       ImVec2(right_bar_x + slash_pad, top_y), slash_color,
                       thickness);
  }
}

void DrawOutputCoilSymbol(ImDrawList* draw_list,
                          ImVec2 p_min,
                          ImVec2 p_max,
                          ImU32 color,
                          float thickness) {
  if (!draw_list || thickness <= 0.0f) {
    return;
  }

  const float w = p_max.x - p_min.x;
  const float h = p_max.y - p_min.y;
  if (w <= 1.0f || h <= 1.0f) {
    return;
  }

  const float cy = p_min.y + h * 0.5f;
  const float coil_h = h * 0.44f;
  const float coil_top = cy - coil_h * 0.5f;
  const float coil_bottom = cy + coil_h * 0.5f;
  const float left_paren_x = p_min.x + w * 0.40f;
  const float right_paren_x = p_min.x + w * 0.60f;
  const float curve_dx = w * 0.07f;
  const float wire_gap = std::max(thickness * 0.9f, w * 0.01f);
  const float wire_left_end = left_paren_x - curve_dx * 0.85f - wire_gap;
  const float wire_right_start = right_paren_x + curve_dx * 0.85f + wire_gap;

  draw_list->AddLine(ImVec2(p_min.x, cy), ImVec2(wire_left_end, cy), color,
                     thickness);
  draw_list->AddLine(ImVec2(wire_right_start, cy), ImVec2(p_max.x, cy), color,
                     thickness);

  draw_list->AddBezierCubic(
      ImVec2(left_paren_x, coil_top), ImVec2(left_paren_x - curve_dx, coil_top),
      ImVec2(left_paren_x - curve_dx, coil_bottom),
      ImVec2(left_paren_x, coil_bottom), color, thickness);
  draw_list->AddBezierCubic(
      ImVec2(right_paren_x, coil_top),
      ImVec2(right_paren_x + curve_dx, coil_top),
      ImVec2(right_paren_x + curve_dx, coil_bottom),
      ImVec2(right_paren_x, coil_bottom), color, thickness);
}

}  // namespace

void ProgrammingMode::RenderProgrammingModeUI(bool isPlcRunning) {
  RenderProgrammingToolbar(isPlcRunning);
  RenderProgrammingMainArea();
  RenderStatusBar(isPlcRunning);

  RenderAddressPopup();
  RenderRungMemoPopup();
  RenderVerticalDialog();
}

void ProgrammingMode::RenderProgrammingHeader() {
  // Header UI is rendered by the parent application.
}

void ProgrammingMode::RenderProgrammingToolbar(bool isPlcRunning) {
  const float layout_scale = GetLayoutScale();
  const char* edit_label =
      TR("ui.programming.toolbar_edit", "Edit (F3)");
  const char* monitor_label =
      TR("ui.programming.toolbar_monitor", "Monitor (F2)");
  const char* save_label =
      TR("ui.programming.toolbar_save", "Save");
  const char* compile_label =
      TR("ui.programming.toolbar_compile", "Compile");
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
  if (ImGui::BeginChild("ProgrammingToolbar", ImVec2(0, 50 * layout_scale),
                        true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPos(ImVec2(20 * layout_scale, 10 * layout_scale));

    if (isPlcRunning) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      ImGui::Button(edit_label,
                    ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            !is_monitor_mode_
                                ? ImVec4(0.4f, 0.6f, 0.8f, 1.0f)
                                : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
      if (ImGui::Button(edit_label,
                        ImVec2(100 * layout_scale, 30 * layout_scale))) {
        is_monitor_mode_ = false;
      }
      ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,
                          is_monitor_mode_ ? ImVec4(0.4f, 0.6f, 0.8f, 1.0f)
                                          : ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    if (ImGui::Button(monitor_label,
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
      ImGui::Button(save_label,
                    ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      if (ImGui::Button(save_label,
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
      ImGui::Button(compile_label,
                    ImVec2(100 * layout_scale, 30 * layout_scale));
      ImGui::PopStyleVar();
    } else {
      if (ImGui::Button(compile_label,
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
    float cellWidth = cellAreaWidth / static_cast<float>(kLadderColumnCount);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetWindowPos();
    const float rail_top = p_min.y +
                           (kLadderRungHeight - kLadderCellHeight) * 0.5f *
                               layout_scale;

    ImGui::Dummy(ImVec2(rungNumberWidth, 0));
    ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(railWidth, 0));
    ImGui::SameLine(0, 0);

    for (int i = 0; i < kLadderColumnCount; i++) {
      ImGui::BeginGroup();
      ImGui::Dummy(ImVec2(cellWidth, kLadderCellHeight * layout_scale));
      ImVec2 cellMin = ImGui::GetItemRectMin();
      char numStr[3];
      snprintf(numStr, 3, "%d", i);
      ImVec2 textSize = ImGui::CalcTextSize(numStr);
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(cellMin.x + (cellWidth - textSize.x) * 0.5f,
                 cellMin.y + (kLadderCellHeight * layout_scale - textSize.y) *
                                 0.5f),
          IM_COL32(80, 80, 80, 255), numStr);
      ImGui::EndGroup();

      if (i < kLadderColumnCount - 1) {
        ImGui::SameLine(0, 0);
      }
    }

    ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(railWidth, 0));

    if (!is_monitor_mode_) {
      ImGui::SameLine(0, 0);
      ImGui::Dummy(ImVec2(deleteButtonWidth, 0));
    }

    DrawContinuousGrid(
        draw_list, p_min.x + rungNumberWidth + railWidth, rail_top, cellWidth,
        kLadderCellHeight * layout_scale, kLadderColumnCount,
        GetLadderGridColor(is_monitor_mode_), std::max(1.0f, layout_scale));
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}

void ProgrammingMode::RenderLadderDiagram() {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  for (size_t i = 0; i < ladder_program_.rungs.size(); ++i) {
    if (ladder_program_.rungs[i].isEndRung) {
      RenderEndRung(i);
    } else {
      RenderRung(i);
    }
  }
  ImGui::PopStyleVar();
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

  const float layout_scale = GetLayoutScale();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 windowPos = ImGui::GetWindowPos();
  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();

  float rungNumberWidth = 80.0f * layout_scale;
  float railWidth = 22.0f * layout_scale;

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
                  (connection.x / static_cast<float>(kLadderColumnCount)) *
                      cellAreaWidth;

    float rungCenterY = windowPos.y + contentMin.y + 25.0f * layout_scale;
    ImU32 lineColor = GetVerticalConnectionColor(is_monitor_mode_);

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
      lineStartY = windowPos.y + contentMin.y - 2.5f * layout_scale;
    }

    // ????????????????????????????????????????????????????????????? ??????????????
    if (rungIndex < maxRung) {
      lineEndY = windowPos.y + contentMin.y + 52.5f * layout_scale;
    }

    // ???????????????????????????????????????????????
    const float line_thickness = std::max(1.5f, 2.0f * layout_scale);
    draw_list->AddLine(ImVec2(lineX, lineStartY), ImVec2(lineX, lineEndY),
                       lineColor, line_thickness);

    const float node_radius = std::max(1.8f, 2.2f * layout_scale);
    draw_list->AddCircleFilled(ImVec2(lineX, rungCenterY), node_radius,
                               lineColor);
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

  if (ImGui::BeginChild("Rung", ImVec2(0, kLadderRungHeight * layout_scale),
                        false,
                        ImGuiWindowFlags_NoScrollbar)) {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float rungNumberWidth = 80.0f * layout_scale;
    float railWidth = 22.0f * layout_scale;
    float deleteButtonWidth =
        is_monitor_mode_ ? 0.0f : 40.0f * layout_scale;
    float cellAreaWidth =
        availableWidth - rungNumberWidth - (railWidth * 2) - deleteButtonWidth;
    float cellWidth = cellAreaWidth / static_cast<float>(kLadderColumnCount);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetWindowPos();
    const float rail_top = p_min.y +
                           (kLadderRungHeight - kLadderCellHeight) * 0.5f *
                               layout_scale;
    const float rail_bottom = rail_top + kLadderCellHeight * layout_scale;
    const float left_rail_x = p_min.x + rungNumberWidth + railWidth;
    const float right_rail_x = left_rail_x + cellAreaWidth;
    const float rail_thickness = std::max(2.0f, 2.4f * layout_scale);
    const float rail_overlap = std::max(0.5f, kRailSeamOverlap * layout_scale);
    const ImU32 rail_color = GetLadderRailColor(is_monitor_mode_);

    ImGui::SetCursorPos(ImVec2(10 * layout_scale, 15 * layout_scale));
    char num[5];
    snprintf(num, 5, "%04d", ladder_program_.rungs[rungIndex].number);
    ImGui::TextUnformatted(num);
    if (hasCompileError) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1.0f), "!");
    }
    ImGui::SetCursorPos(
        ImVec2((80.0f + 22.0f) * layout_scale,
               (kLadderRungHeight - kLadderCellHeight) * 0.5f * layout_scale));

    for (int i = 0; i < kLadderColumnCount; ++i) {
      RenderLadderCell(rungIndex, i, cellWidth);
      if (i < kLadderColumnCount - 1)
        ImGui::SameLine(0, 0);
    }

    DrawContinuousGrid(
        draw_list, p_min.x + rungNumberWidth + railWidth, rail_top, cellWidth,
        kLadderCellHeight * layout_scale, kLadderColumnCount,
        GetLadderGridColor(is_monitor_mode_), std::max(1.0f, layout_scale));

    RenderVerticalConnectionsForRung(rungIndex, cellAreaWidth);
    DrawVerticalRailLine(draw_list, left_rail_x, rail_top - rail_overlap,
                         rail_bottom + rail_overlap, rail_color,
                         rail_thickness);
    DrawVerticalRailLine(draw_list, right_rail_x, rail_top - rail_overlap,
                         rail_bottom + rail_overlap, rail_color,
                         rail_thickness);

    ImGui::SameLine(0, 0);
    ImGui::Dummy(ImVec2(railWidth, 0));
    if (!is_monitor_mode_) {
      ImGui::SameLine(0, 0);
      if (ImGui::Button("X",
                        ImVec2(28 * layout_scale,
                               kLadderCellHeight * layout_scale))) {
        pending_action_.type = PendingActionType::DELETE_RUNG;
        pending_action_.rungIndex = rungIndex;
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopID();
}

void ProgrammingMode::RenderEndRung(int rungIndex) {
  const float layout_scale = GetLayoutScale();
  ImGui::PushID(rungIndex);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        (selected_rung_ == rungIndex)
                            ? ImVec4(1.0f, 1.0f, 0.8f, 1.0f)
                            : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));

  if (ImGui::BeginChild("EndRung",
                        ImVec2(0, kLadderRungHeight * layout_scale), false,
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
    const float rail_top = p_min.y +
                           (kLadderRungHeight - kLadderCellHeight) * 0.5f *
                               layout_scale;
    const float rail_bottom = rail_top + kLadderCellHeight * layout_scale;
    const float left_rail_x = p_min.x + rungNumberWidth + railWidth;
    const float right_rail_x = left_rail_x + cellAreaWidth;
    const float rail_thickness = std::max(2.0f, 2.4f * layout_scale);
    const float rail_overlap = std::max(0.5f, kRailSeamOverlap * layout_scale);
    const ImU32 rail_color = GetLadderRailColor(is_monitor_mode_);
    DrawVerticalRailLine(draw_list, left_rail_x, rail_top - rail_overlap,
                         rail_bottom + rail_overlap, rail_color,
                         rail_thickness);
    DrawVerticalRailLine(draw_list, right_rail_x, rail_top - rail_overlap,
                         rail_bottom + rail_overlap, rail_color,
                         rail_thickness);

    if (ImGui::InvisibleButton("##EndSelect", ImVec2(-1, -1))) {
      selected_rung_ = rungIndex;
      selected_cell_ = 0;
    }

    ImGui::SetCursorPos(ImVec2(10 * layout_scale, 15 * layout_scale));
    ImGui::TextUnformatted("END");

    float line_y = p_min.y + 25.0f * layout_scale;
    float line_start_x = p_min.x + rungNumberWidth + railWidth;
    float line_end_x = line_start_x + cellAreaWidth;
    draw_list->AddLine(ImVec2(line_start_x, line_y), ImVec2(line_end_x, line_y),
                       IM_COL32(88, 88, 88, 255),
                       std::max(1.5f, 2.0f * layout_scale));

    const char* endText = "[END]";
    ImVec2 endTextSize = ImGui::CalcTextSize(endText);
    ImVec2 textPos =
        ImVec2(line_start_x + (cellAreaWidth - endTextSize.x) * 0.5f,
               line_y - endTextSize.y * 0.5f);

    draw_list->AddRectFilled(
        textPos, ImVec2(textPos.x + endTextSize.x, textPos.y + endTextSize.y),
        ImGui::GetColorU32(ImGuiCol_ChildBg));
    draw_list->AddText(textPos, IM_COL32_BLACK, endText);

    if (!is_monitor_mode_) {
      ImGui::SetCursorPos(ImVec2(rungNumberWidth + railWidth + cellAreaWidth +
                                     railWidth,
                                 (kLadderRungHeight - kLadderCellHeight) * 0.5f *
                                     layout_scale));
      ImGui::Dummy(
          ImVec2(deleteButtonWidth, kLadderCellHeight * layout_scale));
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
                    ImVec2(cellWidth, kLadderCellHeight * layout_scale))) {
    selected_rung_ = rungIndex;
    selected_cell_ = cellIndex;
  }

  if (ImGui::IsItemVisible()) {
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float wire_thickness = std::max(1.0f, 2.0f * layout_scale);
    if (isSelected) {
      draw_list->AddRect(p_min, p_max, IM_COL32(130, 118, 62, 255), 0.0f, 0,
                         2.0f);
    }
    bool isTimerCounter = (instruction.type == LadderInstructionType::TON ||
                           instruction.type == LadderInstructionType::CTU ||
                           instruction.type == LadderInstructionType::RST_TMR_CTR);
    bool isBkrst = (instruction.type == LadderInstructionType::BKRST);

    if (instruction.type == LadderInstructionType::HLINE) {
      float centerY = p_min.y + (kLadderCellHeight * 0.5f) * layout_scale;
      ImU32 lineColor = instruction.isActive ? IM_COL32(100, 100, 100, 255)
                                             : IM_COL32(50, 50, 50, 255);
      draw_list->AddLine(ImVec2(p_min.x, centerY), ImVec2(p_max.x, centerY),
                         lineColor, wire_thickness);
    } else if (isTimerCounter) {
      const char* symbol = GetInstructionSymbol(instruction.type);
      float fontSize = ImGui::GetFontSize();
      float topY = p_min.y + 1.0f * layout_scale;
      float midY =
          p_min.y + (kLadderCellHeight * layout_scale - fontSize) * 0.5f;
      float bottomY =
          p_min.y + kLadderCellHeight * layout_scale - fontSize -
          1.0f * layout_scale;

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
    } else if (isBkrst) {
      std::string topLine = "BKRST";
      if (!instruction.address.empty()) {
        topLine += " " + instruction.address;
      }
      ImVec2 topSize = ImGui::CalcTextSize(topLine.c_str());
      float topY = p_min.y + 1.0f * layout_scale;
      draw_list->AddText(
          ImVec2(p_min.x + (cellWidth - topSize.x) * 0.5f, topY),
          IM_COL32_BLACK, topLine.c_str());

      if (!instruction.preset.empty()) {
        ImVec2 preset_size = ImGui::CalcTextSize(instruction.preset.c_str());
        float bottomY =
            p_min.y + kLadderCellHeight * layout_scale - preset_size.y -
            1.0f * layout_scale;
        draw_list->AddText(
            ImVec2(p_min.x + (cellWidth - preset_size.x) * 0.5f, bottomY),
            IM_COL32(20, 20, 200, 255), instruction.preset.c_str());
      }
    } else {
      bool renderedSymbol = false;
      if (instruction.type == LadderInstructionType::XIC ||
          instruction.type == LadderInstructionType::XIO) {
        ImU32 lineColor = instruction.isActive ? IM_COL32(100, 100, 100, 255)
                                               : IM_COL32(50, 50, 50, 255);
        ImU32 slashColor = lineColor;
        DrawLadderContactSymbol(draw_list, p_min, p_max, instruction.type,
                                lineColor, slashColor, wire_thickness);
        renderedSymbol = true;
      } else if (instruction.type == LadderInstructionType::OTE) {
        ImU32 lineColor = instruction.isActive ? IM_COL32(100, 100, 100, 255)
                                               : IM_COL32(50, 50, 50, 255);
        DrawOutputCoilSymbol(draw_list, p_min, p_max, lineColor,
                             wire_thickness);
        renderedSymbol = true;
      }
      if (!renderedSymbol) {
        const char* symbol = GetInstructionSymbol(instruction.type);
        ImVec2 text_size = ImGui::CalcTextSize(symbol);
        draw_list->AddText(
            ImVec2(p_min.x + (cellWidth - text_size.x) * 0.5f,
                   p_min.y + (kLadderCellHeight * layout_scale - text_size.y) *
                                 0.5f),
            IM_COL32_BLACK, symbol);
      }
    }

    if (!isTimerCounter && !isBkrst) {
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
      RenderRungMemoEditor();
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
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_f5", "F5: XIC (NO)"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_f6", "F6: XIO (NC)"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_f7", "F7: Coil (OUT)"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_f9", "F9: Add vertical"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_shift_f9",
               "Shift+F9: Remove vertical"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_del", "Del: Delete"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_insert", "Insert: Add"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_ctrl_arrow",
               "Ctrl+Arrow: Toggle line path"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_ctrl_z", "Ctrl+Z: Undo"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_ctrl_y",
               "Ctrl+Y / Ctrl+Shift+Z: Redo"));
  ImGui::BulletText(
      "%s", TR("ui.programming.shortcut_memo_popup",
               "`: Open memo popup"));
}

void ProgrammingMode::RenderCursorInfo() {
  ImGui::Text("%s", TR("ui.programming.cursor_info", "Cursor Info"));
  char cursor_buf[128] = {0};
  FormatString(cursor_buf, sizeof(cursor_buf),
               "ui.programming.cursor_pos_fmt", "Rung %04d, Cell: %d",
               selected_rung_, selected_cell_);
  ImGui::TextUnformatted(cursor_buf);
}

void ProgrammingMode::RenderRungMemoEditor() {
  const float layout_scale = GetLayoutScale();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.985f, 0.985f, 0.99f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.75f, 0.75f, 0.80f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * layout_scale);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

  if (!ImGui::BeginChild("RungMemoCard",
                         ImVec2(0, 110.0f * layout_scale), true,
                         ImGuiWindowFlags_NoScrollbar)) {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    return;
  }

  ImGui::Text("%s", TR("ui.programming.rung_memo", "Rung Memo"));

  if (selected_rung_ < 0 ||
      selected_rung_ >= static_cast<int>(ladder_program_.rungs.size())) {
    ImGui::TextDisabled("%s", TR("ui.programming.rung_memo_none",
                                 "No rung selected."));
    memo_edit_rung_ = -1;
    rung_memo_buffer_[0] = '\0';
    memo_edit_session_active_ = false;
    focus_rung_memo_next_frame_ = false;
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    return;
  }

  Rung& rung = ladder_program_.rungs[static_cast<size_t>(selected_rung_)];
  if (rung.isEndRung) {
    ImGui::TextDisabled("%s", TR("ui.programming.rung_memo_end",
                                 "END rung memo is disabled."));
    memo_edit_rung_ = -1;
    rung_memo_buffer_[0] = '\0';
    memo_edit_session_active_ = false;
    focus_rung_memo_next_frame_ = false;
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    return;
  }

  if (memo_edit_rung_ != selected_rung_ ||
      (!memo_edit_session_active_ &&
       std::string(rung_memo_buffer_) != rung.memo)) {
    memo_edit_rung_ = selected_rung_;
    std::strncpy(rung_memo_buffer_, rung.memo.c_str(),
                 sizeof(rung_memo_buffer_) - 1);
    rung_memo_buffer_[sizeof(rung_memo_buffer_) - 1] = '\0';
    memo_edit_session_active_ = false;
  }

  char memo_label[64] = {0};
  FormatString(memo_label, sizeof(memo_label), "ui.programming.rung_memo_label",
               "Rung %04d memo", selected_rung_);
  ImGui::TextUnformatted(memo_label);
  ImGui::SameLine();
  char memo_count[32] = {0};
  FormatString(memo_count, sizeof(memo_count), "ui.programming.rung_memo_count",
               "%d/255", static_cast<int>(std::strlen(rung_memo_buffer_)));
  float countWidth = ImGui::CalcTextSize(memo_count).x;
  float sameLinePos =
      ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - countWidth - 12.0f;
  if (sameLinePos > ImGui::GetCursorPosX()) {
    ImGui::SameLine(sameLinePos);
  } else {
    ImGui::SameLine();
  }
  ImGui::TextDisabled("%s", memo_count);

  if (focus_rung_memo_next_frame_) {
    ImGui::SetKeyboardFocusHere();
    focus_rung_memo_next_frame_ = false;
  }
  ImGui::SetNextItemWidth(-1.0f);
  bool edited = ImGui::InputTextWithHint(
      "##RungMemoInput",
      TR("ui.programming.rung_memo_placeholder",
         "Type memo for this rung..."),
      rung_memo_buffer_, sizeof(rung_memo_buffer_),
      ImGuiInputTextFlags_AutoSelectAll);
  if (edited) {
    std::string newMemo = rung_memo_buffer_;
    if (newMemo != rung.memo) {
      if (!memo_edit_session_active_) {
        PushProgrammingUndoState();
        memo_edit_session_active_ = true;
      }
      rung.memo = newMemo;
    }
  }

  if (!ImGui::IsItemActive()) {
    if (!focus_rung_memo_next_frame_) {
      memo_edit_session_active_ = false;
    }
  }

  ImGui::Spacing();
  if (ImGui::Button(TR("ui.programming.rung_memo_clear", "Clear"),
                    ImVec2(70.0f * layout_scale, 0))) {
    if (!rung.memo.empty()) {
      PushProgrammingUndoState();
      rung.memo.clear();
      rung_memo_buffer_[0] = '\0';
      memo_edit_session_active_ = false;
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%s", TR("ui.programming.rung_memo_hint",
                               "Saved with project file"));
  ImGui::Dummy(ImVec2(0.0f, 6.0f * layout_scale));
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void ProgrammingMode::RenderSimulationControl() {
  const float layout_scale = GetLayoutScale();
  const bool allow_toggle = !monitor_external_plc_;
  ImGui::Text("%s", TR("ui.programming.input_controls", "Input Controls"));
  ImGui::Text("%s", TR("ui.programming.inputs_label", "Inputs (X0-X15)"));
  if (!allow_toggle) {
    ImGui::BeginDisabled();
  }
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
      if (allow_toggle) {
        SetDeviceState(address, !state);
      }
    }
    ImGui::PopStyleColor();
    if ((i + 1) % 4 != 0)
      ImGui::SameLine();
  }
  ImGui::PopStyleVar();
  if (!allow_toggle) {
    ImGui::EndDisabled();
  }
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
  const std::string popup_name =
      std::string(TR("ui.programming.address_popup_title", "Address Input")) +
      "###AddressInputPopup";
  const char* popup_id = popup_name.c_str();
  const char* address_label = TR("ui.programming.address_label", "Address");

  if (show_address_popup_ && !ImGui::IsPopupOpen(popup_id)) {
    ImGui::OpenPopup(popup_id);
  }

  if (ImGui::BeginPopupModal(popup_id, &show_address_popup_,
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

    if (ImGui::InputText(address_label, temp_address_buffer_,
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
      ImGui::TextDisabled("%s", TR("ui.programming.device_example", "Example: Y0, T1 K10, C2 K5, SET M0, RST Y0, BKRST M0 K4"));
    }

    ImGui::Spacing();
    if (ImGui::Button(TR("ui.common.ok", "OK"), ImVec2(120 * layout_scale, 0))) {
      ConfirmInstruction();
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("ui.common.cancel", "Cancel"),
                      ImVec2(120 * layout_scale, 0))) {
      ImGui::CloseCurrentPopup();
      show_address_popup_ = false;
    }
    ImGui::EndPopup();
  }
}

void ProgrammingMode::RenderRungMemoPopup() {
  const std::string popup_name =
      std::string(TR("ui.programming.rung_memo_popup_window",
                     "Rung Memo Input")) +
      "###RungMemoInputPopup";
  const char* popup_id = popup_name.c_str();

  if (show_rung_memo_popup_ && !ImGui::IsPopupOpen(popup_id)) {
    ImGui::OpenPopup(popup_id);
  }

  if (ImGui::BeginPopupModal(popup_id, &show_rung_memo_popup_,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    const float layout_scale = GetLayoutScale();

    bool valid_target =
        rung_memo_popup_target_rung_ >= 0 &&
        rung_memo_popup_target_rung_ <
            static_cast<int>(ladder_program_.rungs.size()) &&
        !ladder_program_.rungs[static_cast<size_t>(rung_memo_popup_target_rung_)]
             .isEndRung;

    if (!valid_target) {
      ImGui::TextDisabled("%s", TR("ui.programming.rung_memo_none",
                                   "No rung selected."));
      if (ImGui::Button(TR("ui.common.close", "Close"),
                        ImVec2(120 * layout_scale, 0))) {
        show_rung_memo_popup_ = false;
        rung_memo_popup_buffer_[0] = '\0';
        rung_memo_popup_target_rung_ = -1;
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        show_rung_memo_popup_ = false;
        rung_memo_popup_buffer_[0] = '\0';
        rung_memo_popup_target_rung_ = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
      return;
    }

    char title_buf[96] = {0};
    FormatString(title_buf, sizeof(title_buf),
                 "ui.programming.rung_memo_popup_title",
                 "Rung %04d memo", rung_memo_popup_target_rung_);
    ImGui::TextUnformatted(title_buf);
    ImGui::Spacing();

    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(380.0f * layout_scale);
    bool submitted = ImGui::InputTextWithHint(
        "##RungMemoPopupInput",
        TR("ui.programming.rung_memo_placeholder",
           "Type memo for this rung..."),
        rung_memo_popup_buffer_, sizeof(rung_memo_popup_buffer_),
        ImGuiInputTextFlags_EnterReturnsTrue);

    char memo_count[32] = {0};
    FormatString(memo_count, sizeof(memo_count), "ui.programming.rung_memo_count",
                 "%d/255",
                 static_cast<int>(std::strlen(rung_memo_popup_buffer_)));
    ImGui::TextDisabled("%s", memo_count);

    auto apply_popup_memo = [&]() {
      Rung& rung = ladder_program_.rungs[static_cast<size_t>(
          rung_memo_popup_target_rung_)];
      std::string newMemo = rung_memo_popup_buffer_;
      if (newMemo != rung.memo) {
        PushProgrammingUndoState();
        rung.memo = newMemo;
      }
      if (selected_rung_ == rung_memo_popup_target_rung_) {
        std::strncpy(rung_memo_buffer_, newMemo.c_str(),
                     sizeof(rung_memo_buffer_) - 1);
        rung_memo_buffer_[sizeof(rung_memo_buffer_) - 1] = '\0';
      }
      memo_edit_rung_ = -1;
      memo_edit_session_active_ = false;
      focus_rung_memo_next_frame_ = false;
      show_rung_memo_popup_ = false;
      rung_memo_popup_buffer_[0] = '\0';
      rung_memo_popup_target_rung_ = -1;
      ImGui::CloseCurrentPopup();
    };

    if (submitted || ImGui::Button(TR("ui.common.apply", "Apply"),
                                   ImVec2(120 * layout_scale, 0))) {
      apply_popup_memo();
      ImGui::EndPopup();
      return;
    }

    ImGui::SameLine();
    if (ImGui::Button(TR("ui.common.cancel", "Cancel"),
                      ImVec2(120 * layout_scale, 0))) {
      show_rung_memo_popup_ = false;
      rung_memo_popup_buffer_[0] = '\0';
      rung_memo_popup_target_rung_ = -1;
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      show_rung_memo_popup_ = false;
      rung_memo_popup_buffer_[0] = '\0';
      rung_memo_popup_target_rung_ = -1;
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    ImGui::EndPopup();
  }
}
void ProgrammingMode::RenderVerticalDialog() {
  const std::string popup_name =
      std::string(TR("ui.programming.vertical_popup_title",
                     "Vertical Connection")) +
      "###VerticalConnectionPopup";
  const char* popup_id = popup_name.c_str();

  if (show_vertical_dialog_ && !ImGui::IsPopupOpen(popup_id)) {
    ImGui::OpenPopup(popup_id);
  }

  if (ImGui::BeginPopupModal(popup_id, &show_vertical_dialog_,
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
    if (ImGui::Button(TR("ui.common.confirm", "Confirm"),
                      ImVec2(120 * layout_scale, 0))) {
      ConfirmVerticalConnection();
      ImGui::CloseCurrentPopup();
      show_vertical_dialog_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("ui.common.cancel", "Cancel"),
                      ImVec2(120 * layout_scale, 0))) {
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
    case LadderInstructionType::BKRST:
      return "[BKRST]";
    default:
      return "";
  }
}
ImVec4 ProgrammingMode::GetInstructionColor(bool isSelected,
                                            bool isActive) const {
  if (is_monitor_mode_) {
    return isActive ? ImVec4(0.75f, 0.88f, 1.0f, 1.0f)
                    : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  }
  if (isSelected) {
    return ImVec4(1.0f, 1.0f, 0.78f, 1.0f);
  }
  return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

}  // namespace plc


