// application.cpp

//

// Implementation of main application class.



// src/Application.cpp

#include "plc_emulator/core/application.h"
#include "plc_emulator/core/win32_input.h"



#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX

#define NOMINMAX

#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#ifndef WINVER
#define WINVER 0x0602
#endif

#include <windows.h>

#include <commdlg.h>
#include <windowsx.h>

#endif



#include "imgui.h"

#include "imgui_impl_glfw.h"

#include "imgui_impl_opengl3.h"

#include "plc_emulator/programming/programming_mode.h"

#include "plc_emulator/project/ladder_to_ld_converter.h"

#include "plc_emulator/project/ld_to_ladder_converter.h"

#include "plc_emulator/project/openplc_compiler_integration.h"



#include <glad/glad.h>

#include <GLFW/glfw3.h>



#ifdef _WIN32

#define GLFW_EXPOSE_NATIVE_WIN32

#include <GLFW/glfw3native.h>

#endif



#include <algorithm>

#include <cmath>

#include <cstdio>

#include <fstream>

#include <iostream>
#include <unordered_map>



#include "nlohmann/json.hpp"



using json = nlohmann::json;

namespace plc {

namespace {
Application* g_physics_warning_app = nullptr;

void PhysicsWarningDialogCallback(const char* message) {
  if (g_physics_warning_app && message) {
    g_physics_warning_app->QueuePhysicsWarningDialog(message);
  }
}
}  // namespace




Position GetPortRelativePositionById(const PlacedComponent& comp, int portId);



Application::Application(bool enable_debug_mode)

    : window_(nullptr),

      current_mode_(Mode::WIRING),

      current_tool_(ToolType::SELECT),

      running_(false),

      is_plc_running_(false),

      selected_component_id_(-1),

      next_instance_id_(0),

      is_dragging_(false),

      dragged_component_index_(-1),

      is_moving_component_(false),

      moving_component_id_(-1),

      next_wire_id_(0),

      selected_wire_id_(-1),

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
      debug_mode_requested_(enable_debug_mode),
      debug_mode_(false),
      enable_debug_logging_(false),
      debug_update_counter_(0),
      show_physics_warning_dialog_(false),
      physics_warning_message_("") {

  drag_start_offset_ = {0, 0};

  wire_start_pos_ = {0, 0};

  wire_current_pos_ = {0, 0};

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


  if (programming_mode_) {



    programming_mode_->UpdateWithPlcState(is_plc_running_);

  }





  if (current_mode_ == Mode::WIRING) {

    UpdatePortPositions();

  }








  UpdatePortPositions();





  UpdatePhysics();






  if (debug_mode_) {

    UpdateDebugLogging();

  }

}



void Application::ProcessInput() {

  glfwPollEvents();




  ImGuiIO& io = ImGui::GetIO();



  if (io.WantCaptureKeyboard)

    return;



  if (current_mode_ == Mode::PROGRAMMING) {

    if (programming_mode_) {


      if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_Escape);

      if (ImGui::IsKeyPressed(ImGuiKey_F5, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_F5);

      if (ImGui::IsKeyPressed(ImGuiKey_F6, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_F6);

      if (ImGui::IsKeyPressed(ImGuiKey_F7, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_F7);

      if (ImGui::IsKeyPressed(ImGuiKey_F9, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_F9);

      if (ImGui::IsKeyPressed(ImGuiKey_F2, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_F2);

      if (ImGui::IsKeyPressed(ImGuiKey_F3, false) && !is_plc_running_)

        programming_mode_->HandleKeyboardInput(ImGuiKey_F3);

      if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_Delete);

      if (ImGui::IsKeyPressed(ImGuiKey_Insert, false))

        programming_mode_->HandleKeyboardInput(ImGuiKey_Insert);

      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))

        programming_mode_->HandleKeyboardInput(ImGuiKey_LeftArrow);

      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))

        programming_mode_->HandleKeyboardInput(ImGuiKey_RightArrow);

      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))

        programming_mode_->HandleKeyboardInput(ImGuiKey_UpArrow);

      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))

        programming_mode_->HandleKeyboardInput(ImGuiKey_DownArrow);

    }

  } else {

    static bool deleteKeyPressed = false;

    bool deleteKeyDown = (glfwGetKey(window_, GLFW_KEY_DELETE) == GLFW_PRESS);

    if (deleteKeyDown && !deleteKeyPressed) {

      if (selected_component_id_ != -1)

        DeleteSelectedComponent();

      if (selected_wire_id_ != -1)

        DeleteWire(selected_wire_id_);

    }

    deleteKeyPressed = deleteKeyDown;

  }

}



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

  glfwSwapInterval(1);

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

  static const ImWchar korean_ranges[] = {

      0x0020, 0x00FF, 0x2500, 0x257F, 0x3131, 0x3163, 0xAC00,

      0xD7A3, 0x2010, 0x205E, 0xFF01, 0xFF5E, 0,

  };

  io.Fonts->AddFontFromFileTTF("unifont.ttf", 16.0f, &font_config,

                               korean_ranges);



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

}



