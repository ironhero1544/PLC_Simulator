// app_render.cpp
//
// Application rendering and UI.

#include "plc_emulator/core/application.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "plc_emulator/lang/lang_manager.h"

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#endif

#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace plc {
namespace {

void SetHighPrecisionTimer(bool enable, bool* active) {
#ifdef _WIN32
  if (!active) {
    return;
  }
  if (enable && !(*active)) {
    timeBeginPeriod(1);
    *active = true;
  } else if (!enable && *active) {
    timeEndPeriod(1);
    *active = false;
  }
#else
  (void)enable;
  (void)active;
#endif
}

double GetMonitorRefreshRateForWindow(GLFWwindow* window) {
  if (!window) {
    return 0.0;
  }
  int monitor_count = 0;
  GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
  if (!monitors || monitor_count <= 0) {
    return 0.0;
  }

  int win_x = 0;
  int win_y = 0;
  int win_w = 0;
  int win_h = 0;
  glfwGetWindowPos(window, &win_x, &win_y);
  glfwGetWindowSize(window, &win_w, &win_h);

  GLFWmonitor* best_monitor = nullptr;
  int best_overlap = 0;

  for (int i = 0; i < monitor_count; ++i) {
    GLFWmonitor* monitor = monitors[i];
    if (!monitor) {
      continue;
    }
    int mon_x = 0;
    int mon_y = 0;
    glfwGetMonitorPos(monitor, &mon_x, &mon_y);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
      continue;
    }
    int mon_w = mode->width;
    int mon_h = mode->height;

    int overlap_w =
        std::max(0, std::min(win_x + win_w, mon_x + mon_w) -
                        std::max(win_x, mon_x));
    int overlap_h =
        std::max(0, std::min(win_y + win_h, mon_y + mon_h) -
                        std::max(win_y, mon_y));
    int overlap = overlap_w * overlap_h;
    if (overlap > best_overlap) {
      best_overlap = overlap;
      best_monitor = monitor;
    }
  }

  if (!best_monitor) {
    best_monitor = glfwGetPrimaryMonitor();
  }
  if (!best_monitor) {
    return 0.0;
  }
  const GLFWvidmode* best_mode = glfwGetVideoMode(best_monitor);
  if (!best_mode || best_mode->refreshRate <= 0) {
    return 0.0;
  }
  return static_cast<double>(best_mode->refreshRate);
}

bool EqualsIgnoreCase(const char* lhs, const char* rhs) {
  if (!lhs || !rhs) {
    return false;
  }
  while (*lhs && *rhs) {
    unsigned char l = static_cast<unsigned char>(*lhs);
    unsigned char r = static_cast<unsigned char>(*rhs);
    if (std::tolower(l) != std::tolower(r)) {
      return false;
    }
    ++lhs;
    ++rhs;
  }
  return *lhs == '\0' && *rhs == '\0';
}

void OpenExternalUrl(const char* url) {
  if (!url) {
    return;
  }
#ifdef _WIN32
  ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
  (void)url;
#endif
}

}  // namespace

void Application::Render() {
  glClearColor(0.94f, 0.94f, 0.94f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  RenderUI();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  glfwSwapBuffers(window_);
  if (!ui_settings_.vsync_enabled && ui_settings_.frame_limit_enabled) {
    using clock = std::chrono::steady_clock;
    double refresh = GetMonitorRefreshRateForWindow(window_);
    if (refresh > 1.0) {
      monitor_refresh_rate_ = refresh;
    }
    double target_hz =
        (monitor_refresh_rate_ > 1.0) ? monitor_refresh_rate_ : 60.0;
    auto target_dt = std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(1.0 / target_hz));
    auto now = clock::now();
    if (!render_time_initialized_) {
      last_render_time_ = now;
      next_frame_time_ = now + target_dt;
      render_time_initialized_ = true;
    } else {
      if (now < next_frame_time_) {
        auto remaining = next_frame_time_ - now;
        const auto spin_margin = std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<double>(0.001));
        if (remaining > spin_margin) {
          std::this_thread::sleep_for(remaining - spin_margin);
        }
        while (clock::now() < next_frame_time_) {
          std::this_thread::yield();
        }
        now = clock::now();
      } else {
        const auto reset_threshold =
            std::chrono::duration_cast<clock::duration>(
                std::chrono::duration<double>(0.25));
        if (now - next_frame_time_ > reset_threshold) {
          next_frame_time_ = now + target_dt;
        }
      }
      last_render_time_ = now;
      next_frame_time_ += target_dt;
    }
  } else {
    render_time_initialized_ = false;
  }
}

