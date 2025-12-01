// programming_mode.h
// Copyright 2024 PLC Emulator Project
//
// Ladder diagram editor and simulator.

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_

#include "imgui.h"
#include "plc_emulator/core/data_types.h"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * FORWARD DECLARATIONS: Prevent circular dependencies
 */
namespace plc {
class Application;  // Application 전방 선언
class CompiledPLCExecutor;
class LadderToLDConverter;
}  // namespace plc

namespace plc {

enum class LadderInstructionType {
  EMPTY,
  XIC,
  XIO,
  OTE,
  HLINE,
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
  std::map<std::string, bool> deviceStates;           // X, Y, M 디바이스 상태
  std::map<std::string, TimerState> timerStates;      // T 타이머 상태
  std::map<std::string, CounterState> counterStates;  // C 카운터 상태
  uint64_t seqNo = 0;                                 // 상태 시퀀스 번호
  std::chrono::steady_clock::time_point timestamp;    // 상태 타임스탬프

  SimulatorState() : timestamp(std::chrono::steady_clock::now()) {}

  /**
   * @brief Deep copy constructor
    * Deep copy c위구조체또는
   * @param other Source state to copy from
    * Source st에서e copy 부터
   */
  SimulatorState(const SimulatorState& other)
      : deviceStates(other.deviceStates),
        timerStates(other.timerStates),
        counterStates(other.counterStates),
        seqNo(other.seqNo),
        timestamp(std::chrono::steady_clock::now()) {}

  /**
   * @brief Assignment operator with timestamp update
    * Assignment oper에서또는 와 함께 timestmp 업데이트
   * @param other Source state to assign from
    * Source st에서e 로서sign 부터
   * @return Reference to this object
    * Reference 이 객체
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
    * 업데이트 device st에서e 와 함께 um에서ic sequence trck내g
   * @param address Device address to update
    * Device 추가ress 업데이트
   * @param state New device state
    * New device st에서e
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
    * 가져오기 strt내g rung 번호
   * @return First rung number or 0 if empty
    * First rung 번호 또는 0 만약 empty
   */
  int startRung() const { return rungs.empty() ? 0 : rungs.front(); }

  /**
   * @brief Get ending rung number
    * 가져오기 end내g rung 번호
   * @return Last rung number or 0 if empty
    * L로서t rung 번호 또는 0 만약 empty
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
    * C위구조체또는 와 함께 opti위l pplic에서i위 참조
   * @param app Pointer to Application instance for project operations
    * Po내ter Applic에서i위 인스턴스 위한 project oper에서i위s
   */
  ProgrammingMode(plc::Application* app = nullptr);

  /**
   * @brief Initialize programming mode components and state
    * 초기화 프로그램m내g mode 컴포넌트s 및 st에서e
   */
  void Initialize();

  /**
   * @brief Update programming mode logic and state
    * 업데이트 프로그램m내g mode logic 및 st에서e
   */
  void Update();

  /**
   * @brief Update with PLC running state for conditional behavior
    * 업데이트 와 함께 PLC runn내g st에서e 위한 c위diti위l 이다hvi또는
   * @param isPlcRunning Current PLC execution state
    * Current PLC executi위 st에서e
   */
  void UpdateWithPlcState(bool isPlcRunning);

  /**
   * @brief Handle keyboard input for ladder editing
    * 처리 keybord 입력 위한 래더 edit내g
   * @param key Key code of pressed key
    * Key code 의 pressed key
   */
  void HandleKeyboardInput(int key);

  /**
   * @brief Render complete programming mode user interface
    * 렌더링 complete 프로그램m내g mode user 내terfce
   * @param isPlcRunning Current PLC execution state for UI context
    * Current PLC executi위 st에서e 위한 UI c위text
   */
  void RenderProgrammingModeUI(bool isPlcRunning);

  /**
   * @brief Set monitor mode for read-only ladder viewing
    * 설정 m위it또는 mode 위한 red-위ly 래더 view내g
   * @param monitor True for monitor mode, false for edit mode
    * True 위한 m위it또는 mode, 거짓 위한 edit mode
   */
  void SetMonitorMode(bool monitor) { is_monitor_mode_ = monitor; }

  /**
   * @brief Save current ladder program to OpenPLC .ld format
    * 저장 current 래더 프로그램 OpenPLC .ld 위한m에서
   * @param filepath Target file path for .ld output
    * Tr가져오기 파일 경로 위한 .ld 출력
   */
  void SaveLadderProgramToLD(const std::string& filepath);