void Application::RenderUI() {

  ImGui::SetNextWindowPos(ImVec2(0, 0));

  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

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

}
void Application::RenderWiringModeUI() {

  RenderToolbar();

  RenderMainArea();

}



void Application::RenderHeader() {

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

  if (ImGui::BeginChild("Header", ImVec2(0, 80), true,

                        ImGuiWindowFlags_NoScrollbar)) {

    ImGui::Columns(3, "HeaderColumns", false);

    ImGui::SetColumnWidth(0, 300);



    ImGui::SetCursorPos(ImVec2(20, 25));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    ImGui::Text("FX3U PLC Simulator");

    ImGui::PopStyleColor();



    ImGui::NextColumn();

    float centerX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() / 2 - 115;

    ImGui::SetCursorPosX(centerX);

    ImGui::SetCursorPosY(25);



    bool isWiringMode = (current_mode_ == Mode::WIRING);

    ImGui::PushStyleColor(ImGuiCol_Button,

                          isWiringMode ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f)

                                       : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, isWiringMode

                                             ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)

                                             : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

    if (ImGui::Button("Wiring", ImVec2(100, 30))) {

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

    if (ImGui::Button("Programming", ImVec2(120, 30))) {

      current_mode_ = Mode::PROGRAMMING;

    }

    ImGui::PopStyleColor(2);



    ImGui::NextColumn();

    float rightX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - 150;

    ImGui::SetCursorPosX(rightX);

    ImGui::SetCursorPosY(25);

    ImVec4 statusColor = is_plc_running_ ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f)

                                        : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

    const char* statusText = is_plc_running_ ? "[RUN]" : "[STOP]";

    ImGui::PushStyleColor(ImGuiCol_Text, statusColor);

    ImGui::TextUnformatted(statusText);

    ImGui::PopStyleColor();

    ImGui::SameLine();



    if (ImGui::Button(is_plc_running_ ? "STOP" : "RUN", ImVec2(80, 30))) {

      if (!is_plc_running_) {

        std::cout << "[INFO] PLC RUN: Initializing PLC system..." << std::endl;




        SyncLadderProgramFromProgrammingMode();




        plc_device_states_.clear();

        for (int i = 0; i < 16; ++i) {

          std::string i_str = std::to_string(i);

          plc_device_states_["X" + i_str] =

              false;


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



        std::cout << "[INFO] PLC RUN: System initialized and ready for execution!"

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

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));

  if (ImGui::BeginChild("Toolbar", ImVec2(0, 60), true,

                        ImGuiWindowFlags_NoScrollbar)) {

    ImGui::SetCursorPos(ImVec2(20, 15));

    const char* toolNames[] = {"Select", "Pneumatic", "Electric"};

    const ToolType toolTypes[] = {ToolType::SELECT, ToolType::PNEUMATIC,

                                  ToolType::ELECTRIC};

    for (int i = 0; i < 3; i++) {

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

      if (ImGui::Button(toolNames[i], ImVec2(180, 30))) {

        current_tool_ = toolTypes[i];

      }

      ImGui::PopStyleColor(2);

    }



    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);



    if (ImGui::Button("Load Project", ImVec2(180, 30))) {


#ifdef _WIN32

      OPENFILENAMEA ofn;

      CHAR szFile[260] = {0};

      ZeroMemory(&ofn, sizeof(ofn));

      ofn.lStructSize = sizeof(ofn);

      ofn.hwndOwner = glfwGetWin32Window(window_);

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

        bool success = LoadProject(ofn.lpstrFile);

        if (success) {

          std::cout << "[INFO] Project loaded: " << ofn.lpstrFile << std::endl;

          

        } else {

          std::cout << "[ERROR] Project load failed: " << ofn.lpstrFile

                    << std::endl;

        }

      }

#else

      bool success =

          LoadProject("project.csv");  // Fallback for non-windows

      if (success) {

        std::cout << "[INFO] Project loaded: project.csv" << std::endl;

        

        

      } else {

        std::cout << "[ERROR] Project load failed: project.csv" << std::endl;

      }

#endif

    }

  }

  ImGui::EndChild();

  ImGui::PopStyleColor();

}



void Application::RenderMainArea() {

  const float status_bar_height = 25.0f;
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

    ImGui::SetCursorPosY(5);

    ImGui::SetCursorPosX(10);



    ImGui::Text(

        "Zoom %.1fx | Pos: (%.0f, %.0f) | Components: %zu | Wires: %zu",

        camera_zoom_, camera_offset_.x, camera_offset_.y,

        placed_components_.size(), wires_.size());



    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);

    const char* tool_name = "";

    switch (current_tool_) {

      case ToolType::SELECT:

        tool_name = "Select";

        break;

      case ToolType::PNEUMATIC:

        tool_name = "Pneumatic";

        break;

      case ToolType::ELECTRIC:

        tool_name = "Electric";

        break;

    }

    ImGui::Text("Tool: %s", tool_name);

  }

  ImGui::EndChild();

  ImGui::PopStyleColor();

}



