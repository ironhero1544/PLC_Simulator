#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_

#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/xml_serializer.h"

#include <map>
#include <memory>
#include <string>

namespace plc {

/**
 * @brief .plcproj 프로젝트 파일 관리자
 *
 * .plcproj 파일 구조:
 * - layout.xml: 완전한 LadderProgram 직렬화 (UI 구조 보존)
 * - program.ld: 실행용 OpenPLC 명령어
 * - manifest.json: 프로젝트 메타데이터 및 체크섬
 *
 * 이 클래스는 miniz 라이브러리를 사용하여 ZIP 압축/해제를 수행합니다.
 */
class ProjectFileManager {
 public:
  struct ProjectInfo {
    std::string projectName;
    std::string version;
    std::string createdDate;
    std::string lastModified;
    std::string toolVersion;
    std::string schemaVersion = "1.0";
    size_t layoutChecksum = 0;
    size_t programChecksum = 0;
  };

  struct SaveResult {
    bool success = false;
    std::string errorMessage;
    std::string savedPath;
    size_t compressedSize = 0;
    ProjectInfo info;
  };

  struct LoadResult {
    bool success = false;
    std::string errorMessage;
    LadderProgram program;
    std::string ldProgram;
    ProjectInfo info;
  };

  ProjectFileManager();
  ~ProjectFileManager();

  /**
   * @brief 프로젝트를 .plcproj 파일로 저장
   * @param program 래더 프로그램
   * @param filePath 저장할 파일 경로
   * @param projectName 프로젝트 이름 (선택적)
   * @return 저장 결과
   */
  SaveResult SaveProject(const LadderProgram& program,
                         const std::string& filePath,
                         const std::string& projectName = "");

  /**
   * @brief .plcproj 파일에서 프로젝트 로드
   * @param filePath 로드할 파일 경로
   * @return 로드 결과
   */
  LoadResult LoadProject(const std::string& filePath);

  /**
   * @brief 프로젝트 파일 정보 조회 (압축 해제 없이)
   * @param filePath 파일 경로
   * @return 프로젝트 정보
   */
  ProjectInfo GetProjectInfo(const std::string& filePath);

  /**
   * @brief 프로젝트 파일 유효성 검사
   * @param filePath 파일 경로
   * @return 유효성 검사 결과
   */
  bool ValidateProjectFile(const std::string& filePath);

  /**
   * @brief 디버그 모드 설정
   * @param enable 디버그 모드 활성화 여부
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  /**
   * @brief 마지막 에러 메시지 반환
   * @return 에러 메시지
   */
  const std::string& GetLastError() const { return last_error_; }

 private:
  bool debug_mode_ = false;
  std::string last_error_;
  std::unique_ptr<XMLSerializer> xml_serializer_;
  std::unique_ptr<LadderToLDConverter> ld_converter_;

  // ZIP 관련 내부 함수들
  bool CreateZipFile(const std::string& filePath,
                     const std::map<std::string, std::string>& files);
  bool ExtractZipFile(const std::string& filePath,
                      std::map<std::string, std::string>& files);

  // 프로젝트 내용 생성
  std::string GenerateManifest(const ProjectInfo& info);
  std::string GenerateLDProgram(const LadderProgram& program);

  // 프로젝트 내용 파싱
  bool ParseManifest(const std::string& manifestContent, ProjectInfo& info);

  // 유틸리티 함수들
  size_t CalculateChecksum(const std::string& content);
  std::string GetCurrentDateTime();
  std::string ExtractFileName(const std::string& filePath);
  void LogDebug(const std::string& message);
  void SetError(const std::string& error);

  // JSON 헬퍼 함수들 (간단한 JSON 생성/파싱)
  std::string CreateSimpleJSON(
      const std::map<std::string, std::string>& keyValues);
  std::map<std::string, std::string> ParseSimpleJSON(
      const std::string& jsonContent);
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_
