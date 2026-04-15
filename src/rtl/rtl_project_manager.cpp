/*
 * rtl_project_manager.cpp
 *
 * RTL project library management and lightweight Verilog analysis helpers.
 */

#include "plc_emulator/rtl/rtl_project_manager.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <regex>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
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

// --- Internal Utilities (Private Linkage) ---

static bool LocalFileExists(const std::string& path) {
  if (path.empty()) return false;
#ifdef _WIN32
  DWORD attrs = GetFileAttributesA(path.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
  std::ifstream f(path.c_str()); return f.good();
#endif
}

static bool LocalPathExists(const std::string& path) {
  if (path.empty()) return false;
#ifdef _WIN32
  return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
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

#ifdef _WIN32
static std::wstring LocalToWide(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (n <= 0) return L"";
  std::wstring res(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &res[0], n);
  if (!res.empty() && res.back() == L'\0') res.pop_back();
  return res;
}
#endif

static bool LocalRunCommand(const std::string& cmd, std::string* out, DWORD timeout) {
  if (out) out->clear();
  if (cmd.empty()) return false;
#ifdef _WIN32
  SECURITY_ATTRIBUTES sa; memset(&sa, 0, sizeof(sa)); sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
  HANDLE rPipe = nullptr, wPipe = nullptr;
  if (!CreatePipe(&rPipe, &wPipe, &sa, 0)) return false;
  SetHandleInformation(rPipe, HANDLE_FLAG_INHERIT, 0);
  HANDLE nIn = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES; si.wShowWindow = SW_HIDE;
  si.hStdInput = (nIn != INVALID_HANDLE_VALUE ? nIn : nullptr); si.hStdOutput = si.hStdError = wPipe;
  PROCESS_INFORMATION pi; memset(&pi, 0, sizeof(pi));
  std::wstring wcmd = LocalToWide(cmd);
  if (!CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle(rPipe); CloseHandle(wPipe); if (nIn != INVALID_HANDLE_VALUE) CloseHandle(nIn);
    if (out) {
      DWORD err = GetLastError();
      char msg[512] = {0};
      FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, err, 0, msg, sizeof(msg), nullptr);
      *out = std::string("CreateProcess failed (") + std::to_string(err) + "): " + msg + "\nCommand: " + cmd;
    }
    return false;
  }
  CloseHandle(wPipe); if (nIn != INVALID_HANDLE_VALUE) CloseHandle(nIn);
  std::string captured; DWORD start = GetTickCount(); bool tO = false;
  while (true) {
    DWORD avail = 0;
    if (PeekNamedPipe(rPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
      std::vector<char> buf(avail); DWORD read = 0;
      if (ReadFile(rPipe, buf.data(), avail, &read, nullptr)) captured.append(buf.data(), read);
    }
    if (WaitForSingleObject(pi.hProcess, 50) != WAIT_TIMEOUT) break;
    if (GetTickCount() - start > timeout) { tO = true; TerminateProcess(pi.hProcess, 1); break; }
  }
  DWORD fDrainStart = GetTickCount();
  while (GetTickCount() - fDrainStart < 500) {
    DWORD avail = 0;
    if (PeekNamedPipe(rPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
      std::vector<char> buf(avail); DWORD read = 0;
      if (ReadFile(rPipe, buf.data(), avail, &read, nullptr)) captured.append(buf.data(), read);
    } else break;
  }
  DWORD exit = 1; GetExitCodeProcess(pi.hProcess, &exit);
  CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(rPipe);

#ifdef _WIN32
  {
    int wlen = MultiByteToWideChar(CP_OEMCP, 0, captured.c_str(), static_cast<int>(captured.size()), nullptr, 0);
    if (wlen > 0) {
      std::wstring wbuf(wlen, L'\0');
      MultiByteToWideChar(CP_OEMCP, 0, captured.c_str(), static_cast<int>(captured.size()), &wbuf[0], wlen);
      int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), wlen, nullptr, 0, nullptr, nullptr);
      if (ulen > 0) {
        std::string utf8(ulen, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), wlen, &utf8[0], ulen, nullptr, nullptr);
        captured = std::move(utf8);
      }
    }
  }
#endif

  if (out) {
    *out = LocalTrim(captured);
    if (tO) *out += "\nCommand timed out.";
    if (exit != 0 && out->empty()) {
      char hexBuf[32];
      snprintf(hexBuf, sizeof(hexBuf), "%lX", static_cast<unsigned long>(exit));
      *out = "Process crashed or exited silently with code " + std::to_string(exit) + " (0x" + hexBuf + ").\nIf the code is 0xC0000135, a required DLL (e.g., libintl-8.dll) is missing from the toolchain.";
    }
  }
  return !tO && exit == 0;
#else
  (void)timeout; return false;
#endif
}

