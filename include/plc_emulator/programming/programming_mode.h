/*
 * programming_mode.h
 *
 * 래더 편집기 및 시믬레이터 모드 선언.
 * Declarations for ladder editor and simulator mode.
 */



#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_

#include "imgui.h"
#include "plc_emulator/core/data_types.h"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

/*
 * 전방 선언으로 순환 참조를 방지합니다.
 * Forward declarations to avoid circular dependencies.
 */
namespace plc {
class Application;
class CompiledPLCExecutor;
class LadderToLDConverter;
}

namespace plc {

/*
 * 래더 셀 명령 타입.
 * Ladder cell instruction types.
 */
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
  RST_TMR_CTR,
  BKRST
};

/*
 * 래더 셀의 명령과 상태.
 * Instruction and state for a ladder cell.
 */
struct LadderInstruction {
  LadderInstructionType type = LadderInstructionType::EMPTY;
  std::string address;
  std::string preset;
  bool isActive = false;
};

/*
 * 타이머 상태 값.
 * Timer state values.
 */
struct TimerState {
  int value = 0;
  bool done = false;
  int preset = 0;
  bool enabled = false;
};

/*
 * 카운터 상태 값.
 * Counter state values.
 */
struct CounterState {
  int value = 0;
  bool done = false;
  int preset = 0;
  bool lastPower = false;
};

/*
 * 시믬레이터 실행 상태 스냅샷.
 * Snapshot of simulator execution state.
 */
struct SimulatorState {
  std::map<std::string, bool> deviceStates;
  std::map<std::string, TimerState> timerStates;
  std::map<std::string, CounterState> counterStates;
  uint64_t seqNo = 0;
  std::chrono::steady_clock::time_point timestamp;

  SimulatorState() : timestamp(std::chrono::steady_clock::now()) {}

  SimulatorState(const SimulatorState& other)
      : deviceStates(other.deviceStates),
        timerStates(other.timerStates),
        counterStates(other.counterStates),
        seqNo(other.seqNo),
        timestamp(std::chrono::steady_clock::now()) {}

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

  void UpdateDeviceState(const std::string& address, bool state) {
    deviceStates[address] = state;
    seqNo++;
    timestamp = std::chrono::steady_clock::now();
  }
};

/*
 * 래더 룽 데이터.
 * Ladder rung data.
 */
struct Rung {
  int number = 0;
  std::vector<LadderInstruction> cells;
  std::string memo;
  bool isEndRung = false;
  Rung() : cells(12) {}
};

/*
 * 룽 간 세로 연결 정보.
 * Vertical connection between rungs.
 */
struct VerticalConnection {
  int x = 0;
  std::vector<int> rungs;

  VerticalConnection() = default;
  VerticalConnection(int x_pos, int start_rung, int end_rung) : x(x_pos) {
    for (int i = start_rung; i <= end_rung; i++) {
      rungs.push_back(i);
    }
  }

  int startRung() const { return rungs.empty() ? 0 : rungs.front(); }

  int endRung() const { return rungs.empty() ? 0 : rungs.back(); }
};

/*
 * 래더 프로그램 구성.
 * Ladder program container.
 */
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

/*
 * 래더 편집 및 실행 UI/로직을 관리합니다.
 * Manages ladder editing and simulation UI/logic.
 */
class ProgrammingMode {
 public:
  ProgrammingMode(plc::Application* app = nullptr);

  void Initialize();

  void Update();

  void UpdateWithPlcState(bool isPlcRunning);

  void HandleKeyboardInput(int key);

  void RenderProgrammingModeUI(bool isPlcRunning);

  void SetMonitorMode(bool monitor) { is_monitor_mode_ = monitor; }

  void SaveLadderProgramToLD(const std::string& filepath);

  void TestCompileLDFile(const std::string& ldFilepath);

  const LadderProgram& GetLadderProgram() const;

  void SetLadderProgram(const LadderProgram& program);

  const std::map<std::string, bool>& GetDeviceStates() const;

  const std::map<std::string, TimerState>& GetTimerStates() const;

  const std::map<std::string, CounterState>& GetCounterStates() const;

  void UpdateInputsFromSystem(const std::map<std::string, bool>& inputs);

