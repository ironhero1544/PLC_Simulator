// src/Application.cpp
#include "Application.h"
#include "ProgrammingMode.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"
#include "LadderToLDConverter.h"
#include "LDToLadderConverter.h"
#include "OpenPLCCompilerIntegration.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <commdlg.h>
#endif

using json = nlohmann::json;
namespace plc {

    Position GetPortRelativePositionById(const PlacedComponent &comp, int portId);


    Application::Application()
            : m_window(nullptr),
              m_currentMode(Mode::WIRING),
              m_currentTool(ToolType::SELECT),
              m_running(false),
              m_isPlcRunning(false),
              m_selectedComponentId(-1),
              m_nextInstanceId(0),
              m_isDragging(false),
              m_draggedComponentIndex(-1),
              m_isMovingComponent(false),
              m_movingComponentId(-1),
              m_nextWireId(0),
              m_selectedWireId(-1),
              m_isConnecting(false),
              m_wireStartComponentId(-1),
              m_wireStartPortId(-1),
              m_wireEditMode(WireEditMode::NONE),
              m_editingWireId(-1),
              m_editingPointIndex(-1),
              m_cameraZoom(1.0f),
              m_programmingMode(std::make_unique<ProgrammingMode>(this)),
              m_simulatorCore(std::make_unique<PLCSimulatorCore>()),
              m_physicsEngine(nullptr),
              m_projectFileManager(std::make_unique<ProjectFileManager>()),
              m_debugMode(false),
              m_enableDebugLogging(false),
              m_debugKeyPressed(false),
              m_debugUpdateCounter(0) {
        m_dragStartOffset = {0, 0};
        m_wireStartPos = {0, 0};
        m_wireCurrentPos = {0, 0};
        m_cameraOffset = {0.0f, 0.0f};
        m_canvasTopLeft = {0.0f, 0.0f};
        m_canvasSize = {0.0f, 0.0f};
        m_snapSettings.enableGridSnap = true;
        m_snapSettings.enablePortSnap = true;
        m_snapSettings.enableVerticalSnap = true;
        m_snapSettings.enableHorizontalSnap = true;
        m_snapSettings.snapDistance = 15.0f;
        m_snapSettings.gridSize = 25.0f;
    }

    Application::~Application() = default;

    bool Application::Initialize() {
        if (!InitializeWindow()) return false;
        if (!InitializeImGui()) return false;
        if (m_programmingMode) m_programmingMode->Initialize();
        if (m_simulatorCore && !m_simulatorCore->Initialize()) {
            std::cerr << "Failed to initialize PLCSimulatorCore!" << std::endl;
            return false;
        }
        m_physicsEngine = CreateDefaultPhysicsEngine();
        if (!m_physicsEngine) {
            std::cerr << "Failed to create PhysicsEngine!" << std::endl;
            return false;
        }

        std::cout << "[Application] PhysicsEngine initialized successfully" << std::endl;
        m_compiledPLCExecutor = std::make_unique<CompiledPLCExecutor>();
        m_compiledPLCExecutor->SetDebugMode(false);
        m_compiledPLCExecutor->ResetMemory();

        std::cout << "[Application] Compiled PLC Executor initialized successfully" << std::endl;
        std::cout << std::endl;
        std::cout << "=====================================" << std::endl;
        std::cout << "  PLC 시뮬레이터 키보드 단축키 안내" << std::endl;
        std::cout << "=====================================" << std::endl;
        std::cout << "• 프로그램 종료: Ctrl+Q" << std::endl;
        std::cout << "• 디버그 모드: Ctrl+F12" << std::endl;
        std::cout << "• ESC: 편집 취소/선택 해제 (프로그래밍 모드)" << std::endl;
        std::cout << "=====================================" << std::endl;
        std::cout << std::endl;

        m_running = true;
        return true;
    }

    void Application::Run() {
        while (m_running && !glfwWindowShouldClose(m_window)) {
            ProcessInput();
            Update();
            Render();
        }
    }

    void Application::Shutdown() {
        if (m_physicsEngine) {
            DestroyPhysicsEngine(m_physicsEngine);
            m_physicsEngine = nullptr;
            std::cout << "[Application] PhysicsEngine destroyed" << std::endl;
        }

        if (m_simulatorCore) {
            m_simulatorCore->Shutdown();
        }

        Cleanup();
    }

    void Application::Update() {
        // 1. 프로그래밍 모드의 내부 상태 업데이트 (편집/모니터링 상태 관리)
        if (m_programmingMode) {
            // Application의 PLC 실행 상태를 ProgrammingMode에 전달하여 자동 모드 전환 등을 처리
            m_programmingMode->UpdateWithPlcState(m_isPlcRunning);
        }

        // 2. 현재 UI 모드가 '실배선 모드'일 때, 포트 위치를 항상 업데이트합니다.
        //    이것이 PLC가 STOP 상태일 때도 배선이 보이게 하는 핵심입니다.
        if (m_currentMode == Mode::WIRING) {
            UpdatePortPositions();
        }

        // 3. 물리 시뮬레이션은 PLC 상태와 무관하게 항상 실행 (실제 현장과 동일)
        // 물리 시뮬레이션은 컴포넌트의 최신 포트 위치 정보가 필요하므로,
        // 만약 현재 '프로그래밍 모드'를 보고 있더라도 포트 위치를 다시 한번 업데이트 해줍니다.
        // (실배선 모드였다면 위에서 이미 업데이트 했으므로 중복 호출이지만, 안전을 위해 실행)
        UpdatePortPositions();

        // 전기, 공압, 기구학적 시뮬레이션을 모두 포함하는 메인 물리 엔진 실행
        // PLC STOP/RUN 상태 구분은 UpdatePhysics() 내부에서 처리됨
        UpdatePhysics();

        //  물리 시뮬레이션 후 디버그 정보 수집 및 출력
        // - 디버그 모드가 활성화된 경우에만 실행 (성능 고려)
        // - 매 프레임이 아닌 주기적으로 출력 (가독성 고려)
        if (m_debugMode) {
            UpdateDebugLogging();
        }
    }