static std::string LocalHashHex(const std::string& text) {
  uint64_t h = 1469598103934665603ULL;
  for (unsigned char ch : text) { h ^= static_cast<uint64_t>(ch); h *= 1099511628211ULL; }
  std::ostringstream ss; ss << std::hex << std::setfill('0') << std::setw(16) << h;
  return ss.str();
}

static std::string LocalReadToolchain(const std::string& path, const std::string& key) {
  std::ifstream in(path); if (!in.is_open()) return "";
  std::string prefix = "set(" + key + " \"", line;
  while (std::getline(in, line)) {
    size_t pos = line.find(prefix); if (pos == std::string::npos) continue;
    size_t start = pos + prefix.size(), end = line.find('"', start);
    if (end == std::string::npos) continue;
    std::string val = line.substr(start, end - start), unesc;
    for (size_t i = 0; i < val.size(); ++i) {
      if (val[i] == '\\' && i + 1 < val.size() && val[i+1] == '\\') { unesc += '\\'; i++; }
      else unesc += val[i];
    }
    return unesc;
  }
  return "";
}

static std::string LocalFirstExisting(const std::vector<std::string>& paths, bool allowDir = false) {
  for (const auto& p : paths) {
    if (!p.empty() && (allowDir ? LocalPathExists(p) : LocalFileExists(p))) return p;
  }
  return "";
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
  std::ofstream f(path, std::ios::binary); if (!f.is_open()) return false;
  f << content; return true;
}

static std::string LocalGetTempBasePath(const std::string& suffix) {
#ifdef _WIN32
  char buf[MAX_PATH] = {0}; DWORD len = GetTempPathA(MAX_PATH, buf);
  if (len > 0 && len < MAX_PATH) {
    std::string p(buf, len); if (!p.empty() && p.back() != '\\' && p.back() != '/') p += "\\";
    p += suffix; return p;
  }
#endif
  return suffix;
}

static bool LocalEnsureDirRecursive(const std::string& path) {
  if (path.empty()) return false;
  std::string norm = path; std::replace(norm.begin(), norm.end(), '/', '\\');
#ifdef _WIN32
  size_t pos = norm.find('\\', 3);
  while (pos != std::string::npos) { CreateDirectoryA(norm.substr(0, pos).c_str(), nullptr); pos = norm.find('\\', pos + 1); }
  if (CreateDirectoryA(norm.c_str(), nullptr) != 0) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
#else
  return true;
#endif
}

static std::string GetLocalAppDataPathInternal() {
#ifdef _WIN32
  char buf[MAX_PATH] = {0}; if (GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH) > 0) return std::string(buf);
#endif
  return ".";
}

static std::string BuildCacheRootDirectory() {
  return LocalJoinPath(GetLocalAppDataPathInternal(), "PLCSimulator\\rtl_cache");
}

} // anonymous namespace

// --- Public Function ---
bool FileExists(const std::string& path) { return LocalFileExists(path); }

// --- RtlProjectManager Implementation ---

RtlProjectManager::RtlProjectManager() { LoadGlobalLibrary(); }
RtlProjectManager::~RtlProjectManager() { SaveGlobalLibrary(); }

void RtlProjectManager::LoadGlobalLibrary() {
  std::ifstream f(LocalJoinPath(GetLocalAppDataPathInternal(), "PLCSimulator\\rtl_library.json"), std::ios::binary);
  if (!f.is_open()) return;
  std::ostringstream ss; ss << f.rdbuf();
  if (!ss.str().empty()) DeserializeLibraryJson(ss.str());
}

void RtlProjectManager::SaveGlobalLibrary() const {
  std::string p = LocalJoinPath(GetLocalAppDataPathInternal(), "PLCSimulator\\rtl_library.json");
  size_t pos = p.find_last_of("\\/");
  if (pos != std::string::npos) LocalEnsureDirRecursive(p.substr(0, pos));
  std::ofstream f(p, std::ios::binary); if (f.is_open()) f << SerializeLibraryJson();
}

RtlLibraryEntry* RtlProjectManager::FindEntryById(const std::string& id) {
  for (auto& e : library_) { if (e.moduleId == id) return &e; } return nullptr;
}

const RtlLibraryEntry* RtlProjectManager::FindEntryById(const std::string& id) const {
  for (const auto& e : library_) { if (e.moduleId == id) return &e; } return nullptr;
}

