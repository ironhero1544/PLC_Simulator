/*
 * project_file_manager.h
 *
 * 프로젝트 파일 저장/로드 관리 선언.
 * Declarations for project file save/load management.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_

#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/xml_serializer.h"

#include <map>
#include <memory>
#include <string>

namespace plc {

/*
 * 프로젝트 파일 입출력을 관리합니다.
 * Manages project file input/output.
 */
class ProjectFileManager {
 public:
  /*
   * 프로젝트 메타데이터.
   * Project metadata.
   */
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

  /*
   * 저장 결과 요약.
   * Save result summary.
   */
  struct SaveResult {
    bool success = false;
    std::string errorMessage;
    std::string savedPath;
    size_t compressedSize = 0;
    ProjectInfo info;
  };

  /*
   * 로드 결과 요약.
   * Load result summary.
   */
  struct LoadResult {
    bool success = false;
    std::string errorMessage;
    LadderProgram program;
    std::string ldProgram;
    ProjectInfo info;
  };

  ProjectFileManager();
  ~ProjectFileManager();

  SaveResult SaveProject(const LadderProgram& program,
                         const std::string& filePath,
                         const std::string& projectName = "");

  LoadResult LoadProject(const std::string& filePath);

  ProjectInfo GetProjectInfo(const std::string& filePath);

  bool ValidateProjectFile(const std::string& filePath);

  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  const std::string& GetLastError() const { return last_error_; }

 private:
  bool debug_mode_ = false;
  std::string last_error_;
  std::unique_ptr<XMLSerializer> xml_serializer_;
  std::unique_ptr<LadderToLDConverter> ld_converter_;

  bool CreateZipFile(const std::string& filePath,
                     const std::map<std::string, std::string>& files);
  bool ExtractZipFile(const std::string& filePath,
                      std::map<std::string, std::string>& files);

  std::string GenerateManifest(const ProjectInfo& info);
  std::string GenerateLDProgram(const LadderProgram& program);

  bool ParseManifest(const std::string& manifestContent, ProjectInfo& info);

  size_t CalculateChecksum(const std::string& content);
  std::string GetCurrentDateTime();
  std::string ExtractFileName(const std::string& filePath);
  void LogDebug(const std::string& message);
  void SetError(const std::string& error);

  std::string CreateSimpleJSON(
      const std::map<std::string, std::string>& keyValues);
  std::map<std::string, std::string> ParseSimpleJSON(
      const std::string& jsonContent);
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_ */
