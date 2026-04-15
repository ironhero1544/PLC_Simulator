/*
 * app_rtl.cpp
 *
 * RTL library UI and dynamic RTL component helpers.
 */

#include "plc_emulator/core/application.h"
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/components/state_keys.h"

#include "imgui.h"
#include "imgui_internal.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <thread>

namespace plc {
namespace {

std::string BuildRtlPortStateKey(int port_id) {
  return std::string("rtl_port_") + std::to_string(port_id);
}

const char* kRtlClockLevelKey = "rtl_clock_level";
const char* kRtlClockAccumKey = "rtl_clock_accum";
const char* kRtlResetElapsedKey = "rtl_reset_elapsed";
const char* kRtlResetActiveKey = "rtl_reset_active";

const RtlPortDescriptor* FindRtlPortDescriptor(const RtlLibraryEntry* entry,
                                               const std::string& pin_name) {
  if (!entry) {
    return nullptr;
  }
  for (const auto& port : entry->ports) {
    if (port.pinName == pin_name) {
      return &port;
    }
  }
  return nullptr;
}

bool LooksLikeRtlPowerPin(const RtlPortDescriptor& port) {
  if (!port.isInput) {
    return false;
  }
  std::string name = port.baseName.empty() ? port.pinName : port.baseName;
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return name == "vcc" || name == "vdd" || name == "vpwr" || name == "power";
}

bool LooksLikeRtlGroundPin(const RtlPortDescriptor& port) {
  if (!port.isInput) {
    return false;
  }
  std::string name = port.baseName.empty() ? port.pinName : port.baseName;
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return name == "gnd" || name == "vss" || name == "vgnd" || name == "ground";
}

int TextEditCallback(ImGuiInputTextCallbackData* data) {
  if (!data || data->EventFlag != ImGuiInputTextFlags_CallbackResize) {
    return 0;
  }
  auto* text = static_cast<std::string*>(data->UserData);
  if (!text) {
    return 0;
  }
  text->resize(static_cast<size_t>(data->BufTextLen));
  data->Buf = text->data();
  return 0;
}

bool InputTextMultilineString(const char* label,
                              std::string* value,
                              const ImVec2& size) {
  if (!value) {
    return false;
  }
  if (value->empty()) {
    value->push_back('\0');
    value->clear();
  }
  value->reserve(std::max<size_t>(value->capacity(), 4096));
  return ImGui::InputTextMultiline(
      label, value->data(), value->capacity() + 1, size,
      ImGuiInputTextFlags_CallbackResize, TextEditCallback, value);
}

std::string ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return "";
  }
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

bool WriteFile(const std::string& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file << content;
  return true;
}

struct RtlEditorTheme {
  ImVec4 frame_bg = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
  ImVec4 border = ImVec4(0.28f, 0.31f, 0.36f, 1.0f);
  ImVec4 text = ImVec4(0.86f, 0.90f, 0.96f, 1.0f);
  ImVec4 selection_bg = ImVec4(0.28f, 0.48f, 0.78f, 0.60f);
  ImVec4 child_bg = ImVec4(0.14f, 0.16f, 0.20f, 1.0f);

  ImU32 keyword = IM_COL32(198, 120, 221, 255);
  ImU32 identifier = IM_COL32(220, 226, 235, 255);
  ImU32 function = IM_COL32(97, 175, 239, 255);
  ImU32 number = IM_COL32(209, 154, 102, 255);
  ImU32 comment = IM_COL32(92, 99, 112, 255);
  ImU32 system_task = IM_COL32(86, 182, 194, 255);
  ImU32 string = IM_COL32(152, 195, 121, 255);
  ImU32 signal = IM_COL32(224, 108, 117, 255);
};

struct RtlEditorDiagnosticMarker {
  int line = 1;
  int column = 1;
  bool isError = true;
  std::string message;
};

struct CachedRtlEditorDiagnostics {
  std::string sourcePath;
  std::string analyzeDiagnostics;
  std::string buildDiagnostics;
  std::vector<RtlEditorDiagnosticMarker> markers;
};

bool g_rtl_editor_theme_loaded = false;
RtlEditorTheme g_rtl_editor_theme;
std::filesystem::file_time_type g_rtl_editor_theme_mtime{};
CachedRtlEditorDiagnostics g_cached_rtl_editor_diagnostics;
constexpr float kRtlEditorGutterWidth = 52.0f;

bool ParseHexColor(const std::string& value, ImVec4* out_color) {
  if (!out_color || (value.size() != 6 && value.size() != 8)) {
    return false;
  }

  auto hex_to_int = [](const std::string& hex) -> int {
    return static_cast<int>(std::strtol(hex.c_str(), nullptr, 16));
  };

  const int r = hex_to_int(value.substr(0, 2));
  const int g = hex_to_int(value.substr(2, 2));
  const int b = hex_to_int(value.substr(4, 2));
  const int a = value.size() == 8 ? hex_to_int(value.substr(6, 2)) : 255;
  *out_color = ImVec4(static_cast<float>(r) / 255.0f,
                      static_cast<float>(g) / 255.0f,
                      static_cast<float>(b) / 255.0f,
                      static_cast<float>(a) / 255.0f);
  return true;
}

ImU32 ToImU32(const ImVec4& color) {
  return IM_COL32(static_cast<int>(color.x * 255.0f + 0.5f),
                  static_cast<int>(color.y * 255.0f + 0.5f),
                  static_cast<int>(color.z * 255.0f + 0.5f),
                  static_cast<int>(color.w * 255.0f + 0.5f));
}

std::string FindXmlOptionValue(const std::string& xml,
                               const std::string& name) {
  const std::regex option_regex(
      "<option\\s+name=\"" + name + "\"\\s+value=\"([0-9A-Fa-f]{6,8})\"");
  std::smatch match;
  if (!std::regex_search(xml, match, option_regex) || match.size() < 2) {
    return "";
  }
  return match[1].str();
}

std::string FindXmlAttributeColor(const std::string& xml,
                                  const std::string& name,
                                  const std::string& attribute_name) {
  const std::string option_tag = "<option name=\"" + name + "\">";
  const size_t option_pos = xml.find(option_tag);
  if (option_pos == std::string::npos) {
    return "";
  }

  const size_t value_start = xml.find("<value>", option_pos);
  const size_t value_end = value_start == std::string::npos
                               ? std::string::npos
                               : xml.find("</value>", value_start);
  if (value_start == std::string::npos || value_end == std::string::npos) {
    return "";
  }

  const std::string block =
      xml.substr(value_start, value_end + 8 - value_start);
  return FindXmlOptionValue(block, attribute_name);
}

void ApplyXmlThemeColor(const std::string& value, ImVec4* target) {
  ImVec4 parsed;
  if (ParseHexColor(value, &parsed)) {
    *target = parsed;
  }
}

void ApplyXmlThemeColor(const std::string& value, ImU32* target) {
  ImVec4 parsed;
  if (ParseHexColor(value, &parsed)) {
    *target = ToImU32(parsed);
  }
}

const RtlEditorTheme& GetRtlEditorTheme() {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path theme_path("resources/themes/rtl_editor_theme.xml");
  const fs::file_time_type current_mtime =
      fs::last_write_time(theme_path, ec);
  if (!ec) {
    if (!g_rtl_editor_theme_loaded || current_mtime != g_rtl_editor_theme_mtime) {
      g_rtl_editor_theme_loaded = false;
      g_rtl_editor_theme_mtime = current_mtime;
    }
  }

  if (g_rtl_editor_theme_loaded) {
    return g_rtl_editor_theme;
  }

  g_rtl_editor_theme_loaded = true;
  g_rtl_editor_theme = RtlEditorTheme{};
  const std::string xml = ReadFile("resources/themes/rtl_editor_theme.xml");
  if (xml.empty()) {
    return g_rtl_editor_theme;
  }

  ApplyXmlThemeColor(FindXmlAttributeColor(xml, "TEXT", "BACKGROUND"),
                     &g_rtl_editor_theme.frame_bg);
  ApplyXmlThemeColor(FindXmlAttributeColor(xml, "TEXT", "BACKGROUND"),
                     &g_rtl_editor_theme.child_bg);
  ApplyXmlThemeColor(FindXmlOptionValue(xml, "SELECTION_BACKGROUND"),
                     &g_rtl_editor_theme.selection_bg);
  ApplyXmlThemeColor(FindXmlAttributeColor(xml, "TEXT", "FOREGROUND"),
                     &g_rtl_editor_theme.text);
  ApplyXmlThemeColor(FindXmlOptionValue(xml, "INDENT_GUIDE"),
                     &g_rtl_editor_theme.border);

  ApplyXmlThemeColor(FindXmlAttributeColor(xml, "DEFAULT_KEYWORD", "FOREGROUND"),
                     &g_rtl_editor_theme.keyword);
  ApplyXmlThemeColor(
      FindXmlAttributeColor(xml, "DEFAULT_IDENTIFIER", "FOREGROUND"),
      &g_rtl_editor_theme.identifier);
  ApplyXmlThemeColor(
      FindXmlAttributeColor(xml, "DEFAULT_FUNCTION_CALL", "FOREGROUND"),
      &g_rtl_editor_theme.function);
  ApplyXmlThemeColor(FindXmlAttributeColor(xml, "DEFAULT_NUMBER", "FOREGROUND"),
                     &g_rtl_editor_theme.number);
  ApplyXmlThemeColor(
      FindXmlAttributeColor(xml, "DEFAULT_LINE_COMMENT", "FOREGROUND"),
      &g_rtl_editor_theme.comment);
  ApplyXmlThemeColor(
      FindXmlAttributeColor(xml, "DEFAULT_PREDEFINED_SYMBOL", "FOREGROUND"),
      &g_rtl_editor_theme.system_task);
  ApplyXmlThemeColor(FindXmlAttributeColor(xml, "DEFAULT_STRING", "FOREGROUND"),
                     &g_rtl_editor_theme.string);
  ApplyXmlThemeColor(
      FindXmlAttributeColor(xml, "DEFAULT_PARAMETER", "FOREGROUND"),
      &g_rtl_editor_theme.signal);
  return g_rtl_editor_theme;
}

void PushJetBrainsEditorTheme(const RtlEditorTheme& theme) {
  ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.frame_bg);
  ImGui::PushStyleColor(ImGuiCol_Border, theme.border);
  ImGui::PushStyleColor(
      ImGuiCol_Text,
      ImVec4(theme.text.x, theme.text.y, theme.text.z, 0.14f));
  ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, theme.selection_bg);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.child_bg);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(kRtlEditorGutterWidth + 10.0f, 10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}

void PopJetBrainsEditorTheme() {
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
}

std::string TrimCopy(const std::string& text) {
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(start, end - start);
}

bool IsVerilogKeyword(const std::string& token) {
  static const char* const kKeywords[] = {
      "module",      "endmodule",  "input",      "output",    "inout",
      "wire",        "reg",        "logic",      "assign",    "always",
      "initial",     "begin",      "end",        "if",        "else",
      "case",        "endcase",    "posedge",    "negedge",   "or",
      "parameter",   "localparam", "generate",   "endgenerate",
      "for",         "while",      "repeat",     "function",
      "endfunction", "task",       "endtask",    "return"};
  for (const char* keyword : kKeywords) {
    if (token == keyword) {
      return true;
    }
  }
  return false;
}

