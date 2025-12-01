// compiled_plc_executor.h
// Copyright 2024 PLC Emulator Project
//
// Executes compiled PLC programs.

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_

#include "plc_emulator/project/openplc_compiler_integration.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace plc {
/**
 * @brief 컴파일된 PLC C++ 코드를 실행하는 엔진
 *
 * OpenPLCCompilerIntegration에서 생성된 C++ 코드를 실제로 실행하여
 * FX3U-32M PLC의 동작을 시뮬레이션합니다.
 *
 * 주요 기능:
 * - 컴파일된 C++ 코드 로드 및 파싱
 * - PLC 스캔 사이클 실행 (INPUT-PROGRAM-OUTPUT)
 * - I/O 상태 관리 (X0~X15, Y0~Y15)
 * - 내부 메모리 관리 (M0~M999)
 * - 타이머/카운터 시뮬레이션
 * - 기존 물리엔진과의 인터페이스
 */
class CompiledPLCExecutor {
 public:
  struct PLCMemory {
    // I/O 배열
    bool X[16] = {false};    // 입력 X0~X15
    bool Y[16] = {false};    // 출력 Y0~Y15
    bool M[1000] = {false};  // 내부 메모리 M0~M999

    // 타이머/카운터
    int T[256] = {0};  // 타이머 T0~T255 (현재값, ms 단위)
    int C[256] = {0};  // 카운터 C0~C255 (현재값)

    // 실행 상태
    bool accumulator = false;
    bool accumulator_stack[16] = {false};
    int stack_pointer = 0;

    // 타이밍 정보
    std::chrono::steady_clock::time_point last_scan_time;
    int scan_cycle_ms = 0;
  };

  struct ExecutionResult {
    bool success = false;
    std::string errorMessage;
    int cycleTime_us = 0;      // 실행 시간 (마이크로초)
    int instructionCount = 0;  // 실행된 명령어 수
  };

  CompiledPLCExecutor();
  ~CompiledPLCExecutor();

  /**
   * @brief 컴파일된 C++ 코드 로드
   * @param compiledCode OpenPLCCompilerIntegration에서 생성된 C++ 코드
    * OpenPLC컴파일rIntegr에서i위에서 생성된 C++ 코드
   * @return 로드 성공 여부
   */
  bool LoadCompiledCode(const std::string& compiledCode);

  /**
   * @brief 컴파일 결과에서 직접 로드
   * @param result OpenPLCCompilerIntegration::CompilationResult
    * OpenPLC컴파일rIntegr에서i위::Compil에서i위Result
   * @return 로드 성공 여부
   */
  bool LoadFromCompilationResult(
      const OpenPLCCompilerIntegration::CompilationResult& result);

  /**
   * @brief PLC 스캔 사이클 1회 실행
   * @return 실행 결과
   */
  ExecutionResult ExecuteScanCycle();

  /**
   * @brief 연속 실행 모드 (별도 스레드에서 실행)
   * @param enable 연속 실행 활성화 여부
   * @param cycleTime_ms 스캔 사이클 시간 (기본값: 65ms, FX3U 표준)
   */
  void SetContinuousExecution(bool enable, int cycleTime_ms = 65);

  /**
   * @brief 입력 상태 설정 (물리엔진에서 호출)
   * @param address 입력 주소 (0~15)
   * @param state 입력 상태
   */
  void SetInput(int address, bool state);

  /**
   * @brief 출력 상태 읽기 (물리엔진에서 호출)
   * @param address 출력 주소 (0~15)
   * @return 출력 상태
   */
  bool GetOutput(int address) const;

  /**
   * @brief 내부 메모리 상태 설정
   * @param address 메모리 주소 (0~999)
   * @param state 메모리 상태
   */
  void SetMemory(int address, bool state);

  /**
   * @brief 내부 메모리 상태 읽기
   * @param address 메모리 주소 (0~999)
   * @return 메모리 상태
   */
  bool GetMemory(int address) const;

  /**
   * @brief 기존 인터페이스 호환성을 위한 함수
   * @param address 주소 문자열 ("X5", "Y10", "M100" 등)
   * @return 디바이스 상태
   */
  bool GetDeviceState(const std::string& address) const;

  /**
   * @brief 기존 인터페이스 호환성을 위한 함수
   * @param address 주소 문자열 ("X5", "Y10", "M100" 등)
   * @param state 디바이스 상태
   */
  void SetDeviceState(const std::string& address, bool state);

  /**
   * @brief 메모리 상태 초기화
   */
  void ResetMemory();

  /**
   * @brief 실행 통계 정보 반환
   * @return 실행 통계
   */
  const ExecutionResult& GetLastExecutionResult() const { return last_result_; }

  /**
   * @brief PLC 메모리 직접 접근 (디버깅용)
   * @return 메모리 참조
   */
  const PLCMemory& GetMemory() const { return memory_; }

  /**
   * @brief 디버그 모드 설정
   * @param enable 디버그 모드 활성화 여부
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  /**
   * @brief 실행 중인지 확인
   * @return 실행 중이면 true
    * 실행 중이면 참
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

  // C++ 코드 파싱 및 실행을 위한 명령어 구조
  struct ParsedInstruction {
    enum Type {
      ASSIGNMENT,     // M[1] = accumulator;
      LOGIC_OP,       // accumulator = accumulator && X[5];
      FUNCTION_CALL,  // 함수 호출
      COMMENT,        // 주석
      UNKNOWN
    };

    Type type;
    std::string originalLine;
    std::string target;     // 대상 변수
    std::string operation;  // 연산자
    std::string operand1;   // 첫 번째 피연산자
    std::string operand2;   // 두 번째 피연산자
    int lineNumber = 0;
  };

  std::vector<ParsedInstruction> instructions_;

  /**
   * @brief C++ 코드 파싱
   * @param code 컴파일된 C++ 코드
   * @return 파싱 성공 여부
   */
  bool ParseCompiledCode(const std::string& code);

  /**
   * @brief 개별 명령어 실행
   * @param instruction 파싱된 명령어
   * @return 실행 성공 여부
   */
  bool ExecuteInstruction(const ParsedInstruction& instruction);

  /**
   * @brief 논리 연산 실행 (accumulator 관련)
    * 논리 연산 실행 (ccumul에서또는 관련)
   * @param instruction 명령어
   * @return 실행 성공 여부
   */
  bool ExecuteLogicOperation(const ParsedInstruction& instruction);

  /**
   * @brief 할당 연산 실행 (변수 = 값)
   * @param instruction 명령어
   * @return 실행 성공 여부
   */
  bool ExecuteAssignment(const ParsedInstruction& instruction);

  /**
   * @brief 변수 이름을 메모리 주소로 변환
   * @param varName 변수 이름 (예: "X[5]", "Y[10]", "M[100]")
   * @return 메모리 포인터
   */
  bool* GetVariablePointer(const std::string& varName);

  /**
   * @brief 표현식 평가 (예: "X[5] && !M[2]")
   * @param expression 표현식 문자열
   * @return 평가 결과
   */
  bool EvaluateExpression(const std::string& expression);

  /**
   * @brief 문자열에서 숫자 추출
   * @param str 입력 문자열
   * @return 추출된 숫자 (실패 시 -1)
   */
  int ExtractNumber(const std::string& str);

  /**
   * @brief 오류 설정
   * @param error 오류 메시지
   */
  void SetError(const std::string& error);

  /**
   * @brief 디버그 로그 출력
   * @param message 로그 메시지
   */
  void DebugLog(const std::string& message);
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROGRAMMING_COMPILED_PLC_EXECUTOR_H_
