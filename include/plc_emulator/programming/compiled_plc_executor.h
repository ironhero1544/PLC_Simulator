// compiled_plc_executor.h
//
// Executes compiled PLC programs.

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
/**
 * @brief 而댄뙆?쇰맂 PLC C++ 肄붾뱶瑜??ㅽ뻾?섎뒗 ?붿쭊
 *
 * OpenPLCCompilerIntegration?먯꽌 ?앹꽦??C++ 肄붾뱶瑜??ㅼ젣濡??ㅽ뻾?섏뿬
 * FX3U-32M PLC???숈옉???쒕??덉씠?섑빀?덈떎.
 *
 * 二쇱슂 湲곕뒫:
 * - 而댄뙆?쇰맂 C++ 肄붾뱶 濡쒕뱶 諛??뚯떛
 * - PLC ?ㅼ틪 ?ъ씠???ㅽ뻾 (INPUT-PROGRAM-OUTPUT)
 * - I/O ?곹깭 愿由?(X0~X15, Y0~Y15)
 * - ?대? 硫붾え由?愿由?(M0~M999)
 * - ??대㉧/移댁슫???쒕??덉씠??
 * - 湲곗〈 臾쇰━?붿쭊怨쇱쓽 ?명꽣?섏씠??
 */
class CompiledPLCExecutor {
 public:
  struct PLCMemory {
    // I/O 諛곗뿴
    bool X[16] = {false};    // ?낅젰 X0~X15
    bool Y[16] = {false};    // 異쒕젰 Y0~Y15
    bool M[1000] = {false};  // ?대? 硫붾え由?M0~M999

    // ??대㉧/移댁슫??
    int T[256] = {0};  // ??대㉧ T0~T255 (?꾩옱媛? ms ?⑥쐞)
    int C[256] = {0};  // 移댁슫??C0~C255 (?꾩옱媛?

    // ?ㅽ뻾 ?곹깭
    bool accumulator = false;
    bool accumulator_stack[16] = {false};
    int stack_pointer = 0;

    // ??대컢 ?뺣낫
    std::chrono::steady_clock::time_point last_scan_time;
    int scan_cycle_ms = 0;
  };

  struct ExecutionResult {
    bool success = false;
    std::string errorMessage;
    int cycleTime_us = 0;      // ?ㅽ뻾 ?쒓컙 (留덉씠?щ줈珥?
    int instructionCount = 0;  // ?ㅽ뻾??紐낅졊????
  };

  CompiledPLCExecutor();
  ~CompiledPLCExecutor();

  /**
   * @brief 而댄뙆?쇰맂 C++ 肄붾뱶 濡쒕뱶
   * @param compiledCode OpenPLCCompilerIntegration?먯꽌 ?앹꽦??C++ 肄붾뱶
    * OpenPLC而댄뙆?펢Integr?먯꽌i?꾩뿉???앹꽦??C++ 肄붾뱶
   * @return 濡쒕뱶 ?깃났 ?щ?
   */
  bool LoadCompiledCode(const std::string& compiledCode);

  /**
   * @brief 而댄뙆??寃곌낵?먯꽌 吏곸젒 濡쒕뱶
   * @param result OpenPLCCompilerIntegration::CompilationResult
    * OpenPLC而댄뙆?펢Integr?먯꽌i??:Compil?먯꽌i?껽esult
   * @return 濡쒕뱶 ?깃났 ?щ?
   */
  bool LoadFromCompilationResult(
      const OpenPLCCompilerIntegration::CompilationResult& result);

  /**
   * @brief PLC ?ㅼ틪 ?ъ씠??1???ㅽ뻾
   * @return ?ㅽ뻾 寃곌낵
   */
  ExecutionResult ExecuteScanCycle();

  /**
   * @brief ?곗냽 ?ㅽ뻾 紐⑤뱶 (蹂꾨룄 ?ㅻ젅?쒖뿉???ㅽ뻾)
   * @param enable ?곗냽 ?ㅽ뻾 ?쒖꽦???щ?
   * @param cycleTime_ms ?ㅼ틪 ?ъ씠???쒓컙 (湲곕낯媛? 65ms, FX3U ?쒖?)
   */
  void SetContinuousExecution(bool enable, int cycleTime_ms = 65);

  /**
   * @brief ?낅젰 ?곹깭 ?ㅼ젙 (臾쇰━?붿쭊?먯꽌 ?몄텧)
   * @param address ?낅젰 二쇱냼 (0~15)
   * @param state ?낅젰 ?곹깭
   */
  void SetInput(int address, bool state);

  /**
   * @brief 異쒕젰 ?곹깭 ?쎄린 (臾쇰━?붿쭊?먯꽌 ?몄텧)
   * @param address 異쒕젰 二쇱냼 (0~15)
   * @return 異쒕젰 ?곹깭
   */
  bool GetOutput(int address) const;

  /**
   * @brief ?대? 硫붾え由??곹깭 ?ㅼ젙
   * @param address 硫붾え由?二쇱냼 (0~999)
   * @param state 硫붾え由??곹깭
   */
  void SetMemory(int address, bool state);

  /**
   * @brief ?대? 硫붾え由??곹깭 ?쎄린
   * @param address 硫붾え由?二쇱냼 (0~999)
   * @return 硫붾え由??곹깭
   */
  bool GetMemory(int address) const;

  /**
   * @brief 湲곗〈 ?명꽣?섏씠???명솚?깆쓣 ?꾪븳 ?⑥닔
   * @param address 二쇱냼 臾몄옄??("X5", "Y10", "M100" ??
   * @return ?붾컮?댁뒪 ?곹깭
   */
  bool GetDeviceState(const std::string& address) const;