std::string ExpandTabs(const std::string& line) {
  std::string expanded;
  expanded.reserve(line.size());
  int column = 0;
  for (char ch : line) {
    if (ch == '\t') {
      const int spaces = 4 - (column % 4);
      expanded.append(static_cast<size_t>(spaces), ' ');
      column += spaces;
      continue;
    }
    expanded.push_back(ch);
    ++column;
  }
  return expanded;
}

size_t CountOccurrences(const std::string& text, const std::string& needle) {
  if (needle.empty()) {
    return 0;
  }
  size_t count = 0;
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

std::string NormalizeSourcePath(std::string path);
std::string SourceBaseName(const std::string& path);

ImVec2 ComputeLineColumnForOffset(const std::string& text, size_t offset) {
  const size_t clamped = std::min(offset, text.size());
  int line = 1;
  int column = 1;
  for (size_t i = 0; i < clamped; ++i) {
    if (text[i] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  return ImVec2(static_cast<float>(line), static_cast<float>(column));
}

std::vector<RtlEditorDiagnosticMarker> ParseRtlDiagnosticsForSource(
    const std::string& source_path,
    const std::string& diagnostics) {
  std::vector<RtlEditorDiagnosticMarker> markers;
  if (source_path.empty() || diagnostics.empty()) {
    return markers;
  }

  const std::string normalized_source = NormalizeSourcePath(source_path);
  const std::string base_name = SourceBaseName(normalized_source);
  const std::regex location_regex(
      R"(([^:\r\n]+(?:/[^:\r\n]+)*\.(?:v|sv|vh|svh)):(\d+)(?::(\d+))?:?\s*(.*))");

  std::istringstream stream(diagnostics);
  std::string line;
  while (std::getline(stream, line)) {
    std::smatch match;
    if (!std::regex_search(line, match, location_regex) || match.size() < 5) {
      continue;
    }
    const std::string raw_path = NormalizeSourcePath(TrimCopy(match[1].str()));
    const std::string raw_base = SourceBaseName(raw_path);
    if (raw_path != normalized_source &&
        raw_base != base_name &&
        raw_path.find("/" + base_name) == std::string::npos &&
        raw_path.find("\\" + base_name) == std::string::npos) {
      continue;
    }

    RtlEditorDiagnosticMarker marker;
    marker.line = std::max(1, std::atoi(match[2].str().c_str()));
    marker.column = match[3].matched
                        ? std::max(1, std::atoi(match[3].str().c_str()))
                        : 1;
    marker.message = TrimCopy(match[4].str());
    std::string lowered = line;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) {
                     return static_cast<char>(std::tolower(ch));
                   });
    marker.isError = lowered.find("warning") == std::string::npos;
    markers.push_back(marker);
  }
  return markers;
}

const std::vector<RtlEditorDiagnosticMarker>& GetCachedRtlEditorDiagnostics(
    const std::string& source_path,
    const std::string& analyze_diagnostics,
    const std::string& build_diagnostics) {
  if (g_cached_rtl_editor_diagnostics.sourcePath == source_path &&
      g_cached_rtl_editor_diagnostics.analyzeDiagnostics ==
          analyze_diagnostics &&
      g_cached_rtl_editor_diagnostics.buildDiagnostics == build_diagnostics) {
    return g_cached_rtl_editor_diagnostics.markers;
  }

  g_cached_rtl_editor_diagnostics.sourcePath = source_path;
  g_cached_rtl_editor_diagnostics.analyzeDiagnostics = analyze_diagnostics;
  g_cached_rtl_editor_diagnostics.buildDiagnostics = build_diagnostics;
  g_cached_rtl_editor_diagnostics.markers =
      ParseRtlDiagnosticsForSource(source_path, analyze_diagnostics);
  std::vector<RtlEditorDiagnosticMarker> build_markers =
      ParseRtlDiagnosticsForSource(source_path, build_diagnostics);
  g_cached_rtl_editor_diagnostics.markers.insert(
      g_cached_rtl_editor_diagnostics.markers.end(), build_markers.begin(),
      build_markers.end());
  return g_cached_rtl_editor_diagnostics.markers;
}

std::string NormalizeSourcePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

std::string SourceBaseName(const std::string& path) {
  const std::string normalized = NormalizeSourcePath(path);
  const size_t pos = normalized.find_last_of('/');
  return pos == std::string::npos ? normalized : normalized.substr(pos + 1);
}

std::string SourceDirectory(const std::string& path) {
  const std::string normalized = NormalizeSourcePath(path);
  const size_t pos = normalized.find_last_of('/');
  return pos == std::string::npos ? "" : normalized.substr(0, pos);
}

std::string BuildUniqueRtlSourceName(const RtlLibraryEntry& entry) {
  int next_index = 1;
  while (true) {
    const std::string candidate =
        "module_" + std::to_string(next_index) + ".v";
    const bool exists = std::any_of(
        entry.sourceFiles.begin(), entry.sourceFiles.end(),
        [&](const RtlSourceFile& file) { return file.path == candidate; });
    if (!exists) {
      return candidate;
    }
    ++next_index;
  }
}

void SelectRtlSource(const RtlSourceFile& file,
                     std::string* selected_path,
                     std::string* editor_buffer) {
  if (!selected_path || !editor_buffer) {
    return;
  }
  *selected_path = file.path;
  *editor_buffer = file.content;
}

bool AddNewRtlSource(RtlProjectManager* manager,
                     RtlLibraryEntry* entry,
                     std::string* selected_path,
                     std::string* editor_buffer) {
  if (!manager || !entry || !selected_path || !editor_buffer) {
    return false;
  }
  const std::string source_name = BuildUniqueRtlSourceName(*entry);
  if (!manager->ImportSourceFile(entry->moduleId, source_name, "")) {
    return false;
  }
  *selected_path = source_name;
  editor_buffer->clear();
  return true;
}

std::string BuildRenamedSourcePath(const std::string& current_path,
                                   const std::string& new_base_name) {
  const std::string trimmed = TrimCopy(new_base_name);
  if (trimmed.empty()) {
    return "";
  }
  const std::string directory = SourceDirectory(current_path);
  return directory.empty() ? trimmed : (directory + "/" + trimmed);
}

enum class TestbenchVerdict {
  kNotRun,
  kPass,
  kFail,
  kTimeout,
};

TestbenchVerdict GetTestbenchVerdict(const RtlLibraryEntry& entry) {
  if (entry.testbenchDiagnostics.find("timed out") != std::string::npos) {
    return TestbenchVerdict::kTimeout;
  }
  if (entry.testbenchSuccess) {
    return TestbenchVerdict::kPass;
  }
  if (!entry.testbenchDiagnostics.empty() || !entry.testbenchRunLog.empty() ||
      !entry.testbenchBuildLog.empty()) {
    return TestbenchVerdict::kFail;
  }
  return TestbenchVerdict::kNotRun;
}

ImVec4 GetTestbenchVerdictColor(TestbenchVerdict verdict) {
  switch (verdict) {
    case TestbenchVerdict::kPass:
      return ImVec4(0.15f, 0.65f, 0.22f, 1.0f);
    case TestbenchVerdict::kTimeout:
      return ImVec4(0.85f, 0.55f, 0.12f, 1.0f);
    case TestbenchVerdict::kFail:
      return ImVec4(0.80f, 0.18f, 0.18f, 1.0f);
    case TestbenchVerdict::kNotRun:
    default:
      return ImVec4(0.55f, 0.58f, 0.64f, 1.0f);
  }
}

const char* GetTestbenchVerdictLabel(TestbenchVerdict verdict) {
  switch (verdict) {
    case TestbenchVerdict::kPass:
      return TR("ui.rtl.testbench_verdict_pass", "PASS");
    case TestbenchVerdict::kTimeout:
      return TR("ui.rtl.testbench_verdict_timeout", "TIMEOUT");
    case TestbenchVerdict::kFail:
      return TR("ui.rtl.testbench_verdict_fail", "FAIL");
    case TestbenchVerdict::kNotRun:
    default:
      return TR("ui.rtl.testbench_verdict_not_run", "NOT RUN");
  }
}

void DrawHighlightedToken(ImDrawList* draw_list,
                          ImFont* font,
                          float font_size,
                          ImVec2 pos,
                          ImU32 color,
                          const std::string& text,
                          ImVec2 clip_min,
                          ImVec2 clip_max) {
  if (!draw_list || !font || text.empty()) {
    return;
  }
  draw_list->PushClipRect(clip_min, clip_max, true);
  draw_list->AddText(font, font_size, pos, color, text.c_str());
  draw_list->PopClipRect();
}

int CountLines(const std::string& text) {
  int lines = 1;
  for (char ch : text) {
    if (ch == '\n') {
      ++lines;
    }
  }
  return lines;
}

void RenderLineNumberGutter(const std::string& source,
                            ImVec2 rect_min,
                            ImVec2 rect_max,
                            ImFont* font,
                            const RtlEditorTheme& theme,
                            ImVec2 scroll_offset) {
  if (!font) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const float font_size = font->FontSize;
  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 gutter_min(rect_min.x, rect_min.y);
  const ImVec2 gutter_max(rect_min.x + kRtlEditorGutterWidth, rect_max.y);
  const ImU32 gutter_bg = IM_COL32(
      static_cast<int>(theme.child_bg.x * 255.0f + 0.5f),
      static_cast<int>(theme.child_bg.y * 255.0f + 0.5f),
      static_cast<int>(theme.child_bg.z * 255.0f + 0.5f), 255);
  const ImU32 separator = IM_COL32(
      static_cast<int>(theme.border.x * 255.0f + 0.5f),
      static_cast<int>(theme.border.y * 255.0f + 0.5f),
      static_cast<int>(theme.border.z * 255.0f + 0.5f), 255);
  const ImU32 line_color = IM_COL32(92, 99, 112, 255);

  draw_list->PushClipRect(rect_min, rect_max, true);
  draw_list->AddRectFilled(gutter_min, gutter_max, gutter_bg);
  draw_list->AddLine(ImVec2(gutter_max.x, gutter_min.y),
                     ImVec2(gutter_max.x, gutter_max.y), separator, 1.0f);

  const int total_lines = CountLines(source);
  const float start_y = rect_min.y + 10.0f - scroll_offset.y;
  for (int line = 1; line <= total_lines; ++line) {
    const float y = start_y + static_cast<float>(line - 1) * line_height;
    if (y + line_height < rect_min.y) {
      continue;
    }
    if (y > rect_max.y) {
      break;
    }
    const std::string label = std::to_string(line);
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f,
                                                 label.c_str());
    const ImVec2 pos(gutter_max.x - text_size.x - 8.0f, y);
    draw_list->AddText(font, font_size, pos, line_color, label.c_str());
  }
  draw_list->PopClipRect();
}