  /**
   * @brief Test compilation of .ld file using OpenPLC compiler
    * Test compil에서i위 의 .ld 파일 us내g OpenPLC 컴파일r
   * @param ldFilepath Path to .ld file to compile and test
    * P에서h .ld 파일 컴파일 및 test
   */
  void TestCompileLDFile(const std::string& ldFilepath);

  /**
   * @brief Get current ladder program (read-only access)
    * 가져오기 current 래더 프로그램 (red-위ly ccess)
   * @return Const reference to current ladder program
    * C위st 참조 current 래더 프로그램
   */
  const LadderProgram& GetLadderProgram() const;

  /**
   * @brief Set new ladder program with state synchronization
    * 설정 new 래더 프로그램 와 함께 st에서e synchr위iz에서i위
   * @param program New ladder program to load
    * New 래더 프로그램 로드
   */
  void SetLadderProgram(const LadderProgram& program);

  /**
   * @brief Get current device states (read-only access)
    * 가져오기 current device st에서es (red-위ly ccess)
   * @return Const reference to device state map
    * C위st 참조 device st에서e 맵
   */
  const std::map<std::string, bool>& GetDeviceStates() const;

  /**
   * @brief Get current timer states (read-only access)
    * 가져오기 current timer st에서es (red-위ly ccess)
   * @return Const reference to timer state map
    * C위st 참조 timer st에서e 맵
   */
  const std::map<std::string, TimerState>& GetTimerStates() const;

  /**
   * @brief Get current counter states (read-only access)
    * 가져오기 current 개수er st에서es (red-위ly ccess)
   * @return Const reference to counter state map
    * C위st 참조 개수er st에서e 맵
   */
  const std::map<std::string, CounterState>& GetCounterStates() const;

  void UpdateInputsFromSystem(const std::map<std::string, bool>& inputs);

  bool IsUsingCompiledEngine() const { return use_compiled_engine_; }
  bool HasCompiledCodeLoaded() const { return !current_compiled_code_.empty(); }
  const char* GetEngineType() const {
    return use_compiled_engine_ ? "Compiled(OpenPLC)" : "Legacy";
  }
  bool IsRecompileNeeded() const;  // 내부 NeedsRecompilation() 래퍼
  const std::string& GetLastCompileError() const { return last_compile_error_; }
  // 스캔 결과 요약
  bool GetLastScanSuccess() const { return last_scan_success_; }
  int GetLastCycleTimeUs() const { return last_cycle_time_us_; }
  int GetLastInstructionCount() const { return last_instruction_count_; }
  const std::string& GetLastScanError() const { return last_scan_error_; }

  void SetGX2NormalizationEnabled(bool enabled) {
    gx2_normalization_enabled_ = enabled;
  }
  bool IsGX2NormalizationEnabled() const { return gx2_normalization_enabled_; }
  int GetNormalizationFixCount() const { return last_normalization_fix_count_; }
  const std::string& GetNormalizationSummary() const {
    return last_normalization_summary_;
  }

 private:
  plc::Application*
      application_;  // Application 포인터 (SaveProject/LoadProject 호출용)

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
  PendingAction pending_action_;
  void ExecutePendingAction();

  void RenderProgrammingHeader();
  void RenderProgrammingToolbar(bool isPlcRunning);
  void RenderProgrammingMainArea();
  void RenderLadderEditor();
  void RenderColumnHeader();
  void RenderLadderDiagram();
  void RenderVerticalConnections();
  void RenderVerticalConnectionsForRung(
      int rungIndex, float cellAreaWidth);  // [NEW] 뱃별 세로선 렌더링
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
  void ExecuteWithOpenPLCEngine();     // OpenPLC 엔진으로 시뮬레이션
  void ExecuteWithLegacySimulation();  // 기존 수동 시뮬레이션 (백업용)
  bool CompileLadderToOpenPLC();       // 레더 → .ld → C++ → OpenPLC 로드
  void SyncPhysicsToOpenPLC();         // 물리 상태 → OpenPLC 입력
  void SyncOpenPLCToDevices();         // OpenPLC 출력 → 디바이스 상태
  void UpdateVisualActiveStates();     // 레더 셀 시각적 활성화 업데이트
  bool NeedsRecompilation() const;     // 재컴파일 필요 여부 ���인

