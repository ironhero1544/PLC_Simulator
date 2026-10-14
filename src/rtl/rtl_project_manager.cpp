/*
 * rtl_project_manager.cpp
 *
 * RTL project library management and lightweight Verilog analysis helpers.
 */

#include "plc_emulator/rtl/rtl_project_manager.h"
#include "plc_emulator/core/windows_power_utils.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <processenv.h>
#endif

using json = nlohmann::json;

namespace plc {

namespace {
// --- Internal Constants ---

#ifdef _WIN32
static constexpr DWORD kLocalDefaultTimeoutMs = 30000;
static constexpr DWORD kLocalBuildTimeoutMs = 300000;
#else
static constexpr unsigned int kLocalDefaultTimeoutMs = 30000;
static constexpr unsigned int kLocalBuildTimeoutMs = 300000;
#endif
static constexpr size_t kMaxStoredRtlLogBytes = 256 * 1024;

// --- Internal Utilities (Private Linkage) ---

static std::string LocalHashHex(const std::string& text) {
  uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char ch : text) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << hash;
  return stream.str();
}

static bool LocalFileExists(const std::string& path) {
  if (path.empty()) return false;
#ifdef _WIN32
  const DWORD attrs = GetFileAttributesW(plc_emulator::WslToolchain::ToWide(path).c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
  std::ifstream f(path.c_str()); return f.good();
#endif
}

static std::string LocalJoinPath(const std::string& left, const std::string& right) {
  if (left.empty()) return right;
  std::string res = left;
  if (res.back() != '\\' && res.back() != '/') res += "\\";
  res += right; return res;
}

static std::string LocalToForwardSlashes(const std::string& path) {
  std::string res = path;
  std::replace(res.begin(), res.end(), '\\', '/');
  return res;
}

static std::string LocalTrim(const std::string& text) {
  if (text.empty()) return "";
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
  return text.substr(start, end - start);
}

static void LocalCapStoredLog(std::string* text) {
  if (!text || text->size() <= kMaxStoredRtlLogBytes) return;
  static const std::string kPrefix =
      "[Log truncated. Showing the most recent output.]\n";
  size_t keep = kMaxStoredRtlLogBytes > kPrefix.size()
                    ? kMaxStoredRtlLogBytes - kPrefix.size()
                    : 0;
  size_t start = text->size() > keep ? text->size() - keep : 0;
  *text = kPrefix + text->substr(start);
}

static void LocalCapEntryLogs(plc::RtlLibraryEntry* entry) {
  if (!entry) return;
  LocalCapStoredLog(&entry->diagnostics);
  LocalCapStoredLog(&entry->buildDiagnostics);
  LocalCapStoredLog(&entry->testbenchDiagnostics);
  LocalCapStoredLog(&entry->testbenchBuildLog);
  LocalCapStoredLog(&entry->testbenchRunLog);
}

static json LocalSerializePort(const plc::RtlPortDescriptor& port) {
  return {{"pin_name", port.pinName},
          {"base_name", port.baseName},
          {"bit_index", port.bitIndex},
          {"width", port.width},
          {"is_input", port.isInput},
          {"is_clock", port.isClock},
          {"is_reset", port.isReset},
          {"port_id", port.portId}};
}

static json LocalSerializeEntry(const plc::RtlLibraryEntry& entry,
                                bool includeSourceFiles) {
  json item;
  item["module_id"] = entry.moduleId;
  item["display_name"] = entry.displayName;
  item["top_module"] = entry.topModule;
  item["top_file"] = entry.topFile;
  item["build_hash"] = entry.buildHash;
  item["analyze_success"] = entry.analyzeSuccess;
  item["build_success"] = entry.buildSuccess;
  item["testbench_top_module"] = entry.testbenchTopModule;
  item["testbench_top_file"] = entry.testbenchTopFile;
  item["diagnostics"] = entry.diagnostics;
  item["build_diagnostics"] = entry.buildDiagnostics;
  item["testbench_diagnostics"] = entry.testbenchDiagnostics;
  item["testbench_build_log"] = entry.testbenchBuildLog;
  item["testbench_run_log"] = entry.testbenchRunLog;
  item["testbench_vcd_path"] = entry.testbenchVcdPath;
  item["testbench_success"] = entry.testbenchSuccess;
  item["component_enabled"] = entry.componentEnabled;

  json sources = json::array();
  if (includeSourceFiles) {
    for (const auto& file : entry.sourceFiles) {
      sources.push_back({{"path", file.path}, {"content", file.content}});
    }
  }
  item["source_files"] = sources;

  json ports = json::array();
  for (const auto& port : entry.ports) {
    ports.push_back(LocalSerializePort(port));
  }
  item["ports"] = std::move(ports);
  return item;
}

static bool LocalDeserializeEntry(const json& item,
                                  plc::RtlLibraryEntry* entry) {
  if (!entry || !item.is_object()) {
    return false;
  }
  const std::string moduleId = item.value("module_id", "");
  if (moduleId.empty()) {
    return false;
  }

  plc::RtlLibraryEntry parsed;
  parsed.moduleId = moduleId;
  parsed.displayName = item.value("display_name", "RTL Module");
  parsed.topModule = item.value("top_module", "");
  parsed.topFile = item.value("top_file", "");
  parsed.buildHash = item.value("build_hash", "");
  parsed.analyzeSuccess = item.value("analyze_success", false);
  parsed.buildSuccess = item.value("build_success", false);
  parsed.testbenchTopModule = item.value("testbench_top_module", "");
  parsed.testbenchTopFile = item.value("testbench_top_file", "");
  parsed.diagnostics = item.value("diagnostics", "");
  parsed.buildDiagnostics = item.value("build_diagnostics", "");
  parsed.testbenchDiagnostics = item.value("testbench_diagnostics", "");
  parsed.testbenchBuildLog = item.value("testbench_build_log", "");
  parsed.testbenchRunLog = item.value("testbench_run_log", "");
  parsed.testbenchVcdPath = item.value("testbench_vcd_path", "");
  parsed.testbenchSuccess = item.value("testbench_success", false);
  parsed.componentEnabled = item.value("component_enabled", false);

  auto sourcesIt = item.find("source_files");
  if (sourcesIt != item.end() && sourcesIt->is_array()) {
    for (const auto& source : *sourcesIt) {
      parsed.sourceFiles.push_back(
          {source.value("path", ""), source.value("content", "")});
    }
  }

  auto portsIt = item.find("ports");
  if (portsIt != item.end() && portsIt->is_array()) {
    for (const auto& port : *portsIt) {
      plc::RtlPortDescriptor descriptor;
      descriptor.pinName = port.value("pin_name", "");
      descriptor.baseName = port.value("base_name", "");
      descriptor.bitIndex = port.value("bit_index", -1);
      descriptor.width = port.value("width", 1);
      descriptor.isInput = port.value("is_input", true);
      descriptor.isClock = port.value("is_clock", false);
      descriptor.isReset = port.value("is_reset", false);
      descriptor.portId = port.value("port_id", -1);
      parsed.ports.push_back(std::move(descriptor));
    }
  }

  LocalCapEntryLogs(&parsed);
  *entry = std::move(parsed);
  return true;
}

static std::vector<std::string> LocalSplitPortDeclarations(const std::string& text) {
  std::vector<std::string> declarations;
  std::string current;
  int bracketDepth = 0;
  for (char ch : text) {
    if (ch == '[') ++bracketDepth;
    else if (ch == ']') bracketDepth = std::max(0, bracketDepth - 1);

    if (ch == ',' && bracketDepth == 0) {
      std::string token = LocalTrim(current);
      if (!token.empty()) declarations.push_back(token);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  std::string token = LocalTrim(current);
  if (!token.empty()) declarations.push_back(token);
  return declarations;
}

static bool LocalEqualsIgnoreCase(const std::string& lhs, const std::string& rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
        std::tolower(static_cast<unsigned char>(rhs[i]))) return false;
  }
  return true;
}

static bool LocalSplitDisplayNameSuffix(const std::string& displayName,
                                        std::string* baseName,
                                        int* suffix) {
  const std::string trimmed = LocalTrim(displayName);
  if (baseName) {
    *baseName = trimmed;
  }
  if (suffix) {
    *suffix = 0;
  }

  static const std::regex kSuffixRegex(R"(^(.*)\s\((\d+)\)$)");
  std::smatch match;
  if (!std::regex_match(trimmed, match, kSuffixRegex) || match.size() != 3) {
    return false;
  }

  const std::string parsedBase = LocalTrim(match[1].str());
  if (parsedBase.empty()) {
    return false;
  }

  if (baseName) {
    *baseName = parsedBase;
  }
  if (suffix) {
    *suffix = std::atoi(match[2].str().c_str());
  }
  return true;
}

static bool LocalDisplayNameBelongsToFamily(const std::string& candidate,
                                            const std::string& familyBase,
                                            int* suffix) {
  std::string candidateBase;
  int candidateSuffix = 0;
  const bool hasSuffix =
      LocalSplitDisplayNameSuffix(candidate, &candidateBase, &candidateSuffix);
  if (!LocalEqualsIgnoreCase(candidateBase, familyBase)) {
    return false;
  }

  if (suffix) {
    *suffix = hasSuffix ? candidateSuffix : 0;
  }
  return true;
}

static Color LocalGetRtlPortColor(bool isInput) {
  return isInput ? Color{0.10f, 0.55f, 0.92f, 1.0f}
                 : Color{0.93f, 0.40f, 0.12f, 1.0f};
}

static std::string LocalEscape(const std::string& arg) {
  if (arg.empty()) return "\"\"";
  std::string escaped = "\"";
  for (size_t i = 0; i < arg.size(); ++i) {
    size_t backslashes = 0;
    while (i < arg.size() && arg[i] == '\\') { i++; backslashes++; }
    if (i == arg.size()) {
      for (size_t j = 0; j < backslashes * 2; ++j) escaped.push_back('\\');
    } else if (arg[i] == '"') {
      for (size_t j = 0; j < backslashes * 2 + 1; ++j) escaped.push_back('\\');
      escaped.push_back('"');
    } else {
      for (size_t j = 0; j < backslashes; ++j) escaped.push_back('\\');
      escaped.push_back(arg[i]);
    }
  }
  escaped.push_back('"');
  return escaped;
}

static bool LocalIsWindowsWorker(const std::string& data) {
  if (data.size() < 64 || data[0] != 'M' || data[1] != 'Z') return false;
  uint32_t offset = 0;
  std::memcpy(&offset, data.data() + 60, sizeof(offset));
  return offset <= data.size() - 4 && data.compare(offset, 4, "PE\0\0", 4) == 0;
}

static std::string LocalStripComments(const std::string& text) {
  std::string s; bool lC = false, bC = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char ch = text[i], nx = (i + 1 < text.size()) ? text[i + 1] : '\0';
    if (!lC && !bC && ch == '/' && nx == '/') { lC = true; ++i; continue; }
    if (!lC && !bC && ch == '/' && nx == '*') { bC = true; ++i; continue; }
    if (lC) { if (ch == '\n') { lC = false; s += ch; } continue; }
    if (bC) { if (ch == '*' && nx == '/') { bC = false; ++i; } continue; }
    s += ch;
  }
  return s;
}

static std::string LocalWinPathToWslPath(const std::string& path) {
  if (path.size() < 3 || path[1] != ':') return path;
  std::string res = "/mnt/";
  res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(path[0]))));
  for (size_t i = 2; i < path.size(); ++i) res.push_back(path[i] == '\\' ? '/' : path[i]);
  return res;
}