void RenderDiagnosticMarkers(
    const std::vector<RtlEditorDiagnosticMarker>& markers,
    ImVec2 rect_min,
    ImVec2 rect_max,
    ImFont* font,
    ImVec2 scroll_offset) {
  if (!font || markers.empty()) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 gutter_min(rect_min.x, rect_min.y);
  const ImVec2 gutter_max(rect_min.x + kRtlEditorGutterWidth, rect_max.y);
  const ImVec2 text_min(gutter_max.x, rect_min.y);
  const ImVec2 text_max(rect_max.x, rect_max.y);
  const float origin_y = rect_min.y + 10.0f - scroll_offset.y;

  draw_list->PushClipRect(gutter_min, rect_max, true);
  for (const auto& marker : markers) {
    const float y = origin_y + (static_cast<float>(marker.line - 1) * line_height);
    const float y2 = y + line_height;
    const ImU32 color = marker.isError ? IM_COL32(220, 86, 86, 210)
                                       : IM_COL32(214, 170, 74, 190);
    draw_list->AddRectFilled(ImVec2(gutter_min.x + 2.0f, y + 1.0f),
                             ImVec2(gutter_max.x - 2.0f, y2 - 1.0f),
                             color, 3.0f);
  }
  draw_list->PopClipRect();

  draw_list->PushClipRect(text_min, text_max, true);
  for (const auto& marker : markers) {
    const float y = origin_y + (static_cast<float>(marker.line - 1) * line_height);
    const ImU32 color = marker.isError ? IM_COL32(220, 86, 86, 220)
                                       : IM_COL32(214, 170, 74, 210);
    draw_list->AddLine(ImVec2(text_min.x + 4.0f, y + line_height - 2.0f),
                       ImVec2(text_max.x - 4.0f, y + line_height - 2.0f),
                       color, 1.5f);
  }
  draw_list->PopClipRect();
}

void RenderVerilogHighlightOverlay(const std::string& source,
                                   ImVec2 rect_min,
                                   ImVec2 rect_max,
                                   ImFont* font,
                                   const RtlEditorTheme& theme,
                                   ImVec2 scroll_offset) {
  if (!font || source.empty()) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const float font_size = font->FontSize;
  const float line_height = ImGui::GetTextLineHeight();
  const float char_width =
      font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, "M").x;
  const ImVec2 origin(rect_min.x + kRtlEditorGutterWidth + 10.0f -
                          scroll_offset.x,
                      rect_min.y + 10.0f - scroll_offset.y);

  std::istringstream input(source);
  std::string raw_line;
  int line_index = 0;
  while (std::getline(input, raw_line)) {
    std::string line = ExpandTabs(raw_line);
    const float y = origin.y + static_cast<float>(line_index) * line_height;
    if (y > rect_max.y) {
      break;
    }

    size_t i = 0;
    while (i < line.size()) {
      const float x = origin.x + static_cast<float>(i) * char_width;
      char ch = line[i];
      if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
        ++i;
        continue;
      }
      if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') {
        DrawHighlightedToken(draw_list, font, font_size, ImVec2(x, y),
                             theme.comment, line.substr(i), rect_min, rect_max);
        break;
      }
      if (ch == '"') {
        size_t end = i + 1;
        while (end < line.size() && line[end] != '"') {
          if (line[end] == '\\' && end + 1 < line.size()) {
            end += 2;
          } else {
            ++end;
          }
        }
        if (end < line.size()) {
          ++end;
        }
        DrawHighlightedToken(draw_list, font, font_size, ImVec2(x, y),
                             theme.string, line.substr(i, end - i), rect_min,
                             rect_max);
        i = end;
        continue;
      }
      if (ch == '$') {
        size_t end = i + 1;
        while (end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[end])) != 0 ||
                line[end] == '_')) {
          ++end;
        }
        DrawHighlightedToken(draw_list, font, font_size, ImVec2(x, y),
                             theme.system_task, line.substr(i, end - i),
                             rect_min,
                             rect_max);
        i = end;
        continue;
      }
      if (std::isdigit(static_cast<unsigned char>(ch)) != 0 ||
          (ch == '\'' && i + 1 < line.size())) {
        size_t end = i + 1;
        while (end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[end])) != 0 ||
                line[end] == '\'' || line[end] == '_' || line[end] == '.')) {
          ++end;
        }
        DrawHighlightedToken(draw_list, font, font_size, ImVec2(x, y),
                             theme.number, line.substr(i, end - i), rect_min,
                             rect_max);
        i = end;
        continue;
      }
      if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
        size_t end = i + 1;
        while (end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[end])) != 0 ||
                line[end] == '_')) {
          ++end;
        }
        std::string token = line.substr(i, end - i);
        size_t next = end;
        while (next < line.size() &&
               std::isspace(static_cast<unsigned char>(line[next])) != 0) {
          ++next;
        }
        ImU32 color = theme.identifier;
        if (IsVerilogKeyword(token)) {
          color = theme.keyword;
        } else if (next < line.size() && line[next] == '(') {
          color = theme.function;
        } else if (token == "clk" || token == "clock" || token == "rst" ||
                   token == "rst_n" || token == "reset" || token == "en" ||
                   token == "q") {
          color = theme.signal;
        }
        DrawHighlightedToken(draw_list, font, font_size, ImVec2(x, y), color,
                             token, rect_min, rect_max);
        i = end;
        continue;
      }
      DrawHighlightedToken(draw_list, font, font_size, ImVec2(x, y),
                           theme.identifier, std::string(1, ch), rect_min,
                           rect_max);
      ++i;
    }
    ++line_index;
  }
}

}  // namespace

void Application::RequestRtlToolchainRefresh() {
  if (!rtl_project_manager_ || rtl_toolchain_loading_) {
    return;
  }
  rtl_toolchain_loading_ = true;
  rtl_toolchain_future_ = std::async(std::launch::async, [this]() {
    return rtl_project_manager_->DetectToolchain();
  });
}

void Application::PollRtlToolchainRefresh() {
  if (!rtl_toolchain_loading_ || !rtl_toolchain_future_.valid()) {
    return;
  }
  if (rtl_toolchain_future_.wait_for(std::chrono::milliseconds(0)) !=
      std::future_status::ready) {
    return;
  }
  rtl_toolchain_status_ = rtl_toolchain_future_.get();
  rtl_toolchain_status_valid_ = true;
  rtl_toolchain_loading_ = false;
}

void Application::RequestRtlAnalyzeAsync(const std::string& moduleId) {
  if (!rtl_project_manager_ || rtl_async_task_running_) {
    return;
  }
  RtlLibraryEntry* entry = rtl_project_manager_->FindEntryById(moduleId);
  if (!entry) {
    return;
  }

  InvalidateRtlModuleRuntimeState(moduleId);
  entry->analyzeSuccess = false;
  entry->buildSuccess = false;
  entry->testbenchSuccess = false;
  entry->diagnostics.clear();
  entry->buildDiagnostics.clear();
  entry->buildHash.clear();
  entry->testbenchDiagnostics.clear();
  entry->testbenchBuildLog.clear();
  entry->testbenchRunLog.clear();
  entry->testbenchVcdPath.clear();

  RtlLibraryEntry entryCopy = *entry;
  rtl_async_task_running_ = true;
  rtl_status_message_ = "Analyzing RTL module...";
  rtl_async_task_future_ =
      std::async(std::launch::async, [this, entryCopy]() mutable {
        RtlAsyncTaskResult result;
        result.moduleId = entryCopy.moduleId;
        result.taskKind = "analyze";
        result.success = false;
        result.entry = entryCopy;
        result.entry.analyzeSuccess = false;
        result.entry.buildSuccess = false;
        result.entry.testbenchSuccess = false;

        if (!rtl_project_manager_) {
          result.entry.diagnostics = "RTL Project Manager not initialized.";
          result.statusMessage = "RTL analyze failed.";
          return result;
        }

        RtlToolchainStatus status = rtl_project_manager_->DetectToolchain();
        if (!status.verilatorFound) {
          result.entry.diagnostics = status.description;
          result.statusMessage = "RTL analyze failed. Check Analyze Log.";
          return result;
        }

        result.success = rtl_project_manager_->Analyze(entryCopy.moduleId);
        const RtlLibraryEntry* updated =
            rtl_project_manager_->FindEntryById(entryCopy.moduleId);
        if (updated) {
          result.entry = *updated;
        }
        result.entry.analyzeSuccess = result.success;
        if (!result.success && result.entry.diagnostics.empty()) {
          result.entry.diagnostics = "RTL analyze failed.";
        }
        result.statusMessage = result.success ? "RTL analyze completed."
                                              : "RTL analyze failed. Check Analyze Log.";
        return result;
      });
}

void Application::RequestRtlBuildAsync(const std::string& moduleId) {
  if (!rtl_project_manager_ || rtl_async_task_running_) {
    return;
  }
  RtlLibraryEntry* entry = rtl_project_manager_->FindEntryById(moduleId);
  if (!entry) {
    return;
  }

  InvalidateRtlModuleRuntimeState(moduleId);
  entry->buildSuccess = false;
  entry->buildDiagnostics.clear();
  entry->buildHash.clear();
  entry->testbenchSuccess = false;
  entry->testbenchDiagnostics.clear();
  entry->testbenchBuildLog.clear();
  entry->testbenchRunLog.clear();
  entry->testbenchVcdPath.clear();

  RtlLibraryEntry entryCopy = *entry;
  rtl_async_task_running_ = true;
  rtl_status_message_ = "Building RTL module...";
  rtl_async_task_future_ =
      std::async(std::launch::async, [this, entryCopy]() mutable {
        RtlAsyncTaskResult result;
        result.moduleId = entryCopy.moduleId;
        result.taskKind = "build";
        result.success = false;
        result.entry = entryCopy;
        result.entry.buildSuccess = false;
        result.entry.testbenchSuccess = false;

        if (!rtl_project_manager_) {
          result.entry.buildDiagnostics = "RTL Project Manager not initialized.";
          result.statusMessage = "Build failed.";
          return result;
        }

        RtlToolchainStatus status = rtl_project_manager_->DetectToolchain();
        if (!status.verilatorFound || !status.compilerFound) {
          result.entry.buildDiagnostics = status.description;
          result.statusMessage = "Build failed: " + status.description;
          return result;
        }

        result.success = rtl_project_manager_->Build(entryCopy.moduleId);
        const RtlLibraryEntry* updated =
            rtl_project_manager_->FindEntryById(entryCopy.moduleId);
        if (updated) {
          result.entry = *updated;
        }
        result.entry.buildSuccess = result.success;
        if (!result.success && result.entry.buildDiagnostics.empty()) {
          result.entry.buildDiagnostics = "RTL build failed.";
        }
        result.statusMessage = result.success ? "RTL build completed."
                                              : "Build failed: " + result.entry.buildDiagnostics;
        return result;
      });
}

void Application::RequestRtlTestbenchAsync(const std::string& moduleId) {
  if (!rtl_project_manager_ || rtl_async_task_running_) {
    return;
  }
  RtlLibraryEntry* entry = rtl_project_manager_->FindEntryById(moduleId);
  if (!entry) {
    return;
  }

  entry->testbenchSuccess = false;
  entry->testbenchDiagnostics.clear();
  entry->testbenchBuildLog.clear();
  entry->testbenchRunLog.clear();
  entry->testbenchVcdPath.clear();

  RtlLibraryEntry entryCopy = *entry;
  rtl_async_task_running_ = true;
  rtl_status_message_ = "Running RTL testbench...";
  rtl_async_task_future_ =
      std::async(std::launch::async, [this, entryCopy]() mutable {
        RtlAsyncTaskResult result;
        result.moduleId = entryCopy.moduleId;
        result.taskKind = "testbench";
        result.success = false;
        result.entry = entryCopy;
        result.entry.testbenchSuccess = false;

        if (!rtl_project_manager_) {
          result.entry.testbenchDiagnostics = "RTL Project Manager not initialized.";
          result.statusMessage = "RTL testbench failed.";
          return result;
        }
        result.success = rtl_project_manager_->RunTestbench(entryCopy.moduleId);
        const RtlLibraryEntry* updated =
            rtl_project_manager_->FindEntryById(entryCopy.moduleId);
        if (updated) {
          result.entry = *updated;
        }
        result.entry.testbenchSuccess = result.success;
        if (!result.success && result.entry.testbenchDiagnostics.empty() &&
            result.entry.testbenchRunLog.empty() &&
            result.entry.testbenchBuildLog.empty()) {
          result.entry.testbenchDiagnostics = "RTL testbench failed.";
        }
        result.statusMessage = result.success ? "RTL testbench completed."
                                              : "RTL testbench failed.";
        return result;
      });
}

