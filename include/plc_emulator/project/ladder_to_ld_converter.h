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

  struct DeviceSet {
    std::set<int> x_inputs;
    std::set<int> y_outputs;
    std::set<int> m_bits;
    std::set<int> t_timers;
    std::set<int> c_counters;
  };

  std::string GenerateLDHeader(const DeviceSet& devices);

  void ConvertSingleRung(const Rung& rung, int rungIndex,
                         std::string& ldContent);

  std::string ConvertInstruction(const LadderInstruction& instruction);

  std::string ConvertAddress(const std::string& address) const;

  std::string GetLDInstructionName(LadderInstructionType type);

  void DebugLog(const std::string& message);
  DeviceSet CollectUsedDevices(const LadderProgram& program) const;
  void AppendOutputInstruction(const LadderInstruction& output,
                               std::string& ldContent) const;
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_TO_LD_CONVERTER_H_ */