  /**
   * @brief 湲곗〈 ?명꽣?섏씠???명솚?깆쓣 ?꾪븳 ?⑥닔
   * @param address 二쇱냼 臾몄옄??("X5", "Y10", "M100" ??
   * @param state ?붾컮?댁뒪 ?곹깭
   */
  void SetDeviceState(const std::string& address, bool state);

  /**
   * @brief 硫붾え由??곹깭 珥덇린??
   */
  void ResetMemory();

  /**
   * @brief ?ㅽ뻾 ?듦퀎 ?뺣낫 諛섑솚
   * @return ?ㅽ뻾 ?듦퀎
   */
  const ExecutionResult& GetLastExecutionResult() const { return last_result_; }

  /**
   * @brief PLC 硫붾え由?吏곸젒 ?묎렐 (?붾쾭源낆슜)
   * @return 硫붾え由?李몄“
   */
  const PLCMemory& GetMemory() const { return memory_; }

  int GetTimerValue(int index) const;
  bool GetTimerEnabled(int index) const;
  int GetCounterValue(int index) const;
  bool GetCounterLastPower(int index) const;

  /**
   * @brief ?붾쾭洹?紐⑤뱶 ?ㅼ젙
   * @param enable ?붾쾭洹?紐⑤뱶 ?쒖꽦???щ?
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  /**
   * @brief ?ㅽ뻾 以묒씤吏 ?뺤씤
   * @return ?ㅽ뻾 以묒씠硫?true
    * ?ㅽ뻾 以묒씠硫?李?
   */
  bool IsRunning() const { return is_running_; }

 private:
  PLCMemory memory_;
  ExecutionResult last_result_;
  std::string loaded_code_;
  bool debug_mode_ = false;
  bool is_running_ = false;
  bool continuous_mode_ = false;
  int cycle_time_ms_ = 65;
  int current_elapsed_ms_ = 0;
  std::array<bool, 256> timer_enabled_ = {};
  std::array<bool, 256> counter_last_power_ = {};
  std::array<int, 256> timer_presets_ = {};
  std::array<int, 256> counter_presets_ = {};

  // C++ 肄붾뱶 ?뚯떛 諛??ㅽ뻾???꾪븳 紐낅졊??援ъ“
  struct ParsedInstruction {
    enum Type {
      ASSIGNMENT,     // M[1] = accumulator;
      LOGIC_OP,       // accumulator = accumulator && X[5];
      FUNCTION_CALL,  // ?⑥닔 ?몄텧
      COND_ASSIGN,    // if (accumulator) Y0 = true;
      PLC_TON,        // PLC_TON T0 1000;
      PLC_CTU,        // PLC_CTU C0 5;
      PLC_RST_T,      // PLC_RST T0;
      PLC_RST_C,      // PLC_RST C0;
      COMMENT,        // 二쇱꽍
      UNKNOWN
    };

    Type type;
    std::string originalLine;
    std::string target;     // ???蹂??
    std::string operation;  // ?곗궛??
    std::string operand1;   // 泥?踰덉㎏ ?쇱뿰?곗옄
    std::string operand2;   // ??踰덉㎏ ?쇱뿰?곗옄
    int lineNumber = 0;
    int index = -1;
    int preset = 0;
    bool boolValue = false;
  };

  std::vector<ParsedInstruction> instructions_;

  /**
   * @brief C++ 肄붾뱶 ?뚯떛
   * @param code 而댄뙆?쇰맂 C++ 肄붾뱶
   * @return ?뚯떛 ?깃났 ?щ?
   */
  bool ParseCompiledCode(const std::string& code);

  /**
   * @brief 媛쒕퀎 紐낅졊???ㅽ뻾
   * @param instruction ?뚯떛??紐낅졊??
   * @return ?ㅽ뻾 ?깃났 ?щ?
   */
  bool ExecuteInstruction(const ParsedInstruction& instruction);

  /**
   * @brief ?쇰━ ?곗궛 ?ㅽ뻾 (accumulator 愿??
    * ?쇰━ ?곗궛 ?ㅽ뻾 (ccumul?먯꽌?먮뒗 愿??
   * @param instruction 紐낅졊??
   * @return ?ㅽ뻾 ?깃났 ?щ?
   */
  bool ExecuteLogicOperation(const ParsedInstruction& instruction);

  /**
   * @brief ?좊떦 ?곗궛 ?ㅽ뻾 (蹂??= 媛?
   * @param instruction 紐낅졊??
   * @return ?ㅽ뻾 ?깃났 ?щ?
   */
  bool ExecuteAssignment(const ParsedInstruction& instruction);

  /**
   * @brief 蹂???대쫫??硫붾え由?二쇱냼濡?蹂??
   * @param varName 蹂???대쫫 (?? "X[5]", "Y[10]", "M[100]")
   * @return 硫붾え由??ъ씤??
   */
  bool* GetVariablePointer(const std::string& varName);

  /**
   * @brief ?쒗쁽???됯? (?? "X[5] && !M[2]")
   * @param expression ?쒗쁽??臾몄옄??
   * @return ?됯? 寃곌낵
   */
  bool EvaluateExpression(const std::string& expression);

  /**
   * @brief 臾몄옄?댁뿉???レ옄 異붿텧
   * @param str ?낅젰 臾몄옄??
   * @return 異붿텧???レ옄 (?ㅽ뙣 ??-1)
   */
  int ExtractNumber(const std::string& str);

  /**
   * @brief ?ㅻ쪟 ?ㅼ젙
   * @param error ?ㅻ쪟 硫붿떆吏
   */
  void SetError(const std::string& error);

  /**
   * @brief ?붾쾭洹?濡쒓렇 異쒕젰
   * @param message 濡쒓렇 硫붿떆吏
   */
  void DebugLog(const std::string& message);
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_
