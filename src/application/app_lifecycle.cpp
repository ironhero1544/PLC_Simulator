// app_lifecycle.cpp
//
// Application lifecycle and initialization.

#include "plc_emulator/core/application.h"
#include "plc_emulator/core/win32_input.h"
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/core/ui_settings.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <mmsystem.h>
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <cstring>
#include <iostream>

namespace plc {
namespace {
Application* g_physics_warning_app = nullptr;

void PhysicsWarningDialogCallback(const char* message) {
  if (g_physics_warning_app && message) {
    g_physics_warning_app->QueuePhysicsWarningDialog(message);
  }
}

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
  int center_x = win_x + win_w / 2;
  int center_y = win_y + win_h / 2;

  GLFWmonitor* best_monitor = monitors[0];
  int best_overlap = -1;

  for (int i = 0; i < monitor_count; ++i) {
    GLFWmonitor* monitor = monitors[i];
    if (!monitor) {
      continue;
    }
    int mx = 0;
    int my = 0;
    glfwGetMonitorPos(monitor, &mx, &my);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
      continue;
    }
    int mw = mode->width;
    int mh = mode->height;

    bool contains_center = center_x >= mx && center_x < (mx + mw) &&
                           center_y >= my && center_y < (my + mh);
    if (contains_center) {
      best_monitor = monitor;
      break;
    }

    int overlap_w = std::max(0, std::min(win_x + win_w, mx + mw) -
                                    std::max(win_x, mx));
    int overlap_h = std::max(0, std::min(win_y + win_h, my + mh) -
                                    std::max(win_y, my));
    int overlap = overlap_w * overlap_h;
    if (overlap > best_overlap) {
      best_overlap = overlap;
      best_monitor = monitor;
    }
  }

  const GLFWvidmode* mode = glfwGetVideoMode(best_monitor);
  if (mode && mode->refreshRate > 0) {
    return static_cast<double>(mode->refreshRate);
  }
  return 0.0;
}

}  // namespace

