/*
 * xml_serializer.h
 *
 * XML 직렬화 유틸리티 선언.
 * Declarations for XML serialization utilities.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_XML_SERIALIZER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_XML_SERIALIZER_H_

#include "plc_emulator/programming/programming_mode.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace plc {

/*
 * 래더 프로그램 XML 직렬화 클래스.
 * XML serializer for ladder programs.
 */
class XMLSerializer {
 public:
  /*
   * 직렬화 결과 요약.
   * Serialization result summary.
   */
  struct SerializationResult {
    bool success = false;
    std::string errorMessage;
    std::string xmlContent;
    size_t bytesWritten = 0;
  };

  /*
   * 역직렬화 결과 요약.
   * Deserialization result summary.
   */
  struct DeserializationResult {
    bool success = false;
    std::string errorMessage;
    LadderProgram program;
    std::string sourceVersion;
    std::string createdDate;
  };

  XMLSerializer();
  ~XMLSerializer();

  SerializationResult SerializeToXML(const LadderProgram& program);

  DeserializationResult DeserializeFromXML(const std::string& xmlContent);

  bool SaveToXMLFile(const LadderProgram& program, const std::string& filePath);

  DeserializationResult LoadFromXMLFile(const std::string& filePath);

  void SetDebugMode(bool enable) { debug_mode_ = enable; }

 private:
  bool debug_mode_ = false;
  std::string last_error_;

  std::string GenerateXMLHeader() const;
  std::string GenerateMetadata() const;
  std::string SerializeRungs(const std::vector<Rung>& rungs) const;
  std::string SerializeRung(const Rung& rung, int index) const;
  std::string SerializeCell(const LadderInstruction& cell, int rungIndex,
                            int cellIndex) const;
  std::string SerializeVerticalConnections(
      const std::vector<VerticalConnection>& connections) const;
  std::string SerializeVerticalConnection(const VerticalConnection& conn,
                                          int index) const;

  bool ParseXMLHeader(const std::string& xmlContent);
  bool ParseMetadata(const std::string& xmlContent,
                     DeserializationResult& result);
  bool ParseRungs(const std::string& xmlContent, LadderProgram& program);
  bool ParseRung(const std::string& rungXML, Rung& rung);
  bool ParseCell(const std::string& cellXML, LadderInstruction& cell);
  bool ParseVerticalConnections(const std::string& xmlContent,
                                LadderProgram& program);
  bool ParseVerticalConnection(const std::string& connXML,
                               VerticalConnection& conn);

  std::string EscapeXMLString(const std::string& str) const;
  std::string UnescapeXMLString(const std::string& str) const;
  std::string GetCurrentDateTime() const;
  std::string InstructionTypeToString(LadderInstructionType type) const;
  LadderInstructionType StringToInstructionType(const std::string& str) const;

  std::string ExtractXMLTag(const std::string& xml,
                            const std::string& tagName) const;
  std::vector<std::string> ExtractXMLTags(const std::string& xml,
                                          const std::string& tagName) const;
  std::string GetXMLAttribute(const std::string& xmlTag,
                              const std::string& attrName) const;
  std::string GetXMLContent(const std::string& xmlTag) const;

  void LogDebug(const std::string& message) const;
  void SetError(const std::string& error);
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_XML_SERIALIZER_H_ */
