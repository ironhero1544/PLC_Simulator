// application.h

//
// Main application class for the PLC emulator.
// PLC 에뮬레이터의 메인 애플리케이션 클래스입니다.
//
// This file manages the application lifecycle and coordinates mode switching.
// 이 파일은 애플리케이션의 생명주기를 관리하고 모드 간 전환을 조정합니다.
//

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_

#include "imgui.h"
#include "plc_emulator/core/data_types.h"
#include "plc_emulator/core/plc_simulator_core.h"
#include "plc_emulator/physics/physics_engine.h"
#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/project_file_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;

namespace plc {

    class ProgrammingMode;

//
// The main class that orchestrates the entire PLC emulator application.
// 전체 PLC 에뮬레이터 애플리케이션을 총괄하는 메인 클래스입니다.
//
    class Application {
    public:
        /**
         * @brief Constructs the Application object and initializes default values.
         * Application 객체를 생성하고 기본값을 초기화합니다.
         */
        Application();

        /**
         * @brief Destructor. Ensures all resources are cleaned up properly.
         * 소멸자. 모든 리소스가 올바르게 정리되도록 보장합니다.
         */
        ~Application();

        // Disable copy and assignment to prevent accidental duplication.
        // 의도하지 않은 복제를 방지하기 위해 복사 및 할당을 비활성화합니다.
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        /**
         * @brief Initializes all subsystems, including the window, GUI, and simulator.
         * 윈도우, GUI, 시뮬레이터를 포함한 모든 하위 시스템을 초기화합니다.
         * @return True if initialization is successful, false otherwise.
         * 초기화에 성공하면 true, 그렇지 않으면 false를 반환합니다.
         */
        bool Initialize();

        /**
         * @brief Starts and runs the main application loop.
         * 메인 애플리케이션 루프를 시작하고 실행합니다.
         */
        void Run();

        /**
         * @brief Shuts down the application and releases all resources.
         * 애플리케이션을 종료하고 모든 리소스를 해제합니다.
         */
        void Shutdown();

        /**
         * @brief Saves the current project state to a .plcproj file.
         * 현재 프로젝트 상태를 .plcproj 파일에 저장합니다.
         * @param filePath The path to save the project file.
         * filePath는 프로젝트 파일을 저장할 경로입니다.
         * @param projectName An optional name for the project.
         * projectName은 프로젝트의 선택적 이름입니다.
         * @return True on successful save, false otherwise.
         * 저장에 성공하면 true, 그렇지 않으면 false를 반환합니다.
         */
        bool SaveProject(const std::string& filePath,
                         const std::string& projectName = "");

        /**
         * @brief Loads a project from a .plcproj file.
         * .plcproj 파일로부터 프로젝트를 불러옵니다.
         * @param filePath The path of the project file to load.
         * filePath는 불러올 프로젝트 파일의 경로입니다.
         * @return True on successful load, false otherwise.
         * 불러오기에 성공하면 true, 그렇지 않으면 false를 반환합니다.
         */
        bool LoadProject(const std::string& filePath);

    private:
        /**
         * @brief Initializes the GLFW window.
         * GLFW 윈도우를 초기화합니다.
         */
        bool InitializeWindow();

        /**
         * @brief Initializes the ImGui context.
         * ImGui 컨텍스트를 초기화합니다.
         */
        bool InitializeImGui();

        /**
         * @brief Sets up a custom visual style for ImGui.
         * ImGui에 대한 사용자 정의 비주얼 스타일을 설정합니다.
         */
        void SetupCustomStyle();

        /**
         * @brief Processes user input for the current frame.
         * 현재 프레임에 대한 사용자 입력을 처리합니다.
         */
        void ProcessInput();

        /**
         * @brief Main update function called once per frame.
         * 프레임당 한 번 호출되는 메인 업데이트 함수입니다.
         */
        void Update();

        /**
         * @brief Updates the physics simulation state.
         * 물리 시뮬레이션 상태를 업데이트합니다.
         */
        void UpdatePhysics();

        /**
         * @brief Simulates basic physics phenomena, independent of the PLC state.
         * PLC 상태와 독립적으로 기본적인 물리 현상을 시뮬레이션합니다.
         */
        void UpdateBasicPhysics();

        /**
         * @brief Executes one scan of the loaded ladder logic program.
         * 불러온 래더 로직 프로그램의 한 스캔을 실행합니다.
         */
        void SimulateLoadedLadder();