void Application::PollRtlAsyncTask() {
  if (!rtl_async_task_running_ || !rtl_async_task_future_.valid() ||
      !rtl_project_manager_) {
    return;
  }
  if (rtl_async_task_future_.wait_for(std::chrono::milliseconds(0)) !=
      std::future_status::ready) {
    return;
  }

  RtlAsyncTaskResult result = rtl_async_task_future_.get();
  rtl_async_task_running_ = false;
  rtl_status_message_ = result.statusMessage;
  if (rtl_runtime_manager_) {
    rtl_runtime_manager_->InvalidateModule(result.moduleId);
  }

  RtlLibraryEntry* target = rtl_project_manager_->FindEntryById(result.moduleId);
  if (!target) {
    return;
  }

  // Selective update to avoid overwriting user edits (displayName, topModule, etc)
  // that may have occurred during async task execution.
  if (result.taskKind == "analyze") {
    target->analyzeSuccess = result.entry.analyzeSuccess;
    target->diagnostics = result.entry.diagnostics;
    target->ports = result.entry.ports;
    target->buildSuccess = false;
    target->buildDiagnostics.clear();
    target->buildHash.clear();
    target->testbenchSuccess = false;
    target->testbenchDiagnostics.clear();
    target->testbenchBuildLog.clear();
    target->testbenchRunLog.clear();
    target->testbenchVcdPath.clear();
  } else if (result.taskKind == "build") {
    target->buildSuccess = result.entry.buildSuccess;
    target->buildDiagnostics = result.entry.buildDiagnostics;
    target->buildHash = result.entry.buildHash;
    target->testbenchSuccess = false;
    target->testbenchDiagnostics.clear();
    target->testbenchBuildLog.clear();
    target->testbenchRunLog.clear();
    target->testbenchVcdPath.clear();
  } else if (result.taskKind == "testbench") {
    target->testbenchSuccess = result.entry.testbenchSuccess;
    target->testbenchDiagnostics = result.entry.testbenchDiagnostics;
    target->testbenchBuildLog = result.entry.testbenchBuildLog;
    target->testbenchRunLog = result.entry.testbenchRunLog;
    target->testbenchVcdPath = result.entry.testbenchVcdPath;
  }

  if (result.taskKind == "analyze") {
    for (auto& comp : placed_components_) {
      if (comp.rtlModuleId == result.moduleId) {
        if (result.success) {
          RefreshRtlComponentFromLibrary(&comp);
        } else {
          comp.runtimePorts.clear();
          comp.rtlPinBindings.clear();
        }
      }
    }
  }
  
  rtl_project_manager_->SaveGlobalLibrary();
}

void Application::InvalidateRtlModuleRuntimeState(const std::string& moduleId) {
  if (moduleId.empty()) {
    return;
  }
  if (rtl_runtime_manager_) {
    rtl_runtime_manager_->InvalidateModule(moduleId);
  }
  for (auto& comp : placed_components_) {
    if (comp.rtlModuleId != moduleId) {
      continue;
    }
    comp.internalStates[kRtlClockAccumKey] = 0.0f;
    comp.internalStates[kRtlClockLevelKey] = 0.0f;
    comp.internalStates[kRtlResetElapsedKey] = 0.0f;
    comp.internalStates[kRtlResetActiveKey] =
        comp.rtlUseStartupReset ? 1.0f : 0.0f;
    comp.internalStates[state_keys::kStatus] = 0.0f;
    for (const auto& binding : comp.rtlPinBindings) {
      if (!binding.isInput) {
        comp.internalStates.erase(BuildRtlPortStateKey(binding.portId));
      }
    }
  }
}

void Application::UpdateRtlRuntimeInputState(PlacedComponent* comp,
                                             double step_seconds) {
  if (!comp) {
    return;
  }
  if (comp->rtlUseInternalClock && !comp->rtlClockPinName.empty() &&
      comp->rtlClockFrequencyHz > 0.0f) {
    float accumulator = comp->internalStates.count(kRtlClockAccumKey)
                            ? comp->internalStates[kRtlClockAccumKey]
                            : 0.0f;
    float level = comp->internalStates.count(kRtlClockLevelKey)
                      ? comp->internalStates[kRtlClockLevelKey]
                      : 0.0f;
    double half_period = 0.5 / static_cast<double>(comp->rtlClockFrequencyHz);
    accumulator += static_cast<float>(step_seconds);
    while (half_period > 0.0 && accumulator >= half_period) {
      accumulator -= static_cast<float>(half_period);
      level = level > 0.5f ? 0.0f : 1.0f;
    }
    comp->internalStates[kRtlClockAccumKey] = accumulator;
    comp->internalStates[kRtlClockLevelKey] = level;
  } else {
    comp->internalStates[kRtlClockAccumKey] = 0.0f;
    comp->internalStates[kRtlClockLevelKey] = 0.0f;
  }

  if (comp->rtlUseStartupReset && !comp->rtlResetPinName.empty()) {
    float elapsed = comp->internalStates.count(kRtlResetElapsedKey)
                        ? comp->internalStates[kRtlResetElapsedKey]
                        : 0.0f;
    elapsed += static_cast<float>(step_seconds * 1000.0);
    bool active = elapsed < comp->rtlResetPulseMs;
    comp->internalStates[kRtlResetElapsedKey] = elapsed;
    comp->internalStates[kRtlResetActiveKey] = active ? 1.0f : 0.0f;
  } else {
    comp->internalStates[kRtlResetElapsedKey] = 0.0f;
    comp->internalStates[kRtlResetActiveKey] = 0.0f;
  }
}

void Application::SyncRtlRuntimeComponents() {
  if (rtl_runtime_manager_) {
    rtl_runtime_manager_->SyncComponentInstances(placed_components_);
  }
}

void Application::UpdateRtlSimulation(
    const std::map<PortRef, float>& voltages) {
  if (!rtl_runtime_manager_) {
    return;
  }
  // Lock the placed_components_ vector to prevent concurrent access
  std::lock_guard<std::recursive_mutex> guard(placed_components_mutex_);
  
  SyncRtlRuntimeComponents();
  for (auto& comp : placed_components_) {
    if (!IsRtlComponent(comp) || comp.rtlModuleId.empty() ||
        comp.rtlPinBindings.empty()) {
      continue;
    }
    const RtlLibraryEntry* entry =
        rtl_project_manager_ ? rtl_project_manager_->FindEntryById(comp.rtlModuleId)
                             : nullptr;
    if (!entry || !entry->buildSuccess || !entry->analyzeSuccess ||
        entry->buildHash.empty()) {
      // Call InvalidateRtlModuleRuntimeState only once per module failure
      // Use a flag in internalStates to avoid repeated calls
      auto status_it = comp.internalStates.find(state_keys::kStatus);
      bool already_invalidated = status_it != comp.internalStates.end() && 
                                 status_it->second < -1.5f;
      if (!already_invalidated) {
        InvalidateRtlModuleRuntimeState(comp.rtlModuleId);
        comp.internalStates[state_keys::kStatus] = -2.0f; // Mark as already invalidated
      }
      continue;
    }
    UpdateRtlRuntimeInputState(&comp, kPlcScanStepSeconds * 0.25);
    std::map<int, RtlLogicValue> output_values;
    std::string diagnostics;
    bool ok = rtl_runtime_manager_->EvaluateComponent(comp, voltages,
                                                      &output_values,
                                                      &diagnostics);
    if (!ok) {
      if (!diagnostics.empty()) {
        comp.internalStates[state_keys::kStatus] = -1.0f;
      }
      continue;
    }
    comp.internalStates[state_keys::kStatus] = 1.0f;
    for (const auto& pair : output_values) {
      comp.internalStates[BuildRtlPortStateKey(pair.first)] =
          static_cast<float>(pair.second);
    }
  }
}

RtlLogicValue Application::GetRtlPortLogicValue(const PlacedComponent& comp,
                                                int portId) const {
  auto it = comp.internalStates.find(BuildRtlPortStateKey(portId));
  if (it == comp.internalStates.end()) {
    return RtlLogicValue::UNKNOWN;
  }
  int raw = static_cast<int>(it->second + 0.5f);
  switch (raw) {
    case 0:
      return RtlLogicValue::ZERO;
    case 1:
      return RtlLogicValue::ONE;
    case 3:
      return RtlLogicValue::HIGH_Z;
    default:
      return RtlLogicValue::UNKNOWN;
  }
}

bool Application::IsRtlComponent(const PlacedComponent& comp) const {
  return comp.type == ComponentType::RTL_MODULE;
}

std::vector<Port> Application::GetRuntimePortsForComponent(
    const PlacedComponent& comp) const {
  if (IsRtlComponent(comp)) {
    return comp.runtimePorts;
  }
  std::vector<Port> ports;
  const ComponentDefinition* def = GetComponentDefinition(comp.type);
  if (!def || !def->ports || def->port_count <= 0) {
    return ports;
  }
  ports.reserve(def->port_count);
  for (int i = 0; i < def->port_count; ++i) {
    const ComponentPortDef& src = def->ports[i];
    ports.push_back({src.id, src.rel_pos, src.color, src.is_input, src.type,
                     src.role ? src.role : ""});
  }
  return ports;
}

