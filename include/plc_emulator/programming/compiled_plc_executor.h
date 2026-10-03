/*
 * compiled_plc_executor.h
 *
 * 컴파일된 PLC 프로그램 실행기 선언.
 * Declarations for the compiled PLC program executor.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_

#include "plc_emulator/project/openplc_compiler_integration.h"

#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace plc {
/*
 * 컴파일된 PLC 코드를 실행합니다.
 * Executes compiled PLC code.
 */
class CompiledPLCExecutor {
 public:
  /*
   * PLC 메모리와 실행 상태를 보관합니다.
   * Stores PLC memory and execution state.
   */
  struct PLCMemory {
    bool X[256] = {false};
    bool Y[256] = {false};
    bool M[7680] = {false};

    bool S[4096] = {false};
    bool special_m[512] = {false};
    int16_t D[8512] = {0};
    int16_t R[32768] = {0};
    int16_t V[8] = {0};
    int16_t Z[8] = {0};

    int T[512] = {0};
    int C[256] = {0};

    bool accumulator = false;
    bool accumulator_stack[16] = {false};
    int stack_pointer = 0;

    std::chrono::steady_clock::time_point last_scan_time;
    int scan_cycle_ms = 0;
  };

  /*
   * 스캔 사이클 실행 결과 요약입니다.
   * Summary of a scan cycle execution.
   */
  struct ExecutionResult {
    bool success = false;
    std::string errorMessage;
    int cycleTime_us = 0;
    int instructionCount = 0;
  };

  CompiledPLCExecutor();
  ~CompiledPLCExecutor();

  bool LoadProgram(const plc_emulator::programming::ExecutionProgram& program);

  bool LoadFromCompilationResult(
      const OpenPLCCompilerIntegration::CompilationResult& result);

  ExecutionResult ExecuteScanCycle();

  void SetContinuousExecution(bool enable, int cycleTime_ms = 10);

  void SetInput(int address, bool state);
  bool SetPhysicalInput(int address, bool state);
  bool GetPhysicalOutput(int address) const;
  bool InjectCounterPulses(int counter, int32_t pulses);
  bool SetMatrixInput(int output, int row, bool state);
  // Queues a virtual input/timer/counter interrupt for its Ixxx routine.
  bool RequestInterrupt(int pointer);

  // Virtual extension blocks expose BFM and analog channels to integrations.
  bool SetModuleBufferWord(int unit, int address, int16_t value);
  std::optional<int16_t> GetModuleBufferWord(int unit, int address) const;
  bool SetAnalogInput(int unit, int channel, int16_t value);
  std::optional<int16_t> GetAnalogOutput(int unit, int channel) const;
  bool SetVolume(int channel, uint8_t value);
  bool SetDeviceComment(const std::string& address, std::string_view comment);

  bool GetOutput(int address) const;

  void SetMemory(int address, bool state);

  bool GetMemory(int address) const;

  bool GetDeviceState(const std::string& address) const;
  std::optional<plc_emulator::programming::DeviceAddress> ResolveDeviceAddress(
      const std::string& address) const;
  int32_t GetWordValue(plc_emulator::programming::DeviceAddress address,
                       bool wide = false) const;

  void SetDeviceState(const std::string& address, bool state);

