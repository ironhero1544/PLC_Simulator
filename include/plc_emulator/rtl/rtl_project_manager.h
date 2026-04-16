/*
 * rtl_project_manager.h
 *
 * Project-scoped RTL library, toolchain detection, and source analysis.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_RTL_RTL_PROJECT_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_RTL_RTL_PROJECT_MANAGER_H_

#include "plc_emulator/core/data_types.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace plc {

enum class RtlLogicValue {
  ZERO = 0,
  ONE = 1,
  UNKNOWN = 2,
  HIGH_Z = 3,
};

struct RtlSourceFile {
  std::string path;
  std::string content;
};

struct RtlPortDescriptor {
  std::string pinName;
  std::string baseName;
  int bitIndex = -1;
  int width = 1;
  bool isInput = true;
  bool isClock = false;
  bool isReset = false;
  int portId = -1;
  RtlLogicValue lastValue = RtlLogicValue::ZERO;
};

struct RtlLibraryEntry {
  std::string moduleId;
  std::string displayName;
  std::string topModule;
  std::string topFile;
  std::string testbenchTopModule;
  std::string testbenchTopFile;
  std::vector<RtlSourceFile> sourceFiles;
  std::vector<RtlPortDescriptor> ports;
  std::string diagnostics;
  std::string buildDiagnostics;
  std::string testbenchDiagnostics;
  std::string testbenchBuildLog;
  std::string testbenchRunLog;
  std::string testbenchVcdPath;
  std::string buildHash;
  bool analyzeSuccess = false;
  bool buildSuccess = false;
  bool testbenchSuccess = false;
  bool componentEnabled = false;
  size_t testbenchErrorCount = 0;
  size_t testbenchWarningCount = 0;
  bool testbenchLogSummaryValid = false;
};

struct RtlToolchainStatus {
  bool verilatorFound = false;
  bool bashFound = false;
  bool makeFound = false;
  bool compilerFound = false;
  bool wslFound = false;
  bool wslVerilatorFound = false;
  bool wslMakeFound = false;
  bool wslCompilerFound = false;
  std::string verilatorPath;
  std::string verilatorRoot;
  std::string toolsDir;
  std::string bashPath;
  std::string makePath;
  std::string cmakePath;
  std::string compilerPath;
  std::string wslPath;
  std::string wslVerilatorPath;
  std::string wslMakePath;
  std::string wslCompilerPath;
  std::string description;
  std::string mingwBinPath;   // 최종 결정된 mingw64/bin 경로 (PATH 주입용)
  std::string msys2Root;      // MSYS2 루트 경로 (예: C:\msys64)
};

class RtlProjectManager {
 public:
  RtlProjectManager();
  ~RtlProjectManager();

  void LoadGlobalLibrary();
  void SaveGlobalLibrary() const;

  std::vector<RtlLibraryEntry>& GetLibrary() { return library_; }
  const std::vector<RtlLibraryEntry>& GetLibrary() const { return library_; }

  RtlLibraryEntry* FindEntryById(const std::string& moduleId);
  const RtlLibraryEntry* FindEntryById(const std::string& moduleId) const;

  RtlLibraryEntry* CreateEntry(const std::string& displayName);
  RtlLibraryEntry* ImportEntry(const RtlLibraryEntry& entry);
  bool RenameEntryDisplayName(const std::string& moduleId,
                              const std::string& displayName);
  bool DeleteEntry(const std::string& moduleId);
  bool DuplicateEntry(const std::string& moduleId);

  std::string ExportSourceFile(const std::string& moduleId,
                               const std::string& sourcePath) const;
  bool ImportSourceFile(const std::string& moduleId,
                        const std::string& sourcePath,
                        const std::string& content);
  bool RemoveSourceFile(const std::string& moduleId,
                        const std::string& sourcePath);
  bool RenameSourceFile(const std::string& moduleId,
                        const std::string& oldSourcePath,
                        const std::string& newSourcePath);
  bool UpdateSourceFile(const std::string& moduleId,
                        const std::string& sourcePath,
                        const std::string& content);

  bool Analyze(const std::string& moduleId);
  bool Build(const std::string& moduleId);
  bool RunTestbench(const std::string& moduleId);

  RtlToolchainStatus DetectToolchain() const;
  bool HasBuildArtifact(const RtlLibraryEntry& entry) const;
  std::string GetBuildCacheDirectory(const RtlLibraryEntry& entry) const;
  std::string GetWslBuildCacheDirectory(const RtlLibraryEntry& entry) const;
  std::string GetTestbenchCacheDirectory(const RtlLibraryEntry& entry) const;
  std::string GetWslTestbenchCacheDirectory(const RtlLibraryEntry& entry) const;
  std::string GetWorkerLaunchCommand(const RtlLibraryEntry& entry,
                                     const RtlToolchainStatus& status) const;

  std::string SerializeLibraryJson() const;
  std::string SerializeProjectRtlJson() const;
  std::string SerializeEntryJson(const RtlLibraryEntry& entry,
                                 bool includeSourceFiles = true) const;
  bool DeserializeLibraryJson(const std::string& jsonText, bool merge = false);
  bool DeserializeEntryJson(const std::string& jsonText,
                            RtlLibraryEntry* entry) const;

  std::map<std::string, std::string> GetBuildArtifactBundle(
      const std::string& moduleId) const;
  bool RestoreBuildArtifactBundle(
      const std::string& moduleId,
      const std::map<std::string, std::string>& files);
  std::string GetBuildArtifactData(const std::string& moduleId) const;
  bool RestoreBuildArtifact(const std::string& moduleId, const std::string& data);

  static std::vector<Port> BuildRuntimePorts(
      const std::vector<RtlPortDescriptor>& ports);
  static std::vector<RtlPinBinding> BuildDefaultPinBindings(
      const std::vector<RtlPortDescriptor>& ports);

 private:
  std::vector<RtlLibraryEntry> library_;
  mutable std::unordered_map<std::string, size_t> libraryIndexById_;
  mutable bool libraryIndexValid_ = false;

  void InvalidateLibraryIndex();
  void RebuildLibraryIndex() const;
  std::string BuildUniqueDisplayName(
      const std::string& displayName,
      const std::string& ignoredModuleId = "") const;

    bool RunCommandCapture(const std::string& command,
                           std::string* output,
                           const RtlToolchainStatus& status,
                           unsigned int timeout_ms = 30000) const;
  bool AnalyzeWithRegex(RtlLibraryEntry* entry) const;
  bool RunVerilatorLint(const RtlLibraryEntry& entry,
                        const RtlToolchainStatus& status,
                        std::string* diagnostics) const;
  bool RunVerilatorBuild(RtlLibraryEntry* entry,
                         const RtlToolchainStatus& status,
                         std::string* diagnostics) const;
  bool RunVerilatorTestbench(RtlLibraryEntry* entry,
                             const RtlToolchainStatus& status,
                             std::string* diagnostics) const;
  static std::string BuildModuleId(const std::string& displayName,
                                   size_t ordinal);
  static std::string Slugify(const std::string& text);
  static std::string JoinTempFileArguments(
      const std::vector<std::string>& filePaths);
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_RTL_RTL_PROJECT_MANAGER_H_
