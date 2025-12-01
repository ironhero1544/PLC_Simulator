// project_file_manager.cpp
// Copyright 2024 PLC Emulator Project
//
// Implementation of project file manager.

#include "plc_emulator/project/project_file_manager.h"

#include "miniz.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

// filesystem 호환성 처리
#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <windows.h>

// 🔥 **NEW**: 숨겨진 PowerShell 실행 함수 (CMD 창 안 뜸)
int ExecuteHiddenPowerShell(const std::string& command) {
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;  // 창 숨기기
  ZeroMemory(&pi, sizeof(pi));

  // CreateProcess용 명령어 문자열 (수정 가능해야 함)
  std::string cmdLine = command;

  // 프로세스 생성
  BOOL success = CreateProcessA(nullptr,           // 실행 파일 경로
                                &cmdLine[0],       // 명령줄
                                nullptr,           // 프로세스 보안 속성
                                nullptr,           // 스레드 보안 속성
                                FALSE,             // 핸들 상속
                                CREATE_NO_WINDOW,  // 창 없음
                                nullptr,           // 환경 변수
                                nullptr,           // 현재 디렉토리
                                &si,               // 시작 정보
                                &pi                // 프로세스 정보
  );

  if (!success) {
    printf("[ERROR] CreateProcess failed: %lu\n", GetLastError());
    return -1;
  }

  // 프로세스 완료 대기
  WaitForSingleObject(pi.hProcess, INFINITE);

  // 종료 코드 가져오기
  DWORD exitCode;
  GetExitCodeProcess(pi.hProcess, &exitCode);

  // 핸들 닫기
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return static_cast<int>(exitCode);
}

// 🔥 **NEW**: 간단한 ZIP 파일 생성 (Windows PowerShell 사용)
bool CreateZipFileWindows(const std::string& zipPath,
                          const std::map<std::string, std::string>& files) {
  // 1. 임시 디렉토리 생성
  char tempDir[MAX_PATH];
  GetTempPathA(MAX_PATH, tempDir);
  std::string tempPath = std::string(tempDir) + "PLCProject_temp\\";
  CreateDirectoryA(tempPath.c_str(), nullptr);

  // 2. 파일들을 임시 디렉토리에 생성
  for (const auto& [fileName, content] : files) {
    std::string tempFilePath = tempPath + fileName;
    std::ofstream tempFile(tempFilePath, std::ios::binary);
    if (tempFile.is_open()) {
      tempFile << content;
      tempFile.close();
    } else {
      return false;
    }
  }

  // 2.5 저장 대상 디렉터리 보장 및 기존 ZIP 삭제
  // 부모 디렉터리 생성 (단일 단계)
  auto ensure_parent_dir = [](const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      std::string dir = path.substr(0, pos);
      if (!dir.empty()) {
        CreateDirectoryA(dir.c_str(), nullptr);  // 이미 있으면 실패해도 무시
      }
    }
  };
  ensure_parent_dir(zipPath);

  // 3. 한글 경로 문제를 위해 짧은 경로명 변환
  char shortZipPath[MAX_PATH] = {0};
  char shortTempPath[MAX_PATH] = {0};

  DWORD zipResult = GetShortPathNameA(zipPath.c_str(), shortZipPath, MAX_PATH);
  DWORD tempResult =
      GetShortPathNameA(tempPath.c_str(), shortTempPath, MAX_PATH);

  // 짧은 경로명 변환이 실패하면 원본 경로 사용
  std::string finalZipPath =
      (zipResult > 0) ? std::string(shortZipPath) : zipPath;
  std::string finalTempPath =
      (tempResult > 0) ? std::string(shortTempPath) : tempPath;

  // 기존 파일이 있으면 삭제 (ZipFile.CreateFromDirectory는 덮어쓰기 불가)
  DWORD attrs = GetFileAttributesA(finalZipPath.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    DeleteFileA(finalZipPath.c_str());
  }

  // PowerShell 명령어 생성 (더 안전한 방식)
  std::string finalCmd =
      "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass "
      "-Command \"& {";
  finalCmd += "Add-Type -AssemblyName System.IO.Compression.FileSystem; ";
  finalCmd += "try { [System.IO.Compression.ZipFile]::CreateFromDirectory('";
  finalCmd += finalTempPath + "', '" + finalZipPath + "'); exit 0; } ";
  finalCmd += "catch { Write-Error $_.Exception.Message; exit 1; } }\"";

  // PowerShell 실행 (숨겨진 창으로)
  printf("[DEBUG] PowerShell CMD: %s\n", finalCmd.c_str());
  int result = ExecuteHiddenPowerShell(finalCmd);
  printf("[DEBUG] PowerShell Result: %d\n", result);

  // 4. 임시 파일들 정리
  for (const auto& [fileName, content] : files) {
    std::string tempFilePath = tempPath + fileName;
    DeleteFileA(tempFilePath.c_str());
  }
  RemoveDirectoryA(tempPath.c_str());

  return (result == 0);
}