  struct AxisSnapshot {
    int32_t position = 0;
    uint64_t generated_pulses = 0;
    double frequency = 0;
    bool busy = false;
    bool complete = false;
    bool abnormal = false;
  };
  std::optional<AxisSnapshot> GetAxisState(int axis) const;
  bool ConfigurePositioningTable(
      plc_emulator::programming::DeviceAddress head,
      const std::array<plc_emulator::programming::DeviceAddress, 4>&
          directions);
  bool SetPositioningEntry(int axis, int entry,
                           plc_emulator::programming::Opcode opcode,
                           int32_t pulses, int32_t frequency);
  bool SetAbsoluteEncoder(int first_input, int32_t position);
  bool InjectSerialReceived(int channel, std::span<const uint8_t> bytes);
  size_t ReadSerialTransmitted(int channel, std::span<uint8_t> output);
  bool ConfigureInverter(int channel, int station, bool connected = true,
                         int response_ms = 10);
  bool SetInverterParameter(int channel, int station, int parameter,
                            int16_t value);
  std::optional<int16_t> GetInverterParameter(int channel, int station,
                                              int parameter) const;
  bool SetInverterMonitor(int channel, int station, int code, uint16_t value);
  enum class ModbusSpace {
    kCoil,
    kDiscreteInput,
    kHoldingRegister,
    kInputRegister
  };
  bool ConfigureModbusSlave(int channel, int station, bool connected = true,
                            int response_ms = 10);
  bool SetModbusValue(int channel, int station, ModbusSpace space, int address,
                      uint16_t value);
  std::optional<uint16_t> GetModbusValue(int channel, int station,
                                         ModbusSpace space, int address) const;
  bool ConfigureCfCard(int channel, uint32_t capacity_bytes = 16777216,
                       uint32_t record_capacity = 65536,
                       uint32_t buffer_capacity_bytes = 32768);
  bool SetCfCardMounted(int channel, bool mounted);
  bool PowerCycleCfCard(int channel);
  std::optional<std::string> ReadCfCsv(int channel,
                                       std::string_view name) const;

  void ResetMemory();

  const ExecutionResult& GetLastExecutionResult() const { return last_result_; }

  const PLCMemory& GetMemory() const { return memory_; }

  int GetTimerValue(int index) const;
  bool GetTimerEnabled(int index) const;
  int GetCounterValue(int index) const;
  bool GetCounterLastPower(int index) const;

  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  bool IsRunning() const { return is_running_; }

  std::optional<bool> GetCellPower(int rung, int cell) const;