void Application::RefreshRtlComponentFromLibrary(PlacedComponent* comp) {
  if (!comp || comp->rtlModuleId.empty() || !rtl_project_manager_) {
    return;
  }
  const RtlLibraryEntry* entry =
      rtl_project_manager_->FindEntryById(comp->rtlModuleId);
  if (!entry) {
    return;
  }
  comp->customLabel = entry->displayName;
  comp->runtimePorts = RtlProjectManager::BuildRuntimePorts(entry->ports);
  comp->rtlPinBindings =
      RtlProjectManager::BuildDefaultPinBindings(entry->ports);
  if (!comp->rtlClockPinName.empty() &&
      !FindRtlPortDescriptor(entry, comp->rtlClockPinName)) {
    comp->rtlClockPinName.clear();
  }
  if (!comp->rtlResetPinName.empty() &&
      !FindRtlPortDescriptor(entry, comp->rtlResetPinName)) {
    comp->rtlResetPinName.clear();
  }
  if (!comp->rtlPowerPinName.empty() &&
      !FindRtlPortDescriptor(entry, comp->rtlPowerPinName)) {
    comp->rtlPowerPinName.clear();
  }
  if (!comp->rtlGroundPinName.empty() &&
      !FindRtlPortDescriptor(entry, comp->rtlGroundPinName)) {
    comp->rtlGroundPinName.clear();
  }
  if (comp->rtlClockPinName.empty()) {
    for (const auto& port : entry->ports) {
      if (port.isInput && port.isClock) {
        comp->rtlClockPinName = port.pinName;
        break;
      }
    }
  }
  if (comp->rtlResetPinName.empty()) {
    for (const auto& port : entry->ports) {
      if (port.isInput && port.isReset) {
        comp->rtlResetPinName = port.pinName;
        comp->rtlResetActiveLow =
            port.baseName.find("_n") != std::string::npos;
        break;
      }
    }
  }
  if (comp->rtlPowerPinName.empty()) {
    for (const auto& port : entry->ports) {
      if (LooksLikeRtlPowerPin(port)) {
        comp->rtlPowerPinName = port.pinName;
        break;
      }
    }
  }
  if (comp->rtlGroundPinName.empty()) {
    for (const auto& port : entry->ports) {
      if (LooksLikeRtlGroundPin(port)) {
        comp->rtlGroundPinName = port.pinName;
        break;
      }
    }
  }
  if (!comp->rtlClockPinName.empty()) {
    const RtlPortDescriptor* clock_port =
        FindRtlPortDescriptor(entry, comp->rtlClockPinName);
    if (!clock_port || !clock_port->isInput) {
      comp->rtlClockPinName.clear();
    }
  }
  if (!comp->rtlResetPinName.empty()) {
    const RtlPortDescriptor* reset_port =
        FindRtlPortDescriptor(entry, comp->rtlResetPinName);
    if (!reset_port || !reset_port->isInput) {
      comp->rtlResetPinName.clear();
    }
  }
  if (!comp->rtlPowerPinName.empty()) {
    const RtlPortDescriptor* power_port =
        FindRtlPortDescriptor(entry, comp->rtlPowerPinName);
    if (!power_port || !power_port->isInput) {
      comp->rtlPowerPinName.clear();
    }
  }
  if (!comp->rtlGroundPinName.empty()) {
    const RtlPortDescriptor* ground_port =
        FindRtlPortDescriptor(entry, comp->rtlGroundPinName);
    if (!ground_port || !ground_port->isInput) {
      comp->rtlGroundPinName.clear();
    }
  }
  if (comp->rtlClockPinName.empty()) {
    comp->rtlUseInternalClock = false;
  }
  if (comp->rtlResetPinName.empty()) {
    comp->rtlUseStartupReset = false;
  }
  if (comp->rtlClockFrequencyHz < 0.1f) {
    comp->rtlClockFrequencyHz = 0.1f;
  }
  if (comp->rtlResetPulseMs < 1.0f) {
    comp->rtlResetPulseMs = 1.0f;
  }
  if (comp->internalStates.count(kRtlClockAccumKey) == 0U) {
    comp->internalStates[kRtlClockAccumKey] = 0.0f;
  }
  if (comp->internalStates.count(kRtlClockLevelKey) == 0U) {
    comp->internalStates[kRtlClockLevelKey] = 0.0f;
  }
  if (comp->internalStates.count(kRtlResetElapsedKey) == 0U) {
    comp->internalStates[kRtlResetElapsedKey] = 0.0f;
  }
  if (comp->internalStates.count(kRtlResetActiveKey) == 0U) {
    comp->internalStates[kRtlResetActiveKey] =
        comp->rtlUseStartupReset ? 1.0f : 0.0f;
  }
  const size_t inputCount = std::count_if(
      entry->ports.begin(), entry->ports.end(),
      [](const RtlPortDescriptor& port) { return port.isInput; });
  const size_t outputCount = entry->ports.size() - inputCount;
  const size_t maxSideCount = std::max(inputCount, outputCount);
  comp->size.width = 260.0f;
  comp->size.height =
      std::max(140.0f, 92.0f + 18.0f * static_cast<float>(maxSideCount));
}

void Application::PlaceRtlLibraryComponent(const std::string& moduleId,
                                           Position position) {
  if (!rtl_project_manager_) {
    return;
  }
  const RtlLibraryEntry* entry = rtl_project_manager_->FindEntryById(moduleId);
  if (!entry) {
    return;
  }
  PushWiringUndoState();
  PlacedComponent comp;
  comp.instanceId = next_instance_id_++;
  comp.type = ComponentType::RTL_MODULE;
  comp.position = position;
  comp.z_order = next_z_order_++;
  comp.rtlModuleId = entry->moduleId;
  comp.rtlSourceMode = "library";
  comp.customLabel = entry->displayName;
  RefreshRtlComponentFromLibrary(&comp);
  comp.internalStates[kRtlClockAccumKey] = 0.0f;
  comp.internalStates[kRtlClockLevelKey] = 0.0f;
  comp.internalStates[kRtlResetElapsedKey] = 0.0f;
  comp.internalStates[kRtlResetActiveKey] = comp.rtlUseStartupReset ? 1.0f : 0.0f;
  comp.position.x -= comp.size.width * 0.5f;
  comp.position.y -= comp.size.height * 0.5f;
  placed_components_.push_back(comp);
}

bool Application::AnalyzeRtlModule(const std::string& moduleId) {
  if (!rtl_project_manager_) {
    return false;
  }
  InvalidateRtlModuleRuntimeState(moduleId);
  bool ok = rtl_project_manager_->Analyze(moduleId);

  const RtlLibraryEntry* entry = rtl_project_manager_->FindEntryById(moduleId);
  if (ok) {
    rtl_status_message_ = "RTL analyze completed.";
    for (auto& comp : placed_components_) {
      if (comp.rtlModuleId == moduleId) {
        RefreshRtlComponentFromLibrary(&comp);
      }
    }
  } else {
    if (entry && !entry->diagnostics.empty()) {
      rtl_status_message_ = "Analyze failed: " + entry->diagnostics;
    } else {
      rtl_status_message_ = "RTL analyze failed.";
    }
    for (auto& comp : placed_components_) {
      if (comp.rtlModuleId == moduleId) {
        comp.runtimePorts.clear();
        comp.rtlPinBindings.clear();
      }
    }
  }

  rtl_project_manager_->SaveGlobalLibrary();
  return ok;
}

bool Application::BuildRtlModule(const std::string& moduleId) {
  if (!rtl_project_manager_) {
    return false;
  }
  InvalidateRtlModuleRuntimeState(moduleId);
  bool ok = rtl_project_manager_->Build(moduleId);
  rtl_status_message_ = ok ? "RTL build completed." : "RTL build failed.";
  return ok;
}

bool Application::ExportRtlSource(const std::string& moduleId,
                                  const std::string& sourcePath) {
#ifdef _WIN32
  OPENFILENAMEA ofn;
  CHAR szFile[260] = {0};
  std::strncpy(szFile, sourcePath.c_str(), sizeof(szFile) - 1);
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = glfwGetWin32Window(window_);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "Verilog Files (*.v;*.sv)\0*.v;*.sv\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetSaveFileNameA(&ofn) != TRUE) {
    return false;
  }
  std::string content = rtl_project_manager_->ExportSourceFile(moduleId, sourcePath);
  bool ok = WriteFile(ofn.lpstrFile, content);
  rtl_status_message_ = ok ? "RTL source exported." : "Failed to export RTL source.";
  return ok;
#else
  (void)moduleId;
  (void)sourcePath;
  return false;
#endif
}

bool Application::ImportRtlSources(const std::string& moduleId) {
#ifdef _WIN32
  OPENFILENAMEA ofn;
  CHAR szFile[260] = {0};
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = glfwGetWin32Window(window_);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "Verilog Files (*.v;*.sv)\0*.v;*.sv\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameA(&ofn) != TRUE) {
    return false;
  }
  std::string content = ReadFile(ofn.lpstrFile);
  if (content.empty()) {
    rtl_status_message_ = "Failed to read RTL source.";
    return false;
  }
  std::string path = ofn.lpstrFile;
  size_t slash = path.find_last_of("/\\");
  if (slash != std::string::npos) {
    path = path.substr(slash + 1);
  }
  bool ok = rtl_project_manager_->ImportSourceFile(moduleId, path, content);
  rtl_status_message_ = ok ? "RTL source imported." : "Failed to import RTL source.";
  return ok;
#else
  (void)moduleId;
  return false;
#endif
}

bool Application::InstallRtlToolchain() {
  if (!rtl_project_manager_) {
    return false;
  }
#ifdef _WIN32
  namespace fs = std::filesystem;

  // DetectToolchain() 결과에 의존하지 말고 직접 tools 루트를 찾기
  // tools\setup_tools.ps1이 기준점
  fs::path tools_dir = fs::current_path() / "tools";
  fs::path script_path = tools_dir / "setup_tools.ps1";

  if (!fs::exists(script_path)) {
    rtl_status_message_ =
        "setup_tools.ps1 not found: " + script_path.string();
    return false;
  }

  std::string params =
      "-NoProfile -ExecutionPolicy Bypass -File \"" + script_path.string() + "\"";
  std::string tools_dir_str = tools_dir.string();

  rtl_status_message_ = "Starting setup_tools.ps1...";

  std::thread([this, params, tools_dir_str]() {
    // 1. pwsh 존재 확인
    int has_pwsh = system("where pwsh >nul 2>nul");
    
    if (has_pwsh != 0) {
      // 2. pwsh 없으면 winget으로 설치
      rtl_status_message_ = "Installing PowerShell 7...";

      int install_result = system(
          "winget install --id Microsoft.PowerShell "
          "--source winget --accept-package-agreements "
          "--accept-source-agreements -h");

      if (install_result != 0) {
        rtl_status_message_ =
            "Failed to install PowerShell 7 (winget required).";
        return;
      }
    }

    // 3. pwsh로 setup_tools.ps1 실행
    HINSTANCE result = ShellExecuteA(
        nullptr,
        "open",
        "pwsh.exe",
        params.c_str(),
        tools_dir_str.c_str(),
        SW_SHOWNORMAL);

    bool ok = reinterpret_cast<intptr_t>(result) > 32;
    if (!ok) {
      // 4. pwsh 실행 실패 시 powershell 시도
      result = ShellExecuteA(
          nullptr,
          "open",
          "powershell.exe",
          params.c_str(),
          tools_dir_str.c_str(),
          SW_SHOWNORMAL);
      ok = reinterpret_cast<intptr_t>(result) > 32;
    }

    rtl_status_message_ = ok
        ? "Running setup_tools.ps1 with PowerShell."
        : ("Failed to launch PowerShell. ShellExecute code=" +
           std::to_string(reinterpret_cast<intptr_t>(result)));
  }).detach();

  return true;
#else
  rtl_status_message_ =
      "RTL toolchain install is only supported on Windows.";
  return false;
#endif
}
  