void Application::RenderUI() {
  if (warmup_stage_ != WarmupStage::kComplete) {
    RenderSplashScreen();
    return;
  }

  float menu_height = 0.0f;
  RenderMainMenuBar(&menu_height);

  ImVec2 display_size = ImGui::GetIO().DisplaySize;
  if (menu_height > display_size.y) {
    menu_height = 0.0f;
  }

  ImGui::SetNextWindowPos(ImVec2(0, menu_height));
  ImGui::SetNextWindowSize(
      ImVec2(display_size.x, display_size.y - menu_height));

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  if (ImGui::Begin("MainAppWindow", nullptr, window_flags)) {
    RenderHeader();

    switch (current_mode_) {
      case Mode::WIRING:
        RenderWiringModeUI();
        break;
      case Mode::PROGRAMMING:
        if (programming_mode_) {
          programming_mode_->RenderProgrammingModeUI(is_plc_running_);
        }
        break;
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(3);
  if (current_mode_ == Mode::PROGRAMMING && programming_mode_) {
    RenderPLCDebugPanel();
  }
  RenderPhysicsWarningDialog();
  RenderShortcutHelpDialog();
}

void Application::RenderSplashScreen() {
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 display = io.DisplaySize;

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(display);
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

  if (ImGui::Begin("SplashScreen", nullptr, flags)) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetWindowPos();
    ImVec2 p1 = ImVec2(p0.x + display.x, p0.y + display.y);
    draw_list->AddRectFilled(p0, p1, IM_COL32(245, 245, 245, 255));

    float t = static_cast<float>(ImGui::GetTime());
    int dots = static_cast<int>(t * 2.0f) % 4;
    char message[64] = "Starting";
    char cache_line[64] = "Preparing caches";
    std::snprintf(message + std::strlen(message),
                  sizeof(message) - std::strlen(message),
                  "%.*s", dots, "...");
    std::snprintf(cache_line + std::strlen(cache_line),
                  sizeof(cache_line) - std::strlen(cache_line),
                  "%.*s", dots, "...");

    const char* title = TR("ui.header.title", "FX3U PLC Simulator");
    ImVec2 title_size = ImGui::CalcTextSize(title);
    ImVec2 line1_size = ImGui::CalcTextSize(message);
    ImVec2 line2_size = ImGui::CalcTextSize(cache_line);

    float center_x = display.x * 0.5f;
    float center_y = display.y * 0.5f;
    ImGui::SetCursorPos(
        ImVec2(center_x - title_size.x * 0.5f, center_y - 60.0f));
    ImGui::TextUnformatted(title);

    ImGui::SetCursorPos(
        ImVec2(center_x - line1_size.x * 0.5f, center_y - 10.0f));
    ImGui::TextUnformatted(message);

    ImGui::SetCursorPos(
        ImVec2(center_x - line2_size.x * 0.5f, center_y + 15.0f));
    ImGui::TextUnformatted(cache_line);
  }
  ImGui::End();
}

void Application::RenderWiringModeUI() {
  RenderToolbar();
  RenderMainArea();
}

void Application::RenderMainMenuBar(float* out_height) {
  if (out_height) {
    *out_height = 0.0f;
  }

  if (!ImGui::BeginMainMenuBar()) {
    return;
  }

  if (out_height) {
    *out_height = ImGui::GetFrameHeight();
  }

  if (ImGui::BeginMenu(TR("menu.settings", "Settings"))) {
    if (ImGui::BeginMenu(TR("menu.language", "Language"))) {
      const char* user_lang = GetUserLanguageCode();
      const char* active_lang = GetActiveLanguageCode();
      const char* selected = (user_lang && user_lang[0] != '\0')
                                 ? user_lang
                                 : active_lang;

      bool is_en = std::strcmp(selected, "en") == 0;
      bool is_ko = std::strcmp(selected, "ko") == 0;
      bool is_ja = std::strcmp(selected, "ja") == 0;

      if (ImGui::MenuItem(TR("lang.english", "English"), nullptr, is_en)) {
        SetUserLanguageCode("en");
      }
      if (ImGui::MenuItem(TR("lang.korean", "Korean"), nullptr, is_ko)) {
        SetUserLanguageCode("ko");
      }
      if (ImGui::MenuItem(TR("lang.japanese", "Japanese"), nullptr, is_ja)) {
        SetUserLanguageCode("ja");
      }
      ImGui::EndMenu();
    }
    RenderUiSettingsMenu();
    if (IsLanguageRestartRequired()) {
      ImGui::TextDisabled(
          "%s", TR("lang.restart_required", "Restart required to apply."));
      if (ImGui::MenuItem(TR("lang.restart_now", "Restart now..."))) {
        show_restart_popup_ = true;
      }
    }
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();

  if (show_restart_popup_) {
    ImGui::OpenPopup("Restart Application");
    show_restart_popup_ = false;
  }

  bool restart_popup_open = true;
  if (ImGui::BeginPopupModal("Restart Application", &restart_popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        TR("lang.restart_prompt", "Restart application now?"));
    ImGui::Spacing();
    if (ImGui::Button(TR("lang.restart_yes", "Restart"), ImVec2(120, 0))) {
      RestartApplication();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("lang.restart_no", "Cancel"), ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void Application::RenderUiSettingsMenu() {
  if (!ImGui::BeginMenu(TR("menu.ui_settings", "UI"))) {
    return;
  }

  float ui_scale = ui_settings_.ui_scale;
  float font_scale = ui_settings_.font_scale;
  float layout_scale = ui_settings_.layout_scale;
  bool vsync_enabled = ui_settings_.vsync_enabled;
  bool frame_limit_enabled = ui_settings_.frame_limit_enabled;
  bool changed = false;
  bool restart_required = false;

  if (ImGui::SliderFloat(TR("ui.settings.ui_scale", "UI Scale"), &ui_scale,
                         0.75f, 1.5f, "%.2f")) {
    ui_settings_.ui_scale = ui_scale;
    changed = true;
    restart_required = true;
  }
  if (ImGui::SliderFloat(TR("ui.settings.font_scale", "Font Scale"), &font_scale,
                         0.75f, 1.5f, "%.2f")) {
    ui_settings_.font_scale = font_scale;
    changed = true;
    restart_required = true;
  }
  if (ImGui::SliderFloat(TR("ui.settings.layout_scale", "Layout Scale"),
                         &layout_scale, 0.75f, 1.5f, "%.2f")) {
    ui_settings_.layout_scale = layout_scale;
    changed = true;
    restart_required = true;
  }
  if (ImGui::Checkbox(TR("ui.settings.vsync", "VSync"), &vsync_enabled)) {
    ui_settings_.vsync_enabled = vsync_enabled;
    glfwSwapInterval(vsync_enabled ? 1 : 0);
    render_time_initialized_ = false;
    SetHighPrecisionTimer(ui_settings_.frame_limit_enabled &&
                              !ui_settings_.vsync_enabled,
                          &high_precision_timer_active_);
    changed = true;
  }
  if (ImGui::Checkbox(TR("ui.settings.frame_limit",
                         "Frame Limit (Monitor)"),
                      &frame_limit_enabled)) {
    ui_settings_.frame_limit_enabled = frame_limit_enabled;
    render_time_initialized_ = false;
    SetHighPrecisionTimer(ui_settings_.frame_limit_enabled &&
                              !ui_settings_.vsync_enabled,
                          &high_precision_timer_active_);
    changed = true;
  }
  if (monitor_refresh_rate_ > 1.0) {
    char refresh_buf[64] = {0};
    FormatString(refresh_buf, sizeof(refresh_buf),
                 "ui.settings.refresh_rate_fmt",
                 "Monitor Refresh: %d Hz",
                 static_cast<int>(monitor_refresh_rate_ + 0.5));
    ImGui::TextDisabled("%s", refresh_buf);
  }
  char resolution_buf[96] = {0};
  FormatString(resolution_buf, sizeof(resolution_buf),
               "ui.settings.auto_resolution_scale_fmt",
               "Auto Resolution Scale: %.2fx (1920x1080 base)",
               GetResolutionScale());
  ImGui::TextDisabled("%s", resolution_buf);

  if (ImGui::Button(TR("ui.settings.reset_defaults", "Reset Defaults"))) {
    SetDefaultUiSettings(&ui_settings_);
    SaveUiSettings(ui_settings_);
    ui_settings_.restart_required = true;
    changed = false;
    glfwSwapInterval(ui_settings_.vsync_enabled ? 1 : 0);
    render_time_initialized_ = false;
    SetHighPrecisionTimer(ui_settings_.frame_limit_enabled &&
                              !ui_settings_.vsync_enabled,
                          &high_precision_timer_active_);
  }

  if (changed) {
    SaveUiSettings(ui_settings_);
    if (restart_required) {
      MarkUiSettingsRestartRequired(&ui_settings_);
    }
  }

  if (ui_settings_.restart_required) {
    ImGui::TextDisabled("%s", TR("ui.settings.restart_required",
                                 "Restart required to apply UI changes."));
    if (ImGui::Button(TR("ui.settings.restart_now", "Restart now..."))) {
      show_restart_popup_ = true;
    }
  }

  ImGui::EndMenu();
}

void Application::RenderHeader() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

  if (ImGui::BeginChild("Header", ImVec2(0, 80 * layout_scale), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::Columns(3, "HeaderColumns", false);

    ImGui::SetColumnWidth(0, 300 * layout_scale);

    ImGui::SetCursorPos(ImVec2(20 * layout_scale, 25 * layout_scale));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    ImGui::Text("%s", TR("ui.header.title", "FX3U PLC Simulator"));

    ImGui::PopStyleColor();

    ImGui::NextColumn();

    float centerX =
        ImGui::GetCursorPosX() + ImGui::GetColumnWidth() / 2 -
        115 * layout_scale;

    ImGui::SetCursorPosX(centerX);

    ImGui::SetCursorPosY(25 * layout_scale);

    bool isWiringMode = (current_mode_ == Mode::WIRING);

    ImGui::PushStyleColor(ImGuiCol_Button,
                          isWiringMode ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f)
                                       : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, isWiringMode
                                             ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                             : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

    if (ImGui::Button(TR("ui.header.mode_wiring", "Wiring"),
                      ImVec2(100 * layout_scale, 30 * layout_scale))) {
      current_mode_ = Mode::WIRING;
    }

    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,
                          !isWiringMode ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f)
                                        : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, !isWiringMode
                                             ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                             : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

    if (ImGui::Button(TR("ui.header.mode_programming", "Programming"),
                      ImVec2(120 * layout_scale, 30 * layout_scale))) {
      current_mode_ = Mode::PROGRAMMING;
    }

    ImGui::PopStyleColor(2);

    ImGui::NextColumn();

    float rightX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() -
                   150 * layout_scale;

    ImGui::SetCursorPosX(rightX);

    ImGui::SetCursorPosY(25 * layout_scale);

    ImVec4 statusColor = is_plc_running_ ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f)
                                        : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

    const char* statusText = is_plc_running_
                                 ? TR("ui.header.status_run", "[RUN]")
                                 : TR("ui.header.status_stop", "[STOP]");

    ImGui::PushStyleColor(ImGuiCol_Text, statusColor);

    ImGui::TextUnformatted(statusText);

    ImGui::PopStyleColor();

    ImGui::SameLine();

    if (ImGui::Button(is_plc_running_
                          ? TR("ui.header.btn_stop", "STOP")
                          : TR("ui.header.btn_run", "RUN"),
                      ImVec2(80 * layout_scale, 30 * layout_scale))) {
      if (!is_plc_running_) {
        std::cout << "[INFO] PLC RUN: Initializing PLC system..." << std::endl;

        SyncLadderProgramFromProgrammingMode();

        plc_device_states_.clear();
        for (int i = 0; i < 16; ++i) {
          std::string i_str = std::to_string(i);
          plc_device_states_["X" + i_str] = false;
          plc_device_states_["M" + i_str] = false;
        }

        for (auto& [address, timer] : plc_timer_states_) {
          timer.value = 0;
          timer.done = false;
          timer.enabled = false;
        }
        for (auto& [address, counter] : plc_counter_states_) {
          counter.value = 0;
          counter.done = false;
          counter.lastPower = false;
        }

        if (compiled_plc_executor_) {
          std::cout << "[INFO] Resetting CompiledPLCExecutor memory..."
                    << std::endl;
          compiled_plc_executor_->ResetMemory();

          std::cout << "[INFO] Recompiling ladder program for execution..."
                    << std::endl;
          CompileAndLoadLadderProgram();
        }

        if (programming_mode_) {
          std::cout << "[INFO] Activating monitor mode..." << std::endl;
        }

        std::cout
            << "[INFO] PLC RUN: System initialized and ready for execution!"
            << std::endl;
      } else {
        std::cout << "[INFO] PLC STOP: Stopping execution..." << std::endl;

        for (int i = 0; i < 16; ++i) {
          SetPlcDeviceState("Y" + std::to_string(i), false);
        }

        std::cout << "[INFO] PLC STOP: All outputs deactivated" << std::endl;
      }

      is_plc_running_ = !is_plc_running_;
    }

    ImGui::Columns(1);
  }

  ImGui::EndChild();

  ImGui::PopStyleVar();

  ImGui::PopStyleColor();
}

void Application::RenderToolbar() {
  const float layout_scale = GetLayoutScale();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));

  if (ImGui::BeginChild("Toolbar", ImVec2(0, 60 * layout_scale), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPos(ImVec2(20 * layout_scale, 15 * layout_scale));

    const char* toolNames[] = {
        TR("ui.toolbar.tool_select", "Select"),
        TR("ui.toolbar.tool_pneumatic", "Pneumatic"),
        TR("ui.toolbar.tool_electric", "Electric"),
        TR("ui.toolbar.tool_tag", "Tag"),
    };

    const ToolType toolTypes[] = {ToolType::SELECT, ToolType::PNEUMATIC,
                                  ToolType::ELECTRIC, ToolType::TAG};

    for (int i = 0; i < 4; i++) {
      if (i > 0) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
      }

      bool is_selected = (current_tool_ == toolTypes[i]);

      ImGui::PushStyleColor(ImGuiCol_Button,
                            is_selected ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f)
                                        : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

      ImGui::PushStyleColor(ImGuiCol_Text,
                            is_selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                        : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

      ImVec2 button_size(180 * layout_scale, 30 * layout_scale);
      if (toolTypes[i] == ToolType::TAG) {
        if (ImGui::Button(toolNames[i], button_size)) {
          current_tool_ = toolTypes[i];
        }
      } else {
        if (ImGui::Button(toolNames[i], button_size)) {
          current_tool_ = toolTypes[i];
        }
      }

      ImGui::PopStyleColor(2);
    }

    ImGui::SameLine();

    const float wiring_button_width = 140.0f * layout_scale;
    const float wiring_button_height = 30.0f * layout_scale;
    const float wiring_button_spacing = 10.0f * layout_scale;
    const float wiring_total_width =
        wiring_button_width * 2 + wiring_button_spacing;
    float wiring_right_start =
        ImGui::GetWindowWidth() - wiring_total_width - 20.0f * layout_scale;
    if (wiring_right_start < ImGui::GetCursorPosX()) {
      wiring_right_start = ImGui::GetCursorPosX();
    }
    ImGui::SetCursorPosX(wiring_right_start);

    if (ImGui::Button(TR("ui.toolbar.save_layout", "Save"),
                      ImVec2(wiring_button_width, wiring_button_height))) {
#ifdef _WIN32
      OPENFILENAMEA ofn;
      CHAR szFile[260] = "layout.json";
      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = glfwGetWin32Window(window_);
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter =
          "PLC Layout JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = NULL;
      ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
      if (GetSaveFileNameA(&ofn) == TRUE) {
        std::string save_path = ofn.lpstrFile;
        if (save_path.find(".json") == std::string::npos) {
          save_path += ".json";
        }
        bool success = SaveLayout(save_path);
        if (success) {
          std::cout << "[INFO] Layout saved: " << save_path << std::endl;
        } else {
          std::cout << "[ERROR] Layout save failed: " << save_path
                    << std::endl;
        }
      }
#else
      bool success = SaveLayout("layout.json");
      if (success) {
        std::cout << "[INFO] Layout saved: layout.json" << std::endl;
      } else {
        std::cout << "[ERROR] Layout save failed: layout.json" << std::endl;
      }
#endif
    }

    ImGui::SameLine();

    if (ImGui::Button(TR("ui.toolbar.load_layout", "Load"),
                      ImVec2(wiring_button_width, wiring_button_height))) {
#ifdef _WIN32
      OPENFILENAMEA ofn;
      CHAR szFile[260] = {0};
      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = glfwGetWin32Window(window_);
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter =
          "PLC Layout JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = NULL;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
      if (GetOpenFileNameA(&ofn) == TRUE) {
        bool success = LoadLayout(ofn.lpstrFile);
        if (success) {
          std::cout << "[INFO] Layout loaded: " << ofn.lpstrFile << std::endl;
        } else {
          std::cout << "[ERROR] Layout load failed: " << ofn.lpstrFile
                    << std::endl;
        }
      }
#else
      bool success = LoadLayout("layout.json");
      if (success) {
        std::cout << "[INFO] Layout loaded: layout.json" << std::endl;
      } else {
        std::cout << "[ERROR] Layout load failed: layout.json" << std::endl;
      }
#endif
    }
  }

  ImGui::EndChild();

  ImGui::PopStyleColor();
}

void Application::RenderMainArea() {
  const float layout_scale = GetLayoutScale();
  const float status_bar_height = 25.0f * layout_scale;
  ImVec2 available = ImGui::GetContentRegionAvail();
  float main_area_height = available.y - status_bar_height;
  if (main_area_height < 0.0f)
    main_area_height = 0.0f;

  if (ImGui::BeginChild("WiringMainArea", ImVec2(0, main_area_height), false,
                        ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::Columns(2, "MainColumns", true);

    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.75f);

    RenderWiringCanvas();

    ImGui::NextColumn();

    RenderComponentList();

    ImGui::Columns(1);
  }
  ImGui::EndChild();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));

  if (ImGui::BeginChild("StatusBar", ImVec2(0, status_bar_height), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::SetCursorPosY(5 * layout_scale);
    ImGui::SetCursorPosX(10 * layout_scale);

    char status_buf[256] = {0};
    FormatString(status_buf, sizeof(status_buf), "ui.status.zoom_info",
                 "Zoom %.1fx | Pos: (%.0f, %.0f) | Components: %zu | Wires: %zu",
                 camera_zoom_, camera_offset_.x, camera_offset_.y,
                 placed_components_.size(), wires_.size());
    ImGui::TextUnformatted(status_buf);

    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200 * layout_scale);

    const char* tool_name = "";

    switch (current_tool_) {
      case ToolType::SELECT:
        tool_name = "Select";
        break;
      case ToolType::TAG:
        tool_name = "Tag";
        break;
      case ToolType::PNEUMATIC:
        tool_name = "Pneumatic";
        break;
      case ToolType::ELECTRIC:
        tool_name = "Electric";
        break;
    }

    char tool_buf[128] = {0};
    FormatString(tool_buf, sizeof(tool_buf), "ui.status.tool_fmt", "Tool: %s",
                 tool_name);
    ImGui::TextUnformatted(tool_buf);
  }

  ImGui::EndChild();

  ImGui::PopStyleColor();
}