// 🔥 **NEW**: ZIP 파일 압축 해제 (PowerShell 사용)
bool ExtractZipFileWindows(const std::string& zipPath,
                           std::map<std::string, std::string>& files) {
  // 1. 임시 추출 디렉토리
  char tempDir[MAX_PATH];
  GetTempPathA(MAX_PATH, tempDir);
  std::string extractPath = std::string(tempDir) + "PLCProject_extract\\";
  CreateDirectoryA(extractPath.c_str(), nullptr);

  // 2. 짧은 경로명 변환 (한글 경로 문제 해결)
  char shortZipPath[MAX_PATH] = {0};
  char shortExtractPath[MAX_PATH] = {0};

  DWORD zipResult = GetShortPathNameA(zipPath.c_str(), shortZipPath, MAX_PATH);
  DWORD extractResult =
      GetShortPathNameA(extractPath.c_str(), shortExtractPath, MAX_PATH);

  // 짧은 경로명 변환이 실패하면 원본 경로 사용
  std::string finalZipPath =
      (zipResult > 0) ? std::string(shortZipPath) : zipPath;
  std::string finalExtractPath =
      (extractResult > 0) ? std::string(shortExtractPath) : extractPath;

  // 3. PowerShell로 압축 해제 (더 안전한 방식)
  std::string powershellCmd =
      "powershell.exe -ExecutionPolicy Bypass -Command \"& {";
  powershellCmd += "Add-Type -AssemblyName System.IO.Compression.FileSystem; ";
  powershellCmd +=
      "try { [System.IO.Compression.ZipFile]::ExtractToDirectory('";
  powershellCmd += finalZipPath + "', '" + finalExtractPath + "'); exit 0; } ";
  powershellCmd += "catch { Write-Error $_.Exception.Message; exit 1; } }\"";

  // PowerShell 실행 (숨겨진 창으로)
  printf("[DEBUG] Extract CMD: %s\n", powershellCmd.c_str());
  int result = ExecuteHiddenPowerShell(powershellCmd);
  printf("[DEBUG] Extract Result: %d\n", result);

  if (result == 0) {
    // 4. 추출된 파일들을 읽어서 메모리에 로드
    printf("[DEBUG] PowerShell extraction successful, reading files from: %s\n",
           extractPath.c_str());

    WIN32_FIND_DATAA findData;
    std::string searchPath = extractPath + "*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
      printf("[DEBUG] Found files in extract directory:\n");
      do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          std::string fileName = findData.cFileName;
          std::string fullPath = extractPath + fileName;
          printf("[DEBUG]   - %s (%lu bytes)\n", fileName.c_str(),
                 findData.nFileSizeLow);

          std::ifstream file(fullPath, std::ios::binary);
          if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            files[fileName] = content;
            printf("[DEBUG]   - Successfully read %s (%zu bytes)\n",
                   fileName.c_str(), content.size());
            file.close();
          } else {
            printf("[DEBUG]   - Failed to open %s\n", fullPath.c_str());
          }
          DeleteFileA(fullPath.c_str());
        }
      } while (FindNextFileA(hFind, &findData));
      FindClose(hFind);
    } else {
      printf("[DEBUG] No files found in extract directory: %s\n",
             extractPath.c_str());
    }

    printf("[DEBUG] Total files extracted: %zu\n", files.size());
    RemoveDirectoryA(extractPath.c_str());
    return true;
  } else {
    printf("[DEBUG] PowerShell extraction failed with code: %d\n", result);
  }

  // 정리
  RemoveDirectoryA(extractPath.c_str());
  return false;
}
#endif