void Application::RenderRtlToolchainPanel() {
  if (!show_rtl_toolchain_panel_) {
    return;
  }
  PollRtlToolchainRefresh();
  if (!rtl_toolchain_loading_ && !rtl_toolchain_status_valid_) {
    RequestRtlToolchainRefresh();
  }
  if (!ImGui::Begin(TR("ui.rtl.toolchain_title", "RTL Toolchain Settings"), &show_rtl_toolchain_panel_)) {
    ImVec2 window_min = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_max(window_min.x + window_size.x,
                      window_min.y + window_size.y);
    RegisterOverlayInputCaptureRect(
        window_min, window_max,
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    ImGui::End();
    return;
  }
  {
    ImVec2 window_min = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_max(window_min.x + window_size.x,
                      window_min.y + window_size.y);
    RegisterOverlayInputCaptureRect(
        window_min, window_max,
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
            ImGui::IsAnyItemActive());
  }
  if (ImGui::Button(TR("ui.common.refresh", "Refresh"))) {
    rtl_toolchain_status_valid_ = false;
    RequestRtlToolchainRefresh();
  }
  ImGui::SameLine();
  if (rtl_toolchain_loading_) {
    ImGui::TextUnformatted(
        TR("ui.rtl.loading_toolchain", "Loading RTL toolchain status..."));
  } else if (!rtl_toolchain_status_valid_) {
    ImGui::TextUnformatted(
        TR("ui.rtl.not_loaded", "Toolchain status not loaded yet."));
  }
  ImGui::Separator();

  if (!rtl_toolchain_status_valid_) {
    ImGui::TextWrapped(
        "%s", TR("ui.rtl.loading_native_toolchain",
                 "Loading native RTL toolchain state."));
    ImGui::TextWrapped("%s", rtl_status_message_.c_str());
    ImGui::End();
    return;
  }

  const RtlToolchainStatus& status = rtl_toolchain_status_;
  char buf[512];

  plc::FormatString(buf, sizeof(buf), "ui.rtl.verilator_status",
                    "Verilator: %s",
                    status.verilatorFound ? TR("ui.rtl.found", "Found")
                                          : TR("ui.rtl.missing", "Missing"));
  ImGui::TextUnformatted(buf);

  plc::FormatString(buf, sizeof(buf), "ui.rtl.compiler_status",
                    "Compiler (g++): %s",
                    status.compilerFound ? TR("ui.rtl.found", "Found")
                                         : TR("ui.rtl.missing", "Missing"));
  ImGui::TextUnformatted(buf);

  plc::FormatString(buf, sizeof(buf), "ui.rtl.make_status", "make: %s",
                    status.makeFound ? TR("ui.rtl.found", "Found")
                                     : TR("ui.rtl.missing", "Missing"));
  ImGui::TextUnformatted(buf);

  ImGui::TextWrapped("%s", status.description.c_str());
  if (status.verilatorFound) {
    ImGui::TextWrapped("verilator: %s", status.verilatorPath.c_str());
  }
  if (status.compilerFound) {
    ImGui::TextWrapped("compiler: %s", status.compilerPath.c_str());
  }
  if (status.makeFound) {
    ImGui::TextWrapped("make: %s", status.makePath.c_str());
  }
  if (!status.mingwBinPath.empty()) {
    ImGui::TextWrapped("mingw64/bin: %s", status.mingwBinPath.c_str());
  }
  if (!status.msys2Root.empty()) {
    ImGui::TextWrapped("MSYS2 root: %s", status.msys2Root.c_str());
  }

  ImGui::Separator();
  if (ImGui::Button(TR("ui.rtl.install_toolchain",
                       "Install Native RTL Toolchain"))) {
    InstallRtlToolchain();
    rtl_toolchain_status_valid_ = false;
    RequestRtlToolchainRefresh();
  }

  ImGui::End();
}

void Application::RenderRtlEditor() {
  PollRtlAsyncTask();
  if (!rtl_project_manager_ || selected_rtl_module_id_.empty()) {
    return;
  }
  RtlLibraryEntry* entry = rtl_project_manager_->FindEntryById(selected_rtl_module_id_);
  if (!entry) {
    return;
  }

  ImGui::Text("%s", TR("ui.rtl.top_module", "Top Module"));
  char topModuleBuf[128] = {0};
  std::strncpy(topModuleBuf, entry->topModule.c_str(), sizeof(topModuleBuf) - 1);
  if (ImGui::InputText("##RtlTopModule", topModuleBuf, sizeof(topModuleBuf))) {
    std::string updatedTopModule = topModuleBuf;
    if (entry->topModule != updatedTopModule) {
      entry->topModule = updatedTopModule;
      entry->analyzeSuccess = false;
      entry->buildSuccess = false;
      entry->testbenchSuccess = false;
      entry->buildHash.clear();
      entry->buildDiagnostics.clear();
      entry->testbenchDiagnostics.clear();
      entry->testbenchBuildLog.clear();
      entry->testbenchRunLog.clear();
      entry->testbenchVcdPath.clear();
      InvalidateRtlModuleRuntimeState(entry->moduleId);
    }
  }
  ImGui::Text("%s", TR("ui.rtl.testbench_top_module", "Testbench Top Module"));
  char testbenchTopModuleBuf[128] = {0};
  std::strncpy(testbenchTopModuleBuf, entry->testbenchTopModule.c_str(),
                sizeof(testbenchTopModuleBuf) - 1);
  if (ImGui::InputText("##RtlTestbenchTopModule", testbenchTopModuleBuf,
                        sizeof(testbenchTopModuleBuf))) {
    std::string updatedTopModule = TrimCopy(testbenchTopModuleBuf);
    if (entry->testbenchTopModule != updatedTopModule) {
      entry->testbenchTopModule = updatedTopModule;
      entry->testbenchSuccess = false;
      entry->testbenchDiagnostics.clear();
      entry->testbenchBuildLog.clear();
      entry->testbenchRunLog.clear();
      entry->testbenchVcdPath.clear();
    }
  }
  if (entry->sourceFiles.empty()) {
    entry->sourceFiles.push_back({entry->topFile.empty() ? "top.v" : entry->topFile,
                                   ""});
  }
  char displayNameBuf[128] = {0};
  std::strncpy(displayNameBuf, entry->displayName.c_str(),
               sizeof(displayNameBuf) - 1);
  if (ImGui::InputText(TR("ui.rtl.display_name", "Display Name"),
                       displayNameBuf, sizeof(displayNameBuf))) {
    std::string updatedDisplayName = TrimCopy(displayNameBuf);
    if (updatedDisplayName.empty()) {
      updatedDisplayName = TR("ui.rtl.new_module", "RTL Module");
    }
    if (entry->displayName != updatedDisplayName) {
      entry->displayName = updatedDisplayName;
    }
  }
  if (selected_rtl_source_path_.empty() ||
      std::none_of(entry->sourceFiles.begin(), entry->sourceFiles.end(),
                   [&](const RtlSourceFile& file) {
                     return file.path == selected_rtl_source_path_;
                   })) {
    SelectRtlSource(entry->sourceFiles.front(), &selected_rtl_source_path_,
                    &rtl_editor_buffer_);
  }
  if (ImGui::Button(TR("ui.rtl.import", "Import"))) {
    if (ImportRtlSources(entry->moduleId)) {
      InvalidateRtlModuleRuntimeState(entry->moduleId);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.rtl.export", "Export"))) {
    ExportRtlSource(entry->moduleId, selected_rtl_source_path_);
  }
  ImGui::SameLine();
  if (rtl_async_task_running_) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(TR("ui.rtl.analyze", "Analyze"))) {
    if (rtl_project_manager_) {
      rtl_project_manager_->UpdateSourceFile(entry->moduleId, selected_rtl_source_path_,
                                             rtl_editor_buffer_);
    }
    RequestRtlAnalyzeAsync(entry->moduleId);
  }
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.rtl.build", "Build"))) {
    if (rtl_project_manager_) {
      rtl_project_manager_->UpdateSourceFile(entry->moduleId, selected_rtl_source_path_,
                                             rtl_editor_buffer_);
    }
    RequestRtlBuildAsync(entry->moduleId);
  }
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.rtl.run_testbench", "Run Testbench"))) {
    if (rtl_project_manager_) {
      rtl_project_manager_->UpdateSourceFile(entry->moduleId, selected_rtl_source_path_,
                                             rtl_editor_buffer_);
    }
    RequestRtlTestbenchAsync(entry->moduleId);
  }

  if (rtl_async_task_running_) {
    ImGui::EndDisabled();
  }

  ImGui::Separator();
  const float bottom_panel_height = 260.0f;
  const float splitter_gap = 8.0f;
  const float total_available_height = ImGui::GetContentRegionAvail().y;
  const float main_panel_height =
      std::max(420.0f, total_available_height - bottom_panel_height - splitter_gap);
  const float resize_grip_size = 14.0f;
  ImGui::BeginChild("RtlEditorLayout", ImVec2(0.0f, main_panel_height), false);
  ImGui::BeginChild("RtlSourceSidebar", ImVec2(420.0f, 0.0f), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::TextDisabled("%s", TR("ui.rtl.sources", "Sources"));
  ImGui::Separator();
  std::map<std::string, std::vector<const RtlSourceFile*>> grouped_files;
  for (const auto& file : entry->sourceFiles) {
    grouped_files[SourceDirectory(file.path)].push_back(&file);
  }
  auto render_source_entry = [&](const RtlSourceFile& file) {
    ImGui::PushID(file.path.c_str());
    bool selected = selected_rtl_source_path_ == file.path;
    std::string file_label = SourceBaseName(file.path);
    if (entry->topFile == file.path) {
      file_label += "  [TOP]";
    }
    if (entry->testbenchTopFile == file.path) {
      file_label += "  [TB]";
    }
    if (ImGui::Selectable(file_label.c_str(), selected)) {
      SelectRtlSource(file, &selected_rtl_source_path_, &rtl_editor_buffer_);
    }
    if (ImGui::BeginPopupContextItem("RtlSourceContext")) {
      if (ImGui::MenuItem(TR("ui.rtl.add_file", "Add File"))) {
        if (AddNewRtlSource(rtl_project_manager_.get(), entry,
                            &selected_rtl_source_path_,
                            &rtl_editor_buffer_)) {
          InvalidateRtlModuleRuntimeState(entry->moduleId);
        }
      }
      if (ImGui::MenuItem(TR("ui.rtl.rename", "Rename"))) {
        rtl_rename_source_path_ = file.path;
        rtl_rename_source_buffer_ = SourceBaseName(file.path);
        ImGui::OpenPopup("Rename RTL Source");
      }
      const bool can_delete = entry->sourceFiles.size() > 1U;
      if (!can_delete) {
        ImGui::BeginDisabled();
      }
      if (ImGui::MenuItem(TR("ui.rtl.delete", "Delete"))) {
        rtl_project_manager_->RemoveSourceFile(entry->moduleId, file.path);
        InvalidateRtlModuleRuntimeState(entry->moduleId);
        if (selected_rtl_source_path_ == file.path &&
            !entry->sourceFiles.empty()) {
          SelectRtlSource(entry->sourceFiles.front(),
                          &selected_rtl_source_path_, &rtl_editor_buffer_);
        }
      }
      if (!can_delete) {
        ImGui::EndDisabled();
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  };
  auto root_it = grouped_files.find("");
  if (root_it != grouped_files.end()) {
    for (const RtlSourceFile* file : root_it->second) {
      render_source_entry(*file);
      ImGui::Spacing();
    }
    grouped_files.erase(root_it);
  }
  for (const auto& group : grouped_files) {
    if (ImGui::TreeNodeEx(group.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const RtlSourceFile* file : group.second) {
        render_source_entry(*file);
        ImGui::Spacing();
      }
      ImGui::TreePop();
    }
  }
  if (ImGui::BeginPopupContextWindow("RtlSourceSidebarContext",
                                     ImGuiPopupFlags_NoOpenOverItems |
                                         ImGuiPopupFlags_MouseButtonRight)) {
    if (ImGui::MenuItem(TR("ui.rtl.add_file", "Add File"))) {
      if (AddNewRtlSource(rtl_project_manager_.get(), entry,
                          &selected_rtl_source_path_,
                          &rtl_editor_buffer_)) {
        InvalidateRtlModuleRuntimeState(entry->moduleId);
      }
    }
    if (!entry->testbenchTopFile.empty() &&
        ImGui::MenuItem(TR("ui.rtl.clear_tb_file", "Clear TB File"))) {
      entry->testbenchTopFile.clear();
      entry->testbenchSuccess = false;
      entry->testbenchDiagnostics.clear();
      entry->testbenchBuildLog.clear();
      entry->testbenchRunLog.clear();
      entry->testbenchVcdPath.clear();
    }
    ImGui::EndPopup();
  }
  const RtlSourceFile* selected_source = nullptr;
  for (const auto& file : entry->sourceFiles) {
    if (file.path == selected_rtl_source_path_) {
      selected_source = &file;
      break;
    }
  }
  if (selected_source) {
    ImGui::Separator();
    ImGui::TextDisabled("%s", SourceBaseName(selected_source->path).c_str());
    bool is_top = entry->topFile == selected_source->path;
    if (is_top) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(TR("ui.rtl.set_as_top", "Set As TOP"))) {
      entry->topFile = selected_source->path;
      entry->analyzeSuccess = false;
      entry->buildSuccess = false;
      entry->buildHash.clear();
      entry->buildDiagnostics.clear();
      entry->testbenchBuildLog.clear();
      entry->testbenchRunLog.clear();
      InvalidateRtlModuleRuntimeState(entry->moduleId);
    }
    if (is_top) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
    bool is_tb = entry->testbenchTopFile == selected_source->path;
    if (is_tb) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(TR("ui.rtl.set_as_tb", "Set As TB"))) {
      entry->testbenchTopFile = selected_source->path;
      entry->testbenchSuccess = false;
      entry->testbenchDiagnostics.clear();
      entry->testbenchBuildLog.clear();
      entry->testbenchRunLog.clear();
      entry->testbenchVcdPath.clear();
    }
    if (is_tb) {
      ImGui::EndDisabled();
    }
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("RtlEditorThemeFrame",
                    ImVec2(0.0f, 0.0f), true,
                    ImGuiWindowFlags_NoScrollbar);
  ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(108, 108, 108, 96));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(132, 132, 132, 132));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(148, 148, 148, 156));
  ImGui::BeginChild("RtlSourceTabsStrip", ImVec2(0.0f, 38.0f), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (const auto& file : entry->sourceFiles) {
    ImGui::PushID(file.path.c_str());
    std::string tab_title = SourceBaseName(file.path);
    bool selected = selected_rtl_source_path_ == file.path;
    if (ImGui::Selectable(tab_title.c_str(), selected, 0, ImVec2(160.0f, 0.0f))) {
      SelectRtlSource(file, &selected_rtl_source_path_, &rtl_editor_buffer_);
    }
    if (ImGui::BeginPopupContextItem("RtlSourceTabContext")) {
      if (ImGui::MenuItem(TR("ui.rtl.add_file", "Add File"))) {
        if (AddNewRtlSource(rtl_project_manager_.get(), entry,
                            &selected_rtl_source_path_,
                            &rtl_editor_buffer_)) {
          InvalidateRtlModuleRuntimeState(entry->moduleId);
        }
      }
      if (ImGui::MenuItem(TR("ui.rtl.rename", "Rename"))) {
        rtl_rename_source_path_ = file.path;
        rtl_rename_source_buffer_ = SourceBaseName(file.path);
        ImGui::OpenPopup("Rename RTL Source");
      }
      const bool can_delete = entry->sourceFiles.size() > 1U;
      if (!can_delete) {
        ImGui::BeginDisabled();
      }
      if (ImGui::MenuItem(TR("ui.rtl.delete", "Delete"))) {
        rtl_project_manager_->RemoveSourceFile(entry->moduleId, file.path);
        InvalidateRtlModuleRuntimeState(entry->moduleId);
        if (selected_rtl_source_path_ == file.path && !entry->sourceFiles.empty()) {
          SelectRtlSource(entry->sourceFiles.front(), &selected_rtl_source_path_,
                          &rtl_editor_buffer_);
        }
      }
      if (!can_delete) {
        ImGui::EndDisabled();
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::SameLine();
  }
  ImGui::NewLine();
  ImGui::EndChild();
  ImGui::PopStyleColor(3);
  const RtlSourceFile* current_source = nullptr;
  for (const auto& file : entry->sourceFiles) {
    if (file.path == selected_rtl_source_path_) {
      current_source = &file;
      break;
    }
  }
  if (current_source) {
    char curFileBuf[512];
    plc::FormatString(curFileBuf, sizeof(curFileBuf), "ui.rtl.current_file",
                      "Current: %s%s%s", SourceBaseName(current_source->path).c_str(),
                      entry->topFile == current_source->path ? " | TOP" : "",
                      entry->testbenchTopFile == current_source->path ? " | TB"
                                                                      : "");
    ImGui::TextDisabled("%s", curFileBuf);
  }
  const std::vector<RtlEditorDiagnosticMarker>* editor_markers = nullptr;
  if (current_source) {
    editor_markers = &GetCachedRtlEditorDiagnostics(current_source->path,
                                                    entry->diagnostics,
                                                    entry->buildDiagnostics);
  }
  float max_editor_height =
      std::max(260.0f, ImGui::GetContentRegionAvail().y - resize_grip_size - 18.0f);
  rtl_editor_height_ =
      std::clamp(rtl_editor_height_, 220.0f, max_editor_height);
  const float available_height = rtl_editor_height_;

  const RtlEditorTheme& editor_theme = GetRtlEditorTheme();
  PushJetBrainsEditorTheme(editor_theme);
  if (rtl_editor_font_) {
    ImGui::PushFont(rtl_editor_font_);
  }
  bool editorChanged =
      InputTextMultilineString("##RtlEditor", &rtl_editor_buffer_,
                               ImVec2(-1.0f, available_height));
  ImVec2 editor_scroll(0.0f, 0.0f);
  if (ImGuiWindow* frame_window = ImGui::GetCurrentWindow();
      frame_window && frame_window->DC.ChildWindows.Size > 0) {
    ImGuiWindow* editor_window =
        frame_window->DC.ChildWindows[frame_window->DC.ChildWindows.Size - 1];
    if (editor_window) {
      editor_scroll = editor_window->Scroll;
    }
  }
  ImVec2 editor_min = ImGui::GetItemRectMin();
  ImVec2 editor_max = ImGui::GetItemRectMax();
  RenderLineNumberGutter(rtl_editor_buffer_, editor_min, editor_max,
                         rtl_editor_font_ ? rtl_editor_font_ : ImGui::GetFont(),
                         editor_theme, editor_scroll);
  if (editor_markers && !editor_markers->empty()) {
    RenderDiagnosticMarkers(*editor_markers, editor_min, editor_max,
                            rtl_editor_font_ ? rtl_editor_font_
                                             : ImGui::GetFont(),
                            editor_scroll);
  }
  RenderVerilogHighlightOverlay(rtl_editor_buffer_, editor_min, editor_max,
                                rtl_editor_font_ ? rtl_editor_font_
                                                 : ImGui::GetFont(),
                                editor_theme, editor_scroll);
  if (rtl_editor_font_) {
    ImGui::PopFont();
  }
  PopJetBrainsEditorTheme();
  ImVec2 frame_max = ImGui::GetItemRectMax();
  ImVec2 child_pos = ImGui::GetWindowPos();
  ImVec2 child_size = ImGui::GetWindowSize();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 grip_min(child_pos.x + child_size.x - resize_grip_size - 6.0f,
                  frame_max.y + 4.0f);
  ImVec2 grip_max(child_pos.x + child_size.x - 6.0f,
                  frame_max.y + 4.0f + resize_grip_size);
  ImGui::SetCursorScreenPos(grip_min);
  ImGui::InvisibleButton("##RtlEditorResizeGrip",
                         ImVec2(grip_max.x - grip_min.x,
                                grip_max.y - grip_min.y));
  if (ImGui::IsItemActive()) {
    rtl_editor_height_ =
        std::clamp(rtl_editor_height_ + ImGui::GetIO().MouseDelta.y,
                   320.0f, max_editor_height);
  }
  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  ImU32 grip_color = ImGui::IsItemActive()
                         ? IM_COL32(104, 151, 187, 255)
                         : (ImGui::IsItemHovered()
                                ? IM_COL32(88, 110, 130, 255)
                                : IM_COL32(62, 72, 84, 255));
  draw_list->AddLine(ImVec2(grip_min.x + 3.0f, grip_max.y - 3.0f),
                     ImVec2(grip_max.x - 3.0f, grip_min.y + 3.0f),
                     grip_color, 1.5f);
  draw_list->AddLine(ImVec2(grip_min.x + 6.0f, grip_max.y - 3.0f),
                     ImVec2(grip_max.x - 3.0f, grip_min.y + 6.0f),
                     grip_color, 1.5f);
  draw_list->AddLine(ImVec2(grip_min.x + 9.0f, grip_max.y - 3.0f),
                     ImVec2(grip_max.x - 3.0f, grip_min.y + 9.0f),
                     grip_color, 1.5f);
  ImGui::EndChild();
  ImGui::EndChild();
  if (editorChanged) {
    rtl_project_manager_->UpdateSourceFile(entry->moduleId,
                                           selected_rtl_source_path_,
                                           rtl_editor_buffer_);
    InvalidateRtlModuleRuntimeState(entry->moduleId);
  }

  ImGuiID editor_id = ImGui::GetID("##RtlEditor");
  ImVec2 cursor_line_col(1.0f, 1.0f);
  if (ImGuiInputTextState* state = ImGui::GetInputTextState(editor_id)) {
    cursor_line_col = ComputeLineColumnForOffset(
        rtl_editor_buffer_, static_cast<size_t>(state->GetCursorPos()));
  }
  ImGui::TextDisabled("Ln %.0f, Col %.0f | %zu chars",
                      cursor_line_col.x, cursor_line_col.y,
                      rtl_editor_buffer_.size());

  if (!rtl_rename_source_path_.empty()) {
    ImGui::OpenPopup("Rename RTL Source");
  }
  if (ImGui::BeginPopupModal("Rename RTL Source", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    char rename_buf[256] = {0};
    std::strncpy(rename_buf, rtl_rename_source_buffer_.c_str(),
                 sizeof(rename_buf) - 1);
    if (ImGui::InputText("New Name", rename_buf, sizeof(rename_buf))) {
      rtl_rename_source_buffer_ = rename_buf;
    }
    if (ImGui::Button("Rename")) {
      const std::string new_path = BuildRenamedSourcePath(
          rtl_rename_source_path_, rtl_rename_source_buffer_);
      if (!new_path.empty() &&
          rtl_project_manager_->RenameSourceFile(entry->moduleId,
                                                 rtl_rename_source_path_,
                                                 new_path)) {
        if (selected_rtl_source_path_ == rtl_rename_source_path_) {
          selected_rtl_source_path_ = new_path;
        }
        rtl_rename_source_path_.clear();
        rtl_rename_source_buffer_.clear();
        InvalidateRtlModuleRuntimeState(entry->moduleId);
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      rtl_rename_source_path_.clear();
      rtl_rename_source_buffer_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::Dummy(ImVec2(0.0f, splitter_gap));
  ImGui::BeginChild("RtlBottomPanel", ImVec2(0.0f, 0.0f), true);

  if (rtl_async_task_running_) {
    ImGui::TextUnformatted(TR("ui.rtl.running_task", "Running RTL task in background..."));
    ImGui::Separator();
  }

  if (entry->analyzeSuccess) {
    ImGui::TextColored(ImVec4(0.15f, 0.65f, 0.22f, 1.0f),
                       "%s", TR("ui.rtl.analyze_status_ok", "Analyze Status: Success"));
  } else if (!entry->diagnostics.empty()) {
    ImGui::TextColored(ImVec4(0.80f, 0.18f, 0.18f, 1.0f),
                       "%s", TR("ui.rtl.analyze_status_fail", "Analyze Status: Failed"));
  }

  if (entry->buildSuccess) {
    ImGui::TextColored(ImVec4(0.15f, 0.65f, 0.22f, 1.0f),
                       "%s", TR("ui.rtl.build_status_ok", "Build Status: Success"));
  } else if (!entry->buildDiagnostics.empty()) {
    ImGui::TextColored(ImVec4(0.80f, 0.18f, 0.18f, 1.0f),
                       "%s", TR("ui.rtl.build_status_fail", "Build Status: Failed"));
  }
  const TestbenchVerdict testbench_verdict = GetTestbenchVerdict(*entry);
  if (testbench_verdict != TestbenchVerdict::kNotRun) {
    char verdictBuf[256];
    plc::FormatString(verdictBuf, sizeof(verdictBuf), "ui.rtl.testbench_status",
                      "Testbench Status: %s",
                      GetTestbenchVerdictLabel(testbench_verdict));
    ImGui::TextColored(GetTestbenchVerdictColor(testbench_verdict),
                       "%s", verdictBuf);
  }

  bool canEnableComponent = entry->analyzeSuccess && entry->buildSuccess;
  if (!canEnableComponent) {
    ImGui::BeginDisabled();
  }
  bool enabled = entry->componentEnabled;
  if (ImGui::Checkbox(TR("ui.rtl.enable_as_component", "Enable As Component"),
                      &enabled)) {
    entry->componentEnabled = enabled;
  }
  if (!canEnableComponent) {
    ImGui::EndDisabled();
  }

  const bool has_any_logs = !entry->diagnostics.empty() ||
                            !entry->buildDiagnostics.empty() ||
                            !entry->testbenchDiagnostics.empty() ||
                            !entry->testbenchBuildLog.empty() ||
                            !entry->testbenchRunLog.empty();
  if (has_any_logs) {
    ImGui::Separator();
  }

  // LOGS AREA - Inside the proper child window
  if (!entry->diagnostics.empty()) {
    ImGui::Separator();
    bool is_error = !entry->analyzeSuccess;
    ImGuiTreeNodeFlags flags = is_error ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    if (ImGui::CollapsingHeader(TR("ui.rtl.analyze_log", "Analyze Log"), flags)) {
      if (is_error) {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", entry->diagnostics.c_str());
      } else {
        ImGui::TextWrapped("%s", entry->diagnostics.c_str());
      }
    }
  }

  if (!entry->buildDiagnostics.empty()) {
    ImGui::Separator();
    bool is_error = !entry->buildSuccess;
    ImGuiTreeNodeFlags flags = is_error ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    if (ImGui::CollapsingHeader(TR("ui.rtl.build_log", "Build Log"), flags)) {
      if (is_error) {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", entry->buildDiagnostics.c_str());
      } else {
        ImGui::TextWrapped("%s", entry->buildDiagnostics.c_str());
      }
    }
  }

  if (testbench_verdict != TestbenchVerdict::kNotRun) {
    const size_t error_count =
        CountOccurrences(entry->testbenchRunLog, "$error") +
        CountOccurrences(entry->testbenchRunLog, "%Error") +
        CountOccurrences(entry->testbenchRunLog, "$fatal") +
        CountOccurrences(entry->testbenchRunLog, "Assertion failed") +
        CountOccurrences(entry->testbenchRunLog, "assertion failed");
    const size_t warning_count =
        CountOccurrences(entry->testbenchRunLog, "%Warning") +
        CountOccurrences(entry->testbenchRunLog, " warning:") +
        CountOccurrences(entry->testbenchRunLog, "Warning:");

    char summaryBuf[512];
    plc::FormatString(summaryBuf, sizeof(summaryBuf), "ui.rtl.testbench_summary_fmt",
                      "%s | Errors: %zu | Warnings: %zu",
                      GetTestbenchVerdictLabel(testbench_verdict),
                      error_count, warning_count);
    std::string summary = summaryBuf;
    if (!entry->testbenchVcdPath.empty()) {
      summary += TR("ui.rtl.waveform_ready", " | Waveform ready");
    }
    ImGui::TextColored(GetTestbenchVerdictColor(testbench_verdict), "%s",
                       summary.c_str());
  }
  if (!entry->testbenchDiagnostics.empty()) {
    ImGuiTreeNodeFlags flags = testbench_verdict != TestbenchVerdict::kPass
                                   ? ImGuiTreeNodeFlags_DefaultOpen
                                   : 0;
    if (ImGui::CollapsingHeader(TR("ui.rtl.testbench_summary", "Testbench Summary"), flags)) {
      ImGui::TextWrapped("%s", entry->testbenchDiagnostics.c_str());
    }
  }
  if (!entry->testbenchBuildLog.empty()) {
    ImGuiTreeNodeFlags flags = testbench_verdict != TestbenchVerdict::kPass
                                   ? ImGuiTreeNodeFlags_DefaultOpen
                                   : 0;
    if (ImGui::CollapsingHeader(TR("ui.rtl.testbench_build_log", "Testbench Build Log"), flags)) {
      ImGui::TextWrapped("%s", entry->testbenchBuildLog.c_str());
    }
  }
  if (!entry->testbenchRunLog.empty()) {
    ImGuiTreeNodeFlags flags = testbench_verdict != TestbenchVerdict::kPass
                                   ? ImGuiTreeNodeFlags_DefaultOpen
                                   : 0;
    if (ImGui::CollapsingHeader(TR("ui.rtl.testbench_run_log", "Testbench Run Log"), flags)) {
      ImGui::TextWrapped("%s", entry->testbenchRunLog.c_str());
    }
  }
  if (!entry->testbenchVcdPath.empty()) {
    char waveBuf[1024];
    plc::FormatString(waveBuf, sizeof(waveBuf), "ui.rtl.waveform", "Waveform: %s",
                      entry->testbenchVcdPath.c_str());
    ImGui::TextWrapped("%s", waveBuf);
#ifdef _WIN32
    if (ImGui::Button(TR("ui.rtl.open_waveform", "Open Waveform"))) {
      ShellExecuteA(nullptr, "open", entry->testbenchVcdPath.c_str(), nullptr,
                    nullptr, SW_SHOWNORMAL);
    }
#endif
  }
  if (!entry->ports.empty()) {
    ImGui::Separator();
    ImGui::Text("%s", TR("ui.rtl.ports", "Ports"));
    for (const auto& port : entry->ports) {
      ImGui::BulletText("%s %s", port.isInput ? TR("ui.wiring.direction_input", "IN") : TR("ui.wiring.direction_output", "OUT"),
                        port.pinName.c_str());
    }
  }
  if (editor_markers && !editor_markers->empty()) {
    ImGui::Separator();
    ImGui::Text("%s", TR("ui.rtl.current_diagnostics", "Current File Diagnostics"));
    for (const auto& marker : *editor_markers) {
      ImVec4 color = marker.isError ? ImVec4(0.86f, 0.34f, 0.34f, 1.0f)
                                    : ImVec4(0.86f, 0.68f, 0.30f, 1.0f);
      ImGui::TextColored(color, "L%d:C%d %s", marker.line, marker.column,
                         marker.message.empty() ? TR("ui.rtl.no_message", "(no message)")
                                                : marker.message.c_str());
    }
  }
  ImGui::EndChild();
}

void Application::RenderRtlLibraryPanel() {
  if (!show_rtl_library_panel_) {
    return;
  }
  ImGui::SetNextWindowSize(ImVec2(1680.0f, 980.0f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(TR("ui.rtl.library_title", "RTL Library"),
                    &show_rtl_library_panel_)) {
    ImVec2 window_min = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_max(window_min.x + window_size.x,
                      window_min.y + window_size.y);
    RegisterOverlayInputCaptureRect(
        window_min, window_max,
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    ImGui::End();
    return;
  }
  {
    ImVec2 window_min = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_max(window_min.x + window_size.x,
                      window_min.y + window_size.y);
    RegisterOverlayInputCaptureRect(
        window_min, window_max,
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
            ImGui::IsAnyItemActive());
  }

  if (ImGui::Button(TR("ui.rtl.new_module", "New RTL Module"))) {
    RtlLibraryEntry* entry =
        rtl_project_manager_->CreateEntry(TR("ui.rtl.new_module", "New RTL Module"));
    if (entry) {
      selected_rtl_module_id_ = entry->moduleId;
      selected_rtl_source_path_ = entry->topFile;
      rtl_editor_buffer_ = entry->sourceFiles.empty() ? "" : entry->sourceFiles.front().content;
    }
  }
  ImGui::SameLine();
  if (!selected_rtl_module_id_.empty() &&
      ImGui::Button(TR("ui.rtl.duplicate", "Duplicate"))) {
    if (rtl_project_manager_->DuplicateEntry(selected_rtl_module_id_)) {
      selected_rtl_module_id_.clear();
      selected_rtl_source_path_.clear();
      rtl_editor_buffer_.clear();
      rtl_status_message_ = TR("ui.rtl.status_module_duplicated", "RTL module duplicated.");
    }
  }
  ImGui::SameLine();
  if (!selected_rtl_module_id_.empty() &&
      ImGui::Button(TR("ui.rtl.delete", "Delete"))) {
    if (rtl_project_manager_->DeleteEntry(selected_rtl_module_id_)) {
      selected_rtl_module_id_.clear();
      selected_rtl_source_path_.clear();
      rtl_editor_buffer_.clear();
      rtl_status_message_ = TR("ui.rtl.status_module_deleted", "RTL module deleted.");
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.rtl.toolchain", "Toolchain"))) {
    show_rtl_toolchain_panel_ = true;
  }
  ImGui::Separator();

  ImGui::BeginChild("RtlLibraryList", ImVec2(320.0f, 0.0f), true);
  for (auto& entry : rtl_project_manager_->GetLibrary()) {
    bool selected = selected_rtl_module_id_ == entry.moduleId;
    if (ImGui::Selectable(entry.displayName.c_str(), selected)) {
      selected_rtl_module_id_ = entry.moduleId;
      if (!entry.sourceFiles.empty()) {
        SelectRtlSource(entry.sourceFiles.front(), &selected_rtl_source_path_,
                        &rtl_editor_buffer_);
      }
    }
    char statusBuf[128];
    plc::FormatString(
        statusBuf, sizeof(statusBuf), "ui.rtl.testbench_summary_mini_fmt",
        "%s | %s",
        entry.analyzeSuccess ? TR("ui.rtl.analyzed", "analyzed")
                             : TR("ui.rtl.not_analyzed", "not analyzed"),
        entry.buildSuccess ? TR("ui.rtl.built", "built")
                           : TR("ui.rtl.not_built", "not built"));
    ImGui::TextDisabled("%s", statusBuf);
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("RtlEditorPanel", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), true);
  RenderRtlEditor();
  ImGui::EndChild();

  // Status Bar at the bottom
  if (!rtl_status_message_.empty()) {
    ImGui::Separator();
    ImVec4 color = ImVec4(0.2f, 0.7f, 0.2f, 1.0f); // Success green
    if (rtl_status_message_.find("failed") != std::string::npos || 
        rtl_status_message_.find("Failed") != std::string::npos) {
      color = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Error red
    } else if (rtl_status_message_.find("...") != std::string::npos) {
      color = ImVec4(0.3f, 0.6f, 0.9f, 1.0f); // Info blue
    }
    ImGui::TextColored(color, "%s", rtl_status_message_.c_str());
  }

  ImGui::End();
}

}  // namespace plc