void Application::RenderShortcutHelpDialog() {
  const char* popup_id = "Shortcut Guide";
  static std::string secret_buffer;
  constexpr const char* kSecret = "ironhero";
  if (show_shortcut_help_popup_) {
    ImGui::OpenPopup(popup_id);
    show_shortcut_help_popup_ = false;
    secret_buffer.clear();
  }

  bool popup_open = true;
  if (ImGui::BeginPopupModal(popup_id, &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        TR("ui.help.shortcuts.title", "Shortcut Guide"));
    ImGui::Separator();
    ImGui::TextUnformatted(
        TR("ui.help.shortcuts.open_help", "Open this help"));
    ImGui::BulletText("%s",
                      TR("ui.help.shortcuts.open_1", "Ctrl + ?"));
    ImGui::BulletText("%s",
                      TR("ui.help.shortcuts.open_2", "Ctrl + /"));

    ImGui::Spacing();
    ImGui::TextUnformatted(TR("ui.help.shortcuts.common", "Common"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.common_run_stop",
                               "RUN/STOP: Use the header button"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.common_switch_mode",
                               "Switch mode: Wiring / Programming buttons"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.common_save_load",
                               "Save/Load: Use toolbar buttons in Wiring mode"));

    ImGui::Spacing();
    ImGui::TextUnformatted(
        TR("ui.help.shortcuts.quick_start", "Quick Start"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_wiring_drag",
                               "Drag components from the list to the canvas"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_wiring_connect",
                               "Connect ports by dragging from one port to another"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_tools",
                               "Tool buttons: Select / Pneumatic / Electric / Tag"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_tool_select",
                               "Select: select/move components and wires"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_tool_pneumatic",
                               "Pneumatic: create pneumatic wires"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_tool_electric",
                               "Electric: create electrical wires"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.quick_tool_tag",
                               "Tag: add/edit wire tags"));

    ImGui::Spacing();
    ImGui::TextUnformatted(
        TR("ui.help.shortcuts.wiring_mode", "Wiring mode"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_rotate",
                               "Rotate component: R (Shift+R: reverse)"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_delete",
                               "Delete selected: Delete"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_select_toggle",
                               "Select tool: Ctrl+Click toggles components/wires"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_select_box",
                               "Select tool: Drag empty canvas to box-select"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_select_box_add",
                               "Select tool: Ctrl+Drag empty canvas adds/removes by box"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_tool_cycle",
                               "Q: cycle tools (Select -> Pneumatic -> Electric -> Tag)"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_tag",
                               "Tag tool: click wire to add/edit wire tag"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_zoom",
                               "Zoom: Mouse wheel / Trackpad pinch"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_pan",
                               "Pan: Middle drag / Alt + Right drag"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_trackpad_pan",
                               "Trackpad pan: Ctrl + Scroll"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_z_order",
                               "Z-order: use component context menu"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.wiring_z_order_items",
                               "Bring to Front / Send to Back / Bring Forward / Send Backward"));

    ImGui::Spacing();
    ImGui::TextUnformatted(
        TR("ui.help.shortcuts.programming_mode", "Programming mode"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_f2_f3",
                               "F2/F3: Monitor/Edit"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_f5_f6_f7",
                               "F5/F6/F7: XIC/XIO/Coil"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_f9",
                               "F9 / Shift+F9: Add/Remove vertical"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_edit_keys",
                               "Delete / Insert / Enter"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_undo_redo",
                               "Ctrl+Z / Ctrl+Y: Undo/Redo"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_ctrl_arrow",
                               "Ctrl+Arrow: Toggle line path"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_addr_example",
                               "Address example: Y0, T1 K10, C2 K5, SET M0, RST Y0"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_compile",
                               "Use Save / Load / Compile buttons on the top bar"));
    ImGui::BulletText("%s", TR("ui.help.shortcuts.prog_monitor",
                               "Monitor mode: inspect X/Y/M/T/C states in real time"));

    // Hidden easter egg: type "ironhero" while this popup is open.
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) &&
        !secret_buffer.empty()) {
      secret_buffer.pop_back();
    }
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
      ImWchar wc = io.InputQueueCharacters[i];
      if (wc >= 0 && wc <= 127 &&
          std::isalpha(static_cast<unsigned char>(wc))) {
        secret_buffer.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(wc))));
      }
    }
    if (secret_buffer.size() > 64) {
      secret_buffer.erase(0, secret_buffer.size() - 64);
    }
    if (secret_buffer.size() >= std::strlen(kSecret)) {
      std::string tail =
          secret_buffer.substr(secret_buffer.size() - std::strlen(kSecret));
      if (EqualsIgnoreCase(tail.c_str(), kSecret)) {
        OpenExternalUrl("https://github.com/ironhero1544");
        secret_buffer.clear();
        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::Spacing();
    if (ImGui::Button(TR("ui.help.shortcuts.close", "Close"),
                      ImVec2(120.0f, 0.0f))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

}  // namespace plc