namespace plc {

ProjectFileManager::ProjectFileManager() {
  xml_serializer_ = std::make_unique<XMLSerializer>();
  ld_converter_ = std::make_unique<LadderToLDConverter>();
}

ProjectFileManager::~ProjectFileManager() {}

ProjectFileManager::SaveResult ProjectFileManager::SaveProject(
    const LadderProgram& program, const std::string& filePath,
    const std::string& projectName) {
  SaveResult result;
  last_error_.clear();

  LogDebug("🚀 Starting project save to: " + filePath);

  try {
    // 1. XML 직렬화 (UI 구조 완전 보존)
    xml_serializer_->SetDebugMode(debug_mode_);
    auto xmlResult = xml_serializer_->SerializeToXML(program);
    if (!xmlResult.success) {
      result.success = false;
      result.errorMessage =
          "Failed to serialize ladder program: " + xmlResult.errorMessage;
      SetError(result.errorMessage);
      return result;
    }

    // 2. LD 프로그램 생성 (실행용)
    ld_converter_->SetDebugMode(debug_mode_);
    std::string ldContent = ld_converter_->ConvertToLDString(program);
    if (ldContent.empty()) {
      result.success = false;
      result.errorMessage = "Failed to convert ladder to LD format: " +
                            ld_converter_->GetLastError();
      SetError(result.errorMessage);
      return result;
    }

    // 3. 프로젝트 정보 생성
    ProjectInfo info;
    info.projectName =
        projectName.empty() ? ExtractFileName(filePath) : projectName;
    info.version = "1.0";
    info.createdDate = GetCurrentDateTime();
    info.lastModified = info.createdDate;
    info.toolVersion = "PLC Simulator v1.0";
    info.schemaVersion = "1.0";
    info.layoutChecksum = CalculateChecksum(xmlResult.xmlContent);
    info.programChecksum = CalculateChecksum(ldContent);

    std::string manifestContent = GenerateManifest(info);

    // 4. ZIP 파일 생성
    std::map<std::string, std::string> files;
    files["layout.xml"] = xmlResult.xmlContent;
    files["program.ld"] = ldContent;
    files["manifest.json"] = manifestContent;

    if (!CreateZipFile(filePath, files)) {
      result.success = false;
      result.errorMessage = "Failed to create ZIP file: " + last_error_;
      return result;
    }

    // 5. 결과 설정
    result.success = true;
    result.savedPath = filePath;

    // 파일 크기 계산 (Windows Shell API 버전은 원래 경로 사용 가능)
#if __cplusplus >= 201703L
    result.compressedSize = fs::file_size(filePath);
#else
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
      result.compressedSize = file.tellg();
      file.close();
    }
#endif

    result.info = info;

    LogDebug("✅ Project saved successfully: " +
             std::to_string(result.compressedSize) + " bytes");

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = "Project save failed: " + std::string(e.what());
    SetError(result.errorMessage);
  }

  return result;
}

