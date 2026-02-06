/*
 * openplc_compiler_integration.h
 *
 * OpenPLC 컴파일러 통합 인터페이스 선언.
 * Declarations for the OpenPLC compiler integration.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_OPENPLC_COMPILER_INTEGRATION_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_OPENPLC_COMPILER_INTEGRATION_H_

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
 * OpenPLC 컴파일러를 호출해 C++ 코드를 생성합니다.
 * Invokes the OpenPLC compiler to produce C++ code.
 */
class OpenPLCCompilerIntegration {
 public:
  /*
   * 컴파일 결과 요약.
   * Summary of a compilation result.
   */
  struct CompilationResult {
    bool success = false;
    std::string errorMessage;
    std::string generatedCode;
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

  bool SaveGeneratedCode(const CompilationResult& result,
                         const std::string& outputPath);

 private:
  bool debug_mode_ = false;
  int optimization_level_ = 1;
  int input_count_ = 16;
  int output_count_ = 16;
  int memory_count_ = 1000;

  struct LDInstruction {
    enum Type {
      LD,
      LDN,
      AND,
      ANDN,
      OR,
      ORN,
      ST,
      S,
      R,
      TON,
      TOF,
      CTU,
      CTD,
      EQ,
      NE,
      GT,
      LT,
      GE,
      LE,
      ADD,
      SUB,
      MUL,
      DIV,
      MOD
    };

    Type type;
    std::string operand;
    std::string preset;
    int lineNumber = 0;
  };

  struct Variable {
    enum Type {
      BOOL_INPUT,
      BOOL_OUTPUT,
      BOOL_MEMORY,
      INT_MEMORY,
      TIMER,
      COUNTER
    };
    Type type;
    std::string name;
    std::string address;
    int index = 0;
  };

  std::vector<LDInstruction> instructions_;
  std::vector<Variable> variables_;
  std::string last_error_;

  std::string GenerateOpenPLCHeader();

  bool ParseLDContent(const std::string& ldContent);

  bool ParseVariableDeclarations(const std::string& content);

  bool ParseInstructions(const std::string& content);

  std::string GenerateCPPCode(const std::vector<LDInstruction>& instructions);

  std::string GenerateFunctionHeader();

  std::string GenerateVariableDeclarations();

  std::string GenerateIOMapping();

  std::string GenerateExecutionCode(
      const std::vector<LDInstruction>& instructions);

  std::string TranslateInstruction(const LDInstruction& instruction);

  void SetError(const std::string& error);

  void DebugLog(const std::string& message);

  std::string Trim(const std::string& str);

  LDInstruction::Type StringToInstructionType(const std::string& typeStr);
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_OPENPLC_COMPILER_INTEGRATION_H_ */
