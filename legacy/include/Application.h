// include/Application.h

#pragma once

#include "DataTypes.h"
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <memory>
#include "imgui.h"
#include "ProgrammingMode.h"
#include "PLCSimulatorCore.h"
#include "PhysicsEngine.h"
#include "CompiledPLCExecutor.h"
#include "ProjectFileManager.h"

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
    bool SaveProject(const std::string& filePath, const std::string& projectName = "");
    
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
    std::vector<std::pair<int, bool>> GetPortsForComponent(const PlacedComponent& comp);
    
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
    void RenderPLCDebugPanel(); // [PPT: 새로운 내용 5 - 실시간 PLC 디버그 패널]

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

    void RenderPLCComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderFRLComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderManifoldComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderLimitSwitchComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderSensorComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderCylinderComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderValveSingleComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderValveDoubleComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderButtonUnitComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);
    void RenderPowerSupplyComponent(ImDrawList* draw_list, const PlacedComponent& comp, ImVec2 screen_pos);

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

    GLFWwindow* m_window;
    Mode m_currentMode;
    ToolType m_currentTool;
    bool m_running;
    bool m_isPlcRunning;

    static constexpr int WINDOW_WIDTH = 1440;
    static constexpr int WINDOW_HEIGHT = 1024;

    std::vector<PlacedComponent> m_placedComponents;
    int m_selectedComponentId;
    int m_nextInstanceId;

    bool m_isDragging;
    int m_draggedComponentIndex;
    bool m_isMovingComponent;
    int m_movingComponentId;
    ImVec2 m_dragStartOffset;

    std::vector<Wire> m_wires;
    int m_nextWireId;
    int m_selectedWireId;

    bool m_isConnecting;
    int m_wireStartComponentId;
    int m_wireStartPortId;
    ImVec2 m_wireStartPos;
    ImVec2 m_wireCurrentPos;
    std::vector<Position> m_currentWayPoints;
    Port m_tempPortBuffer;

    WireEditMode m_wireEditMode;
    int m_editingWireId;
    int m_editingPointIndex;

    ImVec2 m_cameraOffset;
    float m_cameraZoom;
    ImVec2 m_canvasTopLeft;
    ImVec2 m_canvasSize;

    SnapSettings m_snapSettings;

    std::map<std::pair<int, int>, Position> m_portPositions;
    std::map<std::pair<int, int>, float> m_portVoltages;
    std::map<std::pair<int, int>, float> m_portPressures;

    LadderProgram m_loadedLadderProgram;
    std::map<std::string, bool> m_plcDeviceStates;
    std::map<std::string, TimerState> m_plcTimerStates;
    std::map<std::string, CounterState> m_plcCounterStates;

    std::unique_ptr<ProgrammingMode> m_programmingMode;
    
    // === Phase 1: 새로 추가된 PLCSimulatorCore ===
    std::unique_ptr<PLCSimulatorCore> m_simulatorCore;
    
    // === Phase 3: 새로운 물리엔진 ===
    PhysicsEngine* m_physicsEngine;
    
    // === Phase 4: 컴파일된 PLC 실행 엔진 (OpenPLC 기반) ===
    std::unique_ptr<CompiledPLCExecutor> m_compiledPLCExecutor;
    
    // === Phase 5: 프로젝트 파일 관리자 (.plcproj) ===
    std::unique_ptr<ProjectFileManager> m_projectFileManager;
    
    // === ultrathink: 디버그 시스템 ===
    // [추론] 물리엔진 및 컴포넌트 상태 실시간 디버깅을 위한 종합적 시스템
    // - Ctrl+F12로 토글 가능한 디버그 모드
    // - CMD/콘솔 창에 상세한 상태 정보 출력
    // - 실린더, 리밋스위치, 전기/공압 네트워크 상태 추적
    bool m_debugMode;                           // 디버그 모드 활성화 여부
    bool m_enableDebugLogging;                  // 디버그 로깅 활성화 여부
    bool m_debugKeyPressed;                     // 키 반복 입력 방지
    int m_debugUpdateCounter;                   // 디버그 출력 주기 제어
    std::string m_debugLogBuffer;               // 디버그 로그 버퍼
    
    // 디버그 로깅 함수들
    void ToggleDebugMode();                     // Ctrl+F12 처리 및 디버그 모드 토글
    void UpdateDebugLogging();                  // 매 프레임 디버그 정보 업데이트
    void LogComponentStates();                  // 컴포넌트 상태 로깅
    void LogPhysicsEngineStates();              // 물리엔진 상태 로깅
    void LogElectricalNetwork();                // 전기 네트워크 상태 로깅
    void LogPneumaticNetwork();                 // 공압 네트워크 상태 로깅
    void LogMechanicalSystem();                 // 기계 시스템 상태 로깅
    void PrintDebugToConsole(const std::string& message); // 콘솔 출력
};

} // namespace plc