/*
 * ladder_to_ld_converter.h
 *
 * 래더를 LD 형식으로 변환하는 선언.
 * Declarations for ladder-to-LD conversion.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_

#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace plc {

  /*
   * 전방 선언으로 순환 참조를 방지합니다.
   * Forward declarations to avoid circular dependencies.
   */
struct LadderProgram;
struct Rung;
struct LadderInstruction;
enum class LadderInstructionType;

class LadderIRProgram;
class IRToStackConverter;

/*
 * 래더 프로그램을 LD로 변환합니다.
 * Converts ladder programs to LD format.
 */
class LadderToLDConverter {
 public:
  LadderToLDConverter();
  ~LadderToLDConverter();

  bool ConvertToLDFile(const LadderProgram& program,
                       const std::string& outputPath);

  std::string ConvertToLDString(const LadderProgram& program);

  const std::string& GetLastError() const { return last_error_; }

  void SetDebugMode(bool enable) { debug_mode_ = enable; }


  std::string ConvertToLDStringWithIR(const LadderProgram& program);

  std::vector<std::string> GenerateStackInstructions(
      const LadderIRProgram& irProgram);

 private:
  std::string last_error_;
  bool debug_mode_ = false;


};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_ */
