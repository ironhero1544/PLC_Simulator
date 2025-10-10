#pragma once

#include "DataTypes.h"
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include "imgui.h"

/**
 * FORWARD DECLARATIONS: Prevent circular dependencies
 */
namespace plc {
    class Application;  // Application 전방 선언
    class CompiledPLCExecutor;
    class LadderToLDConverter;
}

namespace plc {

    enum class LadderInstructionType {
        EMPTY, XIC, XIO, OTE, HLINE,
        SET,
        RST,
        TON,
        CTU,
        RST_TMR_CTR
    };

    struct LadderInstruction {
        LadderInstructionType type = LadderInstructionType::EMPTY;
        std::string address;
        std::string preset;
        bool isActive = false;
    };

    struct TimerState {
        int value = 0;
        bool done = false;
        int preset = 0;
        bool enabled = false;
    };

    struct CounterState {
        int value = 0;
        bool done = false;
        int preset = 0;
        bool lastPower = false;
    };

    struct SimulatorState {
        std::map<std::string, bool> deviceStates;          // X, Y, M 디바이스 상태
        std::map<std::string, TimerState> timerStates;     // T 타이머 상태
        std::map<std::string, CounterState> counterStates; // C 카운터 상태
        uint64_t seqNo = 0;                                // 상태 시퀀스 번호
        std::chrono::steady_clock::time_point timestamp;   // 상태 타임스탬프
        
        SimulatorState() : timestamp(std::chrono::steady_clock::now()) {}
        
        /**
         * @brief Deep copy constructor
         * @param other Source state to copy from
         */
        SimulatorState(const SimulatorState& other) 
            : deviceStates(other.deviceStates)
            , timerStates(other.timerStates)
            , counterStates(other.counterStates)
            , seqNo(other.seqNo)
            , timestamp(std::chrono::steady_clock::now()) {}
        
        /**
         * @brief Assignment operator with timestamp update
         * @param other Source state to assign from
         * @return Reference to this object
         */
        SimulatorState& operator=(const SimulatorState& other) {
            if (this != &other) {
                deviceStates = other.deviceStates;
                timerStates = other.timerStates;
                counterStates = other.counterStates;
                seqNo = other.seqNo;
                timestamp = std::chrono::steady_clock::now();
            }
            return *this;
        }
        
        /**
         * @brief Update device state with automatic sequence tracking
         * @param address Device address to update
         * @param state New device state
         */
        void UpdateDeviceState(const std::string& address, bool state) {
            deviceStates[address] = state;
            seqNo++;
            timestamp = std::chrono::steady_clock::now();
        }
    };

    struct Rung {
        int number = 0;
        std::vector<LadderInstruction> cells;
        bool isEndRung = false;
        Rung() : cells(12) {}
    };

    struct VerticalConnection {
        int x = 0;
        std::vector<int> rungs;

        VerticalConnection() = default;
        VerticalConnection(int x_pos, int start_rung, int end_rung) : x(x_pos) {
            for (int i = start_rung; i <= end_rung; i++) {
                rungs.push_back(i);
            }
        }

        /**
         * @brief Get starting rung number
         * @return First rung number or 0 if empty
         */
        int startRung() const { return rungs.empty() ? 0 : rungs.front(); }
        
        /**
         * @brief Get ending rung number
         * @return Last rung number or 0 if empty
         */
        int endRung() const { return rungs.empty() ? 0 : rungs.back(); }
    };

    struct LadderProgram {
        std::vector<Rung> rungs;
        std::vector<VerticalConnection> verticalConnections;
        LadderProgram() {
            rungs.emplace_back();
            rungs.back().number = 0;
            rungs.emplace_back();
            rungs.back().isEndRung = true;
        }
    };

    class ProgrammingMode {
    public:
        /**
         * @brief Constructor with optional application reference
         * @param app Pointer to Application instance for project operations
         */
        ProgrammingMode(plc::Application* app = nullptr);
        
        /**
         * @brief Initialize programming mode components and state
         */
        void Initialize();
        
        /**
         * @brief Update programming mode logic and state
         */
        void Update();
        
        /**
         * @brief Update with PLC running state for conditional behavior
         * @param isPlcRunning Current PLC execution state
         */
        void UpdateWithPlcState(bool isPlcRunning);
        
        /**
         * @brief Handle keyboard input for ladder editing
         * @param key Key code of pressed key
         */
        void HandleKeyboardInput(int key);
        
        /**
         * @brief Render complete programming mode user interface
         * @param isPlcRunning Current PLC execution state for UI context
         */
        void RenderProgrammingModeUI(bool isPlcRunning);
        
        /**
         * @brief Set monitor mode for read-only ladder viewing
         * @param monitor True for monitor mode, false for edit mode
         */
        void SetMonitorMode(bool monitor) { m_isMonitorMode = monitor; }
        
        /**
         * @brief Save current ladder program to OpenPLC .ld format
         * @param filepath Target file path for .ld output
         */
        void SaveLadderProgramToLD(const std::string& filepath);
        