  bool IsUsingCompiledEngine() const { return use_compiled_engine_; }
  bool HasCompiledCodeLoaded() const { return !current_compiled_code_.empty(); }
  const char* GetEngineType() const {
    return use_compiled_engine_ ? "Compiled(OpenPLC)" : "Disabled";
  }
  bool IsRecompileNeeded() const;
  bool HasCompileAttempted() const { return has_compile_attempted_; }
  const std::string& GetLastCompileError() const { return last_compile_error_; }
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
      application_;

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
      int rungIndex, float cellAreaWidth);
  void RenderRung(int rungIndex);
  void RenderEndRung(int rungIndex);
  void RenderLadderCell(int rungIndex, int cellIndex, float cellWidth);
  void RenderDeviceMonitor();
  void RenderKeyboardHelp();
  void RenderCursorInfo();
  void RenderRungMemoEditor();
  void RenderSimulationControl();
  void RenderStatusBar(bool isPlcRunning);
  void RenderAddressPopup();
  void RenderRungMemoPopup();
  void RenderVerticalDialog();

  float GetLayoutScale() const;

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
  void SyncFromExternalPlc();

  void ExecuteWithOpenPLCEngine();
  bool CompileLadderToOpenPLC();
  void SyncPhysicsToOpenPLC();
  void SyncOpenPLCToDevices();
  void SyncOpenPLCToTimersCounters();
  void UpdateVisualActiveStates();
  bool NeedsRecompilation() const;

  LadderProgram DeepCopyLadderProgram(
      const LadderProgram& source) const;
  SimulatorState GetCurrentStateSnapshot()
      const;
  void UpdateUIFromSimulatorState(
      const SimulatorState& state);

  bool IsSafeToUpdateUI() const;
  void SetEditingState(bool isEditing);
  void ProcessPendingStateUpdates();
  void SafeUpdateUI(const SimulatorState& state);

  LadderProgram NormalizeLadderGX2(
      const LadderProgram& src);

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
  bool monitor_external_plc_ = false;

  std::map<std::string, bool> device_states_;
  std::map<std::string, TimerState> timer_states_;
  std::map<std::string, CounterState> counter_states_;

  std::unique_ptr<CompiledPLCExecutor> plc_executor_;
  std::unique_ptr<LadderToLDConverter> ld_converter_;
  bool use_compiled_engine_;
  std::string current_compiled_code_;
  bool has_compile_attempted_ = false;
  bool compile_failed_ = false;
  std::vector<int> compile_error_rungs_;
  std::map<int, size_t> compile_error_rung_hashes_;
  std::string last_ld_code_;
  size_t last_failed_hash_ = 0;

  bool is_editing_in_progress_ = false;
  bool has_ui_focus_ = false;
  SimulatorState pending_state_update_;
  bool has_pending_state_update_ =
      false;
  std::chrono::steady_clock::time_point
      last_safe_update_;
  std::chrono::steady_clock::time_point last_scan_time_;
  double scan_accumulator_ = 0.0;
  bool scan_time_initialized_ = false;

  bool show_address_popup_;
  bool show_rung_memo_popup_ = false;
  bool show_vertical_dialog_;
  LadderInstructionType pending_instruction_type_;
  char temp_address_buffer_[64];
  char rung_memo_buffer_[256] = {0};
  char rung_memo_popup_buffer_[256] = {0};
  int rung_memo_popup_target_rung_ = -1;
  int memo_edit_rung_ = -1;
  bool memo_edit_session_active_ = false;
  bool focus_rung_memo_next_frame_ = false;
  int vertical_line_count_;

  bool is_dirty_ = false;
  size_t last_compiled_hash_ = 0;
  size_t ComputeProgramHash(const LadderProgram& program) const;
  void UpdateCompileErrorRungsOnEdit();
  bool IsRungCompileError(int rungIndex) const;
  size_t ComputeRungHash(const Rung& rung) const;
  void UpdateCompileErrorRungsOnCompileFailure(
      const std::string& ldCode, const std::string& errorMessage);
  void MarkDirty();
  void PushProgrammingUndoState();
  void UndoProgrammingState();
  void RedoProgrammingState();

  void InitializeTimersAndCountersFromProgram();

  void GetUsedCoils(std::vector<std::string>& coils) const;
  void GetUsedInputs(std::vector<std::string>& inputs) const;

  std::string last_compile_error_;
  bool last_scan_success_ = false;
  int last_cycle_time_us_ = 0;
  int last_instruction_count_ = 0;
  std::string last_scan_error_;

  bool gx2_normalization_enabled_ = true;
  int last_normalization_fix_count_ = 0;
  std::string last_normalization_summary_;

  struct ProgrammingUndoState {
    LadderProgram program;
    int selected_rung = 0;
    int selected_cell = 0;
  };

  static constexpr size_t kProgrammingUndoLimit = 50;

  std::vector<ProgrammingUndoState> programming_undo_stack_;
  std::vector<ProgrammingUndoState> programming_redo_stack_;

  static constexpr double kDefaultScanStepSeconds = 0.010;
  static constexpr double kMaxScanCatchupSeconds = 0.25;
  static constexpr int kDefaultScanStepMs =
      static_cast<int>(kDefaultScanStepSeconds * 1000.0 + 0.5);
};
}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_ */