Application::Application(bool enable_debug_mode)

    : window_(nullptr),

      current_mode_(Mode::WIRING),

      current_tool_(ToolType::SELECT),

      running_(false),

      is_plc_running_(false),

      selected_component_id_(-1),

      next_instance_id_(0),
      next_z_order_(0),

      is_dragging_(false),

      dragged_component_index_(-1),

      is_moving_component_(false),

      moving_component_id_(-1),
      component_list_view_mode_(ComponentListViewMode::NAME),
      component_list_filter_(ComponentListFilter::ALL),

      next_wire_id_(0),

      selected_wire_id_(-1),
      show_tag_popup_(false),
      tag_edit_wire_id_(-1),
      tag_color_index_(0),

      is_connecting_(false),

      wire_start_component_id_(-1),

      wire_start_port_id_(-1),

      wire_edit_mode_(WireEditMode::NONE),

      editing_wire_id_(-1),

      editing_point_index_(-1),

      camera_zoom_(1.0f),

      programming_mode_(std::make_unique<ProgrammingMode>(this)),

      simulator_core_(std::make_unique<PLCSimulatorCore>()),

      physics_engine_(nullptr),

      project_file_manager_(std::make_unique<ProjectFileManager>()),
      physics_accumulator_(0.0),
      plc_accumulator_(0.0),
      physics_time_initialized_(false),
      render_time_initialized_(false),
      high_precision_timer_active_(false),
      monitor_refresh_rate_(0.0),
      warmup_stage_(WarmupStage::kComplete),
      advanced_physics_disabled_(false),
      advanced_disable_components_(0),
      advanced_disable_wires_(0),
      advanced_accumulator_(0.0),
      advanced_network_built_(false),
      advanced_topology_hash_(0),
      advanced_disable_topology_hash_(0),
      advanced_last_component_count_(0),
      advanced_last_wire_count_(0),
      advanced_last_input_hash_(0),
      advanced_inputs_dirty_(true),
      physics_stage_total_ms_(0.0),
      physics_stage_electrical_ms_(0.0),
      physics_stage_component_ms_(0.0),
      physics_stage_pneumatic_ms_(0.0),
      physics_stage_actuator_ms_(0.0),
      physics_stage_plc_ms_(0.0),
      physics_stage_advanced_build_ms_(0.0),
      physics_stage_advanced_solve_ms_(0.0),
      physics_stage_advanced_sync_ms_(0.0),
      physics_stage_advanced_electrical_ms_(0.0),
      physics_stage_advanced_pneumatic_ms_(0.0),
      physics_stage_advanced_mechanical_ms_(0.0),
      physics_stage_steps_(0),
      physics_stage_advanced_steps_(0),
      debug_mode_requested_(enable_debug_mode),
      debug_mode_(false),
      enable_debug_logging_(false),
      debug_update_counter_(0),
      show_physics_warning_dialog_(false),
      physics_warning_message_(""),
      show_restart_popup_(false),
      show_component_context_menu_(false),
      context_menu_component_id_(-1),
      context_menu_pos_(ImVec2(0.0f, 0.0f)) {

  drag_start_offset_ = {0, 0};

  wire_start_pos_ = {0, 0};

  wire_current_pos_ = {0, 0};
  std::memset(tag_text_buffer_, 0, sizeof(tag_text_buffer_));

  camera_offset_ = {0.0f, 0.0f};
  last_pointer_world_pos_ = {0.0f, 0.0f};
  last_pointer_move_time_ = 0.0;
  last_auto_waypoint_time_ = 0.0;
  touch_gesture_active_ = false;
  touch_zoom_delta_ = 0.0f;
  touch_pan_delta_ = {0.0f, 0.0f};
  last_pointer_is_pan_input_ = false;
  touch_anchor_screen_pos_ = {0.0f, 0.0f};
  prev_right_button_down_ = false;
  prev_side_button_down_ = false;
      win32_right_click_ = false;
  win32_side_click_ = false;
  win32_side_down_ = false;
  ui_settings_ = {};
  SetDefaultUiSettings(&ui_settings_);

  canvas_top_left_ = {0.0f, 0.0f};

  canvas_size_ = {0.0f, 0.0f};

  snap_settings_.enableGridSnap = true;

  snap_settings_.enablePortSnap = true;

  snap_settings_.enableVerticalSnap = true;

  snap_settings_.enableHorizontalSnap = true;

  snap_settings_.snapDistance = 15.0f;
  snap_settings_.gridSize = 25.0f;

  g_physics_warning_app = this;
  SetPhysicsWarningCallback(PhysicsWarningDialogCallback);
}

Application::~Application() {
  if (g_physics_warning_app == this) {
    g_physics_warning_app = nullptr;
  }
  SetPhysicsWarningCallback(nullptr);
}

bool Application::Initialize() {
  InitializeLanguage();
  LoadUiSettings(&ui_settings_);


  if (!InitializeWindow())

    return false;

  if (!InitializeImGui())

    return false;

  if (programming_mode_)

    programming_mode_->Initialize();

  if (simulator_core_ && !simulator_core_->Initialize()) {

    std::cerr << "Failed to initialize PLCSimulatorCore!" << std::endl;

    return false;

  }

  physics_engine_ = CreateDefaultPhysicsEngine();

  if (!physics_engine_) {

    std::cerr << "Failed to create PhysicsEngine!" << std::endl;

    return false;

  }



  std::cout << "[Application] PhysicsEngine initialized successfully"

            << std::endl;

  compiled_plc_executor_ = std::make_unique<CompiledPLCExecutor>();

  compiled_plc_executor_->SetDebugMode(false);

  compiled_plc_executor_->ResetMemory();
  compiled_plc_executor_->SetContinuousExecution(true, kPlcScanStepMs);



  std::cout << "[Application] Compiled PLC Executor initialized successfully"

            << std::endl;

  std::cout << std::endl;

  std::cout << "=====================================" << std::endl;

  std::cout << "=====================================" << std::endl;

  std::cout << "  PLC Simulator Controls" << std::endl;

  std::cout << "=====================================" << std::endl;

  std::cout << "Debug: launch with --Debug" << std::endl;

  std::cout << std::endl;

  if (debug_mode_requested_) {
    ToggleDebugMode();
    DebugLog("[DEBUG] Debug mode requested via --Debug");
  }

  RequestCacheWarmup();
  running_ = true;

  return true;

}