ProjectFileManager::LoadResult ProjectFileManager::LoadProject(
    const std::string& filePath) {
  LoadResult result;
  last_error_.clear();

  LogDebug("📂 Starting project load from: " + filePath);

  try {
    // 1. ZIP 파일 압축 해제
    std::map<std::string, std::string> files;
    if (!ExtractZipFile(filePath, files)) {
      result.success = false;
      result.errorMessage = "Failed to extract ZIP file: " + last_error_;
      return result;
    }

    // 2. 필수 파일 존재 확인
    if (files.find("layout.xml") == files.end()) {
      result.success = false;
      result.errorMessage = "Missing layout.xml file in project";
      SetError(result.errorMessage);
      return result;
    }

    if (files.find("manifest.json") == files.end()) {
      LogDebug("⚠️ Missing manifest.json (backward compatibility mode)");
    }

    // 3. Manifest 파싱 (선택적)
    if (files.find("manifest.json") != files.end()) {
      if (!ParseManifest(files["manifest.json"], result.info)) {
        LogDebug("⚠️ Failed to parse manifest, continuing...");
      }
    }

    // 4. XML 역직렬화 (UI 구조 복원)
    xml_serializer_->SetDebugMode(debug_mode_);
    auto xmlResult = xml_serializer_->DeserializeFromXML(files["layout.xml"]);
    if (!xmlResult.success) {
      result.success = false;
      result.errorMessage =
          "Failed to deserialize ladder program: " + xmlResult.errorMessage;
      SetError(result.errorMessage);
      return result;
    }

    // 5. LD 프로그램 로드 (실행용, 선택적)
    if (files.find("program.ld") != files.end()) {
      result.ldProgram = files["program.ld"];
    }

    // 6. 결과 설정
    result.success = true;
    result.program = xmlResult.program;

    // 체크섬 검증 (선택적)
    if (result.info.layoutChecksum != 0) {
      size_t actualChecksum = CalculateChecksum(files["layout.xml"]);
      if (actualChecksum != result.info.layoutChecksum) {
        LogDebug("⚠️ Layout checksum mismatch (file may be corrupted)");
      }
    }

    LogDebug("✅ Project loaded successfully: " +
             std::to_string(result.program.rungs.size()) + " rungs, " +
             std::to_string(result.program.verticalConnections.size()) +
             " connections");

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = "Project load failed: " + std::string(e.what());
    SetError(result.errorMessage);
  }

  return result;
}

ProjectFileManager::ProjectInfo ProjectFileManager::GetProjectInfo(
    const std::string& filePath) {
  ProjectInfo info;

  std::map<std::string, std::string> files;
  if (ExtractZipFile(filePath, files)) {
    if (files.find("manifest.json") != files.end()) {
      ParseManifest(files["manifest.json"], info);
    }
  }

  return info;
}

bool ProjectFileManager::ValidateProjectFile(const std::string& filePath) {
  try {
    std::map<std::string, std::string> files;
    if (!ExtractZipFile(filePath, files)) {
      return false;
    }

    // 필수 파일 검사
    if (files.find("layout.xml") == files.end()) {
      return false;
    }

    // XML 유효성 검사
    xml_serializer_->SetDebugMode(false);  // 검증 시에는 조용히
    auto xmlResult = xml_serializer_->DeserializeFromXML(files["layout.xml"]);

    return xmlResult.success;

  } catch (const std::exception&) {
    return false;
  }
}

// =============================================================================
// ZIP 관련 내부 함수들
// =============================================================================

bool ProjectFileManager::CreateZipFile(
    const std::string& filePath,
    const std::map<std::string, std::string>& files) {
  LogDebug("🚀 Creating ZIP file using Windows Shell API: " + filePath);

#ifdef _WIN32
  // Windows Shell API 사용
  if (CreateZipFileWindows(filePath, files)) {
    LogDebug("✅ ZIP file created successfully with Windows Shell API");
    return true;
  } else {
    SetError("Failed to create ZIP file using Windows Shell API");
    return false;
  }
#else
  // 다른 플랫폼에서는 원래 miniz 사용 (Windows 전용 프로젝트이므로 사용되지
  // 않음)
  SetError("Windows-only project: Shell API not available on this platform");
  return false;
#endif
}

bool ProjectFileManager::ExtractZipFile(
    const std::string& filePath, std::map<std::string, std::string>& files) {
  LogDebug("📂 Extracting ZIP file using Windows Shell API: " + filePath);

#ifdef _WIN32
  // Windows Shell API 사용
  if (ExtractZipFileWindows(filePath, files)) {
    LogDebug("✅ ZIP file extracted successfully with Windows Shell API");
    LogDebug("📊 Files extracted: " + std::to_string(files.size()));
    for (const auto& [fileName, content] : files) {
      LogDebug("   - " + fileName + " (" + std::to_string(content.size()) +
               " bytes)");
    }
    return true;
  } else {
    SetError("Failed to extract ZIP file using Windows Shell API");
    return false;
  }
#else
  // 다른 플랫폼에서는 원래 miniz 사용 (Windows 전용 프로젝트이므로 사용되지
  // 않음)
  SetError("Windows-only project: Shell API not available on this platform");
  return false;
#endif
}