        /**
         * @brief Gets the current state of a PLC device (e.g., input, output).
         * PLC 디바이스(예: 입력, 출력)의 현재 상태를 가져옵니다.
         */
        bool GetPlcDeviceState(const std::string& address);

        /**
         * @brief Sets the state of a PLC device.
         * PLC 디바이스의 상태를 설정합니다.
         */
        void SetPlcDeviceState(const std::string& address, bool state);

        /**
         * @brief Loads a ladder program from a specified .ld file.
         * 지정된 .ld 파일로부터 래더 프로그램을 불러옵니다.
         * @param filepath Path to the .ld file.
         * filepath는 .ld 파일의 경로입니다.
         */
        void LoadLadderProgramFromLD(const std::string& filepath);

        /**
         * @brief Converts a string representation of an instruction to its enum type.
         * 명령어의 문자열 표현을 해당 열거형 타입으로 변환합니다.
         */
        LadderInstructionType StringToInstructionType(const std::string& str);

        /**
         * @brief Synchronizes the ladder program from the programming mode to the simulator.
         * 프로그래밍 모드의 래더 프로그램을 시뮬레이터로 동기화합니다.
         */
        void SyncLadderProgramFromProgrammingMode();

        /**
         * @brief Creates a default ladder program for testing and initialization.
         * 테스트 및 초기화를 위해 기본 래더 프로그램을 생성합니다.
         */
        void CreateDefaultTestLadderProgram();

        /**
         * @brief Compiles the current ladder program and loads it into the PLC executor.
         * 현재 래더 프로그램을 컴파일하고 PLC 실행기에 로드합니다.
         */
        void CompileAndLoadLadderProgram();

        /**
         * @brief Simulates the electrical network connecting components.
         * 컴포넌트를 연결하는 전기 네트워크를 시뮬레이션합니다.
         */
        void SimulateElectrical();

        /**
         * @brief Updates the internal logic of placed components.
         * 배치된 컴포넌트의 내부 로직을 업데이트합니다.
         */
        void UpdateComponentLogic();

        /**
         * @brief Simulates the pneumatic network connecting components.
         * 컴포넌트를 연결하는 공압 네트워크를 시뮬레이션합니다.
         */
        void SimulatePneumatic();

        /**
         * @brief Updates the state of actuators (e.g., cylinders) based on simulation.
         * 시뮬레이션 결과에 따라 액추에이터(예: 실린더)의 상태를 업데이트합니다.
         */
        void UpdateActuators();

        /**
         * @brief Retrieves the port information for a given component.
         * 주어진 컴포넌트의 포트 정보를 가져옵니다.
         */
        std::vector<std::pair<int, bool>> GetPortsForComponent(
                const PlacedComponent& comp);

        /**
         * @brief Synchronizes PLC output states to the physics engine's electrical network.
         * PLC 출력 상태를 물리 엔진의 전기 네트워크와 동기화합니다.
         */
        void SyncPLCOutputsToPhysicsEngine();

        /**
         * @brief Synchronizes results from the physics engine back to the application state.
         * 물리 엔진의 결과를 애플리케이션 상태로 다시 동기화합니다.
         */
        void SyncPhysicsEngineToApplication();

        /**
         * @brief Updates and displays physics engine performance statistics.
         * 물리 엔진의 성능 통계를 업데이트하고 표시합니다.
         */
        void UpdatePhysicsPerformanceStats();

        /**
         * @brief Renders the entire application scene for the current frame.
         * 현재 프레임에 대한 전체 애플리케이션 장면을 렌더링합니다.
         */
        void Render();

        /**
         * @brief Cleans up resources before shutting down.
         * 종료하기 전에 리소스를 정리합니다.
         */
        void Cleanup();

        /**
         * @brief Updates the screen positions of all component ports.
         * 모든 컴포넌트 포트의 화면 위치를 업데이트합니다.
         */
        void UpdatePortPositions();

        /**
         * @brief Renders the main user interface using ImGui.
         * ImGui를 사용하여 메인 사용자 인터페이스를 렌더링합니다.
         */
        void RenderUI();

        /**
         * @brief Renders the UI specific to the wiring mode.
         * 배선 모드에 특화된 UI를 렌더링합니다.
         */
        void RenderWiringModeUI();

        /**
         * @brief Renders the application header bar.
         * 애플리케이션 헤더 바를 렌더링합니다.
         */
        void RenderHeader();