        /**
         * @brief Test compilation of .ld file using OpenPLC compiler
         * @param ldFilepath Path to .ld file to compile and test
         */
        void TestCompileLDFile(const std::string& ldFilepath);

        /**
         * @brief Get current ladder program (read-only access)
         * @return Const reference to current ladder program
         */
        const LadderProgram& GetLadderProgram() const;
        
        /**
         * @brief Set new ladder program with state synchronization
         * @param program New ladder program to load
         */
        void SetLadderProgram(const LadderProgram& program);
        
        /**
         * @brief Get current device states (read-only access)
         * @return Const reference to device state map
         */
        const std::map<std::string, bool>& GetDeviceStates() const;
        
        /**
         * @brief Get current timer states (read-only access)
         * @return Const reference to timer state map
         */
        const std::map<std::string, TimerState>& GetTimerStates() const;
        
        /**
         * @brief Get current counter states (read-only access)
         * @return Const reference to counter state map
         */
        const std::map<std::string, CounterState>& GetCounterStates() const;

        // === 신규: 시스템 입력(X) 단일 진입점 반영 ===
        void UpdateInputsFromSystem(const std::map<std::string, bool>& inputs);

        // === Phase 4: 엔진 상태 조회 API ===
        bool IsUsingCompiledEngine() const { return m_useCompiledEngine; }
        bool HasCompiledCodeLoaded() const { return !m_currentCompiledCode.empty(); }
        const char* GetEngineType() const { return m_useCompiledEngine ? "Compiled(OpenPLC)" : "Legacy"; }
        bool IsRecompileNeeded() const; // 내부 NeedsRecompilation() 래퍼
        const std::string& GetLastCompileError() const { return m_lastCompileError; }
        // 스캔 결과 요약
        bool GetLastScanSuccess() const { return m_lastScanSuccess; }
        int GetLastCycleTimeUs() const { return m_lastCycleTimeUs; }
        int GetLastInstructionCount() const { return m_lastInstructionCount; }
        const std::string& GetLastScanError() const { return m_lastScanError; }

        // === GXWorks2 기본 정규화 모드 ===
        void SetGX2NormalizationEnabled(bool enabled) { m_gx2NormalizationEnabled = enabled; }
        bool IsGX2NormalizationEnabled() const { return m_gx2NormalizationEnabled; }
        int GetNormalizationFixCount() const { return m_lastNormalizationFixCount; }
        const std::string& GetNormalizationSummary() const { return m_lastNormalizationSummary; }

    private:
        plc::Application* m_application;  // Application 포인터 (SaveProject/LoadProject 호출용)
        
        enum class PendingActionType {
            NONE,
            ADD_INSTRUCTION,
            DELETE_INSTRUCTION,
            ADD_NEW_RUNG,
            DELETE_RUNG
        };
        struct PendingAction {
            PendingActionType type = PendingActionType::NONE;
            LadderInstructionType instructionType = LadderInstructionType::EMPTY;
            int rungIndex = -1;
        };
        PendingAction m_pendingAction;
        void ExecutePendingAction();

        void RenderProgrammingHeader();
        void RenderProgrammingToolbar(bool isPlcRunning);
        void RenderProgrammingMainArea();
        void RenderLadderEditor();
        void RenderColumnHeader();
        void RenderLadderDiagram();
        void RenderVerticalConnections();
        void RenderVerticalConnectionsForRung(int rungIndex, float cellAreaWidth); // [NEW] 뱃별 세로선 렌더링
        void RenderRung(int rungIndex);
        void RenderEndRung(int rungIndex);
        void RenderLadderCell(int rungIndex, int cellIndex, float cellWidth);
        void RenderDeviceMonitor();
        void RenderKeyboardHelp();
        void RenderCursorInfo();
        void RenderSimulationControl();
        void RenderStatusBar(bool isPlcRunning);
        void RenderAddressPopup();
        void RenderVerticalDialog();

        void HandleEditAction(LadderInstructionType type);
        void ConfirmInstruction();
        void DeleteCurrentInstruction();
        void EditCurrentInstruction();
        void AddNewRung();
        void DeleteRung(int rungIndex);
        void InsertHorizontalLine();
        void UpdateHorizontalLines(int rungIndex);
        void AddVerticalConnection();
        void ConfirmVerticalConnection();

        void SimulateLadderProgram();
        
        // 🔥 **NEW**: OpenPLC 엔진 통합 관련 함수들
        void ExecuteWithOpenPLCEngine();           // OpenPLC 엔진으로 시뮬레이션
        void ExecuteWithLegacySimulation();        // 기존 수동 시뮬레이션 (백업용)
        bool CompileLadderToOpenPLC();             // 레더 → .ld → C++ → OpenPLC 로드
        void SyncPhysicsToOpenPLC();               // 물리 상태 → OpenPLC 입력
        void SyncOpenPLCToDevices();               // OpenPLC 출력 → 디바이스 상태
        void UpdateVisualActiveStates();           // 레더 셀 시각적 활성화 업데이트
        bool NeedsRecompilation() const;           // 재컴파일 필요 여부 ���인

