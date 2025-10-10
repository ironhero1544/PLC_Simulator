// include/Application.h

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

class Application {
 public:
  /**
   * @brief Constructor - Initialize application with default settings
   */
  Application();

  /**
   * @brief Destructor - Clean up resources and shutdown gracefully
   */
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  /**
   * @brief Initialize application systems and create main window
   * @return true if initialization successful, false on failure
   */
  bool Initialize();

  /**
   * @brief Run main application loop until exit requested
   */
  void Run();

  /**
   * @brief Shutdown application and clean up all resources
   */
  void Shutdown();

  /**
   * @brief Save current project to .plcproj file format
   * @param filePath Target file path for project save
   * @param projectName Optional project name (defaults to filename)
   * @return true if save successful, false on failure
   */
  bool SaveProject(const std::string& filePath,
                   const std::string& projectName = "");

  /**
   * @brief Load project from .plcproj file format
   * @param filePath Source file path to load project from
   * @return true if load successful, false on failure
   */
  bool LoadProject(const std::string& filePath);

 private:
  bool InitializeWindow();
  bool InitializeImGui();
  void SetupCustomStyle();
  void ProcessInput();

  void Update();
  void UpdatePhysics();
  /**
   * @brief Basic physics simulation independent of PLC state
   */
  void UpdateBasicPhysics();
  void SimulateLoadedLadder();
  bool GetPlcDeviceState(const std::string& address);
  void SetPlcDeviceState(const std::string& address, bool state);

  /**
   * @brief Load ladder program from .ld file format
   * @param filepath Path to .ld file to load
   */
  void LoadLadderProgramFromLD(const std::string& filepath);

  LadderInstructionType StringToInstructionType(const std::string& str);

  /**
   * @brief Synchronize ladder program from ProgrammingMode to Application
   */
  void SyncLadderProgramFromProgrammingMode();

  /**
   * @brief Create default test ladder program for initialization
   */
  void CreateDefaultTestLadderProgram();

  /**
   * @brief Compile and load ladder program into PLC executor
   */
  void CompileAndLoadLadderProgram();

  void SimulateElectrical();
  void UpdateComponentLogic();
  void SimulatePneumatic();
  void UpdateActuators();
  std::vector<std::pair<int, bool>> GetPortsForComponent(
      const PlacedComponent& comp);

  /**
   * @brief Synchronize PLC outputs to physics engine electrical network
   */
  void SyncPLCOutputsToPhysicsEngine();

  /**
   * @brief Synchronize physics engine results back to application state
   */
  void SyncPhysicsEngineToApplication();

  /**
   * @brief Update physics engine performance statistics and monitoring
   */
  void UpdatePhysicsPerformanceStats();

  void Render();
  void Cleanup();
  void UpdatePortPositions();

  void RenderUI();
  void RenderWiringModeUI();
  void RenderHeader();
  void RenderToolbar();
  void RenderMainArea();
  void RenderPLCDebugPanel();  // [PPT: 새로운 내용 5 - 실시간 PLC 디버그 패널]

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