RtlLibraryEntry* RtlProjectManager::CreateEntry(const std::string& name) {
  RtlLibraryEntry e; e.displayName = name.empty() ? "RTL Module" : name;
  e.moduleId = BuildModuleId(e.displayName, library_.size());
  e.topModule = Slugify(e.displayName); if (e.topModule.empty()) e.topModule = "rtl_top";
  e.topFile = e.topModule + ".v";
  e.sourceFiles.push_back({e.topFile, "module " + e.topModule + "(\n  input clk,\n  input rst,\n  output out\n);\nassign out = 0;\nendmodule\n"});
  library_.push_back(e); SaveGlobalLibrary(); return &library_.back();
}

bool RtlProjectManager::DeleteEntry(const std::string& id) {
  auto it = std::remove_if(library_.begin(), library_.end(), [&](const RtlLibraryEntry& e) { return e.moduleId == id; });
  if (it == library_.end()) return false;
  library_.erase(it, library_.end()); SaveGlobalLibrary(); return true;
}

bool RtlProjectManager::DuplicateEntry(const std::string& id) {
  const auto* e = FindEntryById(id); if (!e) return false;
  RtlLibraryEntry c = *e; c.moduleId = BuildModuleId(c.displayName, library_.size());
  c.displayName += " Copy"; c.buildSuccess = c.componentEnabled = false;
  library_.push_back(c); SaveGlobalLibrary(); return true;
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
  if (!AnalyzeWithRegex(e)) { e->diagnostics = "Port parse failed."; return false; }
  auto s = DetectToolchain(); bool ok = true;
  if (s.verilatorFound) {
    std::string d; ok = RunVerilatorLint(*e, s, &d); if (!d.empty()) e->diagnostics = d;
    else if (!ok) e->diagnostics = "Lint failed with no output.";
  } else e->diagnostics = "Lint skipped: " + s.description;
  e->analyzeSuccess = !e->ports.empty() && ok; if (e->analyzeSuccess) SaveGlobalLibrary();
  return e->analyzeSuccess;
}

bool RtlProjectManager::Build(const std::string& id) {
  auto* e = FindEntryById(id); if (!e) return false;
  e->buildSuccess = false; e->buildDiagnostics.clear();
  auto s = DetectToolchain(); if (!s.verilatorFound || !s.compilerFound) { e->buildDiagnostics = s.description; return false; }
  if (!e->analyzeSuccess) { e->buildDiagnostics = "Analyze first."; return false; }
  e->buildSuccess = RunVerilatorBuild(e, s, &e->buildDiagnostics);
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
      return false;
    }
  }

  auto s = DetectToolchain();
  if (!s.verilatorFound || !s.compilerFound) {
    e->testbenchDiagnostics = s.description;
    return false;
  }
  e->testbenchSuccess = RunVerilatorTestbench(e, s, &e->testbenchDiagnostics);
  return e->testbenchSuccess;
}