 private:
  PLCMemory memory_;
  ExecutionResult last_result_;
  plc_emulator::programming::ExecutionProgram program_;
  plc_emulator::programming::ExecutionProgram resolved_program_;
  std::array<size_t, 4096> pointer_targets_{};
  size_t main_program_end_ = 0;
  bool ResolveOperand(plc_emulator::programming::Operand* operand,
                      bool wide) const;
  std::vector<uint8_t> gate_values_;
  std::vector<std::vector<uint8_t>> edge_states_;
  std::vector<bool> pulse_states_;
  std::vector<int64_t> operation_elapsed_ms_;
  std::vector<uint8_t> instruction_power_;
  std::vector<std::vector<uint8_t>> observed_gates_;
  bool has_scan_observations_ = false;
  std::chrono::sys_seconds rtc_time_{};
  std::chrono::steady_clock::time_point rtc_anchor_;
  int evaluated_group_ = -1;
  size_t evaluated_gate_count_ = 0;
  bool first_scan_ = true;
  std::vector<size_t> flow_targets_;
  std::vector<uint32_t> instruction_step_numbers_;
  void BuildSourceSteps();
  std::array<size_t, 900> interrupt_targets_{};
  std::array<bool, 900> pending_interrupts_{};
  std::array<int, 900> interrupt_elapsed_ms_{};
  bool interrupts_enabled_ = false;
  std::array<bool, 8> master_controls_{};
  int master_level_ = -1;
  struct FlowFrame {
    size_t return_position = 0;
    size_t loop_depth = 0;
    bool interrupt = false;
    std::array<bool, 8> master_controls{};
    int master_level = -1;
    int evaluation_group = -1;
    size_t gate_count = 0;
    std::vector<uint8_t> gates;
  };
  std::array<FlowFrame, 6> flow_frames_;
  struct SortState {
    bool active = false;
    int rows = 0;
    int columns = 0;
    int key = 0;
    int progress = 0;
    plc_emulator::programming::DeviceAddress destination;
    std::array<int32_t, 192> data{};
    bool descending = false;
    std::array<int, 32> order{};
  };
  SortState sort_state_;
  std::array<SortState, 2> sort2_states_{};
  std::array<size_t, 2> sort2_indices_{};
  struct VirtualModule {
    std::vector<int16_t> buffer;
    std::array<int16_t, 8> analog_inputs{};
    std::array<int16_t, 8> analog_outputs{};
  };
  std::array<VirtualModule, 8> modules_;
  std::array<uint8_t, 8> volumes_{};
  std::array<int16_t, 32768> extension_file_{};
  struct DeviceComment {
    plc_emulator::programming::DeviceAddress device;
    std::array<char, 16> text;
  };
  std::vector<DeviceComment> device_comments_;
  std::array<bool, 256> physical_inputs_{};
  std::array<bool, 256> physical_outputs_{};
  std::array<int, 256> physical_input_age_{};
  std::array<uint64_t, 256> input_pulse_totals_{};
  std::array<uint8_t, 256> matrix_inputs_{};
  std::array<bool, 256> matrix_configured_{};
  std::vector<uint64_t> speed_pulse_starts_;
  void HandleInputTransition(int address, bool previous, bool state);
  void RefreshInputs(int begin, int count, int filter_ms);
  void ProcessHighSpeedComparisons(int counter);
  bool ExecuteHighSpeedOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecuteStorageOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  struct ModuleTransfer {
    bool active = false;
    bool complete = false;
    int unit = 0;
    int address = 0;
    int total = 0;
    int chunk = 0;
    int progress = 0;
    plc_emulator::programming::DeviceAddress device;
  };
  std::vector<ModuleTransfer> module_transfers_;
  struct HandyState {
    bool previous = false;
    bool active = false;
    int64_t elapsed_ms = 0;
    int64_t off_elapsed_ms = 0;
  };
  std::vector<HandyState> handy_states_;
  struct PanelState {
    bool active = false;
    int scan = 0;
    int digit = 3;
    uint16_t previous_keys = 0;
    uint16_t keys = 0;
    int64_t elapsed_ms = 0;
    std::array<int, 2> values{};
  };
  std::vector<PanelState> panel_states_;
  struct AxisState {
    AxisSnapshot status;
    size_t owner = SIZE_MAX;
    plc_emulator::programming::Opcode opcode{};
    plc_emulator::programming::DeviceAddress direction;
    plc_emulator::programming::DeviceAddress dog;
    plc_emulator::programming::DeviceAddress zero;
    int sign = 1;
    int home_sign = -1;
    int home_phase = 0;
    int interrupt_input = -1;
    bool input_level = false;
    bool finite = false;
    bool stopping = false;
    int64_t remaining = 0;
    int64_t interrupt_distance = 0;
    double requested_frequency = 0;
    double creep_frequency = 0;
    double ramp_rate = 0;
    double deceleration_rate = 0;
    double fraction = 0;
    int pwm_high = 0;
    int pwm_period = 0;
    int pwm_phase = 0;
    int clear_output = -1;
    int clear_remaining_ms = 0;
  };
  std::array<AxisState, 4> axes_{};
  struct PositionEntry {
    plc_emulator::programming::Opcode opcode{};
    int32_t pulses = 0;
    int32_t frequency = 0;
  };
  bool positioning_table_enabled_ = false;
  plc_emulator::programming::DeviceAddress positioning_table_head_;
  std::array<plc_emulator::programming::DeviceAddress, 4> table_directions_{};
  std::array<std::array<PositionEntry, 100>, 4> positioning_entries_{};
  struct AbsoluteTransfer {
    bool active = false;
    bool complete = false;
    bool waiting_data = false;
    uint8_t pairs = 0;
    uint8_t checksum = 0;
    uint8_t received_checksum = 0;
    uint32_t data = 0;
  };
  std::vector<AbsoluteTransfer> absolute_transfers_;
  struct PidState {
    bool active = false;
    bool tuning = false;
    bool output_high = false;
    int transitions = 0;
    int64_t elapsed_ms = 0;
    int64_t tuning_ms = 0;
    int64_t cycle_start_ms = 0;
    int64_t switch_ms = 0;
    int64_t wait_ms = 0;
    double filtered = 0;
    double previous_filtered = 0;
    double previous_error = 0;
    double derivative = 0;
    double previous_raw = 0;
    double initial_pv = 0;
    double initial_mv = 0;
    double maximum_slope = 0;
    double dead_time = 0;
    double minimum_pv = 0;
    double maximum_pv = 0;
    int previous_output = 0;
    double output_fraction = 0;
  };
  std::vector<PidState> pid_states_;
  struct SerialPort {
    size_t owner = SIZE_MAX;
    bool rs2 = false;
    bool sending = false;
    bool receiving = false;
    bool checksum = false;
    bool receive_checksum = false;
    int channel = 1;
    int baud = 9600;
    int bits_per_character = 10;
    int receive_limit = 0;
    int send_data_count = 0;
    int send_total = 0;
    int send_position = 0;
    int received = 0;
    int idle_ms = 0;
    int header_size = 0;
    int terminator_size = 0;
    int matched_header = 0;
    int matched_terminator = 0;
    int checksum_digits = 0;
    int send_checksum = 0;
    int received_checksum = 0;
    int send_monitor = 0;
    int receive_monitor = 0;
    int request_relay = 0;
    int receive_relay = 0;
    int timeout_relay = 0;
    double byte_credit = 0;
    plc_emulator::programming::DeviceAddress send_device;
    plc_emulator::programming::DeviceAddress receive_device;
    std::array<uint8_t, 4> header{};
    std::array<uint8_t, 4> terminator{};
    std::array<uint8_t, 4106> send_data{};
    std::array<uint8_t, 4096> receive_data{};
    std::array<uint8_t, 8192> incoming{};
    int incoming_head = 0;
    int incoming_count = 0;
    std::array<uint8_t, 8192> outgoing{};
    int outgoing_head = 0;
    int outgoing_count = 0;
  };
  std::array<SerialPort, 3> serial_ports_{};
  int ist_mode_ = -1;
  bool ist_start_previous_ = false;
  bool ist_auto_previous_ = false;
  struct VirtualInverter {
    bool connected = true;
    int response_ms = 10;
    std::array<uint16_t, 256> monitors{};
    std::array<uint16_t, 256> commands{};
    std::vector<int16_t> parameters;
  };
  std::array<std::unique_ptr<VirtualInverter>, 64> inverters_;
  struct InverterTransfer {
    bool active = false;
    bool completed = false;
    int channel = 1;
    int station = 0;
    int elapsed_ms = 0;
    int progress = 0;
  };
  std::vector<InverterTransfer> inverter_transfers_;
  std::array<size_t, 2> inverter_owners_{SIZE_MAX, SIZE_MAX};
  struct ModbusSlave {
    bool connected = true;
    bool listen_only = false;
    int response_ms = 10;
    uint8_t delimiter = '\n';
    uint16_t diagnostic = 0;
    uint8_t exception_status = 0;
    std::array<std::vector<uint16_t>, 4> data;
    std::array<uint16_t, 10> counters{};
    std::array<uint8_t, 64> log{};
    int log_head = 0;
    int log_count = 0;
  };
  std::array<std::unique_ptr<ModbusSlave>, 64> modbus_slaves_;
  struct ModbusTransfer {
    bool active = false;
    bool completed = false;
    int channel = 1;
    int station = 1;
    int elapsed_ms = 0;
    int retries = 0;
  };
  std::vector<ModbusTransfer> modbus_transfers_;
  size_t modbus_owner_ = SIZE_MAX;
  struct CfDatum {
    uint8_t type = 0;
    uint16_t length = 0;
    uint32_t bits = 0;
    std::array<char, 1025> text{};
  };
  struct CfCell {
    uint8_t type = 0;
    uint16_t length = 0;
    uint32_t bits = 0;
    uint32_t page = UINT32_MAX;
    uint32_t next = UINT32_MAX;
  };
  struct CfRow {
    uint32_t key = UINT32_MAX;
    uint32_t first = UINT32_MAX;
    uint16_t count = 0;
    uint32_t bytes = 0;
    int64_t timestamp = 0;
    bool buffered = false;
  };
  struct CfFile {
    bool exists = false;
    bool fifo = false;
    std::array<char, 9> name{};
    int timestamp = 0;
    int type = 0;
    int maximum_lines = 0;
    int policy = 0;
    int next_line = 1;
    int last_line = 0;
    uint64_t generation = 0;
  };
  struct CfMixed {
    bool active = false;
    bool reading = false;
    int file = -1;
    int line = 0;
    int column = 0;
    int total = 0;
    int progress = 0;
    int count = 0;
    int original_count = 0;
    bool buffered = false;
    plc_emulator::programming::DeviceAddress result;
    std::array<CfDatum, 254> data;
  };
  struct CfCard {
    bool mounted = true;
    bool ready = true;
    uint32_t capacity = 0;
    uint32_t used = 0;
    uint32_t buffered_bytes = 0;
    uint32_t buffer_capacity = 32768;
    uint64_t generation = 0;
    size_t owner = SIZE_MAX;
    std::array<int, 64> ids;
    std::array<CfFile, 1064> files;
    std::vector<CfRow> rows;
    std::vector<CfCell> cells;
    std::vector<uint32_t> free_cells;
    std::vector<std::array<char, 1025>> pages;
    std::vector<uint32_t> free_pages;
    std::array<int16_t, 5> errors{};
    CfMixed mixed;
  };
  std::array<std::unique_ptr<CfCard>, 2> cf_cards_;
  struct CfTransfer {
    bool active = false;
    bool completed = false;
    bool clear_pulse = false;
    int channel = 1;
  };
  std::vector<CfTransfer> cf_transfers_;
  bool ExecuteCfOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  void RefreshCfStatus();
  int CreateCfFile(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      CfCard* card);
  int ReadCfStatus(int command,
                   plc_emulator::programming::DeviceAddress destination,
                   CfCard* card);
  bool RotateCfFifo(CfCard* card);
  int ApplyCfOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      CfCard* card);
  CfRow* FindCfRow(CfCard* card, int file, int line, bool create);
  void DeleteCfRow(CfCard* card, CfRow* row);
  void DeleteCfFile(CfCard* card, int file);
  void FlushCfBuffer(CfCard* card, int file = -1);
  int StoreCfRow(CfCard* card, int file, int line,
                 std::span<const CfDatum> data, bool buffered);
  bool LoadCfRow(const CfCard& card, const CfRow& row,
                 std::span<CfDatum> data) const;
  int TransferCfData(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      CfCard* card, bool reading);
  bool ReadCfValues(plc_emulator::programming::DeviceAddress source, int type,
                    std::span<CfDatum> output);
  bool WriteCfValues(plc_emulator::programming::DeviceAddress destination,
                     std::span<const CfDatum> data);
  bool ExecuteModbusOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  int ApplyModbusCommand(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      ModbusSlave* slave, int station);
  bool ExecuteInverterOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  int ApplyInverterCommand(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      InverterTransfer* transfer, VirtualInverter* inverter);
  int step_state_ = -1;
  bool step_active_ = false;
  bool step_cleanup_ = false;
  bool step_group_previous_ = false;
  std::array<int, 8> step_sources_{};
  size_t step_source_count_ = 0;
  std::array<bool, 4096> step_previous_{};
  std::array<bool, 4096> step_pending_reset_{};
  std::vector<int> instruction_steps_;
  std::vector<size_t> step_output_slots_;
  std::vector<uint8_t> step_output_values_;
  std::vector<std::vector<size_t>> step_output_peers_;
  void InitializeSteps();
  void BeginStepScan();
  void FinishStepScan();
  void WriteStepOutput(size_t index, bool value);
  bool ExecuteInitialState(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      bool power);
  bool ExecuteSerialOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  void AdvanceSerialPorts();
  bool StartSerialSend(SerialPort* port,
                       const plc_emulator::programming::Operand& source);
  void ReceiveSerialByte(SerialPort* port, uint8_t value);
  void FinishSerialReceive(SerialPort* port);
  bool ExecutePidOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecutePidTuning(PidState* state, std::span<int16_t> parameters,
                        int target, int measured, int sampling_ms,
                        plc_emulator::programming::DeviceAddress output);
  std::array<bool, 256> encoder_configured_{};
  std::array<int32_t, 256> encoder_positions_{};
  bool ExecuteAbsoluteRead(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecutePositionTable(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  void InitializeAxes(bool initialize_registers);
  void AdvanceAxes();
  void HandleAxisInputs();
  void FinishAxis(int axis, bool abnormal);
  bool ExecuteMotionOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecuteDrumOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecutePanelOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  std::array<size_t, 8> module_transfer_owners_{};
  mutable std::map<std::string,
                   std::optional<plc_emulator::programming::DeviceAddress>>
      resolved_devices_;
  bool debug_mode_ = false;
  bool is_running_ = false;
  bool continuous_mode_ = false;
  int cycle_time_ms_ = 10;
  int current_elapsed_ms_ = 0;
  std::array<bool, 512> timer_enabled_ = {};
  std::array<bool, 256> counter_last_power_ = {};
  std::array<int, 512> timer_presets_ = {};
  std::array<int, 512> timer_remainders_ms_ = {};
  std::array<bool, 512> timer_contacts_ = {};
  std::array<bool, 256> counter_contacts_ = {};
  std::array<int, 256> counter_presets_ = {};

  /*
   * 파싱된 명령 정보를 보관합니다.
   * Holds parsed instruction data.
   */
  struct ParsedInstruction {
    enum Type { PLC_TON, PLC_CTU, PLC_RST_T, PLC_RST_C, COMMENT };
    Type type;
    int index = -1;
    int preset = 0;
  };

  bool ExecuteInstruction(const ParsedInstruction& instruction);
  bool ExecuteStructuredInstruction(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index);
  bool ExecuteProgramScan(int* executed_instructions);
  std::span<int16_t> WordMemory(
      plc_emulator::programming::DeviceAddress device);
  std::span<const int16_t> WordMemory(
      plc_emulator::programming::DeviceAddress device) const;
  bool ReadBit(plc_emulator::programming::DeviceAddress device) const;
  bool ExecuteDataOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  bool ExecuteCpuOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  bool ExecuteFloatOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  bool ExecuteStringOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  bool ExecuteTableOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  bool ExecuteHandyOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecuteModuleOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecuteUtilityOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      size_t index, bool power);
  bool ExecuteSortOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction,
      bool power, size_t index = SIZE_MAX);
  bool ReadString(const plc_emulator::programming::Operand& operand,
                  std::span<char> buffer, size_t* length);
  bool WriteString(plc_emulator::programming::DeviceAddress device,
                   std::span<const char> text);
  double ReadFloat(const plc_emulator::programming::Operand& operand) const;
  bool WriteFloat(plc_emulator::programming::DeviceAddress device, double value,
                  bool flags);
  bool ExecuteClockOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  bool ExecuteCpuBlockOperation(
      const plc_emulator::programming::OpenPLCInstruction& instruction);
  void WriteBit(plc_emulator::programming::DeviceAddress device, bool value);
  int32_t ReadValue(const plc_emulator::programming::Operand& operand,
                    bool wide = false) const;
  bool WriteWord(plc_emulator::programming::DeviceAddress device, int64_t value,
                 bool wide = false);
  bool WriteValue(const plc_emulator::programming::Operand& operand,
                  int64_t value, bool wide = false);

  void SetError(const std::string& error);

  void DebugLog(const std::string& message);
};

} /* namespace plc */
#endif /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_ \
        */