static bool LocalWriteFileContent(const std::string& path, const std::string& content) {
  std::ofstream f(
      std::filesystem::path(plc_emulator::WslToolchain::ToWide(path)),
      std::ios::binary);
  if (!f.is_open())
    return false;
  f << content;
  return true;
}

static bool LocalWriteWslSources(const RtlLibraryEntry& entry,
                                 const std::string& directory,
                                 std::vector<std::wstring>* arguments,
                                 std::string* diagnostics) {
  for (const auto& source : entry.sourceFiles) {
    const std::string relative = LocalToForwardSlashes(source.path);
    const std::filesystem::path path(
        plc_emulator::WslToolchain::ToWide(relative));
    if (relative.empty() || path.is_absolute() || path.has_root_name() ||
        path.has_root_directory() || relative.find(':') != std::string::npos) {
      if (diagnostics)
        *diagnostics = "HDL source paths must be relative.";
      return false;
    }
    for (const auto& segment : path) {
      if (segment == "..") {
        if (diagnostics)
          *diagnostics = "HDL paths cannot escape the source directory.";
        return false;
      }
    }
    const auto destination =
        std::filesystem::path(plc_emulator::WslToolchain::ToWide(directory)) /
        L"src" / path;
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) {
      if (diagnostics)
        *diagnostics = "Cannot create HDL source directory: " + error.message();
      return false;
    }
    std::ofstream output(destination, std::ios::binary);
    output << source.content;
    if (!output.good()) {
      if (diagnostics)
        *diagnostics = "Cannot write HDL source: " + relative;
      return false;
    }
    arguments->push_back(plc_emulator::WslToolchain::ToWide("src/" + relative));
  }
  return !entry.sourceFiles.empty();
}