  void RenderPLCComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                          ImVec2 screen_pos);
  void RenderFRLComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                          ImVec2 screen_pos);
  void RenderManifoldComponent(ImDrawList* draw_list,
                               const PlacedComponent& comp, ImVec2 screen_pos);
  void RenderLimitSwitchComponent(ImDrawList* draw_list,
                                  const PlacedComponent& comp,
                                  ImVec2 screen_pos);
  void RenderSensorComponent(ImDrawList* draw_list, const PlacedComponent& comp,
                             ImVec2 screen_pos);
  void RenderCylinderComponent(ImDrawList* draw_list,
                               const PlacedComponent& comp, ImVec2 screen_pos);
  void RenderValveSingleComponent(ImDrawList* draw_list,
                                  const PlacedComponent& comp,
                                  ImVec2 screen_pos);
  void RenderValveDoubleComponent(ImDrawList* draw_list,
                                  const PlacedComponent& comp,
                                  ImVec2 screen_pos);
  void RenderButtonUnitComponent(ImDrawList* draw_list,
                                 const PlacedComponent& comp,
                                 ImVec2 screen_pos);
  void RenderPowerSupplyComponent(ImDrawList* draw_list,
                                  const PlacedComponent& comp,
                                  ImVec2 screen_pos);

  void RenderWiringCanvas();
  void StartWireConnection(int componentId, int portId, ImVec2 portWorldPos);
  void UpdateWireDrawing(ImVec2 mousePos);
  void CompleteWireConnection(int componentId, int portId, ImVec2 portWorldPos);
  void RenderWires(ImDrawList* draw_list);
  void DeleteWire(int wireId);
  void HandleWayPointClick(ImVec2 worldPos);
  void AddWayPoint(ImVec2 position);
  void RenderTemporaryWire(ImDrawList* draw_list);
  void HandleWireSelection(ImVec2 worldPos);
  Wire* FindWireAtPosition(ImVec2 worldPos, float tolerance = 5.0f);
  int FindWayPointAtPosition(Wire& wire, ImVec2 worldPos, float radius = 8.0f);
  bool IsValidWireConnection(const Port& fromPort, const Port& toPort);
  Port* FindPortAtPosition(ImVec2 worldPos, int& outComponentId);

  ImVec2 ApplySnap(ImVec2 position, bool isWirePoint = false);
  ImVec2 ApplyGridSnap(ImVec2 position);
  ImVec2 ApplyPortSnap(ImVec2 position);
  ImVec2 ApplyLineSnap(ImVec2 position, ImVec2 referencePoint);
  void RenderSnapGuides(ImDrawList* draw_list, ImVec2 worldSnapPos);

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

  bool is_dragging_;
  int dragged_component_index_;
  bool is_moving_component_;
  int moving_component_id_;
  ImVec2 drag_start_offset_;

  std::vector<Wire> wires_;
  int next_wire_id_;
  int selected_wire_id_;

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

  SnapSettings snap_settings_;

  std::map<std::pair<int, int>, Position> port_positions_;
  std::map<std::pair<int, int>, float> port_voltages_;
  std::map<std::pair<int, int>, float> port_pressures_;

  LadderProgram loaded_ladder_program_;
  std::map<std::string, bool> plc_device_states_;
  std::map<std::string, TimerState> plc_timer_states_;
  std::map<std::string, CounterState> plc_counter_states_;

  std::unique_ptr<ProgrammingMode> programming_mode_;

  // === Phase 1: 새로 추가된 PLCSimulatorCore ===
  std::unique_ptr<PLCSimulatorCore> simulator_core_;

  // === Phase 3: 새로운 물리엔진 ===
  PhysicsEngine* physics_engine_;

  // === Phase 4: 컴파일된 PLC 실행 엔진 (OpenPLC 기반) ===
  std::unique_ptr<CompiledPLCExecutor> compiled_plc_executor_;

  // === Phase 5: 프로젝트 파일 관리자 (.plcproj) ===
  std::unique_ptr<ProjectFileManager> project_file_manager_;

  // === ultrathink: 디버그 시스템 ===
  // [추론] 물리엔진 및 컴포넌트 상태 실시간 디버깅을 위한 종합적 시스템
  // - Ctrl+F12로 토글 가능한 디버그 모드
  // - CMD/콘솔 창에 상세한 상태 정보 출력
  // - 실린더, 리밋스위치, 전기/공압 네트워크 상태 추적
  bool debug_mode_;              // 디버그 모드 활성화 여부
  bool enable_debug_logging_;     // 디버그 로깅 활성화 여부
  bool debug_key_pressed_;        // 키 반복 입력 방지
  int debug_update_counter_;      // 디버그 출력 주기 제어
  std::string debug_log_buffer_;  // 디버그 로그 버퍼

  // 디버그 로깅 함수들
  void ToggleDebugMode();         // Ctrl+F12 처리 및 디버그 모드 토글
  void UpdateDebugLogging();      // 매 프레임 디버그 정보 업데이트
  void LogComponentStates();      // 컴포넌트 상태 로깅
  void LogPhysicsEngineStates();  // 물리엔진 상태 로깅
  void LogElectricalNetwork();    // 전기 네트워크 상태 로깅
  void LogPneumaticNetwork();     // 공압 네트워크 상태 로깅
  void LogMechanicalSystem();     // 기계 시스템 상태 로깅
  void PrintDebugToConsole(const std::string& message);  // 콘솔 출력
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_APPLICATION_H_