RtlToolchainStatus RtlProjectManager::DetectToolchain() const {
  static RtlToolchainStatus cached_st;
  static bool is_cached = false;
  if (is_cached) return cached_st;
  
  RtlToolchainStatus st;
#ifdef _WIN32
  auto GetExecDir = []() {
    char p[MAX_PATH] = {0}; GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s(p); size_t pos = s.find_last_of("\\/");
    return (pos != std::string::npos) ? s.substr(0, pos) : ".";
  };
  std::string tDir = LocalJoinPath(GetExecDir(), "tools");
  if (!LocalFileExists(LocalJoinPath(tDir, "setup_tools.ps1"))) tDir = "tools";
  st.toolsDir = tDir;
  std::string tcP = LocalJoinPath(tDir, "toolchain.cmake");

  const std::string kDefaultMsys2Root  = "C:\\msys64";
  const std::string kDefaultMsys2Bin   = "C:\\msys64\\ucrt64\\bin";
  const std::string kDefaultMsys2Share = "C:\\msys64\\ucrt64\\share\\verilator";
  const std::string kDefaultUsrBin     = "C:\\msys64\\usr\\bin";

  std::string tcMingwRoot = LocalReadToolchain(tcP, "MINGW_ROOT");
  std::replace(tcMingwRoot.begin(), tcMingwRoot.end(), '/', '\\');
  std::string tcMingwBin = tcMingwRoot.empty() ? "" : LocalJoinPath(tcMingwRoot, "bin");

  auto NormalizePath = [](const std::string& p) {
    std::string n;
    for (size_t i = 0; i < p.size(); ++i) {
      n += p[i];
      if (p[i] == '\\' && i + 1 < p.size() && p[i + 1] == '\\') ++i;
    }
    return n;
  };

  auto FindMsys2RootFromToolchain = [&](const std::string& root) -> std::string {
    if (root.empty()) return kDefaultMsys2Root;
    size_t pos = root.rfind("\\ucrt64");
    if (pos == std::string::npos) pos = root.rfind("\\mingw64");
    if (pos == std::string::npos) pos = root.rfind("\\clang64");
    if (pos == std::string::npos) return kDefaultMsys2Root;
    return root.substr(0, pos);
  };

  st.compilerPath = LocalFirstExisting({
      LocalReadToolchain(tcP, "CMAKE_CXX_COMPILER"),
      tcMingwBin.empty() ? "" : LocalJoinPath(tcMingwBin, "g++.exe"),
      LocalJoinPath(kDefaultMsys2Bin, "g++.exe")});

  st.makePath = LocalFirstExisting({
      LocalReadToolchain(tcP, "CMAKE_MAKE_PROGRAM"),
      tcMingwBin.empty() ? "" : LocalJoinPath(tcMingwBin, "mingw32-make.exe"),
      tcMingwBin.empty() ? "" : LocalJoinPath(FindMsys2RootFromToolchain(tcMingwRoot), "usr\\bin\\make.exe"),
      LocalJoinPath(kDefaultUsrBin, "make.exe"),
      LocalJoinPath(kDefaultMsys2Bin, "mingw32-make.exe")});

  st.verilatorPath = LocalFirstExisting({
      tcMingwBin.empty() ? "" : LocalJoinPath(tcMingwBin, "verilator_bin.exe"),
      tcMingwBin.empty() ? "" : LocalJoinPath(tcMingwBin, "verilator.exe"),
      LocalJoinPath(kDefaultMsys2Bin, "verilator_bin.exe"),
      LocalJoinPath(kDefaultMsys2Bin, "verilator.exe")});

  st.verilatorRoot = LocalFirstExisting({
      LocalReadToolchain(tcP, "VERILATOR_ROOT"),
      tcMingwRoot.empty() ? "" : LocalJoinPath(tcMingwRoot, "share\\verilator"),
      kDefaultMsys2Share
  }, true);

  st.makePath = NormalizePath(st.makePath);
  st.compilerPath = NormalizePath(st.compilerPath);
  st.verilatorPath = NormalizePath(st.verilatorPath);
  st.verilatorRoot = NormalizePath(st.verilatorRoot);

  st.mingwBinPath = !tcMingwBin.empty() && LocalPathExists(tcMingwBin)
                    ? tcMingwBin
                    : kDefaultMsys2Bin;
  st.msys2Root = FindMsys2RootFromToolchain(tcMingwRoot);

  st.compilerFound = !st.compilerPath.empty();
  st.makeFound = !st.makePath.empty();
  st.verilatorFound = !st.verilatorPath.empty();
  st.description = st.verilatorFound ? "Ready." : "Verilator not found.";
#else
  st.description = "Windows only.";
#endif
  cached_st = st;
  is_cached = true;
  return st;
}

bool RtlProjectManager::HasBuildArtifact(const RtlLibraryEntry& e) const {
  if (e.buildHash.empty()) return false;
  return LocalFileExists(LocalJoinPath(GetBuildCacheDirectory(e), "rtl_worker.exe"));
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
  std::ostringstream ss; ss << "tb-v1|" << e.testbenchTopModule << "|" << e.testbenchTopFile;
  for (const auto& f : e.sourceFiles) ss << "|" << f.path << "|" << f.content.size();
  return LocalJoinPath(BuildCacheRootDirectory(), LocalHashHex(ss.str()));
}

std::string RtlProjectManager::GetWslTestbenchCacheDirectory(const RtlLibraryEntry& e) const {
  std::string d = GetTestbenchCacheDirectory(e); return d.empty() ? "" : LocalWinPathToWslPath(d);
}

std::string RtlProjectManager::GetWorkerLaunchCommand(const RtlLibraryEntry& e, const RtlToolchainStatus& s) const {
  (void)s; if (!HasBuildArtifact(e)) return "";
  std::string exe = LocalJoinPath(GetBuildCacheDirectory(e), "rtl_worker.exe");
  return LocalFileExists(exe) ? LocalEscape(exe) : "";
}