void Application::Run() {

  while (running_ && !glfwWindowShouldClose(window_)) {

    ProcessInput();

    Update();

    Render();

  }

}

void Application::Shutdown() {

  if (physics_engine_) {

    DestroyPhysicsEngine(physics_engine_);

    physics_engine_ = nullptr;

    std::cout << "[Application] PhysicsEngine destroyed" << std::endl;

  }



  if (simulator_core_) {

    simulator_core_->Shutdown();

  }



  Cleanup();

}

void Application::Update() {

  if (ProcessCacheWarmup()) {
    return;
  }

  if (programming_mode_ && current_mode_ == Mode::PROGRAMMING) {



    programming_mode_->UpdateWithPlcState(is_plc_running_);

  }







  UpdatePortPositions();





  UpdatePhysicsLod();
  UpdatePhysics();






  if (debug_mode_) {

    UpdateDebugLogging();

  }

}

void Application::RequestCacheWarmup() {
  warmup_stage_ = WarmupStage::kPending;
  advanced_network_built_ = false;
  advanced_physics_disabled_ = false;
  advanced_disable_components_ = 0;
  advanced_disable_wires_ = 0;
  advanced_accumulator_ = 0.0;
  advanced_last_component_count_ = 0;
  advanced_last_wire_count_ = 0;
  advanced_topology_hash_ = 0;
  advanced_disable_topology_hash_ = 0;
  advanced_last_input_hash_ = 0;
  advanced_inputs_dirty_ = true;
  physics_stage_total_ms_ = 0.0;
  physics_stage_electrical_ms_ = 0.0;
  physics_stage_component_ms_ = 0.0;
  physics_stage_pneumatic_ms_ = 0.0;
  physics_stage_actuator_ms_ = 0.0;
  physics_stage_plc_ms_ = 0.0;
  physics_stage_advanced_build_ms_ = 0.0;
  physics_stage_advanced_solve_ms_ = 0.0;
  physics_stage_advanced_sync_ms_ = 0.0;
  physics_stage_advanced_electrical_ms_ = 0.0;
  physics_stage_advanced_pneumatic_ms_ = 0.0;
  physics_stage_advanced_mechanical_ms_ = 0.0;
  physics_stage_steps_ = 0;
  physics_stage_advanced_steps_ = 0;
}

bool Application::ProcessCacheWarmup() {
  if (warmup_stage_ == WarmupStage::kComplete) {
    return false;
  }

  if (warmup_stage_ == WarmupStage::kPending) {
    warmup_stage_ = WarmupStage::kRunning;
    return true;
  }

  if (warmup_stage_ == WarmupStage::kRunning) {
    RunCacheWarmup();
    warmup_stage_ = WarmupStage::kFinishing;
    return true;
  }

  if (warmup_stage_ == WarmupStage::kFinishing) {
    warmup_stage_ = WarmupStage::kComplete;
    return false;
  }

  warmup_stage_ = WarmupStage::kComplete;
  return false;
}

void Application::RunCacheWarmup() {
  UpdatePortPositions();
  SimulateElectrical();
  SimulatePneumatic();
  UpdateComponentLogic();
  UpdateSensorsBox2d();
  UpdateWorkpieceInteractionsBox2d(0.0f, true);
  PrewarmAdvancedPhysicsNetworks();
}

