// xml_serializer.h
// Copyright 2024 PLC Emulator Project
//
// XML serialization utilities.

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_XML_SERIALIZER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_XML_SERIALIZER_H_

#include "plc_emulator/programming/programming_mode.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace plc {

/**
 * @brief LadderProgram의 XML 직렬화/역직렬화를 담당하는 클래스
  * L추가erProgrm의 XML 직렬화/역직렬화를 담당하는 클래스
 *
 * 이 클래스는 LadderProgram의 모든 구조적 정보를 완전히 보존하여
 * XML 형태로 저장하고 복원할 수 있습니다.
 *
 * 저장되는 정보:
 * - 모든 rungs (number, isEndRung)
 * - 모든 cells (type, address, preset, 좌표)
 * - 모든 verticalConnections (x, rungs)
 * - 메타데이터 (버전, 생성일시 등)
 */
class XMLSerializer {
 public:
  struct SerializationResult {
    bool success = false;
    std::string errorMessage;
    std::string xmlContent;
    size_t bytesWritten = 0;
  };

  struct DeserializationResult {
    bool success = false;
    std::string errorMessage;
    LadderProgram program;
    std::string sourceVersion;
    std::string createdDate;
  };

  XMLSerializer();
  ~XMLSerializer();

  /**
   * @brief LadderProgram을 XML 문자열로 직렬화
    * L추가erProgrm을 XML 문자열로 직렬화
   * @param program 직렬화할 래더 프로그램
   * @return 직렬화 결과
   */
  SerializationResult SerializeToXML(const LadderProgram& program);

  /**
   * @brief XML 문자열을 LadderProgram으로 역직렬화
    * XML 문자열을 L추가erProgrm으로 역직렬화
   * @param xmlContent XML 문자열
   * @return 역직렬화 결과
   */
  DeserializationResult DeserializeFromXML(const std::string& xmlContent);

  /**
   * @brief XML 파일로 저장
   * @param program 래더 프로그램
   * @param filePath 파일 경로
   * @return 성공 여부
   */
  bool SaveToXMLFile(const LadderProgram& program, const std::string& filePath);

  /**
   * @brief XML 파일에서 로드
   * @param filePath 파일 경로
   * @return 역직렬화 결과
   */
  DeserializationResult LoadFromXMLFile(const std::string& filePath);

  /**
   * @brief 디버그 모드 설정
   * @param enable 디버그 모드 활성화 여부
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

 private:
  bool debug_mode_ = false;
  std::string last_error_;

  // XML 생성 관련 함수들
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

  // XML 파싱 관련 함수들
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

  // 유틸리티 함수들
  std::string EscapeXMLString(const std::string& str) const;
  std::string UnescapeXMLString(const std::string& str) const;
  std::string GetCurrentDateTime() const;
  std::string InstructionTypeToString(LadderInstructionType type) const;
  LadderInstructionType StringToInstructionType(const std::string& str) const;

  // XML 파싱 헬퍼들
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

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_XML_SERIALIZER_H_