std::string RtlProjectManager::SerializeLibraryJson() const {
  json r; r["version"] = 1; json ents = json::array();
  for (const auto& e : library_) {
    json i; i["module_id"] = e.moduleId; i["display_name"] = e.displayName;
    i["top_module"] = e.topModule; i["top_file"] = e.topFile;
    i["build_hash"] = e.buildHash; i["analyze_success"] = e.analyzeSuccess;
    i["build_success"] = e.buildSuccess;

    i["testbench_top_module"] = e.testbenchTopModule;
    i["testbench_top_file"] = e.testbenchTopFile;
    i["diagnostics"] = e.diagnostics;
    i["build_diagnostics"] = e.buildDiagnostics;
    i["testbench_diagnostics"] = e.testbenchDiagnostics;
    i["testbench_build_log"] = e.testbenchBuildLog;
    i["testbench_run_log"] = e.testbenchRunLog;
    i["testbench_vcd_path"] = e.testbenchVcdPath;
    i["testbench_success"] = e.testbenchSuccess;
    i["component_enabled"] = e.componentEnabled;

    json srcs = json::array(); for (const auto& f : e.sourceFiles) srcs.push_back({{"path", f.path}, {"content", f.content}});
    i["source_files"] = srcs;
    json pts = json::array(); for (const auto& p : e.ports) pts.push_back({{"pin_name", p.pinName}, {"base_name", p.baseName}, {"bit_index", p.bitIndex}, {"width", p.width}, {"is_input", p.isInput}, {"port_id", p.portId}});
    i["ports"] = pts; ents.push_back(i);
  }
  r["entries"] = ents; return r.dump(2, ' ', false, json::error_handler_t::replace);
}

std::string RtlProjectManager::SerializeProjectRtlJson() const { return SerializeLibraryJson(); }

bool RtlProjectManager::DeserializeLibraryJson(const std::string& jText, bool merge) {
  json r = json::parse(jText, nullptr, false); if (r.is_discarded() || !r.is_object()) return false;
  if (!merge) library_.clear();
  auto it = r.find("entries"); if (it == r.end() || !it->is_array()) return true;
  for (const auto& i : *it) {
    std::string id = i.value("module_id", ""); if (id.empty()) continue;
    RtlLibraryEntry e; e.moduleId = id; e.displayName = i.value("display_name", "RTL Module");
    e.topModule = i.value("top_module", ""); e.topFile = i.value("top_file", "");
    e.buildHash = i.value("build_hash", ""); e.analyzeSuccess = i.value("analyze_success", false);
    e.buildSuccess = i.value("build_success", false);

    e.testbenchTopModule = i.value("testbench_top_module", "");
    e.testbenchTopFile = i.value("testbench_top_file", "");
    e.diagnostics = i.value("diagnostics", "");
    e.buildDiagnostics = i.value("build_diagnostics", "");
    e.testbenchDiagnostics = i.value("testbench_diagnostics", "");
    e.testbenchBuildLog = i.value("testbench_build_log", "");
    e.testbenchRunLog = i.value("testbench_run_log", "");
    e.testbenchVcdPath = i.value("testbench_vcd_path", "");
    e.testbenchSuccess = i.value("testbench_success", false);
    e.componentEnabled = i.value("component_enabled", false);

    auto sIt = i.find("source_files"); if (sIt != i.end() && sIt->is_array()) {
      for (const auto& s : *sIt) e.sourceFiles.push_back({s.value("path", ""), s.value("content", "")});
    }
    auto pIt = i.find("ports"); if (pIt != i.end() && pIt->is_array()) {
      for (const auto& p : *pIt) {
        RtlPortDescriptor d; d.pinName = p.value("pin_name", ""); d.baseName = p.value("base_name", "");
        d.bitIndex = p.value("bit_index", -1); d.width = p.value("width", 1);
        d.isInput = p.value("is_input", true); d.portId = p.value("port_id", -1); e.ports.push_back(d);
      }
    }
    library_.push_back(e);
  }
  if (merge) SaveGlobalLibrary();
  return true;
}

