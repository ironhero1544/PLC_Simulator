/*
 * application.h
 *
 * PLC 에뮬레이터의 메인 애플리케이션 클래스 선언.
 * Declaration of the main application class for the PLC emulator.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_

#include "imgui.h"
#include "plc_emulator/core/data_types.h"
#include "plc_emulator/core/plc_simulator_core.h"
#include "plc_emulator/core/ui_settings.h"
#include "plc_emulator/physics/physics_engine.h"
#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/project_file_manager.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;

namespace plc {

    class ProgrammingMode;

    /*
     * UI, 시뮬레이션, 파일 I/O 흐름을 오케스트레이션합니다.
     * Orchestrates UI, simulation, and file I/O flows.
     */
    class Application {
    public:
        explicit Application(bool enable_debug_mode);

        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        /*
         * 애플리케이션 수명주기.
         * Application lifecycle.
         */
        bool Initialize();

        void Run();

        void Shutdown();

        /*
         * 입력 처리와 디버그 보조 API.
         * Input handling and debug helpers.
         */
        void UpdateTouchGesture(float zoom_delta, ImVec2 pan_delta,
                                bool active);
        void SetPanInputActive(bool active);
        void SetTouchAnchor(ImVec2 screen_pos);
        void RegisterWin32RightClick();
        void RegisterWin32SideClick();
        void RegisterWin32SideDown(bool is_down);
        bool IsPointInCanvas(ImVec2 screen_pos) const;
        bool IsDebugEnabled() const;
        void DebugLog(const std::string& message);
        void QueuePhysicsWarningDialog(const std::string& message);
        float GetLayoutScale() const;
        const CompiledPLCExecutor* GetCompiledPlcExecutor() const {
          return compiled_plc_executor_.get();
        }
        /*
         * 프로젝트 저장과 로드.
         * Project save/load.
         */
        bool SaveProject(const std::string& filePath,
                         const std::string& projectName = "");

        bool LoadProject(const std::string& filePath);

    private:
        /*
         * 배선 편집용 Undo 스냅샷.
         * Undo snapshot for wiring edits.
         */
        struct WiringUndoState {
          std::vector<PlacedComponent> components;
          std::vector<Wire> wires;
          int selected_component_id = -1;
          int selected_wire_id = -1;
          int next_instance_id = 0;
          int next_wire_id = 0;
          int next_z_order = 0;
        };

        static constexpr size_t kWiringUndoLimit = 50;

        enum class ComponentListViewMode {
          ICON,
          NAME
        };

        enum class ComponentListFilter {
          ALL,
          AUTOMATION,
          SEMICONDUCTOR
        };

        enum class PhysicsLodTier {
          kHigh,
          kMedium,
          kLow
        };

        enum class WarmupStage {
          kPending,
          kRunning,
          kFinishing,
          kComplete
        };

        struct PhysicsLodState {
          PhysicsLodTier tier = PhysicsLodTier::kHigh;
          bool has_view = false;
          float zoom = 1.0f;
          float view_min_x = 0.0f;
          float view_max_x = 0.0f;
          float view_min_y = 0.0f;
          float view_max_y = 0.0f;
          float view_radius = 0.0f;
        };

        /*
         * PLC 스캔 주기 기본값.
         * Default PLC scan step.
         */
        static constexpr double kPlcScanStepSeconds = 0.010;
        static constexpr int kPlcScanStepMs =
            static_cast<int>(kPlcScanStepSeconds * 1000.0 + 0.5);

        /*
         * 서브시스템 초기화 헬퍼.
         * Subsystem initialization helpers.
         */
        bool InitializeWindow();

        bool InitializeImGui();

        void SetupCustomStyle();

        void ProcessInput();

        /*
         * 메인 업데이트 및 시뮬레이션 단계.
         * Main update and simulation steps.
         */
        void Update();
        void UpdatePhysicsLod();
        void RequestCacheWarmup();
        bool ProcessCacheWarmup();
        void RunCacheWarmup();
        void PrewarmAdvancedPhysicsNetworks();
        void RenderSplashScreen();

        void UpdatePhysics();
        void UpdatePhysicsImpl();

        void UpdateBasicPhysics(float delta_time);
        void UpdateBasicPhysicsImpl(float delta_time);
        bool UpdateSensorsBox2d();

        /*
         * 래더 로직 실행과 PLC 상태 동기화.
         * Ladder execution and PLC state sync.
         */
        void SimulateLoadedLadder();
        bool RestartApplication();

        bool GetPlcDeviceState(const std::string& address);

        void SetPlcDeviceState(const std::string& address, bool state);

        void LoadLadderProgramFromLD(const std::string& filepath);

        LadderInstructionType StringToInstructionType(const std::string& str);

        void SyncLadderProgramFromProgrammingMode();

        void CreateDefaultTestLadderProgram();

        void CompileAndLoadLadderProgram();

        /*
         * 네트워크 시뮬레이션과 컴포넌트 로직.
         * Network simulation and component logic.
         */
        void SimulateElectrical();
        void SimulateElectricalImpl();

        void UpdateComponentLogic();
        void UpdateComponentLogicImpl();

        void UpdateWorkpieceInteractions(float delta_time);
        void UpdateWorkpieceInteractionsImpl(float delta_time);
        bool UpdateWorkpieceInteractionsBox2d(float delta_time,
                                              bool warmup_only);

        void SimulatePneumatic();
        void SimulatePneumaticImpl();

        void UpdateActuators(float delta_time, bool skip_cylinder_update);
        void UpdateActuatorsImpl(float delta_time, bool skip_cylinder_update);

        std::vector<std::pair<int, bool>> GetPortsForComponent(
                const PlacedComponent& comp);

        void SyncPLCOutputsToPhysicsEngine();

        void SyncPhysicsEngineToApplication();

        void UpdatePhysicsPerformanceStats();

        /*
         * 렌더링 및 UI 구성.
         * Rendering and UI composition.
         */
        void Render();

        void Cleanup();

        void UpdatePortPositions();
        float GetResolutionScale() const;

        void RenderUI();

        void RenderMainMenuBar(float* out_height);

        void RenderWiringModeUI();

        void RenderHeader();

        void RenderToolbar();

        void RenderMainArea();

        void RenderPLCDebugPanel();
        void RenderPhysicsWarningDialog();
        void RenderShortcutHelpDialog();
        ImVec2 WorldToScreen(const ImVec2& world_pos) const;

        ImVec2 ScreenToWorld(const ImVec2& screen_pos) const;

        void RenderComponentList();

        void RenderPlacedComponents(ImDrawList* draw_list);

        void HandleComponentDragStart(int componentIndex);

        void HandleComponentDrag();

        void HandleComponentDrop(Position position);

        void HandleComponentSelection(int instanceId);

        void DeleteSelectedComponent();

        void HandleComponentMoveStart(int instanceId, ImVec2 mousePos);

        void HandleComponentMoveEnd();

        /*
         * 배선 캔버스 상호작용.
         * Wiring canvas interactions.
         */
        void RenderWiringCanvas();
        void RenderTagPopup();
        void OpenTagPopupForWire(int wire_id);
        void RenderComponentContextMenu();
        void OpenComponentContextMenu(int component_id, ImVec2 screen_pos);
        void RotateSelectedComponent(int delta_quadrants);
        WiringUndoState CaptureWiringState() const;
        void ApplyWiringState(const WiringUndoState& state);
        void PushWiringUndoState();
        void UndoWiringState();
        void RedoWiringState();


        /*
         * 배선 캔버스 보조 동작.
         * Wiring canvas helper routines.
         */
        bool HandleFRLPressureAdjustment(ImVec2 mouse_world_pos, const ImGuiIO& io);

        void HandleZoomAndPan(bool is_canvas_hovered, const ImGuiIO& io,
                              bool frl_handled, bool& wheel_handled);

        void RenderGrid(ImDrawList* draw_list);

        void HandleComponentDropAndWireDelete(bool is_canvas_hovered,
                                              ImVec2 mouse_world_pos,
                                              const ImGuiIO& io);

        void HandleWireEditMode(ImVec2 mouse_world_pos);

        void HandleSelectMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        void HandleWireConnectionMode(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        void RenderCanvasContent(ImDrawList* draw_list, ImVec2 mouse_world_pos);

        void ShowPortTooltip(bool is_canvas_hovered, ImVec2 mouse_world_pos);

        void ShowCameraHelpOverlay();

        void StartWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        void UpdateWireDrawing(ImVec2 mousePos);

        void CompleteWireConnection(int componentId, int portId, ImVec2 portWorldPos);

        void RenderWires(ImDrawList* draw_list);

        void DeleteWire(int wireId);

        void HandleWayPointClick(ImVec2 worldPos, bool use_port_snap_only);

        void AddWayPoint(ImVec2 position);

        void RenderTemporaryWire(ImDrawList* draw_list);

        void HandleWireSelection(ImVec2 worldPos);

        Wire* FindWireAtPosition(ImVec2 worldPos, float tolerance = 5.0f);
        Wire* FindTaggedWireAtScreenPos(ImVec2 screen_pos);

        int FindWayPointAtPosition(Wire& wire, ImVec2 worldPos, float radius = 8.0f);

        bool IsValidWireConnection(const Port& fromPort, const Port& toPort);

        Port* FindPortAtPosition(ImVec2 worldPos, int& outComponentId);

        /*
         * 스냅과 탐색 유틸리티.
         * Snapping and query helpers.
         */
        ImVec2 ApplySnap(ImVec2 position, bool isWirePoint = false);

        ImVec2 ApplyGridSnap(ImVec2 position);

        ImVec2 ApplyPortSnap(ImVec2 position);

        ImVec2 ApplyLineSnap(ImVec2 position, ImVec2 referencePoint,
                             bool force_orthogonal);
        ImVec2 ApplyAngleSnap(ImVec2 position, ImVec2 referencePoint);

        void RenderSnapGuides(ImDrawList* draw_list, ImVec2 worldSnapPos);
        PlacedComponent* FindComponentById(int instance_id);
        void ApplyRotationToComponent(PlacedComponent* comp, int delta_quadrants);
        void BringComponentToFront(int component_id);
        void SendComponentToBack(int component_id);
        void BringComponentForward(int component_id);
        void SendComponentBackward(int component_id);

        /*
         * 런타임 상태.
         * Runtime state.
         */
        GLFWwindow* window_;

        Mode current_mode_;

        ToolType current_tool_;

        bool running_;

        bool is_plc_running_;

        static constexpr int kWindowWidth = 1440;
        static constexpr int kWindowHeight = 1024;

        std::vector<PlacedComponent> placed_components_;

        int selected_component_id_;

        int next_instance_id_;
        int next_z_order_;
        std::vector<WiringUndoState> wiring_undo_stack_;
        std::vector<WiringUndoState> wiring_redo_stack_;

        bool is_dragging_;
        int dragged_component_index_;
        bool is_moving_component_;
        int moving_component_id_;
        ImVec2 drag_start_offset_;
        ComponentListViewMode component_list_view_mode_;
        ComponentListFilter component_list_filter_;

        std::vector<Wire> wires_;
        int next_wire_id_;
        int selected_wire_id_;
        bool show_tag_popup_;
        int tag_edit_wire_id_;
        int tag_color_index_;
        char tag_text_buffer_[64];

        bool is_connecting_;
        int wire_start_component_id_;
        int wire_start_port_id_;
        ImVec2 wire_start_pos_;
        ImVec2 wire_current_pos_;
        std::vector<Position> current_way_points_;
        Port temp_port_buffer_;

        WireEditMode wire_edit_mode_;
        int editing_wire_id_;
        int editing_point_index_;

        ImVec2 camera_offset_;
        float camera_zoom_;
        ImVec2 canvas_top_left_;
        ImVec2 canvas_size_;
        PhysicsLodState physics_lod_state_;

        SnapSettings snap_settings_;

        std::map<std::pair<int, int>, Position> port_positions_;
        std::map<std::pair<int, int>, float> port_voltages_;
        std::map<std::pair<int, int>, float> port_pressures_;
        std::map<std::pair<int, int>, bool> port_supply_reachable_;
        std::map<std::pair<int, int>, float> port_exhaust_quality_;
        std::map<std::pair<int, int>, bool> port_pneumatic_connected_;

        LadderProgram loaded_ladder_program_;
        std::map<std::string, bool> plc_device_states_;
        std::map<std::string, TimerState> plc_timer_states_;
        std::map<std::string, CounterState> plc_counter_states_;

        std::unique_ptr<ProgrammingMode> programming_mode_;
        std::unique_ptr<PLCSimulatorCore> simulator_core_;
        PhysicsEngine* physics_engine_;
        std::unique_ptr<CompiledPLCExecutor> compiled_plc_executor_;
        std::unique_ptr<ProjectFileManager> project_file_manager_;
        std::chrono::steady_clock::time_point last_physics_time_;
        std::chrono::steady_clock::time_point last_render_time_;
        std::chrono::steady_clock::time_point next_frame_time_;
        double physics_accumulator_;
        double plc_accumulator_;
        bool physics_time_initialized_;
        bool render_time_initialized_;
        bool high_precision_timer_active_;
        double monitor_refresh_rate_;
        WarmupStage warmup_stage_;
        bool advanced_physics_disabled_;
        size_t advanced_disable_components_;
        size_t advanced_disable_wires_;
        double advanced_accumulator_;
        bool advanced_network_built_;
        uint64_t advanced_topology_hash_;
        uint64_t advanced_disable_topology_hash_;
        size_t advanced_last_component_count_;
        size_t advanced_last_wire_count_;
        uint64_t advanced_last_input_hash_;
        bool advanced_inputs_dirty_;
        double physics_stage_total_ms_;
        double physics_stage_electrical_ms_;
        double physics_stage_component_ms_;
        double physics_stage_pneumatic_ms_;
        double physics_stage_actuator_ms_;
        double physics_stage_plc_ms_;
        double physics_stage_advanced_build_ms_;
        double physics_stage_advanced_solve_ms_;
        double physics_stage_advanced_sync_ms_;
        double physics_stage_advanced_electrical_ms_;
        double physics_stage_advanced_pneumatic_ms_;
        double physics_stage_advanced_mechanical_ms_;
        int physics_stage_steps_;
        int physics_stage_advanced_steps_;

        bool debug_mode_requested_;
        bool debug_mode_;
        bool enable_debug_logging_;
        int debug_update_counter_;
        std::string debug_log_buffer_;
        bool show_physics_warning_dialog_;
        std::string physics_warning_message_;
        std::mutex physics_warning_mutex_;
        ImVec2 last_pointer_world_pos_;
        double last_pointer_move_time_;
        double last_auto_waypoint_time_;
        bool touch_gesture_active_;
        float touch_zoom_delta_;
        ImVec2 touch_pan_delta_;
        bool last_pointer_is_pan_input_;
        ImVec2 touch_anchor_screen_pos_;
        bool prev_right_button_down_;
        bool prev_side_button_down_;
        bool win32_right_click_;
        bool win32_side_click_;
        bool win32_side_down_;
        bool show_restart_popup_;
        bool show_shortcut_help_popup_;
        bool show_component_context_menu_;
        int context_menu_component_id_;
        ImVec2 context_menu_pos_;
        UiSettings ui_settings_;

        /*
         * 디버그/로그 출력.
         * Debug/log output.
         */
        void ToggleDebugMode();

        void UpdateDebugLogging();

        void LogComponentStates();

        void LogPhysicsEngineStates();

        void LogElectricalNetwork();

        void LogPneumaticNetwork();

        void LogMechanicalSystem();

        void RenderUiSettingsMenu();

        void PrintDebugToConsole(const std::string& message);
        bool SaveLayout(const std::string& file_path);
        bool LoadLayout(const std::string& file_path);
    };

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_ */