  // 🔥 **NEW**: UI 구조 보존을 위한 유틸리티 함수들
  LadderProgram DeepCopyLadderProgram(
      const LadderProgram& source) const;  // 레더 프로그램 깊은 복사
  SimulatorState GetCurrentStateSnapshot()
      const;  // 현재 시뮬레이터 상태 스냅샷
  void UpdateUIFromSimulatorState(
      const SimulatorState& state);  // 상태로부터 UI 업데이트 (구조 불변)

  // 🔥 **NEW**: 안전한 동기화 메커니즘
  bool IsSafeToUpdateUI() const;         // UI 업데이트가 안전한지 확인
  void SetEditingState(bool isEditing);  // 편집 상태 설정
  void ProcessPendingStateUpdates();     // 대��� 중인 상태 업데이트 처리
  void SafeUpdateUI(const SimulatorState& state);  // 안전한 UI 업데이트

  LadderProgram NormalizeLadderGX2(
      const LadderProgram& src);  // 컴파일/실행 직전 내부 정규화

  bool GetDeviceState(const std::string& address) const;
  void SetDeviceState(const std::string& address, bool state);

  LadderInstructionType StringToInstructionType(const std::string& str);

  void GetUsedDevices(std::vector<std::string>& M, std::vector<std::string>& T,
                      std::vector<std::string>& C) const;

  const char* GetInstructionSymbol(LadderInstructionType type) const;
  ImVec4 GetInstructionColor(bool isSelected, bool isActive) const;

  std::string InstructionTypeToString(LadderInstructionType type) const;

  LadderProgram ladder_program_;
  int selected_rung_;
  int selected_cell_;
  bool is_monitor_mode_;

  std::map<std::string, bool> device_states_;
  std::map<std::string, TimerState> timer_states_;
  std::map<std::string, CounterState> counter_states_;

  // 🔥 **NEW**: OpenPLC 검증된 레더 엔진 통합
  std::unique_ptr<CompiledPLCExecutor> plc_executor_;
  std::unique_ptr<LadderToLDConverter> ld_converter_;
  bool use_compiled_engine_;           // OpenPLC 엔진 사용 여부
  std::string current_compiled_code_;  // 현재 컴파일된 C++ 코드

  // 🔥 **NEW**: 안전한 UI 동기화를 위한 상태 관리
  bool is_editing_in_progress_ = false;   // 현재 편집 중인지 여부
  bool has_ui_focus_ = false;            // UI가 포커스를 가지고 있는지 여부
  SimulatorState pending_state_update_;  // 대기 중인 상태 업데이트
  bool has_pending_state_update_ =
      false;  // 대기 ��인 상태 업데이트가 있는지 여부
  std::chrono::steady_clock::time_point
      last_safe_update_;  // 마지막 안전한 업데이트 시간

  bool show_address_popup_;
  bool show_vertical_dialog_;
  LadderInstructionType pending_instruction_type_;
  char temp_address_buffer_[64];
  int vertical_line_count_;

  bool is_dirty_ = false;         // 레더 변경됨
  size_t last_compiled_hash_ = 0;  // 마지막 컴파일된 레더 해시
  size_t ComputeProgramHash(const LadderProgram& program) const;  // 해시 계산
  void MarkDirty() { is_dirty_ = true; }  // 더티 표시 헬퍼

  void InitializeTimersAndCountersFromProgram();

  // 사용 중인 코일 주소 수집 (OTE/SET/RST 대상)
  void GetUsedCoils(std::vector<std::string>& coils) const;
  // 사용 중인 입력(X) 주소 수집 (XIC/XIO 대상)
  void GetUsedInputs(std::vector<std::string>& inputs) const;

  std::string last_compile_error_;  // 마지막 컴파일 에러 메시지
  bool last_scan_success_ = false;  // 최근 스캔 성공 여부
  int last_cycle_time_us_ = 0;       // 최근 스캔 사이클 시간(us)
  int last_instruction_count_ = 0;  // 최근 실행된 명령어 수
  std::string last_scan_error_;     // 최근 스캔 에러 메시지

  bool gx2_normalization_enabled_ = true;   // 기본 ON
  int last_normalization_fix_count_ = 0;     // 최근 정규화에서 적용된 수정 수
  std::string last_normalization_summary_;  // 최근 정규화 요약(경고/보정 내역)
};
}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_
