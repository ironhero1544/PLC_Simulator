// project_file_manager.h
//
// Project file save/load management.

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
 * @brief .csv ?꾨줈?앺듃 ?뚯씪 愿由ъ옄
 *
 * .csv ?뚯씪 援ъ“:
 * - layout.xml: ?꾩쟾??LadderProgram 吏곷젹??(UI 援ъ“ 蹂댁〈)
 * - program.ld: ?ㅽ뻾??OpenPLC 紐낅졊??
 * - manifest.json: ?꾨줈?앺듃 硫뷀??곗씠??諛?泥댄겕??
 *
 * ???대옒?ㅻ뒗 miniz ?쇱씠釉뚮윭由щ? ?ъ슜?섏뿬 ZIP ?뺤텞/?댁젣瑜??섑뻾?⑸땲??
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
   * @brief ?꾨줈?앺듃瑜?.csv ?뚯씪濡????
   * @param program ?섎뜑 ?꾨줈洹몃옩
   * @param filePath ??ν븷 ?뚯씪 寃쎈줈
   * @param projectName ?꾨줈?앺듃 ?대쫫 (?좏깮??
   * @return ???寃곌낵
   */
  SaveResult SaveProject(const LadderProgram& program,
                         const std::string& filePath,
                         const std::string& projectName = "");

  /**
   * @brief .csv ?뚯씪?먯꽌 ?꾨줈?앺듃 濡쒕뱶
   * @param filePath 濡쒕뱶???뚯씪 寃쎈줈
   * @return 濡쒕뱶 寃곌낵
   */
  LoadResult LoadProject(const std::string& filePath);

  /**
   * @brief ?꾨줈?앺듃 ?뚯씪 ?뺣낫 議고쉶 (?뺤텞 ?댁젣 ?놁씠)
   * @param filePath ?뚯씪 寃쎈줈
   * @return ?꾨줈?앺듃 ?뺣낫
   */
  ProjectInfo GetProjectInfo(const std::string& filePath);

  /**
   * @brief ?꾨줈?앺듃 ?뚯씪 ?좏슚??寃??
   * @param filePath ?뚯씪 寃쎈줈
   * @return ?좏슚??寃??寃곌낵
   */
  bool ValidateProjectFile(const std::string& filePath);

  /**
   * @brief ?붾쾭洹?紐⑤뱶 ?ㅼ젙
   * @param enable ?붾쾭洹?紐⑤뱶 ?쒖꽦???щ?
   */
  void SetDebugMode(bool enable) { debug_mode_ = enable; }

  /**
   * @brief 留덉?留??먮윭 硫붿떆吏 諛섑솚
   * @return ?먮윭 硫붿떆吏
   */
  const std::string& GetLastError() const { return last_error_; }

 private:
  bool debug_mode_ = false;
  std::string last_error_;
  std::unique_ptr<XMLSerializer> xml_serializer_;
  std::unique_ptr<LadderToLDConverter> ld_converter_;

  // ZIP 愿???대? ?⑥닔??
  bool CreateZipFile(const std::string& filePath,
                     const std::map<std::string, std::string>& files);
  bool ExtractZipFile(const std::string& filePath,
                      std::map<std::string, std::string>& files);

  // ?꾨줈?앺듃 ?댁슜 ?앹꽦
  std::string GenerateManifest(const ProjectInfo& info);
  std::string GenerateLDProgram(const LadderProgram& program);

  // ?꾨줈?앺듃 ?댁슜 ?뚯떛
  bool ParseManifest(const std::string& manifestContent, ProjectInfo& info);

  // ?좏떥由ы떚 ?⑥닔??
  size_t CalculateChecksum(const std::string& content);
  std::string GetCurrentDateTime();
  std::string ExtractFileName(const std::string& filePath);
  void LogDebug(const std::string& message);
  void SetError(const std::string& error);

  // JSON ?ы띁 ?⑥닔??(媛꾨떒??JSON ?앹꽦/?뚯떛)
  std::string CreateSimpleJSON(
      const std::map<std::string, std::string>& keyValues);
  std::map<std::string, std::string> ParseSimpleJSON(
      const std::string& jsonContent);
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_PROJECT_FILE_MANAGER_H_