// =============================================================================
// 프로젝트 내용 생성
// =============================================================================

std::string ProjectFileManager::GenerateManifest(const ProjectInfo& info) {
  std::map<std::string, std::string> manifest;
  manifest["projectName"] = info.projectName;
  manifest["version"] = info.version;
  manifest["createdDate"] = info.createdDate;
  manifest["lastModified"] = info.lastModified;
  manifest["toolVersion"] = info.toolVersion;
  manifest["schemaVersion"] = info.schemaVersion;
  manifest["layoutChecksum"] = std::to_string(info.layoutChecksum);
  manifest["programChecksum"] = std::to_string(info.programChecksum);

  return CreateSimpleJSON(manifest);
}

std::string ProjectFileManager::GenerateLDProgram(
    const LadderProgram& program) {
  return ld_converter_->ConvertToLDString(program);
}

bool ProjectFileManager::ParseManifest(const std::string& manifestContent,
                                       ProjectInfo& info) {
  try {
    auto manifest = ParseSimpleJSON(manifestContent);

    info.projectName = manifest["projectName"];
    info.version = manifest["version"];
    info.createdDate = manifest["createdDate"];
    info.lastModified = manifest["lastModified"];
    info.toolVersion = manifest["toolVersion"];
    info.schemaVersion = manifest["schemaVersion"];

    if (!manifest["layoutChecksum"].empty()) {
      info.layoutChecksum = std::stoull(manifest["layoutChecksum"]);
    }
    if (!manifest["programChecksum"].empty()) {
      info.programChecksum = std::stoull(manifest["programChecksum"]);
    }

    return true;
  } catch (const std::exception& e) {
    LogDebug("Failed to parse manifest: " + std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

size_t ProjectFileManager::CalculateChecksum(const std::string& content) {
  // 간단한 해시 함수 (CRC32 대신)
  std::hash<std::string> hasher;
  return hasher(content);
}

std::string ProjectFileManager::GetCurrentDateTime() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

std::string ProjectFileManager::ExtractFileName(const std::string& filePath) {
#if __cplusplus >= 201703L
  fs::path path(filePath);
  return path.stem().string();  // 확장자 제외한 파일명
#else
  // C++14 호환: 수동으로 파일명 추출
  size_t lastSlash = filePath.find_last_of("/\\");
  size_t lastDot = filePath.find_last_of(".");

  std::string filename = (lastSlash != std::string::npos)
                             ? filePath.substr(lastSlash + 1)
                             : filePath;

  if (lastDot != std::string::npos && lastDot > lastSlash) {
    filename = filename.substr(
        0, lastDot - (lastSlash != std::string::npos ? lastSlash + 1 : 0));
  }

  return filename;
#endif
}

void ProjectFileManager::LogDebug(const std::string& message) {
  if (debug_mode_) {
    std::cout << "[ProjectFileManager] " << message << std::endl;
  }
}

void ProjectFileManager::SetError(const std::string& error) {
  last_error_ = error;
  if (debug_mode_) {
    std::cout << "[ProjectFileManager] ERROR: " << error << std::endl;
  }
}

// =============================================================================
// 간단한 JSON 헬퍼 함수들
// =============================================================================

std::string ProjectFileManager::CreateSimpleJSON(
    const std::map<std::string, std::string>& keyValues) {
  std::stringstream json;
  json << "{\n";

  bool first = true;
  for (const auto& [key, value] : keyValues) {
    if (!first)
      json << ",\n";
    json << "  \"" << key << "\": \"" << value << "\"";
    first = false;
  }

  json << "\n}";
  return json.str();
}

std::map<std::string, std::string> ProjectFileManager::ParseSimpleJSON(
    const std::string& jsonContent) {
  std::map<std::string, std::string> result;

  // 매우 간단한 JSON 파싱 (정규 표현식 사용)
  std::regex pattern("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
  std::sregex_iterator iter(jsonContent.begin(), jsonContent.end(), pattern);
  std::sregex_iterator end;

  for (; iter != end; ++iter) {
    std::smatch match = *iter;
    if (match.size() == 3) {
      result[match[1].str()] = match[2].str();
    }
  }

  return result;
}

}  // namespace plc
