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
    bool X[16] = {false};
    bool Y[16] = {false};
    bool M[1000] = {false};

    int T[256] = {0};
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

  bool LoadCompiledCode(const std::string& compiledCode);

  bool LoadFromCompilationResult(
      const OpenPLCCompilerIntegration::CompilationResult& result);

  ExecutionResult ExecuteScanCycle();

  void SetContinuousExecution(bool enable, int cycleTime_ms = 10);

  void SetInput(int address, bool state);

  bool GetOutput(int address) const;

  void SetMemory(int address, bool state);

  bool GetMemory(int address) const;

  bool GetDeviceState(const std::string& address) const;

  void SetDeviceState(const std::string& address, bool state);

  void ResetMemory();

  const ExecutionResult& GetLastExecutionResult() const { return last_result_; }

  const PLCMemory& GetMemory() const { return memory_; }

  int GetTimerValue(int index) const;
  bool GetTimerEnabled(int index) const;
  int GetCounterValue(int index) const;
  bool GetCounterLastPower(int index) const;

  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  bool IsRunning() const { return is_running_; }

 private:
  PLCMemory memory_;
  ExecutionResult last_result_;
  std::string loaded_code_;
  bool debug_mode_ = false;
  bool is_running_ = false;
  bool continuous_mode_ = false;
  int cycle_time_ms_ = 10;
  int current_elapsed_ms_ = 0;
  std::array<bool, 256> timer_enabled_ = {};
  std::array<bool, 256> counter_last_power_ = {};
  std::array<int, 256> timer_presets_ = {};
  std::array<int, 256> counter_presets_ = {};

  /*
   * 파싱된 명령 정보를 보관합니다.
   * Holds parsed instruction data.
   */
  struct ParsedInstruction {
    enum Type {
      ASSIGNMENT,
      LOGIC_OP,
      FUNCTION_CALL,
      COND_ASSIGN,
      PLC_TON,
      PLC_CTU,
      PLC_RST_T,
      PLC_RST_C,
      COMMENT,
      UNKNOWN
    };

    Type type;
    std::string originalLine;
    std::string target;
    std::string operation;
    std::string operand1;
    std::string operand2;
    int lineNumber = 0;
    int index = -1;
    int preset = 0;
    bool boolValue = false;
  };

  std::vector<ParsedInstruction> instructions_;

  bool ParseCompiledCode(const std::string& code);

  bool ExecuteInstruction(const ParsedInstruction& instruction);

  bool ExecuteLogicOperation(const ParsedInstruction& instruction);

  bool ExecuteAssignment(const ParsedInstruction& instruction);

  bool* GetVariablePointer(const std::string& varName);

  bool EvaluateExpression(const std::string& expression);

  int ExtractNumber(const std::string& str);

  void SetError(const std::string& error);

  void DebugLog(const std::string& message);
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_ */