    void Application::ProcessInput() {
        glfwPollEvents();

        // 프로그램 종료는 Ctrl+Q 조합으로 변경 (더 안전)
        if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS &&
            (glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
             glfwGetKey(m_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)) {
            m_running = false;
        }

        ImGuiIO &io = ImGui::GetIO();

        // - Ctrl+F12 조합으로 디버그 모드 토글
        // - 키 반복 입력 방지 (m_debugKeyPressed 플래그 사용)
        // - ImGui의 키보드 캡처 상태와 무관하게 작동 (전역 디버그 기능)
        if (ImGui::IsKeyPressed(ImGuiKey_F12, false) && io.KeyCtrl) {
            if (!m_debugKeyPressed) {
                ToggleDebugMode();
                m_debugKeyPressed = true;
            }
        } else {
            m_debugKeyPressed = false;
        }

        if (io.WantCaptureKeyboard) return;

        if (m_currentMode == Mode::PROGRAMMING) {
            if (m_programmingMode) {
                // ESC 키를 편집 취소/선택 해제로 사용
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_Escape);
                if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) m_programmingMode->HandleKeyboardInput(ImGuiKey_F5);
                if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) m_programmingMode->HandleKeyboardInput(ImGuiKey_F6);
                if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) m_programmingMode->HandleKeyboardInput(ImGuiKey_F7);
                if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) m_programmingMode->HandleKeyboardInput(ImGuiKey_F9);
                if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) m_programmingMode->HandleKeyboardInput(ImGuiKey_F2);
                if (ImGui::IsKeyPressed(ImGuiKey_F3, false) && !m_isPlcRunning)
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_F3);
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_Delete);
                if (ImGui::IsKeyPressed(ImGuiKey_Insert, false))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_Insert);
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_LeftArrow);
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_RightArrow);
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_UpArrow);
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
                    m_programmingMode->HandleKeyboardInput(ImGuiKey_DownArrow);
            }
        } else {
            static bool deleteKeyPressed = false;
            bool deleteKeyDown = (glfwGetKey(m_window, GLFW_KEY_DELETE) == GLFW_PRESS);
            if (deleteKeyDown && !deleteKeyPressed) {
                if (m_selectedComponentId != -1) DeleteSelectedComponent();
                if (m_selectedWireId != -1) DeleteWire(m_selectedWireId);
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

        glfwSwapBuffers(m_window);
    }

    bool Application::InitializeWindow() {
        if (!glfwInit()) {
            printf("Failed to initialize GLFW!\n");
            return false;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FX3U PLC Simulator", nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            return false;
        }
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            return false;
        }

        printf("=== PLC Simulator Started ===\n");
        return true;
    }

    bool Application::InitializeImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;

        io.Fonts->AddFontDefault();
        ImFontConfig font_config;
        font_config.MergeMode = true;
        font_config.PixelSnapH = true;
        static const ImWchar korean_ranges[] = {
                0x0020, 0x00FF, 0x2500, 0x257F, 0x3131, 0x3163,
                0xAC00, 0xD7A3, 0x2010, 0x205E, 0xFF01, 0xFF5E, 0,
        };
        io.Fonts->AddFontFromFileTTF("unifont.ttf", 16.0f, &font_config, korean_ranges);

        SetupCustomStyle();
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        return true;
    }

    void Application::SetupCustomStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
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
        ImVec4 *colors = style.Colors;
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

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        if (ImGui::Begin("MainAppWindow", nullptr, window_flags)) {
            RenderHeader();

            switch (m_currentMode) {
                case Mode::WIRING:
                    RenderWiringModeUI();
                    break;
                case Mode::PROGRAMMING:
                    if (m_programmingMode) {
                        m_programmingMode->RenderProgrammingModeUI(m_isPlcRunning);
                    }
                    break;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);

        // === Phase 4: 엔진 상태 패널 ===
        if (m_currentMode == Mode::PROGRAMMING && m_programmingMode) {
            RenderPLCDebugPanel();
        }
    }

    void Application::RenderWiringModeUI() {
        RenderToolbar();
        RenderMainArea();
    }

    void Application::RenderHeader() {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        if (ImGui::BeginChild("Header", ImVec2(0, 80), true, ImGuiWindowFlags_NoScrollbar)) {
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

            bool isWiringMode = (m_currentMode == Mode::WIRING);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  isWiringMode ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f) : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  isWiringMode ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("실배선 모드", ImVec2(100, 30))) {
                m_currentMode = Mode::WIRING;
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  !isWiringMode ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f) : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  !isWiringMode ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("프로그래밍 모드", ImVec2(120, 30))) {
                m_currentMode = Mode::PROGRAMMING;
            }
            ImGui::PopStyleColor(2);

            ImGui::NextColumn();
            float rightX = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - 150;
            ImGui::SetCursorPosX(rightX);
            ImGui::SetCursorPosY(25);
            ImVec4 statusColor = m_isPlcRunning ? ImVec4(0.2f, 0.7f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
            const char *statusText = m_isPlcRunning ? "[RUN]" : "[STOP]";
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
            ImGui::TextUnformatted(statusText);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            if (ImGui::Button(m_isPlcRunning ? "STOP" : "RUN", ImVec2(80, 30))) {
                if (!m_isPlcRunning) {
                    // === PLC STOP → RUN 전환 초기화 ===
                    std::cout << "🚀 PLC RUN: Initializing PLC system..." << std::endl;

                    // 1. 래더 프로그램 동기화
                    SyncLadderProgramFromProgrammingMode();

                    // 2. 모든 PLC 디바이스 상태 초기화
                    m_plcDeviceStates.clear();
                    for (int i = 0; i < 16; ++i) {
                        std::string i_str = std::to_string(i);
                        m_plcDeviceStates["X" + i_str] = false;  // 입력은 물리 상태에 따라 업데이트됨
                        m_plcDeviceStates["Y" + i_str] = false;  // 출력 초기화
                        m_plcDeviceStates["M" + i_str] = false;  // 내부 릴레이 초기화
                    }

                    // 3. 타이머/카운터 상태 완전 초기화
                    for (auto &[address, timer]: m_plcTimerStates) {
                        timer.value = 0;
                        timer.done = false;
                        timer.enabled = false;
                    }
                    for (auto &[address, counter]: m_plcCounterStates) {
                        counter.value = 0;
                        counter.done = false;
                        counter.lastPower = false;
                    }

                    // 4. CompiledPLCExecutor 초기화 및 컴파일
                    if (m_compiledPLCExecutor) {
                        std::cout << "🔄 Resetting CompiledPLCExecutor memory..." << std::endl;
                        m_compiledPLCExecutor->ResetMemory();  // 메모리 상태 완전 초기화

                        std::cout << "🔄 Recompiling ladder program for execution..." << std::endl;
                        CompileAndLoadLadderProgram();
                    }

                    // 5. 프로그래밍 모드 초기화 (UI 상태 리셋)
                    if (m_programmingMode) {
                        // 모니터 모드는 PLC RUN 상태에서 자동 활성화됨
                        std::cout << "🖥️ Activating monitor mode..." << std::endl;
                    }

                    std::cout << "✅ PLC RUN: System initialized and ready for execution!" << std::endl;
                } else {
                    // === PLC RUN → STOP 전환 처리 ===
                    std::cout << "⏹️ PLC STOP: Stopping execution..." << std::endl;

                    // 모든 출력 비활성화
                    for (int i = 0; i < 16; ++i) {
                        SetPlcDeviceState("Y" + std::to_string(i), false);
                    }

                    std::cout << "⏹️ PLC STOP: All outputs deactivated" << std::endl;
                }

                m_isPlcRunning = !m_isPlcRunning;
            }

            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void Application::RenderToolbar() {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
        if (ImGui::BeginChild("Toolbar", ImVec2(0, 60), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::SetCursorPos(ImVec2(20, 15));
            const char *toolNames[] = {"선택도구", "공압 배선 도구", "전기 배선 도구"};
            const ToolType toolTypes[] = {ToolType::SELECT, ToolType::PNEUMATIC, ToolType::ELECTRIC};
            for (int i = 0; i < 3; i++) {
                if (i > 0) {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
                }
                bool is_selected = (m_currentTool == toolTypes[i]);
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      is_selected ? ImVec4(0.2f, 0.5f, 0.8f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      is_selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button(toolNames[i], ImVec2(180, 30))) { m_currentTool = toolTypes[i]; }
                ImGui::PopStyleColor(2);
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);

            if (ImGui::Button("프로젝트 불러오기", ImVec2(180, 30))) {
                // 🔥 **NEW**: .plcproj 프로젝트 파일 불러오기
#ifdef _WIN32
                OPENFILENAMEA ofn;
                CHAR szFile[260] = {0};
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = glfwGetWin32Window(m_window);
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = "PLC Project Files (*.plcproj)\0*.plcproj\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn) == TRUE) {
                    bool success = LoadProject(ofn.lpstrFile);
                    if (success) {
                        std::cout << "✅ 프로젝트 업로드 완료: " << ofn.lpstrFile << " → 래더 다이어그램 로드됨" << std::endl;
                    } else {
                        std::cout << "❌ 프로젝트 업로드 실패: " << ofn.lpstrFile << std::endl;
                    }
                }
#else
                bool success = LoadProject("project.plcproj"); // Fallback for non-windows
                if (success) {
                    std::cout << "✅ 프로젝트 업로드 완료: project.plcproj → 래더 다이어그램 로드됨" << std::endl;
                } else {
                    std::cout << "❌ 프로젝트 업로드 실패: project.plcproj" << std::endl;
                }
#endif
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void Application::RenderMainArea() {
        ImGui::Columns(2, "MainColumns", true);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.75f);
        RenderWiringCanvas();
        ImGui::NextColumn();
        RenderComponentList();
        ImGui::Columns(1);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
        if (ImGui::BeginChild("StatusBar", ImVec2(0, 25), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::SetCursorPosY(5);
            ImGui::SetCursorPosX(10);

            ImGui::Text("줌: %.1fx | 위치: (%.0f, %.0f) | 컴포넌트: %zu개 | 배선: %zu개",
                        m_cameraZoom,
                        m_cameraOffset.x, m_cameraOffset.y,
                        m_placedComponents.size(),
                        m_wires.size());

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
            const char *tool_name = "";
            switch (m_currentTool) {
                case ToolType::SELECT:
                    tool_name = "선택";
                    break;
                case ToolType::PNEUMATIC:
                    tool_name = "공압 배선";
                    break;
                case ToolType::ELECTRIC:
                    tool_name = "전기 배선";
                    break;
            }
            ImGui::Text("도구: %s", tool_name);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void Application::Cleanup() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (m_window) glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void Application::UpdatePortPositions() {
        m_portPositions.clear();
        for (const auto &comp: m_placedComponents) {
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
                Position worldPos = {comp.position.x + relPos.x, comp.position.y + relPos.y};
                m_portPositions[{comp.instanceId, portId}] = worldPos;
            }
        }
    }

    ImVec2 Application::WorldToScreen(const ImVec2 &world_pos) const {
        float x = m_canvasTopLeft.x + (world_pos.x + m_cameraOffset.x) * m_cameraZoom;
        float y = m_canvasTopLeft.y + (world_pos.y + m_cameraOffset.y) * m_cameraZoom;
        return ImVec2(x, y);
    }

    ImVec2 Application::ScreenToWorld(const ImVec2 &screen_pos) const {
        float x = (screen_pos.x - m_canvasTopLeft.x) / m_cameraZoom - m_cameraOffset.x;
        float y = (screen_pos.y - m_canvasTopLeft.y) / m_cameraZoom - m_cameraOffset.y;
        return ImVec2(x, y);
    }

    LadderInstructionType Application::StringToInstructionType(const std::string &str) {
        if (str == "XIC") return LadderInstructionType::XIC;
        if (str == "XIO") return LadderInstructionType::XIO;
        if (str == "OTE") return LadderInstructionType::OTE;
        if (str == "SET") return LadderInstructionType::SET;
        if (str == "RST") return LadderInstructionType::RST;
        if (str == "TON") return LadderInstructionType::TON;
        if (str == "CTU") return LadderInstructionType::CTU;
        if (str == "RST_TMR_CTR") return LadderInstructionType::RST_TMR_CTR;
        if (str == "HLINE") return LadderInstructionType::HLINE;
        return LadderInstructionType::EMPTY;
    }


    void Application::SyncLadderProgramFromProgrammingMode() {
        if (!m_programmingMode) {
            printf("Error: ProgrammingMode is not initialized. Cannot sync ladder program.\n");
            return;
        }

        // 기존 방식 유지 (하위 호환성)
        m_loadedLadderProgram = m_programmingMode->GetLadderProgram();
        m_plcDeviceStates = m_programmingMode->GetDeviceStates();
        m_plcTimerStates = m_programmingMode->GetTimerStates();
        m_plcCounterStates = m_programmingMode->GetCounterStates();

        // === Phase 1: PLCSimulatorCore로도 동기화 ===
        if (m_simulatorCore) {
            m_simulatorCore->SyncFromProgrammingMode(m_programmingMode.get());

            // === Phase 2: I/O 매핑도 함께 업데이트 ===
            if (m_simulatorCore->UpdateIOMapping()) {
                printf("🔗 I/O Mapping updated from wiring connections.\n");
            } else {
                printf("⚠️ I/O Mapping update failed - check wiring connections.\n");
            }

            printf("🔄 Data synchronized to PLCSimulatorCore as well.\n");
        }

        // === Phase 4: CompiledPLCExecutor 자동 컴파일 및 로드 ===
        if (m_compiledPLCExecutor) {
            CompileAndLoadLadderProgram();
        }
    }

    void Application::LoadLadderProgramFromLD(const std::string &filepath) {
        std::cout << "🔄 Loading ladder program from .ld file: " << filepath << std::endl;

        // LDToLadderConverter로 .ld 파일을 LadderProgram으로 변환
        LDToLadderConverter converter;
        converter.SetDebugMode(m_debugMode); // 디버그 모드 동기화

        LadderProgram loadedProgram;
        bool success = converter.ConvertFromLDFile(filepath, loadedProgram);

        if (!success) {
            std::cout << "❌ Failed to load .ld file: " << converter.GetLastError() << std::endl;
            std::cout << "💡 .ld 파일 형식을 확인하거나 프로그래밍 모드에서 직접 작성해주세요." << std::endl;
            return;
        }

        // 변환 성공 - 통계 정보 출력
        auto stats = converter.GetConversionStats();
        std::cout << "✅ Successfully loaded .ld file!" << std::endl;
        std::cout << "📊 Conversion stats:" << std::endl;
        std::cout << "   - Networks: " << stats.networksCount << std::endl;
        std::cout << "   - Contacts: " << stats.contactsCount << std::endl;
        std::cout << "   - Coils: " << stats.coilsCount << std::endl;
        std::cout << "   - Blocks: " << stats.blocksCount << std::endl;

        // Application의 래더 프로그램 업데이트
        m_loadedLadderProgram = loadedProgram;

        // ProgrammingMode에도 동기화 (UI에서 편집 가능하도록)
        if (m_programmingMode) {
            // ProgrammingMode의 내부 래더 프로그램을 직접 업데이트
            // 이는 향후 ProgrammingMode에 SetLadderProgram() 메서드 추가 시 사용
            std::cout << "🔄 Syncing to Programming Mode for UI editing..." << std::endl;

            // 임시로 SyncLadderProgramFromProgrammingMode 역방향 동기화
            // (향후 개선 예정)
        }

        // PLC 디바이스 상태 초기화
        m_plcDeviceStates.clear();
        m_plcTimerStates.clear();
        m_plcCounterStates.clear();

        // 기본 X, Y, M 디바이스 초기화
        for (int i = 0; i < 16; ++i) {
            std::string i_str = std::to_string(i);
            m_plcDeviceStates["X" + i_str] = false;
            m_plcDeviceStates["Y" + i_str] = false;
            m_plcDeviceStates["M" + i_str] = false;
        }

        // 타이머/카운터 상태 초기화 (래더 프로그램에서 사용되는 것들)
        for (const auto &rung: loadedProgram.rungs) {
            if (rung.isEndRung) continue;

            for (const auto &cell: rung.cells) {
                if (!cell.address.empty()) {
                    if (cell.type == LadderInstructionType::TON) {
                        TimerState &timer = m_plcTimerStates[cell.address];
                        timer.preset = 10; // 기본값

                        // 프리셋 값 파싱 (K10 형태)
                        if (!cell.preset.empty() && cell.preset[0] == 'K') {
                            try {
                                timer.preset = std::stoi(cell.preset.substr(1));
                            } catch (const std::exception &) {
                                timer.preset = 10;
                            }
                        }
                    } else if (cell.type == LadderInstructionType::CTU) {
                        CounterState &counter = m_plcCounterStates[cell.address];
                        counter.preset = 10; // 기본값

                        // 프리셋 값 파싱 (K10 형태)
                        if (!cell.preset.empty() && cell.preset[0] == 'K') {
                            try {
                                counter.preset = std::stoi(cell.preset.substr(1));
                            } catch (const std::exception &) {
                                counter.preset = 10;
                            }
                        }
                    }
                }
            }
        }

        std::cout << "🔧 Initialized " << m_plcTimerStates.size() << " timers and "
                  << m_plcCounterStates.size() << " counters" << std::endl;

        // 자동 컴파일 및 로드 수행
        if (m_compiledPLCExecutor) {
            std::cout << "🔄 Auto-compiling loaded ladder program..." << std::endl;
            CompileAndLoadLadderProgram();
        }

        std::cout << "✅ Ladder program loaded and ready for execution!" << std::endl;
    }

    void Application::CompileAndLoadLadderProgram() {
        if (!m_compiledPLCExecutor) {
            std::cout << "⚠️ CompiledPLCExecutor not initialized" << std::endl;
            return;
        }

        if (m_loadedLadderProgram.rungs.empty()) {
            std::cout << "📝 No ladder program to compile" << std::endl;
            return;
        }

        try {
            // Step 1: Convert ladder program to .ld file
            std::cout << "🔄 Converting ladder program to OpenPLC .ld format..." << std::endl;

            LadderToLDConverter converter;
            converter.SetDebugMode(m_enableDebugLogging);

            // 임시 .ld 파일 생성
            std::string tempLdPath = "temp_ladder_program.ld";
            bool convertSuccess = converter.ConvertToLDFile(m_loadedLadderProgram, tempLdPath);

            if (!convertSuccess) {
                std::cout << "❌ Failed to convert ladder program to .ld format: " << converter.GetLastError()
                          << std::endl;
                return;
            }

            std::cout << "✅ Ladder program converted to .ld format successfully" << std::endl;

            // Step 2: Compile .ld file to C++ code
            std::cout << "🔄 Compiling .ld file to C++ code..." << std::endl;

            OpenPLCCompilerIntegration compiler;
            compiler.SetDebugMode(m_enableDebugLogging);
            compiler.SetIOConfiguration(16, 16); // FX3U-32M: 16 inputs, 16 outputs

            auto compilationResult = compiler.CompileLDFile(tempLdPath);

            if (!compilationResult.success) {
                std::cout << "❌ Failed to compile .ld file: " << compilationResult.errorMessage << std::endl;
                return;
            }

            std::cout << "✅ .ld file compiled successfully" << std::endl;
            std::cout << "📊 Compilation statistics:" << std::endl;
            std::cout << "   - Generated code size: " << compilationResult.generatedCode.length() << " characters"
                      << std::endl;
            std::cout << "   - Input count: " << compilationResult.inputCount << std::endl;
            std::cout << "   - Output count: " << compilationResult.outputCount << std::endl;
            std::cout << "   - Memory count: " << compilationResult.memoryCount << std::endl;

            // Step 3: Load compiled code into CompiledPLCExecutor
            std::cout << "🔄 Loading compiled code into PLC executor..." << std::endl;

            bool loadSuccess = m_compiledPLCExecutor->LoadFromCompilationResult(compilationResult);

            if (!loadSuccess) {
                std::cout << "❌ Failed to load compiled code into PLC executor" << std::endl;
                return;
            }

            std::cout << "✅ Compiled ladder program loaded successfully!" << std::endl;
            std::cout << "🚀 CompiledPLCExecutor is ready for execution" << std::endl;

            // Step 4: Cleanup temporary files
            std::remove(tempLdPath.c_str());

        } catch (const std::exception &e) {
            std::cout << "❌ Exception during compilation: " << e.what() << std::endl;
        }
    }

    void Application::CreateDefaultTestLadderProgram() {
        std::cout << "🔧 Creating default test ladder program..." << std::endl;

        // 기본 래더 프로그램 생성: X0 → Y0, X1 → Y1
        m_loadedLadderProgram.rungs.clear();
        m_loadedLadderProgram.verticalConnections.clear();

        // Rung 0: X0 → Y0
        Rung rung0;
        rung0.number = 0;
        rung0.cells.resize(12);

        // X0 (XIC)
        rung0.cells[0].type = LadderInstructionType::XIC;
        rung0.cells[0].address = "X0";

        // 수평선 연결
        for (int i = 1; i < 11; i++) {
            rung0.cells[i].type = LadderInstructionType::HLINE;
        }

        // Y0 (OTE)
        rung0.cells[11].type = LadderInstructionType::OTE;
        rung0.cells[11].address = "Y0";

        m_loadedLadderProgram.rungs.push_back(rung0);

        // Rung 1: X1 → Y1
        Rung rung1;
        rung1.number = 1;
        rung1.cells.resize(12);

        // X1 (XIC)
        rung1.cells[0].type = LadderInstructionType::XIC;
        rung1.cells[0].address = "X1";

        // 수평선 연결
        for (int i = 1; i < 11; i++) {
            rung1.cells[i].type = LadderInstructionType::HLINE;
        }

        // Y1 (OTE)
        rung1.cells[11].type = LadderInstructionType::OTE;
        rung1.cells[11].address = "Y1";

        m_loadedLadderProgram.rungs.push_back(rung1);

        // End Rung
        Rung endRung;
        endRung.number = 2;
        endRung.isEndRung = true;
        m_loadedLadderProgram.rungs.push_back(endRung);

        // 기본 디바이스 상태 초기화
        m_plcDeviceStates.clear();
        for (int i = 0; i < 16; i++) {
            m_plcDeviceStates["X" + std::to_string(i)] = false;
            m_plcDeviceStates["Y" + std::to_string(i)] = false;
        }

        // 타이머/카운터 상태 초기화
        m_plcTimerStates.clear();
        m_plcCounterStates.clear();

        std::cout << "✅ Default test ladder program created: X0→Y0, X1→Y1" << std::endl;
    }


// - Ctrl+F12 키 조합 처리 시 호출
// - 콘솔 창 할당 및 디버그 상태 출력
    void Application::ToggleDebugMode() {
        m_debugMode = !m_debugMode;

        if (m_debugMode) {
#ifdef _WIN32
            // Windows에서 콘솔 창 할당 (없으면 새로 생성)
            if (!GetConsoleWindow()) {
                AllocConsole();
                freopen_s((FILE **) stdout, "CONOUT$", "w", stdout);
                freopen_s((FILE **) stderr, "CONOUT$", "w", stderr);
                freopen_s((FILE **) stdin, "CONIN$", "r", stdin);
            }
#endif
            PrintDebugToConsole("[DEBUG MODE ON] Physics Engine Debug System Activated");
            PrintDebugToConsole("Press Ctrl+F12 again to disable debug mode");
            PrintDebugToConsole("==========================================");
        } else {
            PrintDebugToConsole("[DEBUG MODE OFF] Debug System Deactivated");
            PrintDebugToConsole("==========================================");
        }
    }


// - 출력 주기 제어 (60FPS에서 3초마다 출력)
// - 각 디버그 카테고리별 상태 수집 및 출력
    void Application::UpdateDebugLogging() {
        m_debugUpdateCounter++;

        // 3초마다 디버그 정보 출력 (60FPS 기준 180프레임)
        if (m_debugUpdateCounter % 180 == 0) {
            m_debugLogBuffer.clear();

            PrintDebugToConsole("========== DEBUG STATUS UPDATE ==========");
            PrintDebugToConsole(
                    "Frame: " + std::to_string(m_debugUpdateCounter) + " | PLC: " + (m_isPlcRunning ? "RUN" : "STOP"));

            // 컴포넌트 상태 로깅
            LogComponentStates();

            // 물리엔진 상태 로깅 (물리엔진이 활성화된 경우에만)
            if (m_physicsEngine && m_physicsEngine->isInitialized) {
                LogPhysicsEngineStates();
            } else {
                PrintDebugToConsole("Physics Engine: NOT INITIALIZED or LEGACY MODE");
            }

            PrintDebugToConsole("============================================");
        }
    }


// - 실린더 위치, 리밋스위치 상태, 전압/압력 정보 출력
// - 문제 진단을 위한 핵심 정보 수집
    void Application::LogComponentStates() {
        PrintDebugToConsole("=== COMPONENT STATES ===");

        int cylinderCount = 0, limitSwitchCount = 0, sensorCount = 0;

        for (const auto &comp: m_placedComponents) {
            switch (comp.type) {
                case ComponentType::CYLINDER: {
                    cylinderCount++;
                    float position = comp.internalStates.count("position") ? comp.internalStates.at("position") : 0.0f;
                    float velocity = comp.internalStates.count("velocity") ? comp.internalStates.at("velocity") : 0.0f;
                    float pressureA = comp.internalStates.count("pressure_a") ? comp.internalStates.at("pressure_a")
                                                                              : 0.0f;
                    float pressureB = comp.internalStates.count("pressure_b") ? comp.internalStates.at("pressure_b")
                                                                              : 0.0f;

                    PrintDebugToConsole("  Cylinder[" + std::to_string(comp.instanceId) + "] Pos:" +
                                        std::to_string(position) + "mm Vel:" + std::to_string(velocity) +
                                        "mm/s PA:" + std::to_string(pressureA) + "bar PB:" + std::to_string(pressureB) +
                                        "bar");
                    break;
                }

                case ComponentType::LIMIT_SWITCH: {
                    limitSwitchCount++;
                    bool isPressed =
                            comp.internalStates.count("is_pressed") && comp.internalStates.at("is_pressed") > 0.5f;
                    bool manualPress = comp.internalStates.count("is_pressed_manual") &&
                                       comp.internalStates.at("is_pressed_manual") > 0.5f;

                    // 리밋스위치와 가장 가까운 실린더 간 거리 계산
                    float closestDistance = 999999.0f;
                    int closestCylinderId = -1;
                    for (const auto &cylinder: m_placedComponents) {
                        if (cylinder.type == ComponentType::CYLINDER) {
                            float cylinderX_body_start = cylinder.position.x + 170.0f;
                            float pistonPosition = cylinder.internalStates.count("position")
                                                   ? cylinder.internalStates.at("position") : 0.0f;
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

                    PrintDebugToConsole("  LimitSwitch[" + std::to_string(comp.instanceId) + "] Pressed:" +
                                        (isPressed ? "YES" : "NO") + " Manual:" + (manualPress ? "YES" : "NO") +
                                        " Pos:(" + std::to_string((int) comp.position.x) + "," +
                                        std::to_string((int) comp.position.y) + ")" +
                                        " ClosestCyl[" + std::to_string(closestCylinderId) + "]:" +
                                        std::to_string((int) closestDistance) + "mm");
                    break;
                }

                case ComponentType::SENSOR: {
                    sensorCount++;
                    bool isDetected =
                            comp.internalStates.count("is_detected") && comp.internalStates.at("is_detected") > 0.5f;
                    bool isPowered =
                            comp.internalStates.count("is_powered") && comp.internalStates.at("is_powered") > 0.5f;

                    PrintDebugToConsole("  Sensor[" + std::to_string(comp.instanceId) + "] Detected:" +
                                        (isDetected ? "YES" : "NO") + " Powered:" + (isPowered ? "YES" : "NO") +
                                        " Pos:(" + std::to_string((int) comp.position.x) + "," +
                                        std::to_string((int) comp.position.y) + ")");
                    break;
                }

                default:
                    break;
            }
        }

        PrintDebugToConsole("Total Components: " + std::to_string(cylinderCount) + " Cylinders, " +
                            std::to_string(limitSwitchCount) + " LimitSwitches, " + std::to_string(sensorCount) +
                            " Sensors");

        // PLC 디바이스 상태 (Y 출력만)
        std::string activeOutputs = "";
        for (const auto &pair: m_plcDeviceStates) {
            if (pair.first[0] == 'Y' && pair.second) {
                if (!activeOutputs.empty()) activeOutputs += ", ";
                activeOutputs += pair.first;
            }
        }
        PrintDebugToConsole("Active PLC Outputs: " + (activeOutputs.empty() ? "NONE" : activeOutputs));
    }

//  물리엔진 상태 로깅
// - 전기/공압/기계 네트워크 상태 확인
// - 물리엔진 초기화 및 동작 상태 진단
    void Application::LogPhysicsEngineStates() {
        PrintDebugToConsole("=== PHYSICS ENGINE STATES ===");

        if (!m_physicsEngine) {
            PrintDebugToConsole("ERROR: Physics Engine: NULL POINTER");
            return;
        }

        PrintDebugToConsole(
                "Engine Status: " + std::string(m_physicsEngine->isInitialized ? "INITIALIZED" : "NOT INITIALIZED"));
        PrintDebugToConsole("Active Components: " + std::to_string(m_physicsEngine->activeComponents) +
                            "/" + std::to_string(m_physicsEngine->maxComponents));

        // 전기 네트워크 상태
        LogElectricalNetwork();

        // 공압 네트워크 상태
        LogPneumaticNetwork();

        // 기계 시스템 상태
        LogMechanicalSystem();
    }

//  전기 네트워크 상태 로깅
    void Application::LogElectricalNetwork() {
        if (!m_physicsEngine->electricalNetwork) {
            PrintDebugToConsole("Electrical Network: NULL");
            return;
        }

        ElectricalNetwork *elec = m_physicsEngine->electricalNetwork;
        PrintDebugToConsole("Electrical Network: " + std::to_string(elec->nodeCount) + " nodes, " +
                            std::to_string(elec->edgeCount) + " edges");
        PrintDebugToConsole("Convergence: " + std::string(elec->isConverged ? "YES" : "NO") +
                            " Iterations: " + std::to_string(elec->currentIteration));
    }

//  공압 네트워크 상태 로깅
    void Application::LogPneumaticNetwork() {
        if (!m_physicsEngine->pneumaticNetwork) {
            PrintDebugToConsole("Pneumatic Network: NULL");
            return;
        }

        PneumaticNetwork *pneu = m_physicsEngine->pneumaticNetwork;
        PrintDebugToConsole("Pneumatic Network: " + std::to_string(pneu->nodeCount) + " nodes, " +
                            std::to_string(pneu->edgeCount) + " edges");
        PrintDebugToConsole("Convergence: " + std::string(pneu->isConverged ? "YES" : "NO") +
                            " Iterations: " + std::to_string(pneu->currentIteration));
    }

//  기계 시스템 상태 로깅
    void Application::LogMechanicalSystem() {
        if (!m_physicsEngine->mechanicalSystem) {
            PrintDebugToConsole("Mechanical System: NULL");
            return;
        }

        MechanicalSystem *mech = m_physicsEngine->mechanicalSystem;
        PrintDebugToConsole("Mechanical System: " + std::to_string(mech->nodeCount) + " nodes, " +
                            std::to_string(mech->edgeCount) + " edges");
        PrintDebugToConsole("Stability: " + std::string(mech->isStable ? "STABLE" : "UNSTABLE") +
                            " Time: " + std::to_string(mech->currentTime) + "s");
    }

//   콘솔 출력 함수
// - Windows 콘솔 및 표준 출력 동시 지원
// - 타임스탬프 및 색상 코드 포함
    void Application::PrintDebugToConsole(const std::string &message) {
        // 표준 출력 (원래 터미널/IDE 콘솔)
        std::cout << message << std::endl;

#ifdef _WIN32
        // Windows 콘솔 창에도 출력 (AllocConsole로 생성된 창)
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            DWORD written;
            std::string fullMessage = message + "\n";
            WriteConsoleA(hConsole, fullMessage.c_str(), static_cast<DWORD>(fullMessage.length()), &written, NULL);
        }
#endif
    }

// =============================================================================
// 🔥 **NEW**: .plcproj 프로젝트 파일 관리 함수들
// =============================================================================

    bool Application::SaveProject(const std::string &filePath, const std::string &projectName) {
        std::cout << "🚀 Saving project to: " << filePath << std::endl;

        if (!m_programmingMode || !m_projectFileManager) {
            std::cerr << "❌ Project save failed: Components not initialized" << std::endl;
            return false;
        }

        try {
            // 1. 프로그래밍 모드에서 현재 래더 프로그램 가져오기
            const LadderProgram &currentProgram = m_programmingMode->GetLadderProgram();

            // 2. 프로젝트 매니저를 통해 .plcproj 파일로 저장
            m_projectFileManager->SetDebugMode(m_enableDebugLogging);
            auto result = m_projectFileManager->SaveProject(currentProgram, filePath, projectName);

            if (result.success) {
                std::cout << "✅ Project saved successfully!" << std::endl;
                std::cout << "📊 Project info:" << std::endl;
                std::cout << "   - Name: " << result.info.projectName << std::endl;
                std::cout << "   - Size: " << result.compressedSize << " bytes (compressed)" << std::endl;
                std::cout << "   - Rungs: " << currentProgram.rungs.size() << std::endl;
                std::cout << "   - Vertical connections: " << currentProgram.verticalConnections.size() << std::endl;
                std::cout << "   - Layout checksum: " << result.info.layoutChecksum << std::endl;
                return true;
            } else {
                std::cerr << "❌ Project save failed: " << result.errorMessage << std::endl;
                return false;
            }

        } catch (const std::exception &e) {
            std::cerr << "❌ Project save exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool Application::LoadProject(const std::string &filePath) {
        std::cout << "📂 Loading project from: " << filePath << std::endl;

        if (!m_programmingMode || !m_projectFileManager) {
            std::cerr << "❌ Project load failed: Components not initialized" << std::endl;
            return false;
        }

        try {
            // 1. 프로젝트 매니저를 통해 .plcproj 파일에서 로드
            m_projectFileManager->SetDebugMode(m_enableDebugLogging);
            auto result = m_projectFileManager->LoadProject(filePath);

            if (!result.success) {
                std::cerr << "❌ Project load failed: " << result.errorMessage << std::endl;
                return false;
            }

            std::cout << "✅ [DEBUG] Project load successful!" << std::endl;
            std::cout << "📊 [DEBUG] Loaded program info:" << std::endl;
            std::cout << "   - Rungs: " << result.program.rungs.size() << std::endl;
            std::cout << "   - Vertical connections: " << result.program.verticalConnections.size() << std::endl;

            // 2. 로드된 래더 프로그램을 Application 상태로 설정
            m_loadedLadderProgram = result.program;

            // 3. ProgrammingMode도 업데이트
            std::cout << "🔄 [DEBUG] Updating ProgrammingMode with loaded program..." << std::endl;
            m_programmingMode->SetLadderProgram(result.program);

            // 4. PLC 디바이스 상태 초기화
            m_plcDeviceStates.clear();
            m_plcTimerStates.clear();
            m_plcCounterStates.clear();

            // 기본 디바이스 상태 초기화
            for (int i = 0; i <= 15; ++i) {
                m_plcDeviceStates["X" + std::to_string(i)] = false;
                m_plcDeviceStates["Y" + std::to_string(i)] = false;
            }
            for (int i = 0; i <= 999; ++i) {
                std::string i_str = std::to_string(i);
                m_plcDeviceStates["M" + i_str] = false;
            }

            // 5. OpenPLC 엔진에 자동 컴파일 및 로드
            if (m_compiledPLCExecutor) {
                CompileAndLoadLadderProgram();
            }

            // 6. 성공 로그 출력
            std::cout << "✅ Project loaded successfully!" << std::endl;
            std::cout << "📊 Project info:" << std::endl;
            std::cout << "   - Name: " << result.info.projectName << std::endl;
            std::cout << "   - Version: " << result.info.version << std::endl;
            std::cout << "   - Created: " << result.info.createdDate << std::endl;
            std::cout << "   - Tool version: " << result.info.toolVersion << std::endl;
            std::cout << "   - Rungs: " << result.program.rungs.size() << std::endl;
            std::cout << "   - Vertical connections: " << result.program.verticalConnections.size() << std::endl;

            if (result.info.layoutChecksum != 0) {
                std::cout << "   - Layout checksum: " << result.info.layoutChecksum << " ✓" << std::endl;
            }

            if (!result.ldProgram.empty()) {
                std::cout << "   - LD program size: " << result.ldProgram.size() << " bytes" << std::endl;
            }

            return true;

        } catch (const std::exception &e) {
            std::cerr << "❌ Project load exception: " << e.what() << std::endl;
            return false;
        }
    }

    void Application::RenderPLCDebugPanel() {
        if (!m_programmingMode) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 defaultPos(io.DisplaySize.x - 320.0f, 90.0f);
        ImGui::SetNextWindowPos(defaultPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
        if (ImGui::Begin("PLC Engine Status", nullptr, flags)) {
            const char* engineType = m_programmingMode->GetEngineType();
            bool compiledLoaded = m_programmingMode->HasCompiledCodeLoaded();
            bool needRecompile = m_programmingMode->IsRecompileNeeded();

            ImGui::Text("Engine: %s", engineType);
            ImGui::Text("Compiled Code: %s", compiledLoaded ? "Loaded" : "Not Loaded");
            ImGui::Text("Recompile Needed: %s", needRecompile ? "Yes" : "No");

            const std::string& lastCompileErr = m_programmingMode->GetLastCompileError();
            if (!lastCompileErr.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
                ImGui::TextWrapped("Compile Error: %s", lastCompileErr.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Separator();
            bool lastScanOk = m_programmingMode->GetLastScanSuccess();
            int cycleUs = m_programmingMode->GetLastCycleTimeUs();
            int instr = m_programmingMode->GetLastInstructionCount();
            const std::string& scanErr = m_programmingMode->GetLastScanError();

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
            bool gx2 = m_programmingMode->IsGX2NormalizationEnabled();
            ImGui::Text("GX2 Normalize: %s", gx2 ? "Enabled" : "Disabled");
            if (gx2) {
                ImGui::Text("Fix Count: %d", m_programmingMode->GetNormalizationFixCount());
                if (ImGui::CollapsingHeader("Normalization Summary", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const std::string& summary = m_programmingMode->GetNormalizationSummary();
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
}