        /**
         * @brief Renders the toolbar with mode and tool selection.
         * 모드 및 도구 선택 기능이 있는 툴바를 렌더링합니다.
         */
        void RenderToolbar();

        /**
         * @brief Renders the main content area of the application.
         * 애플리케이션의 주 콘텐츠 영역을 렌더링합니다.
         */
        void RenderMainArea();

        /**
         * @brief Renders the real-time PLC debug panel.
         * 실시간 PLC 디버그 패널을 렌더링합니다.
         */
        void RenderPLCDebugPanel();

        /**
         * @brief Converts world coordinates to screen coordinates.
         * 월드 좌표를 화면 좌표로 변환합니다.
         */
        ImVec2 WorldToScreen(const ImVec2& world_pos) const;

        /**
         * @brief Converts screen coordinates to world coordinates.
         * 화면 좌표를 월드 좌표로 변환합니다.
         */
        ImVec2 ScreenToWorld(const ImVec2& screen_pos) const;

        /**
         * @brief Renders the list of available components.
         * 사용 가능한 컴포넌트 목록을 렌더링합니다.
         */
        void RenderComponentList();

        /**
         * @brief Renders all components placed in the workspace.
         * 작업 공간에 배치된 모든 컴포넌트를 렌더링합니다.
         */
        void RenderPlacedComponents(ImDrawList* draw_list);

        /**
         * @brief Handles the start of a component drag operation from the component list.
         * 컴포넌트 목록에서 컴포넌트 드래그 작업 시작을 처리합니다.
         */
        void HandleComponentDragStart(int componentIndex);

        /**
         * @brief Handles the ongoing component drag operation.
         * 진행 중인 컴포넌트 드래그 작업을 처리합니다.
         */
        void HandleComponentDrag();

        /**
         * @brief Handles dropping a component onto the workspace.
         * 작업 공간에 컴포넌트를 드롭하는 작업을 처리합니다.
         */
        void HandleComponentDrop(Position position);

        /**
         * @brief Handles the selection of a placed component.
         * 배치된 컴포넌트의 선택을 처리합니다.
         */
        void HandleComponentSelection(int instanceId);

        /**
         * @brief Deletes the currently selected component.
         * 현재 선택된 컴포넌트를 삭제합니다.
         */
        void DeleteSelectedComponent();

        /**
         * @brief Handles the start of moving an already placed component.
         * 이미 배치된 컴포넌트의 이동 시작을 처리합니다.
         */
        void HandleComponentMoveStart(int instanceId, ImVec2 mousePos);

        /**
         * @brief Handles the end of a component move operation.
         * 컴포넌트 이동 작업의 종료를 처리합니다.
         */
        void HandleComponentMoveEnd();