void Application::Cleanup() {

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



void Application::UpdatePortPositions() {

  port_positions_.clear();

  for (const auto& comp : placed_components_) {

    int max_ports = 0;

    switch (comp.type) {

      case ComponentType::PLC:

        max_ports = 32;

        break;

      case ComponentType::FRL:

        max_ports = 1;

        break;

      case ComponentType::MANIFOLD:

        max_ports = 5;

        break;

      case ComponentType::LIMIT_SWITCH:

        max_ports = 3;

        break;

      case ComponentType::SENSOR:

        max_ports = 3;

        break;

      case ComponentType::CYLINDER:

        max_ports = 2;

        break;

      case ComponentType::VALVE_SINGLE:

        max_ports = 5;

        break;

      case ComponentType::VALVE_DOUBLE:

        max_ports = 7;

        break;

      case ComponentType::BUTTON_UNIT:

        max_ports = 15;

        break;

      case ComponentType::POWER_SUPPLY:

        max_ports = 2;

        break;

    }

    for (int portId = 0; portId < max_ports; ++portId) {

      Position relPos = GetPortRelativePositionById(comp, portId);

      Position worldPos = {comp.position.x + relPos.x,

                           comp.position.y + relPos.y};

      port_positions_[{comp.instanceId, portId}] = worldPos;

    }

  }

}



ImVec2 Application::WorldToScreen(const ImVec2& world_pos) const {

  float x = canvas_top_left_.x + (world_pos.x + camera_offset_.x) * camera_zoom_;

  float y = canvas_top_left_.y + (world_pos.y + camera_offset_.y) * camera_zoom_;

  return ImVec2(x, y);

}



ImVec2 Application::ScreenToWorld(const ImVec2& screen_pos) const {

  float x =

      (screen_pos.x - canvas_top_left_.x) / camera_zoom_ - camera_offset_.x;

  float y =

      (screen_pos.y - canvas_top_left_.y) / camera_zoom_ - camera_offset_.y;

  return ImVec2(x, y);

}



LadderInstructionType Application::StringToInstructionType(

    const std::string& str) {

  if (str == "XIC")

    return LadderInstructionType::XIC;

  if (str == "XIO")

    return LadderInstructionType::XIO;

  if (str == "OTE")

    return LadderInstructionType::OTE;

  if (str == "SET")

    return LadderInstructionType::SET;

  if (str == "RST")

    return LadderInstructionType::RST;

  if (str == "TON")

    return LadderInstructionType::TON;

  if (str == "CTU")

    return LadderInstructionType::CTU;

  if (str == "RST_TMR_CTR")

    return LadderInstructionType::RST_TMR_CTR;

  if (str == "HLINE")

    return LadderInstructionType::HLINE;

  return LadderInstructionType::EMPTY;

}



void Application::SyncLadderProgramFromProgrammingMode() {

  if (!programming_mode_) {

    printf(

        "Error: ProgrammingMode is not initialized. Cannot sync ladder "

        "program.\n");

    return;

  }




  loaded_ladder_program_ = programming_mode_->GetLadderProgram();

  plc_device_states_ = programming_mode_->GetDeviceStates();

  plc_timer_states_ = programming_mode_->GetTimerStates();

  plc_counter_states_ = programming_mode_->GetCounterStates();



  if (simulator_core_) {

    simulator_core_->SyncFromProgrammingMode(programming_mode_.get());



    if (simulator_core_->UpdateIOMapping()) {
      printf("[INFO] I/O mapping updated from wiring connections.\n");
    }



    printf("[INFO] Data synchronized to PLCSimulatorCore as well.\n");

  }



  if (compiled_plc_executor_) {

    CompileAndLoadLadderProgram();

  }

}



void Application::LoadLadderProgramFromLD(const std::string& filepath) {

  std::cout << "[INFO] Loading ladder program from .ld file: " << filepath

            << std::endl;




  LDToLadderConverter converter;




  LadderProgram loadedProgram;

  bool success = converter.ConvertFromLDFile(filepath, loadedProgram);



  if (!success) {

    std::cout << "[ERROR] Failed to load .ld file: " << converter.GetLastError()

              << std::endl;

    std::cout << "[ERROR] Invalid .ld format. Check file contents and try again."
              << std::endl;

    return;

  }




  auto stats = converter.GetConversionStats();

  std::cout << "[INFO] Successfully loaded .ld file!" << std::endl;

  std::cout << "[INFO] Conversion stats:" << std::endl;

  std::cout << "   - Networks: " << stats.networksCount << std::endl;

  std::cout << "   - Contacts: " << stats.contactsCount << std::endl;

  std::cout << "   - Coils: " << stats.coilsCount << std::endl;

  std::cout << "   - Blocks: " << stats.blocksCount << std::endl;




  loaded_ladder_program_ = loadedProgram;




  if (programming_mode_) {



    std::cout << "[INFO] Syncing to Programming Mode for UI editing..."

              << std::endl;





  }




  plc_device_states_.clear();

  plc_timer_states_.clear();

  plc_counter_states_.clear();




  for (int i = 0; i < 16; ++i) {

    std::string i_str = std::to_string(i);

    plc_device_states_["X" + i_str] = false;

    plc_device_states_["Y" + i_str] = false;

    plc_device_states_["M" + i_str] = false;

  }




  for (const auto& rung : loadedProgram.rungs) {

    if (rung.isEndRung)

      continue;



    for (const auto& cell : rung.cells) {

      if (!cell.address.empty()) {

        if (cell.type == LadderInstructionType::TON) {

          TimerState& timer = plc_timer_states_[cell.address];

          timer.preset = 10;




          if (!cell.preset.empty() && cell.preset[0] == 'K') {

            try {

              timer.preset = std::stoi(cell.preset.substr(1));

            } catch (const std::exception&) {

              timer.preset = 10;

            }

          }

        } else if (cell.type == LadderInstructionType::CTU) {

          CounterState& counter = plc_counter_states_[cell.address];

          counter.preset = 10;




          if (!cell.preset.empty() && cell.preset[0] == 'K') {

            try {

              counter.preset = std::stoi(cell.preset.substr(1));

            } catch (const std::exception&) {

              counter.preset = 10;

            }

          }

        }

      }

    }

  }



  std::cout << "[INFO] Initialized " << plc_timer_states_.size() << " timers and "

            << plc_counter_states_.size() << " counters" << std::endl;




  if (compiled_plc_executor_) {

    std::cout << "[INFO] Auto-compiling loaded ladder program..." << std::endl;

    CompileAndLoadLadderProgram();

  }



  std::cout << "[INFO] Ladder program loaded and ready for execution!" << std::endl;

}



void Application::CompileAndLoadLadderProgram() {

  if (!compiled_plc_executor_) {

    std::cout << "[WARN] CompiledPLCExecutor not initialized" << std::endl;

    return;

  }



  if (loaded_ladder_program_.rungs.empty()) {

    std::cout << "[WARN] No ladder program to compile" << std::endl;

    return;

  }



  try {

    // Step 1: Convert ladder program to .ld file

    std::cout << "[INFO] Converting ladder program to OpenPLC .ld format..."

              << std::endl;



    LadderToLDConverter converter;

    converter.SetDebugMode(enable_debug_logging_);




    std::string tempLdPath = "temp_ladder_program.ld";

    bool convertSuccess =

        converter.ConvertToLDFile(loaded_ladder_program_, tempLdPath);



    if (!convertSuccess) {

      std::cout << "[ERROR] Failed to convert ladder program to .ld format: "

                << converter.GetLastError() << std::endl;

      return;

    }



    std::cout << "[INFO] Ladder program converted to .ld format successfully"

              << std::endl;



    // Step 2: Compile .ld file to C++ code

    std::cout << "[INFO] Compiling .ld file to C++ code..." << std::endl;



    OpenPLCCompilerIntegration compiler;

    compiler.SetDebugMode(enable_debug_logging_);

    compiler.SetIOConfiguration(16, 16);  // FX3U-32M: 16 inputs, 16 outputs



    auto compilationResult = compiler.CompileLDFile(tempLdPath);



    if (!compilationResult.success) {

      std::cout << "[ERROR] Failed to compile .ld file: "

                << compilationResult.errorMessage << std::endl;

      return;

    }



    std::cout << "[INFO] .ld file compiled successfully" << std::endl;

    std::cout << "[INFO] Compilation statistics:" << std::endl;

    std::cout << "   - Generated code size: "

              << compilationResult.generatedCode.length() << " characters"

              << std::endl;

    std::cout << "   - Input count: " << compilationResult.inputCount

              << std::endl;

    std::cout << "   - Output count: " << compilationResult.outputCount

              << std::endl;

    std::cout << "   - Memory count: " << compilationResult.memoryCount

              << std::endl;



    // Step 3: Load compiled code into CompiledPLCExecutor

    std::cout << "[INFO] Loading compiled code into PLC executor..." << std::endl;



    bool loadSuccess =

        compiled_plc_executor_->LoadFromCompilationResult(compilationResult);



    if (!loadSuccess) {

      std::cout << "[ERROR] Failed to load compiled code into PLC executor"

                << std::endl;

      return;

    }



    std::cout << "[INFO] Compiled ladder program loaded successfully!" << std::endl;

    std::cout << "[INFO] CompiledPLCExecutor is ready for execution" << std::endl;



    // Step 4: Cleanup temporary files

    std::remove(tempLdPath.c_str());



  } catch (const std::exception& e) {

    std::cout << "[ERROR] Exception during compilation: " << e.what() << std::endl;

  }

}



void Application::CreateDefaultTestLadderProgram() {

  std::cout << "[INFO] Creating default test ladder program..." << std::endl;




  loaded_ladder_program_.rungs.clear();

  loaded_ladder_program_.verticalConnections.clear();




  Rung rung0;

  rung0.number = 0;

  rung0.cells.resize(12);



  // X0 (XIC)

  rung0.cells[0].type = LadderInstructionType::XIC;

  rung0.cells[0].address = "X0";




  for (int i = 1; i < 11; i++) {

    rung0.cells[static_cast<std::size_t>(i)].type = LadderInstructionType::HLINE;

  }



  // Y0 (OTE)

  rung0.cells[11].type = LadderInstructionType::OTE;

  rung0.cells[11].address = "Y0";



  loaded_ladder_program_.rungs.push_back(rung0);




  Rung rung1;

  rung1.number = 1;

  rung1.cells.resize(12);



  // X1 (XIC)

  rung1.cells[0].type = LadderInstructionType::XIC;

  rung1.cells[0].address = "X1";




  for (int i = 1; i < 11; i++) {

    rung1.cells[static_cast<std::size_t>(i)].type = LadderInstructionType::HLINE;

  }



  // Y1 (OTE)

  rung1.cells[11].type = LadderInstructionType::OTE;

  rung1.cells[11].address = "Y1";



  loaded_ladder_program_.rungs.push_back(rung1);



  // End Rung

  Rung endRung;

  endRung.number = 2;

  endRung.isEndRung = true;

  loaded_ladder_program_.rungs.push_back(endRung);




  plc_device_states_.clear();

  for (int i = 0; i < 16; i++) {

    plc_device_states_["X" + std::to_string(i)] = false;

    plc_device_states_["Y" + std::to_string(i)] = false;

  }




  plc_timer_states_.clear();

  plc_counter_states_.clear();



  std::cout << "[INFO] Default test ladder program created: X0->Y0, X1->Y1"

            << std::endl;

}





void Application::ToggleDebugMode() {

  debug_mode_ = !debug_mode_;



  if (debug_mode_) {

#ifdef _WIN32


    if (!GetConsoleWindow()) {

      AllocConsole();

      freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);

      freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

      freopen_s((FILE**)stdin, "CONIN$", "r", stdin);

    }

#endif

    PrintDebugToConsole(

        "[DEBUG MODE ON] Physics Engine Debug System Activated");

    PrintDebugToConsole("Restart without --Debug to disable debug mode");

    PrintDebugToConsole("==========================================");

  } else {

    PrintDebugToConsole("[DEBUG MODE OFF] Debug System Deactivated");

    PrintDebugToConsole("==========================================");

  }

}





