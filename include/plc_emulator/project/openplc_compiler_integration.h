/*
 * openplc_compiler_integration.h
 *
 * OpenPLC 컴파일러 통합 인터페이스 선언.
 * Declarations for the OpenPLC compiler integration.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_OPENPLC_COMPILER_INTEGRATION_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_OPENPLC_COMPILER_INTEGRATION_H_

#include "plc_emulator/programming/execution_program.h"

#include <memory>
#include <string>
#include <vector>

namespace plc {

  /*
   * 전방 선언으로 순환 참조를 방지합니다.
   * Forward declarations to avoid circular dependencies.
   */
class LadderIRProgram;
struct LadderProgram;

/*
 * 래더를 기존 OpenPLC 실행기에 전달할 구조화된 데이터로 변환합니다.
 * Resolves ladder data for the existing OpenPLC execution path.
 */
class OpenPLCCompilerIntegration {
 public:
  /*
   * 컴파일 결과 요약.
   * Summary of a compilation result.
   */
  struct CompilationResult {
    bool success = false;
    plc_emulator::programming::ExecutionProgram program;
    std::string errorMessage;
    std::string intermediateCode;
    int inputCount = 0;
    int outputCount = 0;
    int memoryCount = 0;
  };

  OpenPLCCompilerIntegration();
  ~OpenPLCCompilerIntegration();

  CompilationResult CompileLDFile(const std::string& ldFilePath);

  CompilationResult CompileLDString(const std::string& ldContent);

  void SetIOConfiguration(int inputs = 16, int outputs = 16);

  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  void SetOptimizationLevel(int level) { optimization_level_ = level; }


  CompilationResult CompileLadderProgramWithIR(
      const LadderProgram& ladderProgram);

  CompilationResult CompileIRProgram(const LadderIRProgram& irProgram);

  bool SaveLDProgram(const CompilationResult& result,
                         const std::string& outputPath);

 private:
  bool debug_mode_ = false;
  int optimization_level_ = 1;
  int input_count_ = 16;
  int output_count_ = 16;
  int memory_count_ = 1000;


};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_OPENPLC_COMPILER_INTEGRATION_H_ */
