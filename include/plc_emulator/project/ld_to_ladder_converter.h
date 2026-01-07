// ld_to_ladder_converter.h
//
// Converts LD format to ladder.

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LD_TO_LADDER_CONVERTER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LD_TO_LADDER_CONVERTER_H_

#include <map>
#include <string>
#include <vector>

namespace plc {

// 전방 선언
struct LadderProgram;
struct Rung;
struct LadderInstruction;
enum class LadderInstructionType;

/**
 * @brief OpenPLC .ld 파일을 UI용 LadderProgram으로 역변환하는 클래스
  * OpenPLC .ld 파일을 UI용 L추가erProgrm으로 역변환하는 클래스
 *
 * [추론] 래더 업로드 기능을 위한 역변환기
 * - .ld 파일 파싱 및 구조 분석
 * - OpenPLC 표준 요소들을 UI 래더 다이어그램으로 변환
 * - 에러 처리 및 호환성 검증
 */
class LDToLadderConverter {
 public:
  LDToLadderConverter();
  ~LDToLadderConverter();

  /**
   * @brief .ld 파일을 LadderProgram으로 변환
    * .ld 파일을 L추가erProgrm으로 변환
   * @param ldFilePath .ld 파일 경로
   * @param ladderProgram 출력될 LadderProgram 구조체
    * 출력될 L추가erProgrm 구조체
   * @return 변환 성공 여부
   */
  bool ConvertFromLDFile(const std::string& ldFilePath,
                         LadderProgram& ladderProgram);

  /**
   * @brief .ld 문자열을 LadderProgram으로 변환
    * .ld 문자열을 L추가erProgrm으로 변환
   * @param ldContent .ld 파일 내용
   * @param ladderProgram 출력될 LadderProgram 구조체
    * 출력될 L추가erProgrm 구조체
   * @return 변환 성공 여부
   */
  bool ConvertFromLDString(const std::string& ldContent,
                           LadderProgram& ladderProgram);

  /**
   * @brief 디버그 모드 설정
   * @param enable 디버그 모드 활성화 여부
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  /**
   * @brief 마지막 에러 메시지 반환
   * @return 에러 메시지 문자열
   */
  const std::string& GetLastError() const { return last_error_; }

  /**
   * @brief 변환 통계 정보 반환
   * @return 변환된 요소들의 통계
   */
  struct ConversionStats {
    int networksCount = 0;     // 네트워크(룽) 개수
    int contactsCount = 0;     // 접점 개수
    int coilsCount = 0;        // 코일 개수
    int blocksCount = 0;       // 블록 개수
    int connectionsCount = 0;  // 연결선 개수
  };

  const ConversionStats& GetConversionStats() const { return stats_; }

 private:
  bool debug_mode_;
  std::string last_error_;
  ConversionStats stats_;

  // .ld 파일 파싱 관련
  struct LDElement {
    std::string type;       // "contact", "coil", "block" 등
    std::string name;       // 요소 이름/주소
    std::string operation;  // "LD", "AND", "OR", "ST" 등
    int x = 0, y = 0;       // 위치 좌표
    std::map<std::string, std::string> attributes;  // 추가 속성들
  };

  struct LDNetwork {
    int networkNumber = 0;
    std::vector<LDElement> elements;
    std::vector<std::string> connections;  // 연결선 정보
  };

  std::vector<LDNetwork> parsed_networks_;

  // 파싱 메서드들
  bool ParseLDFile(const std::string& ldContent);
  bool ParseNetwork(const std::string& networkXML, LDNetwork& network);
  bool ParseElement(const std::string& elementXML, LDElement& element);

  // 변환 메서드들
  bool ConvertNetworksToLadder(LadderProgram& ladderProgram);
  bool ConvertNetworkToRung(const LDNetwork& network, Rung& rung);
  LadderInstructionType ConvertLDTypeToInstruction(
      const std::string& ldType, const std::string& operation);

  // 레이아웃 분석 및 최적화
  void AnalyzeNetworkLayout(const LDNetwork& network);
  void OptimizeRungLayout(Rung& rung);
  void GenerateVerticalConnections(LadderProgram& ladderProgram);

  // 유틸리티 메서드들
  std::string ExtractXMLTag(const std::string& xml, const std::string& tagName);
  std::vector<std::string> ExtractXMLTags(const std::string& xml,
                                          const std::string& tagName);
  std::string GetXMLAttribute(const std::string& xmlTag,
                              const std::string& attrName);
  void LogDebug(const std::string& message);
  void SetError(const std::string& error);

  // 검증 메서드들
  bool ValidateConvertedLadder(const LadderProgram& ladderProgram);
  bool CheckInstructionValidity(const LadderInstruction& instruction);
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LD_TO_LADDER_CONVERTER_H_
