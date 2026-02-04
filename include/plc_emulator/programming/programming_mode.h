// programming_mode.h
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
class Application;  // Application ?꾨갑 ?좎뼵
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
  std::map<std::string, bool> deviceStates;           // X, Y, M ?붾컮?댁뒪 ?곹깭
  std::map<std::string, TimerState> timerStates;      // T ??대㉧ ?곹깭
  std::map<std::string, CounterState> counterStates;  // C 移댁슫???곹깭
  uint64_t seqNo = 0;                                 // ?곹깭 ?쒗??踰덊샇
  std::chrono::steady_clock::time_point timestamp;    // ?곹깭 ??꾩뒪?ы봽

  SimulatorState() : timestamp(std::chrono::steady_clock::now()) {}

  /**
   * @brief Deep copy constructor
    * Deep copy c?꾧뎄議곗껜?먮뒗
   * @param other Source state to copy from
    * Source st?먯꽌e copy 遺??
   */
  SimulatorState(const SimulatorState& other)
      : deviceStates(other.deviceStates),
        timerStates(other.timerStates),
        counterStates(other.counterStates),
        seqNo(other.seqNo),
        timestamp(std::chrono::steady_clock::now()) {}

  /**
   * @brief Assignment operator with timestamp update
    * Assignment oper?먯꽌?먮뒗 ? ?④퍡 timestmp ?낅뜲?댄듃
   * @param other Source state to assign from
    * Source st?먯꽌e 濡쒖꽌sign 遺??
   * @return Reference to this object
    * Reference ??媛앹껜
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
    * ?낅뜲?댄듃 device st?먯꽌e ? ?④퍡 um?먯꽌ic sequence trck?큙
   * @param address Device address to update
    * Device 異붽?ress ?낅뜲?댄듃
   * @param state New device state
    * New device st?먯꽌e
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
    * 媛?몄삤湲?strt?큙 rung 踰덊샇
   * @return First rung number or 0 if empty
    * First rung 踰덊샇 ?먮뒗 0 留뚯빟 empty
   */
  int startRung() const { return rungs.empty() ? 0 : rungs.front(); }

  /**
   * @brief Get ending rung number
    * 媛?몄삤湲?end?큙 rung 踰덊샇
   * @return Last rung number or 0 if empty
    * L濡쒖꽌t rung 踰덊샇 ?먮뒗 0 留뚯빟 empty
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
    * C?꾧뎄議곗껜?먮뒗 ? ?④퍡 opti?꼕 pplic?먯꽌i??李몄“
   * @param app Pointer to Application instance for project operations
    * Po?큧er Applic?먯꽌i???몄뒪?댁뒪 ?꾪븳 project oper?먯꽌i?꼜
   */
  ProgrammingMode(plc::Application* app = nullptr);

  /**
   * @brief Initialize programming mode components and state
    * 珥덇린???꾨줈洹몃옩m?큙 mode 而댄룷?뚰듃s 諛?st?먯꽌e
   */
  void Initialize();

  /**
   * @brief Update programming mode logic and state
    * ?낅뜲?댄듃 ?꾨줈洹몃옩m?큙 mode logic 諛?st?먯꽌e
   */
  void Update();

  /**
   * @brief Update with PLC running state for conditional behavior
    * ?낅뜲?댄듃 ? ?④퍡 PLC runn?큙 st?먯꽌e ?꾪븳 c?꼋iti?꼕 ?대떎hvi?먮뒗
   * @param isPlcRunning Current PLC execution state
    * Current PLC executi??st?먯꽌e
   */
  void UpdateWithPlcState(bool isPlcRunning);

  /**
   * @brief Handle keyboard input for ladder editing
    * 泥섎━ keybord ?낅젰 ?꾪븳 ?섎뜑 edit?큙
   * @param key Key code of pressed key
    * Key code ??pressed key
   */
  void HandleKeyboardInput(int key);

  /**
   * @brief Render complete programming mode user interface
    * ?뚮뜑留?complete ?꾨줈洹몃옩m?큙 mode user ?큧erfce
   * @param isPlcRunning Current PLC execution state for UI context
    * Current PLC executi??st?먯꽌e ?꾪븳 UI c?꼝ext
   */
  void RenderProgrammingModeUI(bool isPlcRunning);

  /**
   * @brief Set monitor mode for read-only ladder viewing
    * ?ㅼ젙 m?꼒t?먮뒗 mode ?꾪븳 red-?꼕y ?섎뜑 view?큙
   * @param monitor True for monitor mode, false for edit mode
    * True ?꾪븳 m?꼒t?먮뒗 mode, 嫄곗쭞 ?꾪븳 edit mode
   */
  void SetMonitorMode(bool monitor) { is_monitor_mode_ = monitor; }

  /**
   * @brief Save current ladder program to OpenPLC .ld format
    * ???current ?섎뜑 ?꾨줈洹몃옩 OpenPLC .ld ?꾪븳m?먯꽌
   * @param filepath Target file path for .ld output
    * Tr媛?몄삤湲??뚯씪 寃쎈줈 ?꾪븳 .ld 異쒕젰
   */
  void SaveLadderProgramToLD(const std::string& filepath);

  /**
   * @brief Test compilation of .ld file using OpenPLC compiler
    * Test compil?먯꽌i????.ld ?뚯씪 us?큙 OpenPLC 而댄뙆?펢
   * @param ldFilepath Path to .ld file to compile and test
    * P?먯꽌h .ld ?뚯씪 而댄뙆??諛?test
   */
  void TestCompileLDFile(const std::string& ldFilepath);

  /**
   * @brief Get current ladder program (read-only access)
    * 媛?몄삤湲?current ?섎뜑 ?꾨줈洹몃옩 (red-?꼕y ccess)
   * @return Const reference to current ladder program
    * C?꼜t 李몄“ current ?섎뜑 ?꾨줈洹몃옩
   */
  const LadderProgram& GetLadderProgram() const;

  /**
   * @brief Set new ladder program with state synchronization
    * ?ㅼ젙 new ?섎뜑 ?꾨줈洹몃옩 ? ?④퍡 st?먯꽌e synchr?꼒z?먯꽌i??
   * @param program New ladder program to load
    * New ?섎뜑 ?꾨줈洹몃옩 濡쒕뱶
   */
  void SetLadderProgram(const LadderProgram& program);

  /**
   * @brief Get current device states (read-only access)
    * 媛?몄삤湲?current device st?먯꽌es (red-?꼕y ccess)
   * @return Const reference to device state map
    * C?꼜t 李몄“ device st?먯꽌e 留?
   */
  const std::map<std::string, bool>& GetDeviceStates() const;

  /**
   * @brief Get current timer states (read-only access)
    * 媛?몄삤湲?current timer st?먯꽌es (red-?꼕y ccess)
   * @return Const reference to timer state map
    * C?꼜t 李몄“ timer st?먯꽌e 留?
   */
  const std::map<std::string, TimerState>& GetTimerStates() const;

  /**
   * @brief Get current counter states (read-only access)
    * 媛?몄삤湲?current 媛쒖닔er st?먯꽌es (red-?꼕y ccess)
   * @return Const reference to counter state map
    * C?꼜t 李몄“ 媛쒖닔er st?먯꽌e 留?
   */
  const std::map<std::string, CounterState>& GetCounterStates() const;

  void UpdateInputsFromSystem(const std::map<std::string, bool>& inputs);

  bool IsUsingCompiledEngine() const { return use_compiled_engine_; }
  bool HasCompiledCodeLoaded() const { return !current_compiled_code_.empty(); }
  const char* GetEngineType() const {
    return use_compiled_engine_ ? "Compiled(OpenPLC)" : "Disabled";
  }
  bool IsRecompileNeeded() const;  // ?대? NeedsRecompilation() ?섑띁
  bool HasCompileAttempted() const { return has_compile_attempted_; }
  const std::string& GetLastCompileError() const { return last_compile_error_; }
  // ?ㅼ틪 寃곌낵 ?붿빟
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
      application_;  // Application ?ъ씤??(SaveProject/LoadProject ?몄텧??

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
      int rungIndex, float cellAreaWidth);  // [NEW] 諭껊퀎 ?몃줈???뚮뜑留?
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

  // ?뵦 **NEW**: OpenPLC ?붿쭊 ?듯빀 愿???⑥닔??
  void ExecuteWithOpenPLCEngine();     // OpenPLC ?붿쭊?쇰줈 ?쒕??덉씠??
  bool CompileLadderToOpenPLC();       // ?덈뜑 ??.ld ??C++ ??OpenPLC 濡쒕뱶
  void SyncPhysicsToOpenPLC();         // 臾쇰━ ?곹깭 ??OpenPLC ?낅젰
  void SyncOpenPLCToDevices();         // OpenPLC 異쒕젰 ???붾컮?댁뒪 ?곹깭
  void SyncOpenPLCToTimersCounters();
  void UpdateVisualActiveStates();     // ?덈뜑 ? ?쒓컖???쒖꽦???낅뜲?댄듃
  bool NeedsRecompilation() const;     // ?ъ뺨?뚯씪 ?꾩슂 ?щ? 占쏙옙占쎌씤

  // ?뵦 **NEW**: UI 援ъ“ 蹂댁〈???꾪븳 ?좏떥由ы떚 ?⑥닔??
  LadderProgram DeepCopyLadderProgram(
      const LadderProgram& source) const;  // ?덈뜑 ?꾨줈洹몃옩 源딆? 蹂듭궗
  SimulatorState GetCurrentStateSnapshot()
      const;  // ?꾩옱 ?쒕??덉씠???곹깭 ?ㅻ깄??
  void UpdateUIFromSimulatorState(
      const SimulatorState& state);  // ?곹깭濡쒕???UI ?낅뜲?댄듃 (援ъ“ 遺덈?)

  // ?뵦 **NEW**: ?덉쟾???숆린??硫붿빱?덉쬁
  bool IsSafeToUpdateUI() const;         // UI ?낅뜲?댄듃媛 ?덉쟾?쒖? ?뺤씤
  void SetEditingState(bool isEditing);  // ?몄쭛 ?곹깭 ?ㅼ젙
  void ProcessPendingStateUpdates();     // ?占쏙옙占?以묒씤 ?곹깭 ?낅뜲?댄듃 泥섎━
  void SafeUpdateUI(const SimulatorState& state);  // ?덉쟾??UI ?낅뜲?댄듃

  LadderProgram NormalizeLadderGX2(
      const LadderProgram& src);  // 而댄뙆???ㅽ뻾 吏곸쟾 ?대? ?뺢퇋??

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

  // ?뵦 **NEW**: OpenPLC 寃利앸맂 ?덈뜑 ?붿쭊 ?듯빀
  std::unique_ptr<CompiledPLCExecutor> plc_executor_;
  std::unique_ptr<LadderToLDConverter> ld_converter_;
  bool use_compiled_engine_;           // OpenPLC ?붿쭊 ?ъ슜 ?щ?
  std::string current_compiled_code_;  // ?꾩옱 而댄뙆?쇰맂 C++ 肄붾뱶
  bool has_compile_attempted_ = false;
  bool compile_failed_ = false;
  std::vector<int> compile_error_rungs_;
  std::map<int, size_t> compile_error_rung_hashes_;
  std::string last_ld_code_;
  size_t last_failed_hash_ = 0;

  // ?뵦 **NEW**: ?덉쟾??UI ?숆린?붾? ?꾪븳 ?곹깭 愿由?
  bool is_editing_in_progress_ = false;   // ?꾩옱 ?몄쭛 以묒씤吏 ?щ?
  bool has_ui_focus_ = false;            // UI媛 ?ъ빱?ㅻ? 媛吏怨??덈뒗吏 ?щ?
  SimulatorState pending_state_update_;  // ?湲?以묒씤 ?곹깭 ?낅뜲?댄듃
  bool has_pending_state_update_ =
      false;  // ?湲?占쏙옙???곹깭 ?낅뜲?댄듃媛 ?덈뒗吏 ?щ?
  std::chrono::steady_clock::time_point
      last_safe_update_;  // 留덉?留??덉쟾???낅뜲?댄듃 ?쒓컙
  std::chrono::steady_clock::time_point last_scan_time_;
  double scan_accumulator_ = 0.0;
  bool scan_time_initialized_ = false;

  bool show_address_popup_;
  bool show_vertical_dialog_;
  LadderInstructionType pending_instruction_type_;
  char temp_address_buffer_[64];
  int vertical_line_count_;

  bool is_dirty_ = false;         // ?덈뜑 蹂寃쎈맖
  size_t last_compiled_hash_ = 0;  // 留덉?留?而댄뙆?쇰맂 ?덈뜑 ?댁떆
  size_t ComputeProgramHash(const LadderProgram& program) const;  // ?댁떆 怨꾩궛
  void UpdateCompileErrorRungsOnEdit();
  bool IsRungCompileError(int rungIndex) const;
  size_t ComputeRungHash(const Rung& rung) const;
  void UpdateCompileErrorRungsOnCompileFailure(
      const std::string& ldCode, const std::string& errorMessage);
  void MarkDirty();  // 더티 표시 헬퍼
  void PushProgrammingUndoState();
  void UndoProgrammingState();
  void RedoProgrammingState();

  void InitializeTimersAndCountersFromProgram();

  // ?ъ슜 以묒씤 肄붿씪 二쇱냼 ?섏쭛 (OTE/SET/RST ???
  void GetUsedCoils(std::vector<std::string>& coils) const;
  // ?ъ슜 以묒씤 ?낅젰(X) 二쇱냼 ?섏쭛 (XIC/XIO ???
  void GetUsedInputs(std::vector<std::string>& inputs) const;

  std::string last_compile_error_;  // 留덉?留?而댄뙆???먮윭 硫붿떆吏
  bool last_scan_success_ = false;  // 理쒓렐 ?ㅼ틪 ?깃났 ?щ?
  int last_cycle_time_us_ = 0;       // 理쒓렐 ?ㅼ틪 ?ъ씠???쒓컙(us)
  int last_instruction_count_ = 0;  // 理쒓렐 ?ㅽ뻾??紐낅졊????
  std::string last_scan_error_;     // 理쒓렐 ?ㅼ틪 ?먮윭 硫붿떆吏

  bool gx2_normalization_enabled_ = true;   // 湲곕낯 ON
  int last_normalization_fix_count_ = 0;     // 理쒓렐 ?뺢퇋?붿뿉???곸슜???섏젙 ??
  std::string last_normalization_summary_;  // 理쒓렐 ?뺢퇋???붿빟(寃쎄퀬/蹂댁젙 ?댁뿭)

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
}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_PROGRAMMING_MODE_H_