        // 🔥 **NEW**: UI 구조 보존을 위한 유틸리티 함수들
        LadderProgram DeepCopyLadderProgram(const LadderProgram& source) const;  // 레더 프로그램 깊은 복사
        SimulatorState GetCurrentStateSnapshot() const;                         // 현재 시뮬레이터 상태 스냅샷
        void UpdateUIFromSimulatorState(const SimulatorState& state);          // 상태로부터 UI 업데이트 (구조 불변)
        
        // 🔥 **NEW**: 안전한 동기화 메커니즘
        bool IsSafeToUpdateUI() const;                  // UI 업데이트가 안전한지 확인
        void SetEditingState(bool isEditing);           // 편집 상태 설정
        void ProcessPendingStateUpdates();              // 대��� 중인 상태 업데이트 처리
        void SafeUpdateUI(const SimulatorState& state); // 안전한 UI 업데이트
        
        // === GXWorks2 기본 정규화 ===
        LadderProgram NormalizeLadderGX2(const LadderProgram& src); // 컴파일/실행 직전 내부 정규화

        bool GetDeviceState(const std::string& address) const;
        void SetDeviceState(const std::string& address, bool state);
        
        LadderInstructionType StringToInstructionType(const std::string& str);

        void GetUsedDevices(std::vector<std::string>& M, std::vector<std::string>& T, std::vector<std::string>& C) const;

        const char* GetInstructionSymbol(LadderInstructionType type) const;
        ImVec4 GetInstructionColor(bool isSelected, bool isActive) const;

        std::string InstructionTypeToString(LadderInstructionType type) const;

        LadderProgram m_ladderProgram;
        int m_selectedRung;
        int m_selectedCell;
        bool m_isMonitorMode;

        std::map<std::string, bool> m_deviceStates;
        std::map<std::string, TimerState> m_timerStates;
        std::map<std::string, CounterState> m_counterStates;

        // 🔥 **NEW**: OpenPLC 검증된 레더 엔진 통합
        std::unique_ptr<CompiledPLCExecutor> m_plcExecutor;
        std::unique_ptr<LadderToLDConverter> m_ldConverter;
        bool m_useCompiledEngine;           // OpenPLC 엔진 사용 여부
        std::string m_currentCompiledCode;  // 현재 컴파일된 C++ 코드
        
        // 🔥 **NEW**: 안전한 UI 동기화를 위한 상태 관리
        bool m_isEditingInProgress = false;         // 현재 편집 중인지 여부
        bool m_hasUIFocus = false;                  // UI가 포커스를 가지고 있는지 여부  
        SimulatorState m_pendingStateUpdate;        // 대기 중인 상태 업데이트
        bool m_hasPendingStateUpdate = false;       // 대기 ��인 상태 업데이트가 있는지 여부
        std::chrono::steady_clock::time_point m_lastSafeUpdate; // 마지막 안전한 업데이트 시간

        bool m_showAddressPopup;
        bool m_showVerticalDialog;
        LadderInstructionType m_pendingInstructionType;
        char m_tempAddressBuffer[64];
        int m_verticalLineCount;

        // === 신규: 재컴파일 트리거 상태 ===
        bool m_isDirty = false;                     // 레더 변경됨
        size_t m_lastCompiledHash = 0;              // 마지막 컴파일된 레더 해시
        size_t ComputeProgramHash(const LadderProgram& program) const; // 해시 계산
        void MarkDirty() { m_isDirty = true; }      // 더티 표시 헬퍼

        // === 신규: T/C 초기화 헬퍼 ===
        void InitializeTimersAndCountersFromProgram();

        // 사용 중인 코일 주소 수집 (OTE/SET/RST 대상)
        void GetUsedCoils(std::vector<std::string>& coils) const;
        // 사용 중인 입력(X) 주소 수집 (XIC/XIO 대상)
        void GetUsedInputs(std::vector<std::string>& inputs) const;

        // === Phase 4: 상태 가시성��� 내부 캐시 ===
        std::string m_lastCompileError;             // 마지막 컴파일 에러 메시지
        bool m_lastScanSuccess = false;             // 최근 스캔 성공 여부
        int m_lastCycleTimeUs = 0;                  // 최근 스캔 사이클 시간(us)
        int m_lastInstructionCount = 0;             // 최근 실행된 명령어 수
        std::string m_lastScanError;                // 최근 스캔 에러 메시지

        // === GX2 정규화 상태 ===
        bool m_gx2NormalizationEnabled = true;      // 기본 ON
        int m_lastNormalizationFixCount = 0;        // 최근 정규화에서 적용된 수정 수
        std::string m_lastNormalizationSummary;     // 최근 정규화 요약(경고/보정 내역)
    };
}