static std::string LocalGetTempBasePath(const std::string& suffix) {
  std::error_code error;
  const auto path =
      std::filesystem::temp_directory_path(error) /
      std::filesystem::path(plc_emulator::WslToolchain::ToWide(suffix));
  if (error)
    return suffix;
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

static bool LocalEnsureDirRecursive(const std::string& path) {
  if (path.empty())
    return false;
  std::error_code error;
  std::filesystem::create_directories(
      std::filesystem::path(plc_emulator::WslToolchain::ToWide(path)), error);
  return !error;
}

static std::string GetLocalAppDataPathInternal() {
#ifdef _WIN32
  const wchar_t* value = _wgetenv(L"LOCALAPPDATA");
  if (value != nullptr) {
    const auto utf8 = std::filesystem::path(value).u8string();
    return std::string(utf8.begin(), utf8.end());
  }
#endif
  return ".";
}

static std::string BuildCacheRootDirectory() {
  return LocalJoinPath(GetLocalAppDataPathInternal(),
                       "PLCSimulator\\rtl_cache");
}

static std::string LocalReadFileContent(const std::string& path) {
  std::ifstream f(
      std::filesystem::path(plc_emulator::WslToolchain::ToWide(path)),
      std::ios::binary);
  if (!f.is_open())
    return "";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // anonymous namespace

// --- Public Function ---
bool FileExists(const std::string& path) { return LocalFileExists(path); }

// --- RtlProjectManager Implementation ---

RtlProjectManager::RtlProjectManager() { LoadGlobalLibrary(); }
RtlProjectManager::~RtlProjectManager() { SaveGlobalLibrary(); }

void RtlProjectManager::LoadGlobalLibrary() {
  std::ifstream f(std::filesystem::path(plc_emulator::WslToolchain::ToWide(LocalJoinPath(GetLocalAppDataPathInternal(), "PLCSimulator\\rtl_library.json"))), std::ios::binary);
  if (!f.is_open()) return;
  std::ostringstream ss; ss << f.rdbuf();
  if (!ss.str().empty()) DeserializeLibraryJson(ss.str());
}

void RtlProjectManager::SaveGlobalLibrary() const {
  std::string p = LocalJoinPath(GetLocalAppDataPathInternal(), "PLCSimulator\\rtl_library.json");
  size_t pos = p.find_last_of("\\/");
  if (pos != std::string::npos) LocalEnsureDirRecursive(p.substr(0, pos));
  std::ofstream f(std::filesystem::path(plc_emulator::WslToolchain::ToWide(p)), std::ios::binary); if (f.is_open()) f << SerializeLibraryJson();
}

void RtlProjectManager::InvalidateLibraryIndex() {
  libraryIndexById_.clear();
  libraryIndexValid_ = false;
}

void RtlProjectManager::RebuildLibraryIndex() const {
  if (libraryIndexValid_) {
    return;
  }
  libraryIndexById_.clear();
  libraryIndexById_.reserve(library_.size());
  for (size_t i = 0; i < library_.size(); ++i) {
    libraryIndexById_[library_[i].moduleId] = i;
  }
  libraryIndexValid_ = true;
}

RtlLibraryEntry* RtlProjectManager::FindEntryById(const std::string& id) {
  RebuildLibraryIndex();
  const auto it = libraryIndexById_.find(id);
  if (it == libraryIndexById_.end() || it->second >= library_.size()) {
    return nullptr;
  }
  return &library_[it->second];
}

const RtlLibraryEntry* RtlProjectManager::FindEntryById(const std::string& id) const {
  RebuildLibraryIndex();
  const auto it = libraryIndexById_.find(id);
  if (it == libraryIndexById_.end() || it->second >= library_.size()) {
    return nullptr;
  }
  return &library_[it->second];
}

RtlLibraryEntry* RtlProjectManager::CreateEntry(const std::string& name) {
  RtlLibraryEntry e;
  e.displayName = BuildUniqueDisplayName(name.empty() ? "RTL Module" : name);
  e.moduleId = BuildModuleId(e.displayName, library_.size());
  e.topModule = Slugify(e.displayName); if (e.topModule.empty()) e.topModule = "rtl_top";
  e.topFile = e.topModule + ".v";
  e.sourceFiles.push_back({e.topFile, "module " + e.topModule + "(\n  input clk,\n  input rst,\n  output out\n);\nassign out = 0;\nendmodule\n"});
  library_.push_back(e);
  InvalidateLibraryIndex();
  SaveGlobalLibrary();
  return &library_.back();
}

RtlLibraryEntry* RtlProjectManager::ImportEntry(const RtlLibraryEntry& entry) {
  RtlLibraryEntry imported = entry;
  imported.displayName = BuildUniqueDisplayName(imported.displayName);
  if (imported.topModule.empty()) {
    imported.topModule = Slugify(imported.displayName);
    if (imported.topModule.empty()) {
      imported.topModule = "rtl_top";
    }
  }
  if (imported.moduleId.empty() || FindEntryById(imported.moduleId)) {
    size_t ordinal = library_.size();
    do {
      imported.moduleId = BuildModuleId(imported.displayName, ordinal++);
    } while (FindEntryById(imported.moduleId));
  }
  LocalCapEntryLogs(&imported);
  library_.push_back(std::move(imported));
  InvalidateLibraryIndex();
  SaveGlobalLibrary();
  return &library_.back();
}

bool RtlProjectManager::RenameEntryDisplayName(const std::string& moduleId,
                                               const std::string& displayName) {
  RtlLibraryEntry* entry = FindEntryById(moduleId);
  if (!entry) {
    return false;
  }

  const std::string uniqueDisplayName =
      BuildUniqueDisplayName(displayName, moduleId);
  if (entry->displayName == uniqueDisplayName) {
    return true;
  }

  entry->displayName = uniqueDisplayName;
  SaveGlobalLibrary();
  return true;
}

bool RtlProjectManager::DeleteEntry(const std::string& id) {
  auto it = std::remove_if(library_.begin(), library_.end(), [&](const RtlLibraryEntry& e) { return e.moduleId == id; });
  if (it == library_.end()) return false;
  library_.erase(it, library_.end());
  InvalidateLibraryIndex();
  SaveGlobalLibrary();
  return true;
}

bool RtlProjectManager::DuplicateEntry(const std::string& id) {
  const auto* e = FindEntryById(id); if (!e) return false;
  RtlLibraryEntry c = *e;
  c.displayName = BuildUniqueDisplayName(c.displayName);
  c.moduleId = BuildModuleId(c.displayName, library_.size());
  c.buildSuccess = c.componentEnabled = false;
  library_.push_back(c);
  InvalidateLibraryIndex();
  SaveGlobalLibrary();
  return true;
}

std::string RtlProjectManager::ExportSourceFile(const std::string& id, const std::string& path) const {
  const auto* e = FindEntryById(id); if (!e) return "";
  for (const auto& f : e->sourceFiles) { if (f.path == path) return f.content; } return "";
}

bool RtlProjectManager::ImportSourceFile(const std::string& id, const std::string& path, const std::string& content) {
  auto* e = FindEntryById(id); if (!e || path.empty()) return false;
  for (auto& f : e->sourceFiles) {
    if (f.path == path) { if (f.content == content) return true; f.content = content; e->analyzeSuccess = e->buildSuccess = false; SaveGlobalLibrary(); return true; }
  }
  e->sourceFiles.push_back({path, content}); e->analyzeSuccess = e->buildSuccess = false; SaveGlobalLibrary(); return true;
}

bool RtlProjectManager::RemoveSourceFile(const std::string& id, const std::string& path) {
  auto* e = FindEntryById(id); if (!e || path.empty() || e->sourceFiles.size() <= 1) return false;
  auto it = std::remove_if(e->sourceFiles.begin(), e->sourceFiles.end(), [&](const RtlSourceFile& f) { return f.path == path; });
  if (it == e->sourceFiles.end()) return false;
  e->sourceFiles.erase(it, e->sourceFiles.end()); if (e->topFile == path) e->topFile = e->sourceFiles[0].path;
  e->analyzeSuccess = e->buildSuccess = false; return true;
}

bool RtlProjectManager::RenameSourceFile(const std::string& id, const std::string& oldP, const std::string& newP) {
  auto* e = FindEntryById(id); if (!e || oldP.empty() || newP.empty() || oldP == newP) return false;
  for (const auto& f : e->sourceFiles) { if (f.path == newP) return false; }
  for (auto& f : e->sourceFiles) {
    if (f.path == oldP) { f.path = newP; if (e->topFile == oldP) e->topFile = newP; e->analyzeSuccess = e->buildSuccess = false; return true; }
  }
  return false;
}

bool RtlProjectManager::UpdateSourceFile(const std::string& id, const std::string& path, const std::string& content) { return ImportSourceFile(id, path, content); }

bool RtlProjectManager::Analyze(const std::string& id) {
  auto* e = FindEntryById(id); if (!e) return false;
  e->ports.clear(); e->diagnostics.clear(); e->analyzeSuccess = false;
  if (!AnalyzeWithRegex(e)) {
    e->diagnostics = "Port parse failed.";
    LocalCapEntryLogs(e);
    return false;
  }
  auto s = DetectToolchain(); bool ok = true;
  if (s.verilatorFound) {
    std::string d; ok = RunVerilatorLint(*e, s, &d); if (!d.empty()) e->diagnostics = d;
    else if (!ok) e->diagnostics = "Lint failed with no output.";
  } else e->diagnostics = "Lint skipped: " + s.description;
  LocalCapEntryLogs(e);
  e->analyzeSuccess = !e->ports.empty() && ok; if (e->analyzeSuccess) SaveGlobalLibrary();
  return e->analyzeSuccess;
}

bool RtlProjectManager::Build(const std::string& id) {
  auto* e = FindEntryById(id); if (!e) return false;
  e->buildSuccess = false; e->buildDiagnostics.clear();
  auto s = DetectToolchain(); if (!s.doctor.ready) { e->buildDiagnostics = s.description; LocalCapEntryLogs(e); return false; }
  if (!e->analyzeSuccess) { e->buildDiagnostics = "Analyze first."; LocalCapEntryLogs(e); return false; }
  e->buildSuccess = RunVerilatorBuild(e, s, &e->buildDiagnostics);
  LocalCapEntryLogs(e);
  if (e->buildSuccess) SaveGlobalLibrary();
  return e->buildSuccess;
}

bool RtlProjectManager::RunTestbench(const std::string& id) {
  auto* e = FindEntryById(id);
  if (!e) return false;

  e->testbenchSuccess = false;
  e->testbenchDiagnostics.clear();
  e->testbenchBuildLog.clear();
  e->testbenchRunLog.clear();
  e->testbenchVcdPath.clear();

  // ✅ TB 파일 지정 여부 먼저 확인
  if (e->testbenchTopFile.empty()) {
    e->testbenchDiagnostics =
        "No testbench file set. Right-click a source file and select 'Set As TB'.";
    LocalCapEntryLogs(e);
    return false;
  }

  // ✅ testbenchTopModule 자동 추출
  if (e->testbenchTopModule.empty()) {
    std::string tbContent;
    for (const auto& f : e->sourceFiles) {
      if (f.path == e->testbenchTopFile) { tbContent = f.content; break; }
    }
    if (!tbContent.empty()) {
      std::string stripped = LocalStripComments(tbContent);  // 버그 수정 후 사용
      std::regex modRe("module\\s+([a-zA-Z_][a-zA-Z0-9_$]*)\\s*[\\(;]",
                       std::regex::icase);
      std::smatch m;
      if (std::regex_search(stripped, m, modRe)) {
        e->testbenchTopModule = m[1].str();
      }
    }
    if (e->testbenchTopModule.empty()) {
      e->testbenchDiagnostics = "Could not determine testbench top module name.";
      LocalCapEntryLogs(e);
      return false;
    }
  }

  auto s = DetectToolchain();
  if (!s.doctor.ready) {
    e->testbenchDiagnostics = s.description;
    LocalCapEntryLogs(e);
    return false;
  }
  e->testbenchSuccess = RunVerilatorTestbench(e, s, &e->testbenchDiagnostics);
  LocalCapEntryLogs(e);
  return e->testbenchSuccess;
}

RtlToolchainStatus RtlProjectManager::DetectToolchain() const {
  RtlToolchainStatus status;
  status.doctor = plc_emulator::WslToolchain::Diagnose(plc_emulator::WslToolchain::LoadProfile());
  for (const auto& check : status.doctor.checks) {
    if (check.name == "WSL2 distribution") status.wslFound = check.passed;
    if (check.name == "Verilator") status.verilatorFound = check.passed;
    if (check.name == "MinGW Windows compiler") status.compilerFound = check.passed;
    if (check.name == "make") status.makeFound = check.passed;
  }
  status.wslVerilatorFound = status.verilatorFound;
  status.wslCompilerFound = status.compilerFound;
  status.wslMakeFound = status.makeFound;
  status.wslPath = "wsl.exe";
  status.verilatorPath = status.wslVerilatorPath = "/usr/bin/verilator";
  status.compilerPath = status.wslCompilerPath = "/usr/bin/x86_64-w64-mingw32-g++-posix";
  status.makePath = status.wslMakePath = "/usr/bin/make";
  status.description = status.doctor.ready ? "WSL2 RTL toolchain ready."
      : "Open Tool Doctor to select WSL2 and repair missing tools.";
  return status;
}

bool RtlProjectManager::HasBuildArtifact(const RtlLibraryEntry& e) const {
  if (e.buildHash.empty()) return false;
  std::ifstream file(std::filesystem::path(plc_emulator::WslToolchain::ToWide(
      LocalJoinPath(GetBuildCacheDirectory(e), "rtl_worker.exe"))), std::ios::binary);
  char header[64] = {};
  file.read(header, sizeof(header));
  if (file.gcount() != 64 || header[0] != 'M' || header[1] != 'Z') return false;
  uint32_t offset = 0;
  std::memcpy(&offset, header + 60, sizeof(offset));
  if (offset > 1024 * 1024) return false;
  file.seekg(offset);
  char signature[4] = {};
  file.read(signature, sizeof(signature));
  return file.gcount() == 4 && std::memcmp(signature, "PE\0\0", 4) == 0;
}

std::string RtlProjectManager::GetBuildCacheDirectory(const RtlLibraryEntry& e) const {
  if (e.buildHash.empty()) return "";
  return LocalJoinPath(BuildCacheRootDirectory(), e.buildHash);
}

std::string RtlProjectManager::GetWslBuildCacheDirectory(const RtlLibraryEntry& e) const {
  std::string d = GetBuildCacheDirectory(e); return d.empty() ? "" : LocalWinPathToWslPath(d);
}

std::string RtlProjectManager::GetTestbenchCacheDirectory(const RtlLibraryEntry& e) const {
  if (e.testbenchTopModule.empty()) return "";
  std::ostringstream ss; ss << "wsl2-tb-v1|" << e.testbenchTopModule << "|" << e.testbenchTopFile;
  for (const auto& f : e.sourceFiles) ss << "|" << f.path << "|" << f.content;
  return LocalJoinPath(BuildCacheRootDirectory(), LocalHashHex(ss.str()));
}

std::string RtlProjectManager::GetWslTestbenchCacheDirectory(const RtlLibraryEntry& e) const {
  std::string d = GetTestbenchCacheDirectory(e); return d.empty() ? "" : LocalWinPathToWslPath(d);
}

std::string RtlProjectManager::GetWorkerLaunchCommand(const RtlLibraryEntry& entry, const RtlToolchainStatus& status) const {
  (void)status;
  if (!HasBuildArtifact(entry)) return {};
  return LocalEscape(LocalJoinPath(GetBuildCacheDirectory(entry), "rtl_worker.exe"));
}

std::string RtlProjectManager::SerializeLibraryJson() const {
  json r; r["version"] = 1; json ents = json::array();
  for (const auto& e : library_) {
    ents.push_back(LocalSerializeEntry(e, true));
  }
  r["entries"] = ents; return r.dump(2, ' ', false, json::error_handler_t::replace);
}

std::string RtlProjectManager::SerializeProjectRtlJson(
    const std::vector<std::string>& module_ids) const {
  json result = {{"version", 1}, {"entries", json::array()}};
  for (const auto& entry : library_) {
    if (std::find(module_ids.begin(), module_ids.end(), entry.moduleId) !=
        module_ids.end()) {
      result["entries"].push_back(LocalSerializeEntry(entry, true));
    }
  }
  return result.dump(2, ' ', false, json::error_handler_t::replace);
}

std::string RtlProjectManager::SerializeEntryJson(const RtlLibraryEntry& entry,
                                                 bool includeSourceFiles) const {
  return LocalSerializeEntry(entry, includeSourceFiles)
      .dump(2, ' ', false, json::error_handler_t::replace);
}

bool RtlProjectManager::DeserializeLibraryJson(const std::string& jText, bool merge) {
  json r = json::parse(jText, nullptr, false); if (r.is_discarded() || !r.is_object()) return false;
  if (!merge) {
    library_.clear();
    InvalidateLibraryIndex();
  }
  auto it = r.find("entries");
  if (it == r.end() || !it->is_array()) return true;
  for (const auto& i : *it) {
    RtlLibraryEntry e;
    if (!LocalDeserializeEntry(i, &e)) {
      continue;
    }
    // Project reloads keep module IDs stable instead of cloning the library.
    if (merge) {
      RtlLibraryEntry* existing = FindEntryById(e.moduleId);
      if (existing) {
        e.displayName = BuildUniqueDisplayName(e.displayName, e.moduleId);
        *existing = std::move(e);
        continue;
      }
    }
    e.displayName = BuildUniqueDisplayName(e.displayName);
    if (e.moduleId.empty() || std::any_of(library_.begin(), library_.end(),
                                          [&](const RtlLibraryEntry& existing) {
                                            return existing.moduleId == e.moduleId;
                                          })) {
      size_t ordinal = library_.size();
      do {
        e.moduleId = BuildModuleId(e.displayName, ordinal++);
      } while (std::any_of(library_.begin(), library_.end(),
                           [&](const RtlLibraryEntry& existing) {
                             return existing.moduleId == e.moduleId;
                           }));
    }
    library_.push_back(e);
    InvalidateLibraryIndex();
  }
  InvalidateLibraryIndex();
  if (merge) SaveGlobalLibrary();
  return true;
}

std::string RtlProjectManager::BuildUniqueDisplayName(
    const std::string& displayName,
    const std::string& ignoredModuleId) const {
  std::string desiredName = LocalTrim(displayName);
  if (desiredName.empty()) {
    desiredName = "RTL Module";
  }

  const bool hasExactConflict = std::any_of(
      library_.begin(), library_.end(),
      [&](const RtlLibraryEntry& entry) {
        return entry.moduleId != ignoredModuleId &&
               LocalEqualsIgnoreCase(entry.displayName, desiredName);
      });
  if (!hasExactConflict) {
    return desiredName;
  }

  std::string familyBase;
  LocalSplitDisplayNameSuffix(desiredName, &familyBase, nullptr);
  if (familyBase.empty()) {
    familyBase = desiredName;
  }

  std::set<int> usedSuffixes;
  for (const RtlLibraryEntry& entry : library_) {
    if (entry.moduleId == ignoredModuleId) {
      continue;
    }

    int suffix = 0;
    if (!LocalDisplayNameBelongsToFamily(entry.displayName, familyBase,
                                         &suffix)) {
      continue;
    }
    if (suffix > 0) {
      usedSuffixes.insert(suffix);
    }
  }

  int nextSuffix = 1;
  while (usedSuffixes.count(nextSuffix) > 0) {
    ++nextSuffix;
  }
  return familyBase + " (" + std::to_string(nextSuffix) + ")";
}

bool RtlProjectManager::DeserializeEntryJson(const std::string& jsonText,
                                             RtlLibraryEntry* entry) const {
  json item = json::parse(jsonText, nullptr, false);
  if (item.is_discarded() || !item.is_object()) {
    return false;
  }
  return LocalDeserializeEntry(item, entry);
}

std::map<std::string, std::string> RtlProjectManager::GetBuildArtifactBundle(
    const std::string& id) const {
  std::map<std::string, std::string> files;
  const auto* e = FindEntryById(id);
  if (!e || !HasBuildArtifact(*e)) return files;

  const std::string cacheDir = GetBuildCacheDirectory(*e);
  if (cacheDir.empty()) return files;

  const std::string workerPath = LocalJoinPath(cacheDir, "rtl_worker.exe");
  std::string workerData = LocalReadFileContent(workerPath);
  if (workerData.empty()) return files;
  files["rtl_worker.exe"] = std::move(workerData);
  // Older native bundles may depend on these MinGW runtime DLLs.
  for (const char* name : {"libgcc_s_seh-1.dll", "libstdc++-6.dll",
                           "libwinpthread-1.dll"}) {
    const std::string data = LocalReadFileContent(LocalJoinPath(cacheDir, name));
    if (!data.empty()) files[name] = data;
  }

  return files;
}

bool RtlProjectManager::RestoreBuildArtifactBundle(
    const std::string& id,
    const std::map<std::string, std::string>& files) {
  auto* e = FindEntryById(id);
  if (!e || files.empty()) return false;
  if (e->buildHash.empty()) e->buildHash = LocalHashHex("art-" + id);

  const std::string cacheDir = GetBuildCacheDirectory(*e);
  if (cacheDir.empty() || !LocalEnsureDirRecursive(cacheDir)) return false;

  const auto worker = files.find("rtl_worker.exe");
  if (worker == files.end() || !LocalIsWindowsWorker(worker->second)) {
    e->buildSuccess = false;
    e->buildDiagnostics = "Linux workers require a Windows MinGW rebuild.";
    return false;
  }
  const bool restoredWorker = LocalWriteFileContent(LocalJoinPath(cacheDir, "rtl_worker.exe"), worker->second);

  if (restoredWorker) {
    for (const char* name : {"libgcc_s_seh-1.dll", "libstdc++-6.dll",
                             "libwinpthread-1.dll"}) {
      const auto dependency = files.find(name);
      if (dependency != files.end() &&
          !LocalWriteFileContent(LocalJoinPath(cacheDir, name), dependency->second)) {
        e->buildSuccess = false;
        return false;
      }
    }
    e->buildSuccess = true;
  }
  return restoredWorker;
}

std::string RtlProjectManager::GetBuildArtifactData(const std::string& id) const {
  auto files = GetBuildArtifactBundle(id);
  auto it = files.find("rtl_worker.exe");
  return it == files.end() ? std::string() : it->second;
}

bool RtlProjectManager::RestoreBuildArtifact(const std::string& id, const std::string& data) {
  if (data.empty()) return false;
  return RestoreBuildArtifactBundle(id, {{"rtl_worker.exe", data}});
}

  std::vector<Port> RtlProjectManager::BuildRuntimePorts(
      const std::vector<RtlPortDescriptor>& ports) {
  std::vector<Port> runtimePorts;
  runtimePorts.reserve(ports.size());

  int inputCount = 0;
  int outputCount = 0;
  for (const auto& port : ports) {
    if (port.isInput) {
      ++inputCount;
    } else {
      ++outputCount;
    }
  }

  int inputIndex = 0;
  int outputIndex = 0;
  const int maxSideCount = std::max(inputCount, outputCount);
  const float blockWidth = 260.0f;
  const float topMargin = 52.0f;
  const float usableHeight =
      std::max(64.0f, 18.0f * static_cast<float>(std::max(1, maxSideCount - 1)));
  const float inputSpacing =
      inputCount > 1 ? usableHeight / static_cast<float>(inputCount - 1) : 0.0f;
  const float outputSpacing =
      outputCount > 1 ? usableHeight / static_cast<float>(outputCount - 1) : 0.0f;
  for (const auto& port : ports) {
    Port runtimePort;
    runtimePort.id = port.portId;
    runtimePort.color = LocalGetRtlPortColor(port.isInput);
    runtimePort.isInput = port.isInput;
    runtimePort.type = PortType::ELECTRIC;
    runtimePort.role = port.pinName;
    if (port.isInput) {
      float y = topMargin;
      if (inputCount > 1) {
        y += inputSpacing * static_cast<float>(inputIndex);
      } else {
        y += usableHeight * 0.5f;
      }
      runtimePort.relativePos = Position(0.0f, y);
      ++inputIndex;
    } else {
      float y = topMargin;
      if (outputCount > 1) {
        y += outputSpacing * static_cast<float>(outputIndex);
      } else {
        y += usableHeight * 0.5f;
      }
      runtimePort.relativePos = Position(blockWidth, y);
      ++outputIndex;
    }
    runtimePorts.push_back(runtimePort);
  }
  return runtimePorts;
}

std::vector<RtlPinBinding> RtlProjectManager::BuildDefaultPinBindings(const std::vector<RtlPortDescriptor>& pts) {
  std::vector<RtlPinBinding> b; for (const auto& p : pts) b.push_back({p.pinName, p.portId, p.isInput}); return b;
}

bool RtlProjectManager::AnalyzeWithRegex(RtlLibraryEntry* entry) const {
  if (!entry) {
    return false;
  }
  std::string topContent;
  for (const auto& file : entry->sourceFiles) {
    if (file.path == entry->topFile || topContent.empty()) {
      topContent = file.content;
      if (file.path == entry->topFile) {
        break;
      }
    }
  }
  if (topContent.empty()) {
    return false;
  }
  topContent = LocalStripComments(topContent);
  std::string moduleName = entry->topModule;
  if (moduleName.empty()) {
    std::regex firstModuleRegex(
        "module\\s+([a-zA-Z_][a-zA-Z0-9_$]*)\\s*\\(",
        std::regex::icase);
    std::smatch firstModuleMatch;
    if (std::regex_search(topContent, firstModuleMatch, firstModuleRegex)) {
      moduleName = firstModuleMatch[1].str();
      entry->topModule = moduleName;
    } else {
      return false;
    }
  }

  std::regex moduleRegex("module\\s+" + moduleName +
                             "\\s*\\(([\\s\\S]*?)\\);",
                         std::regex::icase);
  std::smatch moduleMatch;
  if (!std::regex_search(topContent, moduleMatch, moduleRegex)) {
    std::regex firstModuleRegex(
        "module\\s+([a-zA-Z_][a-zA-Z0-9_$]*)\\s*\\(([\\s\\S]*?)\\);",
        std::regex::icase);
    if (std::regex_search(topContent, moduleMatch, firstModuleRegex)) {
      entry->topModule = moduleMatch[1].str();
    } else {
      return false;
    }
  }

  std::string portsText =
      moduleMatch.size() >= 3 ? moduleMatch[2].str() : moduleMatch[1].str();
  std::vector<std::string> declarations = LocalSplitPortDeclarations(portsText);
  std::regex declRegex(
      "^\\s*(input|output|inout)\\s+"
      "(?:(?:wire|reg|logic)\\s+)?"
      "(?:\\[\\s*(\\d+)\\s*:\\s*(\\d+)\\s*\\]\\s+)?"
      "([a-zA-Z_][a-zA-Z0-9_$]*)\\s*$",
      std::regex::icase);

  int portId = 0;
  for (const auto& declaration : declarations) {
    std::smatch declMatch;
    if (!std::regex_match(declaration, declMatch, declRegex)) {
      continue;
    }
    std::string direction = declMatch[1].str();
    if (LocalEqualsIgnoreCase(direction, "inout")) {
      entry->diagnostics =
          "inout ports are not supported in v1. Remove or split them into input/output pins.";
      return false;
    }
    bool isInput = LocalEqualsIgnoreCase(direction, "input");
    int left = -1;
    int right = -1;
    if (declMatch[2].matched && declMatch[3].matched) {
      left = std::stoi(declMatch[2].str());
      right = std::stoi(declMatch[3].str());
    }
    std::string name = declMatch[4].str();
    if (left >= 0 && right >= 0) {
      int step = left <= right ? 1 : -1;
      for (int bit = left;; bit += step) {
        RtlPortDescriptor desc;
        desc.baseName = name;
        desc.bitIndex = bit;
        desc.width = std::abs(left - right) + 1;
        desc.pinName = name + "[" + std::to_string(bit) + "]";
        desc.isInput = isInput;
        desc.isClock = (name == "clk" || name == "clock");
        desc.isReset = (name == "rst" || name == "reset" || name == "rst_n");
        desc.portId = portId++;
        entry->ports.push_back(desc);
        if (bit == right) {
          break;
        }
      }
    } else {
      RtlPortDescriptor desc;
      desc.baseName = name;
      desc.pinName = name;
      desc.width = 1;
      desc.isInput = isInput;
      desc.isClock = (name == "clk" || name == "clock");
      desc.isReset = (name == "rst" || name == "reset" || name == "rst_n");
      desc.portId = portId++;
      entry->ports.push_back(desc);
    }
  }

  return !entry->ports.empty();
}

bool RtlProjectManager::RunVerilatorLint(const RtlLibraryEntry& entry,
                                         const RtlToolchainStatus& status,
                                         std::string* diagnostics) const {
  const std::string directory = LocalGetTempBasePath("plc_rtl_lint") + "\\" +
                                LocalHashHex(entry.moduleId);
  plc_emulator::WslCommand command;
  command.distribution =
      plc_emulator::WslToolchain::ToWide(status.doctor.profile.distribution);
  command.working_directory =
      plc_emulator::WslToolchain::ToWide(LocalWinPathToWslPath(directory));
  command.program = L"/usr/bin/verilator";
  command.arguments = {L"--lint-only",
                       L"-Wno-fatal",
                       L"-Wno-TIMESCALEMOD",
                       L"--timing",
                       L"--top-module",
                       plc_emulator::WslToolchain::ToWide(entry.topModule),
                       L"-Isrc"};
  if (!LocalWriteWslSources(entry, directory, &command.arguments, diagnostics))
    return false;
  const auto result =
      plc_emulator::WslToolchain::Run(command, kLocalDefaultTimeoutMs);
  if (diagnostics)
    *diagnostics = result.output + "\n" + result.error;
  return result.Succeeded();
}

bool RtlProjectManager::RunVerilatorBuild(RtlLibraryEntry* e,
                                          const RtlToolchainStatus& s,
                                          std::string* diag) const {
#ifdef _WIN32
  {
    std::ostringstream hash_source;
    hash_source << "wsl2-mingw-static-worker-v1|"
                << s.doctor.profile.distribution << "|" << e->moduleId << "|"
                << e->topModule;
    for (const auto& source : e->sourceFiles)
      hash_source << "|" << source.path << "|" << source.content;
    e->buildHash = LocalHashHex(hash_source.str());
  }

  std::string d = GetBuildCacheDirectory(*e);
  if (d.empty()) {
    if (diag)
      *diag = "[Build] Cache path generation failed: buildHash is empty.";
    return false;
  }

  std::string sD = LocalJoinPath(d, "src"), oD = LocalJoinPath(d, "obj");
  LocalEnsureDirRecursive(d);
  LocalEnsureDirRecursive(sD);
  LocalEnsureDirRecursive(oD);

  std::string wm = LocalJoinPath(d, "worker.cpp");
  std::ostringstream code;
  code << "#include \"V" << e->topModule << ".h\"\n"
       << "#include <verilated.h>\n"
       << "#include <iostream>\n#include <sstream>\n#include "
          "<string>\n#include <cstdint>\n\n"
       << "static vluint64_t g_main_time = 0;\n"
       << "double sc_time_stamp() { return static_cast<double>(g_main_time); "
          "}\n\n"
       << "int main(int argc, char** argv) {\n"
       << "  Verilated::commandArgs(argc, argv);\n"
       << "  V" << e->topModule << " top;\n"
       << "  std::string line;\n"
       << "  while (std::getline(std::cin, line)) {\n"
       << "    if (line == \"QUIT\") break;\n"
       << "    if (line.rfind(\"EVAL\", 0) == 0) {\n"
       << "      std::istringstream ss(line.substr(4));\n"
       << "      std::string tok;\n"
       << "      while (ss >> tok) {\n"
       << "        auto eq = tok.find('=');\n"
       << "        if (eq == std::string::npos) continue;\n"
       << "        std::string pin = tok.substr(0, eq);\n"
       << "        int val = std::stoi(tok.substr(eq + 1));\n";

  for (const auto& port : e->ports) {
    if (port.isInput) {
      code << "        if (pin == \"" << port.pinName << "\") top."
           << port.pinName << " = val;\n";
    }
  }

  code << "      }\n"
       << "      top.eval();\n"
       << "      ++g_main_time;\n"
       << "      std::cout << \"OUT\";\n";

  for (const auto& port : e->ports) {
    if (!port.isInput) {
      code << "      std::cout << \" \" << \"" << port.pinName
           << "=\" << (int)top." << port.pinName << ";\n";
    }
  }

  code << "      std::cout << std::endl;\n"
       << "    }\n"
       << "  }\n"
       << "  top.final(); return 0;\n"
       << "}\n";

  if (!LocalWriteFileContent(wm, code.str())) {
    if (diag)
      *diag = "Cannot write RTL worker driver.";
    return false;
  }
  plc_emulator::WslCommand command;
  command.distribution =
      plc_emulator::WslToolchain::ToWide(s.doctor.profile.distribution);
  command.working_directory =
      plc_emulator::WslToolchain::ToWide(GetWslBuildCacheDirectory(*e));
  command.program = L"/usr/bin/verilator";
  command.arguments = {L"--cc",
                       L"--exe",
                       L"--compiler",
                       L"gcc",
                       L"-Wno-fatal",
                       L"-Wno-TIMESCALEMOD",
                       L"--timing",
                       L"--top-module",
                       plc_emulator::WslToolchain::ToWide(e->topModule),
                       L"--Mdir",
                       L"obj",
                       L"-CFLAGS",
                       L"-std=c++20",
                       L"-LDFLAGS",
                       L"-static -pthread",
                       L"-o",
                       L"../rtl_worker.exe",
                       L"-Isrc"};
  if (!LocalWriteWslSources(*e, d, &command.arguments, diag))
    return false;
  command.arguments.push_back(L"worker.cpp");
  const auto generated =
      plc_emulator::WslToolchain::Run(command, kLocalBuildTimeoutMs);
  if (diag)
    *diag = generated.output + "\n" + generated.error;
  if (!generated.Succeeded())
    return false;
  command.program = L"/usr/bin/make";
  command.arguments = {
      L"-C",
      L"obj",
      L"-f",
      plc_emulator::WslToolchain::ToWide("V" + e->topModule + ".mk"),
      L"-j",
      std::to_wstring(s.doctor.profile.cpu_budget),
      L"CXX=/usr/bin/x86_64-w64-mingw32-g++-posix",
      L"LINK=/usr/bin/x86_64-w64-mingw32-g++-posix",
      L"AR=/usr/bin/x86_64-w64-mingw32-ar"};
  const auto built =
      plc_emulator::WslToolchain::Run(command, kLocalBuildTimeoutMs);
  if (diag)
    *diag += "\n" + built.output + "\n" + built.error;
  return built.Succeeded() && HasBuildArtifact(*e);
#else
  (void)e;
  (void)s;
  (void)diag;
#endif
  return false;
}

bool RtlProjectManager::RunVerilatorTestbench(RtlLibraryEntry* e,
                                              const RtlToolchainStatus& s,
                                              std::string* diag) const {
#ifdef _WIN32
  // ── 1. 캐시 디렉토리 준비 ──────────────────────────────────────
  std::string d = GetTestbenchCacheDirectory(*e);
  if (d.empty()) {
    if (diag)
      *diag = "Failed to get testbench cache directory.";
    return false;
  }
  std::string sD = LocalJoinPath(d, "src");
  std::string oD = LocalJoinPath(d, "obj");
  LocalEnsureDirRecursive(d);
  LocalEnsureDirRecursive(sD);
  LocalEnsureDirRecursive(oD);

  // ── 3. C++ 시뮬레이션 드라이버 생성 ─────────────────────────
  std::string vcdAbsPath = LocalToForwardSlashes(LocalJoinPath(d, "dump.vcd"));
  std::string tbMain = LocalJoinPath(d, "tb_main.cpp");

  std::ostringstream code;
  code << "#include \"V" << e->testbenchTopModule << ".h\"\n"
       << "#include <verilated.h>\n"
       << "#include <verilated_vcd_c.h>\n"
       << "#include <cstdint>\n\n"
       << "static uint64_t g_sim_time = 0;\n"
       << "double sc_time_stamp() { return (double)g_sim_time; }\n\n"
       << "int main(int argc, char** argv) {\n"
       << "  Verilated::commandArgs(argc, argv);\n"
       << "  V" << e->testbenchTopModule << " top;\n"
       << "  Verilated::traceEverOn(true);\n"
       << "  VerilatedVcdC vcd;\n"
       << "  top.trace(&vcd, 99);\n"
       << "  vcd.open(\"dump.vcd\");\n"
       // 최대 10M 스텝 (무한루프 방지)
       << "  const uint64_t kMaxSteps = 10000000ULL;\n"
       << "  while (!Verilated::gotFinish() && g_sim_time < kMaxSteps) {\n"
       << "    top.eval();\n"
       << "    vcd.dump(g_sim_time);\n"
       << "    ++g_sim_time;\n"
       << "  }\n"
       << "  top.final();\n"
       << "  vcd.flush(); vcd.close();\n"
       << "  return Verilated::gotFinish() ? 0 : 2;\n"
       << "}\n";
  if (!LocalWriteFileContent(tbMain, code.str())) {
    if (diag)
      *diag = "Cannot write testbench driver.";
    return false;
  }

  // ── 4. Verilator: C++ 코드 생성 ───────────────────────────────
  plc_emulator::WslCommand command;
  command.distribution =
      plc_emulator::WslToolchain::ToWide(s.doctor.profile.distribution);
  command.working_directory =
      plc_emulator::WslToolchain::ToWide(GetWslTestbenchCacheDirectory(*e));
  command.program = L"/usr/bin/verilator";
  command.arguments = {
      L"--cc",
      L"--exe",
      L"--build",
      L"-j",
      std::to_wstring(s.doctor.profile.cpu_budget),
      L"-Wno-fatal",
      L"-Wno-TIMESCALEMOD",
      L"--timing",
      L"--trace",
      L"--top-module",
      plc_emulator::WslToolchain::ToWide(e->testbenchTopModule),
      L"--Mdir",
      L"obj",
      L"-CFLAGS",
      L"-std=c++20",
      L"-o",
      L"../tb_sim",
      L"-Isrc"};
  if (!LocalWriteWslSources(*e, d, &command.arguments, diag))
    return false;
  command.arguments.push_back(L"tb_main.cpp");
  const auto result =
      plc_emulator::WslToolchain::Run(command, kLocalBuildTimeoutMs);
  e->testbenchBuildLog = result.output + "\n" + result.error;
  if (!result.Succeeded()) {
    if (diag)
      *diag = e->testbenchBuildLog;
    return false;
  }
  command.program = plc_emulator::WslToolchain::ToWide(
      GetWslTestbenchCacheDirectory(*e) + "/tb_sim");
  command.arguments.clear();
  const auto run_result =
      plc_emulator::WslToolchain::Run(command, kLocalDefaultTimeoutMs);
  const std::string runLog = run_result.output + "\n" + run_result.error;
  e->testbenchRunLog = runLog;
  const bool ran = run_result.Succeeded();
  // ── 7. 합격/실패 판정 ─────────────────────────────────────────
  auto HasKeyword = [&](const std::string& kw) {
    return runLog.find(kw) != std::string::npos;
  };
  bool hasFatal =
      HasKeyword("$fatal") || HasKeyword("%Fatal") || HasKeyword("Aborting");
  bool hasError = HasKeyword("$error") || HasKeyword("%Error") ||
                  HasKeyword("Assertion failed") ||
                  HasKeyword("assertion failed");

  if (hasFatal || hasError) {
    if (diag)
      *diag = "Testbench reported errors. Check Testbench Run Log.";
    return false;
  }
  if (!ran) {
    if (diag)
      *diag = "Testbench timed out or crashed. Check Testbench Run Log.";
    return false;
  }

  // VCD 파일 기록
  if (LocalFileExists(vcdAbsPath))
    e->testbenchVcdPath = vcdAbsPath;

  if (diag)
    *diag = "Testbench passed.";
  return true;
#else
  (void)e;
  (void)s;
  (void)diag;
  return false;
#endif
}

std::string RtlProjectManager::BuildModuleId(const std::string& name, size_t ord) {
  std::string s = Slugify(name); return (s.empty() ? "rtl" : s) + "_" + std::to_string(ord);
}

std::string RtlProjectManager::Slugify(const std::string& t) {
  std::string r; for (char ch : t) { if (std::isalnum(static_cast<unsigned char>(ch))) r += static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); else if (ch == ' ') r += '_'; } return r;
}

std::string RtlProjectManager::JoinTempFileArguments(const std::vector<std::string>& paths) {
  std::string r; for (const auto& p : paths) r += " " + LocalEscape(p); return r;
}

} // namespace plc