        /**
         * @brief Renders a PLC component.
         * PLC 컴포넌트를 렌더링합니다.
         */
        void RenderPLCComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                ImVec2 screen_pos);
        /**
         * @brief Renders an FRL (Filter, Regulator, Lubricator) unit component.
         * FRL(필터, 레귤레이터, 윤활기) 유닛 컴포넌트를 렌더링합니다.
         */
        void RenderFRLComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                ImVec2 screen_pos);
        /**
         * @brief Renders a manifold component.
         * 매니폴드 컴포넌트를 렌더링합니다.
         */
        void RenderManifoldComponent(ImDrawList* draw_list,
                                     const PlacedComponent& comp, ImVec2 screen_pos);
        /**
         * @brief Renders a limit switch component.
         * 리미트 스위치 컴포넌트를 렌더링합니다.
         */
        void RenderLimitSwitchComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a generic sensor component.
         * 일반 센서 컴포넌트를 렌더링합니다.
         */
        void RenderSensorComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                                   ImVec2 screen_pos);
        /**
         * @brief Renders a cylinder component.
         * 실린더 컴포넌트를 렌더링합니다.
         */
        void RenderCylinderComponent(ImDrawList* draw_list,
                                     const PlacedComponent& comp, ImVec2 screen_pos);
        /**
         * @brief Renders a single-solenoid valve component.
         * 단동 솔레노이드 밸브 컴포넌트를 렌더링합니다.
         */
        void RenderValveSingleComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a double-solenoid valve component.
         * 복동 솔레노이드 밸브 컴포넌트를 렌더링합니다.
         */
        void RenderValveDoubleComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);
        /**
         * @brief Renders a button unit component.
         * 버튼 유닛 컴포넌트를 렌더링합니다.
         */
        void RenderButtonUnitComponent(ImDrawList* draw_list,
                                       const PlacedComponent& comp,
                                       ImVec2 screen_pos);
        /**
         * @brief Renders a power supply component.
         * 전원 공급 장치 컴포넌트를 렌더링합니다.
         */
        void RenderPowerSupplyComponent(ImDrawList* draw_list,
                                        const PlacedComponent& comp,
                                        ImVec2 screen_pos);

        /**
         * @brief Renders the main canvas for wiring and component placement.
         * 배선 및 컴포넌트 배치를 위한 메인 캔버스를 렌더링합니다.
         */
        void RenderWiringCanvas();

        // RenderWiringCanvas helper functions
        // RenderWiringCanvas 헬퍼 함수들

        /**
         * @brief Handles user interaction for adjusting FRL pressure.
         * FRL 압력 조정을 위한 사용자 상호작용을 처리합니다.
         */
        bool HandleFRLPressureAdjustment(ImVec2 mouse_world_pos, const ImGuiIO& io);

        /**
         * @brief Handles camera zooming and panning on the canvas.
         * 캔버스에서의 카메라 확대/축소 및 이동을 처리합니다.
         */
        void HandleZoomAndPan(bool is_canvas_hovered, const ImGuiIO& io,
                              bool frl_handled, bool& wheel_handled);

        /**
         * @brief Renders the background grid on the canvas.
         * 캔버스에 배경 그리드를 렌더링합니다.
         */
        void RenderGrid(ImDrawList* draw_list);

        /**
         * @brief Handles component dropping and wire deletion based on user input.
         * 사용자 입력에 따른 컴포넌트 드롭 및 전선 삭제를 처리합니다.
         */
        void HandleComponentDropAndWireDelete(bool is_canvas_hovered,
                                              ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in wire editing mode.
         * 전선 편집 모드에서의 사용자 상호작용을 처리합니다.
         */
        void HandleWireEditMode(ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in selection mode.
         * 선택 모드에서의 사용자 상호작용을 처리합니다.
         */
        void HandleSelectMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Handles user interactions in wire connection mode.
         * 전선 연결 모드에서의 사용자 상호작용을 처리합니다.
         */
        void HandleWireConnectionMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Renders the main content of the canvas (components, wires, etc.).
         * 캔버스의 주 콘텐츠(컴포넌트, 전선 등)를 렌더링합니다.
         */
        void RenderCanvasContent(ImDrawList* draw_list, ImVec2 mouse_world_pos);

        /**
         * @brief Shows a tooltip with information about the port under the cursor.
         * 커서 아래에 있는 포트에 대한 정보가 담긴 툴팁을 표시합니다.
         */
        void ShowPortTooltip(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        /**
         * @brief Displays an overlay with help text for camera controls.
         * 카메라 제어에 대한 도움말 텍스트가 있는 오버레이를 표시합니다.
         */
        void ShowCameraHelpOverlay();

        /**
         * @brief Starts the process of drawing a new wire from a specified port.
         * 지정된 포트에서 새 전선을 그리는 프로세스를 시작합니다.
         */
        void StartWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        /**
         * @brief Updates the endpoint of the wire being drawn to the mouse position.
         * 그려지고 있는 전선의 끝점을 마우스 위치로 업데이트합니다.
         */
        void UpdateWireDrawing(ImVec2 mousePos);

        /**
         * @brief Completes a wire connection to a target port.
         * 대상 포트에 대한 전선 연결을 완료합니다.
         */
        void CompleteWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        /**
         * @brief Renders all established wires on the canvas.
         * 캔버스에 설정된 모든 전선을 렌더링합니다.
         */
        void RenderWires(ImDrawList* draw_list);

        /**
         * @brief Deletes a wire by its ID.
         * ID로 전선을 삭제합니다.
         */
        void DeleteWire(int wireId);

        /**
         * @brief Handles a click on a waypoint of a wire.
         * 전선의 웨이포인트(경유점) 클릭을 처리합니다.
         */
        void HandleWayPointClick(ImVec2 worldPos);

        /**
         * @brief Adds a new waypoint to the wire currently being drawn.
         * 현재 그려지고 있는 전선에 새 웨이포인트를 추가합니다.
         */
        void AddWayPoint(ImVec2 position);

        /**
         * @brief Renders the temporary wire being drawn by the user.
         * 사용자가 그리고 있는 임시 전선을 렌더링합니다.
         */
        void RenderTemporaryWire(ImDrawList* draw_list);

        /**
         * @brief Handles the selection of a wire.
         * 전선 선택을 처리합니다.
         */
        void HandleWireSelection(ImVec2 worldPos);

        /**
         * @brief Finds a wire at a given world position.
         * 주어진 월드 위치에서 전선을 찾습니다.
         */
        Wire* FindWireAtPosition(ImVec2 worldPos, float tolerance = 5.0f);

        /**
         * @brief Finds a waypoint in a wire at a given world position.
         * 주어진 월드 위치에서 전선의 웨이포인트를 찾습니다.
         */
        int FindWayPointAtPosition(Wire& wire, ImVec2 worldPos, float radius = 8.0f);

        /**
         * @brief Checks if a connection between two ports is valid.
         * 두 포트 간의 연결이 유효한지 확인합니다.
         */
        bool IsValidWireConnection(const Port& fromPort, const Port& toPort);

        /**
         * @brief Finds a port at a given world position.
         * 주어진 월드 위치에서 포트를 찾습니다.
         */
        Port* FindPortAtPosition(ImVec2 worldPos, int& outComponentId);

        /**
         * @brief Applies all enabled snap settings to a position.
         * 활성화된 모든 스냅 설정을 위치에 적용합니다.
         */
        ImVec2 ApplySnap(ImVec2 position, bool isWirePoint = false);

        /**
         * @brief Snaps a position to the grid.
         * 위치를 그리드에 스냅합니다.
         */
        ImVec2 ApplyGridSnap(ImVec2 position);

        /**
         * @brief Snaps a position to the nearest component port.
         * 위치를 가장 가까운 컴포넌트 포트에 스냅합니다.
         */
        ImVec2 ApplyPortSnap(ImVec2 position);

        /**
         * @brief Snaps a position to align horizontally or vertically with a reference point.
         * 기준점과 수평 또는 수직으로 정렬되도록 위치를 스냅합니다.
         */
        ImVec2 ApplyLineSnap(ImVec2 position, ImVec2 referencePoint);

        /**
         * @brief Renders visual guides for snapping.
         * 스냅을 위한 시각적 가이드를 렌더링합니다.
         */
        void RenderSnapGuides(ImDrawList* draw_list, ImVec2 worldSnapPos);

        // Pointer to the main GLFW window.
        // 메인 GLFW 윈도우에 대한 포인터입니다.
        GLFWwindow* window_;

        // Current operating mode (e.g., Wiring, Programming).
        // 현재 작동 모드(예: 배선, 프로그래밍)입니다.
        Mode current_mode_;

        // Currently selected tool (e.g., Select, Wire).
        // 현재 선택된 도구(예: 선택, 전선)입니다.
        ToolType current_tool_;

        // Flag indicating if the main application loop is running.
        // 메인 애플리케이션 루프가 실행 중인지 나타내는 플래그입니다.
        bool running_;

        // Flag indicating if the PLC simulation is running.
        // PLC 시뮬레이션이 실행 중인지 나타내는 플래그입니다.
        bool is_plc_running_;

        // Default window width and height constants.
        // 기본 윈도우 너비 및 높이 상수입니다.
        static constexpr int kWindowWidth = 1440;
        static constexpr int kWindowHeight = 1024;

        // Collection of all components placed in the workspace.
        // 작업 공간에 배치된 모든 컴포넌트의 컬렉션입니다.
        std::vector<PlacedComponent> placed_components_;
        // ID of the currently selected component.
        // 현재 선택된 컴포넌트의 ID입니다.
        int selected_component_id_;
        // Counter to generate unique instance IDs for new components.
        // 새 컴포넌트를 위한 고유 인스턴스 ID를 생성하는 카운터입니다.
        int next_instance_id_;

        // State variables for component drag-and-drop and movement.
        // 컴포넌트 드래그 앤 드롭 및 이동을 위한 상태 변수들입니다.
        bool is_dragging_;
        int dragged_component_index_;
        bool is_moving_component_;
        int moving_component_id_;
        ImVec2 drag_start_offset_;

        // Collection of all wires in the workspace.
        // 작업 공간에 있는 모든 전선의 컬렉션입니다.
        std::vector<Wire> wires_;
        // Counter for generating unique wire IDs.
        // 고유한 전선 ID를 생성하기 위한 카운터입니다.
        int next_wire_id_;
        // ID of the currently selected wire.
        // 현재 선택된 전선의 ID입니다.
        int selected_wire_id_;

        // State variables for the interactive wire creation process.
        // 상호작용적인 전선 생성 과정을 위한 상태 변수들입니다.
        bool is_connecting_;
        int wire_start_component_id_;
        int wire_start_port_id_;
        ImVec2 wire_start_pos_;
        ImVec2 wire_current_pos_;
        std::vector<Position> current_way_points_;
        Port temp_port_buffer_;

        // State variables for editing an existing wire's waypoints.
        // 기존 전선의 웨이포인트를 편집하기 위한 상태 변수들입니다.
        WireEditMode wire_edit_mode_;
        int editing_wire_id_;
        int editing_point_index_;

        // State for the workspace camera's pan and zoom.
        // 작업 공간 카메라의 이동 및 확대/축소를 위한 상태입니다.
        ImVec2 camera_offset_;
        float camera_zoom_;
        ImVec2 canvas_top_left_;
        ImVec2 canvas_size_;

        // Configuration for snapping behavior.
        // 스냅 동작에 대한 구성입니다.
        SnapSettings snap_settings_;

        // Maps storing simulation state data for each component port.
        // 각 컴포넌트 포트에 대한 시뮬레이션 상태 데이터를 저장하는 맵입니다.
        std::map<std::pair<int, int>, Position> port_positions_;
        std::map<std::pair<int, int>, float> port_voltages_;
        std::map<std::pair<int, int>, float> port_pressures_;

        // Data for the loaded ladder logic and the live state of PLC devices.
        // 불러온 래더 로직과 PLC 디바이스의 실시간 상태에 대한 데이터입니다.
        LadderProgram loaded_ladder_program_;
        std::map<std::string, bool> plc_device_states_;
        std::map<std::string, TimerState> plc_timer_states_;
        std::map<std::string, CounterState> plc_counter_states_;

        // Smart pointers to the major subsystems of the application.
        // 애플리케이션의 주요 하위 시스템을 가리키는 스마트 포인터입니다.
        std::unique_ptr<ProgrammingMode> programming_mode_;
        std::unique_ptr<PLCSimulatorCore> simulator_core_;
        PhysicsEngine* physics_engine_;
        std::unique_ptr<CompiledPLCExecutor> compiled_plc_executor_;
        std::unique_ptr<ProjectFileManager> project_file_manager_;

        // State variables for the real-time debugging and logging system.
        // 실시간 디버깅 및 로깅 시스템을 위한 상태 변수들입니다.
        bool debug_mode_;
        bool enable_debug_logging_;
        bool debug_key_pressed_;
        int debug_update_counter_;
        std::string debug_log_buffer_;

        /**
         * @brief Toggles the comprehensive debug mode on or off.
         * 종합 디버그 모드를 켜거나 끕니다.
         */
        void ToggleDebugMode();

        /**
         * @brief Updates and logs debug information to the console if enabled.
         * 활성화된 경우 콘솔에 디버그 정보를 업데이트하고 기록합니다.
         */
        void UpdateDebugLogging();

        /**
         * @brief Logs the state of all placed components to the debug buffer.
         * 배치된 모든 컴포넌트의 상태를 디버그 버퍼에 기록합니다.
         */
        void LogComponentStates();

        /**
         * @brief Logs the overall state of the physics engine to the debug buffer.
         * 물리 엔진의 전반적인 상태를 디버그 버퍼에 기록합니다.
         */
        void LogPhysicsEngineStates();

        /**
         * @brief Logs the state of the electrical network to the debug buffer.
         * 전기 네트워크의 상태를 디버그 버퍼에 기록합니다.
         */
        void LogElectricalNetwork();

        /**
         * @brief Logs the state of the pneumatic network to the debug buffer.
         * 공압 네트워크의 상태를 디버그 버퍼에 기록합니다.
         */
        void LogPneumaticNetwork();

        /**
         * @brief Logs the state of the mechanical systems (e.g., cylinders) to the debug buffer.
         * 기계 시스템(예: 실린더)의 상태를 디버그 버퍼에 기록합니다.
         */
        void LogMechanicalSystem();

        /**
         * @brief Prints a message to the system console.
         * 시스템 콘솔에 메시지를 출력합니다.
         */
        void PrintDebugToConsole(const std::string& message);
    };

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_