std::string RtlProjectManager::GetBuildArtifactData(const std::string& id) const {
  const auto* e = FindEntryById(id); if (!e || !HasBuildArtifact(*e)) return "";
  std::ifstream f(LocalJoinPath(GetBuildCacheDirectory(*e), "rtl_worker.exe"), std::ios::binary);
  std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

bool RtlProjectManager::RestoreBuildArtifact(const std::string& id, const std::string& data) {
  auto* e = FindEntryById(id); if (!e || data.empty()) return false;
  if (e->buildHash.empty()) e->buildHash = LocalHashHex("art-" + id);
  std::string d = GetBuildCacheDirectory(*e); LocalEnsureDirRecursive(d);
  std::ofstream f(LocalJoinPath(d, "rtl_worker.exe"), std::ios::binary);
  if (f.is_open()) { f << data; e->buildSuccess = true; return true; } return false;
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

bool RtlProjectManager::RunCommandCapture(const std::string& cmd, std::string* out, const RtlToolchainStatus& s, unsigned int tO) const {
#ifdef _WIN32
  char oP[32768]; DWORD n = GetEnvironmentVariableA("PATH", oP, 32768);
  std::string prev = (n > 0 && n < 32768) ? std::string(oP, n) : "";
  // MSYS2 usr/bin (perl, python, sh 등)도 함께 포함
  std::string msys2UsrBin = LocalJoinPath(s.msys2Root, "usr\\bin");
  std::string eff = s.mingwBinPath;
  if (!msys2UsrBin.empty() && LocalPathExists(msys2UsrBin)) {
    eff += ";" + msys2UsrBin;
  }
  if (!prev.empty()) eff += ";" + prev;
  SetEnvironmentVariableA("PATH", eff.c_str());
  if (!s.verilatorRoot.empty()) {
    SetEnvironmentVariableA("VERILATOR_ROOT", LocalToForwardSlashes(s.verilatorRoot).c_str());
  }
  if (!s.makePath.empty()) SetEnvironmentVariableA("MAKE", s.makePath.c_str());
  bool ok = LocalRunCommand(cmd, out, static_cast<DWORD>(tO));
  SetEnvironmentVariableA("PATH", prev.empty() ? nullptr : prev.c_str());
  SetEnvironmentVariableA("VERILATOR_ROOT", nullptr); SetEnvironmentVariableA("MAKE", nullptr); return ok;
#else
  return false;
#endif
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

bool RtlProjectManager::RunVerilatorLint(const RtlLibraryEntry& e, const RtlToolchainStatus& s, std::string* diag) const {
  std::string tD = LocalGetTempBasePath("plc_lint\\"); LocalEnsureDirRecursive(tD);
  std::string p = LocalJoinPath(tD, e.topFile); LocalWriteFileContent(p, e.sourceFiles[0].content);
  std::ostringstream cmd; cmd << LocalEscape(s.verilatorPath) << " --lint-only -Wno-fatal -Wno-TIMESCALEMOD --timing -I" << LocalEscape(LocalToForwardSlashes(LocalJoinPath(s.verilatorRoot, "include"))) << " --top-module " << LocalEscape(e.topModule) << " " << LocalEscape(p);
  return RunCommandCapture(cmd.str(), diag, s, kLocalDefaultTimeoutMs);
}

bool RtlProjectManager::RunVerilatorBuild(RtlLibraryEntry* e, const RtlToolchainStatus& s, std::string* diag) const {
#ifdef _WIN32
  if (e->buildHash.empty()) {
    std::ostringstream hashSrc;
    hashSrc << e->moduleId << "|" << e->topModule;
    for (const auto& f : e->sourceFiles) {
      hashSrc << "|" << f.path << "|" << f.content.size();
    }
    e->buildHash = LocalHashHex(hashSrc.str());
  }

  std::string d = GetBuildCacheDirectory(*e);
  if (d.empty()) {
    if (diag) *diag = "[Build] Cache path generation failed: buildHash is empty.";
    return false;
  }

  std::string sD = LocalJoinPath(d, "src"), oD = LocalJoinPath(d, "obj");
  LocalEnsureDirRecursive(d); LocalEnsureDirRecursive(sD); LocalEnsureDirRecursive(oD);
  std::string p = LocalJoinPath(sD, e->topFile); LocalWriteFileContent(p, e->sourceFiles[0].content);
  std::string wm = LocalJoinPath(d, "worker.cpp");
  std::ostringstream code;
  code << "#include \"V" << e->topModule << ".h\"\n"
       << "#include <verilated.h>\n"
       << "#include <iostream>\n#include <sstream>\n#include <string>\n#include <cstdint>\n\n"
       << "static vluint64_t g_main_time = 0;\n"
       << "double sc_time_stamp() { return static_cast<double>(g_main_time); }\n\n"
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
      code << "        if (pin == \"" << port.pinName << "\") top." << port.pinName << " = val;\n";
    }
  }

  code << "      }\n"
       << "      top.eval();\n"
       << "      ++g_main_time;\n"
       << "      std::cout << \"OUT\";\n";

  for (const auto& port : e->ports) {
    if (!port.isInput) {
      code << "      std::cout << \" \" << \"" << port.pinName << "=\" << (int)top." << port.pinName << ";\n";
    }
  }

  code << "      std::cout << std::endl;\n"
       << "    }\n"
       << "  }\n"
       << "  top.final(); return 0;\n"
       << "}\n";

  LocalWriteFileContent(wm, code.str());
  std::ostringstream vC;
  vC << LocalEscape(s.verilatorPath) << " --cc --exe -Wno-fatal -Wno-TIMESCALEMOD --timing"
     << " -I" << LocalEscape(LocalToForwardSlashes(LocalJoinPath(s.verilatorRoot, "include")))
     << " --top-module " << LocalEscape(e->topModule) << " -Mdir "
     << LocalEscape(LocalToForwardSlashes(oD)) << " "
     << LocalEscape(LocalToForwardSlashes(p)) << " "
     << LocalEscape(LocalToForwardSlashes(wm));
  std::string out;
  if (!RunCommandCapture(vC.str(), &out, s, kLocalBuildTimeoutMs)) {
    if (diag) {
      *diag = "[Verilator step]\n";
      *diag += out.empty() ? "Process execution failed (check paths/permissions).\nCommand: " + vC.str() : out;
    }
    return false;
  }

  std::ostringstream cC;
  std::string vIncl = LocalToForwardSlashes(LocalJoinPath(s.verilatorRoot, "include"));
  std::string outExe = LocalJoinPath(d, "rtl_worker.exe");

  cC << LocalEscape(LocalToForwardSlashes(s.compilerPath))
     << " -std=c++20 -fcoroutines -Os"
     << " -I" << LocalEscape(vIncl)
     << " -I" << LocalEscape(vIncl + "/vltstd")
     << " -I" << LocalEscape(LocalToForwardSlashes(oD))
     << " " << LocalEscape(LocalToForwardSlashes(wm))
     << " " << LocalEscape(vIncl + "/verilated.cpp");

  if (LocalFileExists(LocalToForwardSlashes(vIncl + "/verilated_threads.cpp"))) {
    cC << " " << LocalEscape(vIncl + "/verilated_threads.cpp");
  }

  WIN32_FIND_DATAA findData;
  std::string searchPath = oD + "\\V" + e->topModule + "*.cpp";
  HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        cC << " " << LocalEscape(LocalToForwardSlashes(LocalJoinPath(oD, findData.cFileName)));
      }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
  }

  cC << " -o " << LocalEscape(LocalToForwardSlashes(outExe));

  std::string compileCmd = cC.str();
  if (!RunCommandCapture(compileCmd, &out, s, kLocalBuildTimeoutMs)) {
    if (diag) {
      *diag = "[C++ Compile step]\n";
      *diag += out.empty() ? "Process execution failed.\nCommand: " + compileCmd : out;
    }
    return false;
  }

  return LocalFileExists(outExe);
