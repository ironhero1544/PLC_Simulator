/*
 * ld_to_ladder_converter.h
 *
 * LD 형식을 래더로 변환하는 선언.
 * Declarations for LD-to-ladder conversion.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LD_TO_LADDER_CONVERTER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LD_TO_LADDER_CONVERTER_H_

#include <map>
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

/*
 * LD를 래더 프로그램으로 변환합니다.
 * Converts LD format to ladder programs.
 */
class LDToLadderConverter {
 public:
  LDToLadderConverter();
  ~LDToLadderConverter();

  bool ConvertFromLDFile(const std::string& ldFilePath,
                         LadderProgram& ladderProgram);

  bool ConvertFromLDString(const std::string& ldContent,
                           LadderProgram& ladderProgram);

  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  const std::string& GetLastError() const { return last_error_; }

  struct ConversionStats {
    int networksCount = 0;
    int contactsCount = 0;
    int coilsCount = 0;
    int blocksCount = 0;
    int connectionsCount = 0;
  };

  const ConversionStats& GetConversionStats() const { return stats_; }

 private:
  bool debug_mode_;
  std::string last_error_;
  ConversionStats stats_;

  struct LDElement {
    std::string type;
    std::string name;
    std::string operation;
    int x = 0, y = 0;
    std::map<std::string, std::string> attributes;
  };

  struct LDNetwork {
    int networkNumber = 0;
    std::vector<LDElement> elements;
    std::vector<std::string> connections;
  };

  std::vector<LDNetwork> parsed_networks_;

  bool ParseLDFile(const std::string& ldContent);
  bool ParseNetwork(const std::string& networkXML, LDNetwork& network);
  bool ParseElement(const std::string& elementXML, LDElement& element);

  bool ConvertNetworksToLadder(LadderProgram& ladderProgram);
  bool ConvertNetworkToRung(const LDNetwork& network, Rung& rung);
  LadderInstructionType ConvertLDTypeToInstruction(
      const std::string& ldType, const std::string& operation);

  void AnalyzeNetworkLayout(const LDNetwork& network);
  void OptimizeRungLayout(Rung& rung);
  void GenerateVerticalConnections(LadderProgram& ladderProgram);

  std::string ExtractXMLTag(const std::string& xml, const std::string& tagName);
  std::vector<std::string> ExtractXMLTags(const std::string& xml,
                                          const std::string& tagName);
  std::string GetXMLAttribute(const std::string& xmlTag,
                              const std::string& attrName);
  void LogDebug(const std::string& message);
  void SetError(const std::string& error);

  bool ValidateConvertedLadder(const LadderProgram& ladderProgram);
  bool CheckInstructionValidity(const LadderInstruction& instruction);
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LD_TO_LADDER_CONVERTER_H_ */