bool Application::InitializeWindow() {

  if (!glfwInit()) {

    printf("Failed to initialize GLFW!\n");

    return false;

  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(1920, 1080, "FX3U PLC Simulator",

                              nullptr, nullptr);

  if (!window_) {

    glfwTerminate();

    return false;

  }

  glfwMakeContextCurrent(window_);

  glfwSwapInterval(ui_settings_.vsync_enabled ? 1 : 0);

  monitor_refresh_rate_ = GetMonitorRefreshRateForWindow(window_);
  SetHighPrecisionTimer(ui_settings_.frame_limit_enabled &&
                            !ui_settings_.vsync_enabled,
                        &high_precision_timer_active_);

  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {

    return false;

  }

#ifdef _WIN32
  InstallWin32InputHook(window_, this);
#endif



  printf("=== PLC Simulator Started ===\n");

  return true;

}

bool Application::InitializeImGui() {

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  (void)io;



  ImFontConfig font_config;

  font_config.PixelSnapH = true;
  float font_size = 16.0f * ui_settings_.font_scale;

  static const ImWchar korean_ranges[] = {

      0x0020, 0x00FF, 0x2500, 0x257F, 0x3131, 0x3163, 0xAC00,

      0xD7A3, 0x2010, 0x205E, 0xFF01, 0xFF5E, 0,

  };

  io.Fonts->AddFontFromFileTTF("unifont.ttf", font_size, &font_config,
                               korean_ranges);
  ImFontConfig merge_config = font_config;
  merge_config.MergeMode = true;
  const ImWchar* jp_ranges = io.Fonts->GetGlyphRangesJapanese();
  io.Fonts->AddFontFromFileTTF("JP.ttf", font_size, &merge_config, jp_ranges);



  SetupCustomStyle();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);

  ImGui_ImplOpenGL3_Init("#version 330");

  return true;

}

void Application::SetupCustomStyle() {

  ImGuiStyle& style = ImGui::GetStyle();

  style.WindowRounding = 8.0f;

  style.ChildRounding = 6.0f;

  style.FrameRounding = 4.0f;

  style.PopupRounding = 4.0f;

  style.ScrollbarRounding = 4.0f;

  style.GrabRounding = 4.0f;

  style.TabRounding = 4.0f;

  style.WindowPadding = ImVec2(12, 8);

  style.FramePadding = ImVec2(8, 4);

  style.ItemSpacing = ImVec2(8, 4);

  style.ItemInnerSpacing = ImVec2(4, 4);

  style.ScrollbarSize = 16.0f;

  ImVec4* colors = style.Colors;

  colors[ImGuiCol_Text] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

  colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);

  colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

  colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);

  colors[ImGuiCol_Border] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);

  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);

  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);

  colors[ImGuiCol_FrameBgActive] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);

  colors[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);

  colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);

  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.90f, 0.90f, 0.75f);

  colors[ImGuiCol_MenuBarBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);

  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

  colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

  colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

  colors[ImGuiCol_Button] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);

  colors[ImGuiCol_ButtonHovered] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);

  colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

  colors[ImGuiCol_Header] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);

  colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);

  colors[ImGuiCol_HeaderActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

  colors[ImGuiCol_Separator] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

  colors[ImGuiCol_SeparatorActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

  style.ScaleAllSizes(ui_settings_.ui_scale);
}

bool Application::RestartApplication() {
#ifdef _WIN32
  const char* cmd = GetCommandLineA();
  if (!cmd || cmd[0] == '\0') {
    return false;
  }
  char cmdline[32768] = {0};
  std::strncpy(cmdline, cmd, sizeof(cmdline) - 1);
  STARTUPINFOA startup_info = {};
  PROCESS_INFORMATION process_info = {};
  startup_info.cb = sizeof(startup_info);
  BOOL created = CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE, 0,
                                nullptr, nullptr, &startup_info,
                                &process_info);
  if (!created) {
    return false;
  }
  CloseHandle(process_info.hProcess);
  CloseHandle(process_info.hThread);
  running_ = false;
  if (window_) {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
  }
  return true;
#else
  return false;
#endif
}

void Application::Cleanup() {
  SetHighPrecisionTimer(false, &high_precision_timer_active_);

  ImGui_ImplOpenGL3_Shutdown();

  ImGui_ImplGlfw_Shutdown();

  ImGui::DestroyContext();

#ifdef _WIN32
  UninstallWin32InputHook(window_);
#endif

  if (window_)

    glfwDestroyWindow(window_);

  glfwTerminate();

}



}  // namespace plc