#else
  (void)e; (void)s; (void)diag;
#endif
  return false;
}

bool RtlProjectManager::RunVerilatorTestbench(RtlLibraryEntry* e, const RtlToolchainStatus& s, std::string* diag) const {
#ifdef _WIN32
  // ── 1. 캐시 디렉토리 준비 ──────────────────────────────────────
  std::string d  = GetTestbenchCacheDirectory(*e);
  if (d.empty()) {
    if (diag) *diag = "Failed to get testbench cache directory.";
    return false;
  }
  std::string sD = LocalJoinPath(d, "src");
  std::string oD = LocalJoinPath(d, "obj");
  LocalEnsureDirRecursive(d);
  LocalEnsureDirRecursive(sD);
  LocalEnsureDirRecursive(oD);

  // ── 2. 소스 파일 전체 기록 ────────────────────────────────────
  for (const auto& f : e->sourceFiles) {
    std::string dest = LocalJoinPath(sD, f.path);
    // 하위 디렉토리 포함 경로 처리
    size_t slash = dest.find_last_of("\\/");
    if (slash != std::string::npos) LocalEnsureDirRecursive(dest.substr(0, slash));
    LocalWriteFileContent(dest, f.content);
  }

  // ── 3. C++ 시뮬레이션 드라이버 생성 ─────────────────────────
  std::string vcdAbsPath = LocalToForwardSlashes(LocalJoinPath(d, "dump.vcd"));
  std::string tbMain     = LocalJoinPath(d, "tb_main.cpp");
  std::string tbExe      = LocalJoinPath(d, "tb_sim.exe");

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
       << "  vcd.open(\"" << vcdAbsPath << "\");\n"
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
  LocalWriteFileContent(tbMain, code.str());

  // ── 4. Verilator: C++ 코드 생성 ───────────────────────────────
  std::ostringstream vC;
  vC << LocalEscape(s.verilatorPath)
     << " --cc --exe --trace -Wno-fatal -Wno-TIMESCALEMOD --timing"
     << " -I" << LocalEscape(LocalToForwardSlashes(LocalJoinPath(s.verilatorRoot, "include")))
     << " --top-module " << LocalEscape(e->testbenchTopModule)
     << " -Mdir " << LocalEscape(LocalToForwardSlashes(oD));
  for (const auto& f : e->sourceFiles) {
    vC << " " << LocalEscape(LocalToForwardSlashes(LocalJoinPath(sD, f.path)));
  }
  vC << " " << LocalEscape(LocalToForwardSlashes(tbMain));

  std::string buildLog;
  if (!RunCommandCapture(vC.str(), &buildLog, s, kLocalBuildTimeoutMs)) {
    if (diag) *diag = "[Verilator step]\n" + (buildLog.empty()
        ? "Process failed.\nCommand: " + vC.str() : buildLog);
    e->testbenchBuildLog = buildLog;
    return false;
  }
  e->testbenchBuildLog = buildLog;

  // ── 5. g++: 수동 컴파일 ────────────────────────────────────────
  std::string vIncl = LocalToForwardSlashes(LocalJoinPath(s.verilatorRoot, "include"));
  std::ostringstream cC;
  cC << LocalEscape(LocalToForwardSlashes(s.compilerPath))
     << " -std=c++20 -fcoroutines -Os"
     << " -I" << LocalEscape(vIncl)
     << " -I" << LocalEscape(vIncl + "/vltstd")
     << " -I" << LocalEscape(LocalToForwardSlashes(oD))
     << " " << LocalEscape(LocalToForwardSlashes(tbMain))
     << " " << LocalEscape(vIncl + "/verilated.cpp")
     << " " << LocalEscape(vIncl + "/verilated_vcd_c.cpp");

  if (LocalFileExists(LocalToForwardSlashes(vIncl + "/verilated_threads.cpp")))
    cC << " " << LocalEscape(vIncl + "/verilated_threads.cpp");
  if (LocalFileExists(LocalToForwardSlashes(vIncl + "/verilated_timing.cpp")))
    cC << " " << LocalEscape(vIncl + "/verilated_timing.cpp");

  // V{top}*.cpp 파일 추가
  WIN32_FIND_DATAA fd;
  std::string searchPat = oD + "\\V" + e->testbenchTopModule + "*.cpp";
  HANDLE hFind = FindFirstFileA(searchPat.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        cC << " " << LocalEscape(LocalToForwardSlashes(LocalJoinPath(oD, fd.cFileName)));
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
  }
  cC << " -o " << LocalEscape(LocalToForwardSlashes(tbExe));

  std::string compileLog;
  if (!RunCommandCapture(cC.str(), &compileLog, s, kLocalBuildTimeoutMs)) {
    e->testbenchBuildLog += "\n[C++ Compile]\n" + (compileLog.empty()
        ? "Compile failed.\nCommand: " + cC.str() : compileLog);
    if (diag) *diag = e->testbenchBuildLog;
    return false;
  }
  e->testbenchBuildLog += "\n[C++ Compile]\n" + compileLog;

  if (!LocalFileExists(tbExe)) {
    if (diag) *diag = "Testbench executable not produced.";
    return false;
  }

  // ── 6. 테스트벤치 실행 ────────────────────────────────────────
  std::string runLog;
  bool ran = RunCommandCapture(LocalEscape(LocalToForwardSlashes(tbExe)),
                               &runLog, s, kLocalDefaultTimeoutMs);
  e->testbenchRunLog = runLog;

  // ── 7. 합격/실패 판정 ─────────────────────────────────────────
  auto HasKeyword = [&](const std::string& kw) {
    return runLog.find(kw) != std::string::npos;
  };
  bool hasFatal = HasKeyword("$fatal") || HasKeyword("%Fatal") ||
                  HasKeyword("Aborting");
  bool hasError = HasKeyword("$error") || HasKeyword("%Error") ||
                  HasKeyword("Assertion failed") ||
                  HasKeyword("assertion failed");

  if (hasFatal || hasError) {
    if (diag) *diag = "Testbench reported errors. Check Testbench Run Log.";
    return false;
  }
  if (!ran) {
    if (diag) *diag = "Testbench timed out or crashed. Check Testbench Run Log.";
    return false;
  }

  // VCD 파일 기록
  if (LocalFileExists(vcdAbsPath)) e->testbenchVcdPath = vcdAbsPath;

  if (diag) *diag = "Testbench passed.";
  return true;
#else
  (void)e; (void)s; (void)diag;
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