void Application::UpdateDebugLogging() {

  debug_update_counter_++;




  if (debug_update_counter_ % 180 == 0) {

    debug_log_buffer_.clear();



    PrintDebugToConsole("========== DEBUG STATUS UPDATE ==========");

    PrintDebugToConsole("Frame: " + std::to_string(debug_update_counter_) +

                        " | PLC: " + (is_plc_running_ ? "RUN" : "STOP"));




    LogComponentStates();




    if (physics_engine_ && physics_engine_->isInitialized) {

      LogPhysicsEngineStates();

    } else {

      PrintDebugToConsole("Physics Engine: NOT INITIALIZED or LEGACY MODE");

    }



    PrintDebugToConsole("============================================");

  }

}

bool Application::IsDebugEnabled() const {
  return debug_mode_ || debug_mode_requested_;
}

void Application::DebugLog(const std::string& message) {
  PrintDebugToConsole(message);
#ifdef _WIN32
  OutputDebugStringA((message + "\n").c_str());
#endif
}

void Application::QueuePhysicsWarningDialog(const std::string& message) {
  std::lock_guard<std::mutex> lock(physics_warning_mutex_);
  physics_warning_message_ = message;
  show_physics_warning_dialog_ = true;
}

void Application::RenderPhysicsWarningDialog() {
  bool show_dialog = false;
  std::string message;
  {
    std::lock_guard<std::mutex> lock(physics_warning_mutex_);
    show_dialog = show_physics_warning_dialog_;
    message = physics_warning_message_;
  }

  if (!show_dialog)
    return;

  if (!ImGui::IsPopupOpen("Physics Warning")) {
    ImGui::OpenPopup("Physics Warning");
  }

  bool open = true;
  if (ImGui::BeginPopupModal("Physics Warning", &open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Fixed buffer limit reached.");
    ImGui::Separator();
    ImGui::TextWrapped("%s", message.c_str());

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      std::lock_guard<std::mutex> lock(physics_warning_mutex_);
      show_physics_warning_dialog_ = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (!open) {
    std::lock_guard<std::mutex> lock(physics_warning_mutex_);
    show_physics_warning_dialog_ = false;
  }
}
void Application::RegisterWin32RightClick() {
  win32_right_click_ = true;
}

void Application::RegisterWin32SideClick() {
  win32_side_click_ = true;
}
void Application::RegisterWin32SideDown(bool is_down) {
  win32_side_down_ = is_down;
}





void Application::LogComponentStates() {

  PrintDebugToConsole("=== COMPONENT STATES ===");



  int cylinderCount = 0, limitSwitchCount = 0, sensorCount = 0;



  for (const auto& comp : placed_components_) {

    switch (comp.type) {

      case ComponentType::CYLINDER: {

        cylinderCount++;

        float position = comp.internalStates.count("position")

                             ? comp.internalStates.at("position")

                             : 0.0f;

        float velocity = comp.internalStates.count("velocity")

                             ? comp.internalStates.at("velocity")

                             : 0.0f;

        float pressureA = comp.internalStates.count("pressure_a")

                              ? comp.internalStates.at("pressure_a")

                              : 0.0f;

        float pressureB = comp.internalStates.count("pressure_b")

                              ? comp.internalStates.at("pressure_b")

                              : 0.0f;



        PrintDebugToConsole("  Cylinder[" + std::to_string(comp.instanceId) +

                            "] Pos:" + std::to_string(position) +

                            "mm Vel:" + std::to_string(velocity) +

                            "mm/s PA:" + std::to_string(pressureA) +

                            "bar PB:" + std::to_string(pressureB) + "bar");

        break;

      }



      case ComponentType::LIMIT_SWITCH: {

        limitSwitchCount++;

        bool isPressed = comp.internalStates.count("is_pressed") &&

                         comp.internalStates.at("is_pressed") > 0.5f;

        bool manualPress = comp.internalStates.count("is_pressed_manual") &&

                           comp.internalStates.at("is_pressed_manual") > 0.5f;




        float closestDistance = 999999.0f;

        int closestCylinderId = -1;

        for (const auto& cylinder : placed_components_) {

          if (cylinder.type == ComponentType::CYLINDER) {

            float cylinderX_body_start = cylinder.position.x + 170.0f;

            float pistonPosition = cylinder.internalStates.count("position")

                                       ? cylinder.internalStates.at("position")

                                       : 0.0f;

            float pistonX_tip = cylinderX_body_start - pistonPosition;

            float pistonY = cylinder.position.y + 25.0f;



            float sensorX_center = comp.position.x + comp.size.width / 2.0f;

            float sensorY_center = comp.position.y + comp.size.height / 2.0f;



            float dx = pistonX_tip - sensorX_center;

            float dy = pistonY - sensorY_center;

            float distance = std::sqrt(dx * dx + dy * dy);



            if (distance < closestDistance) {

              closestDistance = distance;

              closestCylinderId = cylinder.instanceId;

            }

          }

        }



        PrintDebugToConsole("  LimitSwitch[" + std::to_string(comp.instanceId) +

                            "] Pressed:" + (isPressed ? "YES" : "NO") +

                            " Manual:" + (manualPress ? "YES" : "NO") +

                            " Pos:(" + std::to_string(static_cast<int>(comp.position.x)) +

                            "," + std::to_string(static_cast<int>(comp.position.y)) + ")" +

                            " ClosestCyl[" + std::to_string(closestCylinderId) +

                            "]:" + std::to_string(static_cast<int>(closestDistance)) + "mm");

        break;

      }



      case ComponentType::SENSOR: {

        sensorCount++;

        bool isDetected = comp.internalStates.count("is_detected") &&

                          comp.internalStates.at("is_detected") > 0.5f;

        bool isPowered = comp.internalStates.count("is_powered") &&

                         comp.internalStates.at("is_powered") > 0.5f;



        PrintDebugToConsole("  Sensor[" + std::to_string(comp.instanceId) +

                            "] Detected:" + (isDetected ? "YES" : "NO") +

                            " Powered:" + (isPowered ? "YES" : "NO") +

                            " Pos:(" + std::to_string(static_cast<int>(comp.position.x)) +

                            "," + std::to_string(static_cast<int>(comp.position.y)) + ")");

        break;

      }



      default:

        break;

    }

  }



  PrintDebugToConsole("Total Components: " + std::to_string(cylinderCount) +

                      " Cylinders, " + std::to_string(limitSwitchCount) +

                      " LimitSwitches, " + std::to_string(sensorCount) +

                      " Sensors");




  std::string activeOutputs = "";

  for (const auto& pair : plc_device_states_) {

    if (pair.first[0] == 'Y' && pair.second) {

      if (!activeOutputs.empty())

        activeOutputs += ", ";

      activeOutputs += pair.first;

    }

  }

  PrintDebugToConsole("Active PLC Outputs: " +

                      (activeOutputs.empty() ? "NONE" : activeOutputs));

}






void Application::LogPhysicsEngineStates() {

  PrintDebugToConsole("=== PHYSICS ENGINE STATES ===");



  if (!physics_engine_) {

    PrintDebugToConsole("ERROR: Physics Engine: NULL POINTER");

    return;

  }



  PrintDebugToConsole("Engine Status: " +

                      std::string(physics_engine_->isInitialized

                                      ? "INITIALIZED"

                                      : "NOT INITIALIZED"));

  PrintDebugToConsole("Active Components: " +

                      std::to_string(physics_engine_->activeComponents) + "/" +

                      std::to_string(physics_engine_->maxComponents));




  LogElectricalNetwork();




  LogPneumaticNetwork();




  LogMechanicalSystem();

}




void Application::LogElectricalNetwork() {

  if (!physics_engine_->electricalNetwork) {

    PrintDebugToConsole("Electrical Network: NULL");

    return;

  }



  ElectricalNetwork* elec = physics_engine_->electricalNetwork;

  PrintDebugToConsole("Electrical Network: " + std::to_string(elec->nodeCount) +

                      " nodes, " + std::to_string(elec->edgeCount) + " edges");

  PrintDebugToConsole(

      "Convergence: " + std::string(elec->isConverged ? "YES" : "NO") +

      " Iterations: " + std::to_string(elec->currentIteration));

}




void Application::LogPneumaticNetwork() {

  if (!physics_engine_->pneumaticNetwork) {

    PrintDebugToConsole("Pneumatic Network: NULL");

    return;

  }



  PneumaticNetwork* pneu = physics_engine_->pneumaticNetwork;

  PrintDebugToConsole("Pneumatic Network: " + std::to_string(pneu->nodeCount) +

                      " nodes, " + std::to_string(pneu->edgeCount) + " edges");

  PrintDebugToConsole(

      "Convergence: " + std::string(pneu->isConverged ? "YES" : "NO") +

      " Iterations: " + std::to_string(pneu->currentIteration));

}




void Application::LogMechanicalSystem() {

  if (!physics_engine_->mechanicalSystem) {

    PrintDebugToConsole("Mechanical System: NULL");

    return;

  }



  MechanicalSystem* mech = physics_engine_->mechanicalSystem;

  PrintDebugToConsole("Mechanical System: " + std::to_string(mech->nodeCount) +

                      " nodes, " + std::to_string(mech->edgeCount) + " edges");

  PrintDebugToConsole(

      "Stability: " + std::string(mech->isStable ? "STABLE" : "UNSTABLE") +

      " Time: " + std::to_string(mech->currentTime) + "s");

}






void Application::PrintDebugToConsole(const std::string& message) {


  std::cout << message << std::endl;



#ifdef _WIN32


  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hConsole != INVALID_HANDLE_VALUE) {

    DWORD written;

    std::string fullMessage = message + "\n";

    WriteConsoleA(hConsole, fullMessage.c_str(),

                  static_cast<DWORD>(fullMessage.length()), &written, NULL);

  }

#endif

}



// =============================================================================


// =============================================================================



bool Application::SaveProject(const std::string& filePath,

                              const std::string& projectName) {

  std::cout << "[INFO] Saving project to: " << filePath << std::endl;



  if (!programming_mode_ || !project_file_manager_) {

    std::cerr << "[ERROR] Project save failed: components not initialized"

              << std::endl;

    return false;

  }



  try {


    const LadderProgram& currentProgram = programming_mode_->GetLadderProgram();




    project_file_manager_->SetDebugMode(enable_debug_logging_);

    auto result = project_file_manager_->SaveProject(currentProgram, filePath,

                                                    projectName);



    if (result.success) {

      std::cout << "[INFO] Project saved successfully!" << std::endl;

      std::cout << "[INFO] Project info:" << std::endl;

      std::cout << "   - Name: " << result.info.projectName << std::endl;

      std::cout << "   - Size: " << result.compressedSize

                << " bytes (compressed)" << std::endl;

      std::cout << "   - Rungs: " << currentProgram.rungs.size() << std::endl;

      std::cout << "   - Vertical connections: "

                << currentProgram.verticalConnections.size() << std::endl;

      std::cout << "   - Layout checksum: " << result.info.layoutChecksum

                << std::endl;

      return true;

    } else {

      std::cerr << "[ERROR] Project save failed: " << result.errorMessage

                << std::endl;

      return false;

    }



  } catch (const std::exception& e) {

    std::cerr << "[ERROR] Project save exception: " << e.what() << std::endl;

    return false;

  }

}



bool Application::LoadProject(const std::string& filePath) {

  std::cout << "[INFO] Loading project from: " << filePath << std::endl;



  if (!programming_mode_ || !project_file_manager_) {

    std::cerr << "[ERROR] Project load failed: components not initialized"

              << std::endl;

    return false;

  }



  try {


    project_file_manager_->SetDebugMode(enable_debug_logging_);

    auto result = project_file_manager_->LoadProject(filePath);



    if (!result.success) {

      std::cerr << "[ERROR] Project load failed: " << result.errorMessage

                << std::endl;

      return false;

    }



    std::cout << "[DEBUG] Project load successful!" << std::endl;

    std::cout << "[DEBUG] Loaded program info:" << std::endl;

    std::cout << "   - Rungs: " << result.program.rungs.size() << std::endl;

    std::cout << "   - Vertical connections: "

              << result.program.verticalConnections.size() << std::endl;




    loaded_ladder_program_ = result.program;




    std::cout << "[DEBUG] Updating ProgrammingMode with loaded program..."

              << std::endl;

    programming_mode_->SetLadderProgram(result.program);




    plc_device_states_.clear();

    plc_timer_states_.clear();

    plc_counter_states_.clear();




    for (int i = 0; i <= 15; ++i) {

      plc_device_states_["X" + std::to_string(i)] = false;

      plc_device_states_["Y" + std::to_string(i)] = false;

    }

    for (int i = 0; i <= 999; ++i) {

      std::string i_str = std::to_string(i);

      plc_device_states_["M" + i_str] = false;

    }




    if (compiled_plc_executor_) {

      CompileAndLoadLadderProgram();

    }




    std::cout << "[INFO] Project loaded successfully!" << std::endl;

    std::cout << "[INFO] Project info:" << std::endl;

    std::cout << "   - Name: " << result.info.projectName << std::endl;

    std::cout << "   - Version: " << result.info.version << std::endl;

    std::cout << "   - Created: " << result.info.createdDate << std::endl;

    std::cout << "   - Tool version: " << result.info.toolVersion << std::endl;

    std::cout << "   - Rungs: " << result.program.rungs.size() << std::endl;

    std::cout << "   - Vertical connections: "

              << result.program.verticalConnections.size() << std::endl;



    if (result.info.layoutChecksum != 0) {

      std::cout << "   - Layout checksum: " << result.info.layoutChecksum

                << " bytes" << std::endl;

    }



    if (!result.ldProgram.empty()) {

      std::cout << "   - LD program size: " << result.ldProgram.size()

                << " bytes" << std::endl;

    }



    return true;



  } catch (const std::exception& e) {

    std::cerr << "[ERROR] Project load exception: " << e.what() << std::endl;

    return false;

  }

}



void Application::RenderPLCDebugPanel() {

  if (!programming_mode_)

    return;



  ImGuiIO& io = ImGui::GetIO();

  ImVec2 defaultPos(io.DisplaySize.x - 320.0f, 90.0f);

  ImGui::SetNextWindowPos(defaultPos, ImGuiCond_FirstUseEver);

  ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);



  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

  if (ImGui::Begin("PLC Engine Status", nullptr, flags)) {

    const char* engineType = programming_mode_->GetEngineType();

    bool compiledLoaded = programming_mode_->HasCompiledCodeLoaded();

    bool needRecompile = programming_mode_->IsRecompileNeeded();



    ImGui::Text("Engine: %s", engineType);

    ImGui::Text("Compiled Code: %s", compiledLoaded ? "Loaded" : "Not Loaded");

    ImGui::Text("Recompile Needed: %s", needRecompile ? "Yes" : "No");



    const std::string& lastCompileErr =

        programming_mode_->GetLastCompileError();

    if (!lastCompileErr.empty()) {

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));

      ImGui::TextWrapped("Compile Error: %s", lastCompileErr.c_str());

      ImGui::PopStyleColor();

    }



    ImGui::Separator();

    bool lastScanOk = programming_mode_->GetLastScanSuccess();

    int cycleUs = programming_mode_->GetLastCycleTimeUs();

    int instr = programming_mode_->GetLastInstructionCount();

    const std::string& scanErr = programming_mode_->GetLastScanError();



    ImGui::Text("Last Scan: %s", lastScanOk ? "OK" : "FAIL");

    ImGui::Text("Cycle Time: %d us", cycleUs);

    ImGui::Text("Instructions: %d", instr);

    if (!scanErr.empty()) {

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));

      ImGui::TextWrapped("Scan Error: %s", scanErr.c_str());

      ImGui::PopStyleColor();

    }



    ImGui::Separator();

    // GXWorks2 Normalization info

    bool gx2 = programming_mode_->IsGX2NormalizationEnabled();

    ImGui::Text("GX2 Normalize: %s", gx2 ? "Enabled" : "Disabled");

    if (gx2) {

      ImGui::Text("Fix Count: %d",

                  programming_mode_->GetNormalizationFixCount());

      if (ImGui::CollapsingHeader("Normalization Summary",

                                  ImGuiTreeNodeFlags_DefaultOpen)) {

        const std::string& summary =

            programming_mode_->GetNormalizationSummary();

        if (summary.empty()) {

          ImGui::TextUnformatted("(no fixes)");

        } else {

          ImGui::BeginChild("gx2_summary", ImVec2(0, 80), true);

          ImGui::TextUnformatted(summary.c_str());

          ImGui::EndChild();

        }

      }

    }

  }

  ImGui::End();

}

}  // namespace plc

