// project_file_manager.cpp
//
// Implementation of project file manager.

#include "plc_emulator/project/project_file_manager.h"
#include "plc_emulator/programming/ladder_program_utils.h"

#include "miniz.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#if __cplusplus >= 201703L
#include <filesystem>
#else
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <windows.h>

std::string NormalizeWindowsPath(std::string path) {
  for (char& ch : path) {
    if (ch == '/') {
      ch = '\\';
    }
  }
  return path;
}

bool EnsureDirectoryExistsRecursive(const std::string& directory_path) {
  if (directory_path.empty()) {
    return true;
  }

  const std::string normalized = NormalizeWindowsPath(directory_path);
  if (normalized.empty()) {
    return true;
  }

  DWORD attrs = GetFileAttributesA(normalized.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }

  std::string current;
  size_t index = 0;
  if (normalized.size() >= 2 && normalized[1] == ':') {
    current = normalized.substr(0, 2);
    index = 2;
    if (index < normalized.size() &&
        (normalized[index] == '\\' || normalized[index] == '/')) {
      current.push_back('\\');
      ++index;
    }
  } else if (!normalized.empty() &&
             (normalized[0] == '\\' || normalized[0] == '/')) {
    current = "\\";
    index = 1;
  }

  while (index < normalized.size()) {
    size_t next = normalized.find('\\', index);
    std::string part = normalized.substr(
        index, next == std::string::npos ? std::string::npos : next - index);
    if (!part.empty()) {
      if (!current.empty() && current.back() != '\\') {
        current.push_back('\\');
      }
      current += part;
      if (!CreateDirectoryA(current.c_str(), nullptr)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
          return false;
        }
      }
    }
    if (next == std::string::npos) {
      break;
    }
    index = next + 1;
  }

  return true;
}

bool DeleteDirectoryTreeRecursive(const std::string& directory_path) {
  const std::string normalized = NormalizeWindowsPath(directory_path);
  WIN32_FIND_DATAA find_data;
  const std::string search_path = normalized + "\\*";
  HANDLE find_handle = FindFirstFileA(search_path.c_str(), &find_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      const char* name = find_data.cFileName;
      if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
        continue;
      }

      const std::string child_path = normalized + "\\" + name;
      if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        DeleteDirectoryTreeRecursive(child_path);
      } else {
        DeleteFileA(child_path.c_str());
      }
    } while (FindNextFileA(find_handle, &find_data) != 0);
    FindClose(find_handle);
  }

  DWORD attrs = GetFileAttributesA(normalized.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES &&
      (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    RemoveDirectoryA(normalized.c_str());
  }
  return true;
}

bool ReadDirectoryTreeRecursive(const std::string& root_path,
                                const std::string& relative_path,
                                std::map<std::string, std::string>& files) {
  const std::string current_path = relative_path.empty()
                                       ? NormalizeWindowsPath(root_path)
                                       : NormalizeWindowsPath(root_path + "\\" +
                                                              relative_path);

  WIN32_FIND_DATAA find_data;
  const std::string search_path = current_path + "\\*";
  HANDLE find_handle = FindFirstFileA(search_path.c_str(), &find_data);
  if (find_handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  do {
    const char* name = find_data.cFileName;
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
      continue;
    }

    const std::string child_relative =
        relative_path.empty() ? std::string(name)
                              : (relative_path + "\\" + name);
    const std::string child_full =
        NormalizeWindowsPath(root_path + "\\" + child_relative);
    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      ReadDirectoryTreeRecursive(root_path, child_relative, files);
      continue;
    }

    std::ifstream file(child_full, std::ios::binary);
    if (!file.is_open()) {
      continue;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    std::string normalized_relative = child_relative;
    for (char& ch : normalized_relative) {
      if (ch == '\\') {
        ch = '/';
      }
    }
    files[normalized_relative] = content;
  } while (FindNextFileA(find_handle, &find_data) != 0);

  FindClose(find_handle);
  return true;
}

std::string ParentDirectoryOf(const std::string& path) {
  const std::string normalized = NormalizeWindowsPath(path);
  size_t pos = normalized.find_last_of("\\");
  if (pos == std::string::npos) {
    return "";
  }
  return normalized.substr(0, pos);
}

std::string JoinWindowsPath(const std::string& left, const std::string& right) {
  if (left.empty()) {
    return NormalizeWindowsPath(right);
  }
  if (right.empty()) {
    return NormalizeWindowsPath(left);
  }
  std::string joined = NormalizeWindowsPath(left);
  if (joined.back() != '\\') {
    joined.push_back('\\');
  }
  joined += NormalizeWindowsPath(right);
  return joined;
}


int ExecuteHiddenPowerShell(const std::string& command) {
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;  // ??????????????
  ZeroMemory(&pi, sizeof(pi));

  std::string cmdLine = command;

  BOOL success = CreateProcessA(nullptr,
                                &cmdLine[0],
                                nullptr,
                                nullptr,
                                FALSE,
                                CREATE_NO_WINDOW,
                                nullptr,
                                nullptr,
                                &si,
                                &pi
  );

  if (!success) {
    printf("[ERROR] CreateProcess failed: %lu\n", GetLastError());
    return -1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode;
  GetExitCodeProcess(pi.hProcess, &exitCode);

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return static_cast<int>(exitCode);
}

std::string CreateUniqueTempDirectory(const char* prefix) {
  char temp_dir[MAX_PATH] = {0};
  if (GetTempPathA(MAX_PATH, temp_dir) == 0) {
    return "";
  }

  char temp_file[MAX_PATH] = {0};
  if (GetTempFileNameA(temp_dir, prefix, 0, temp_file) == 0) {
    return "";
  }

  DeleteFileA(temp_file);
  if (!CreateDirectoryA(temp_file, nullptr) &&
      GetLastError() != ERROR_ALREADY_EXISTS) {
    return "";
  }
  return temp_file;
}

bool CreateZipFileWindows(const std::string& zipPath,
                          const std::map<std::string, std::string>& files) {
  const std::string tempPath = CreateUniqueTempDirectory("PCZ");
  if (tempPath.empty()) {
    return false;
  }

  auto cleanup_temp = [&tempPath]() {
    DeleteDirectoryTreeRecursive(tempPath);
  };

  for (const auto& [fileName, content] : files) {
    const std::string tempFilePath = JoinWindowsPath(tempPath, fileName);
    if (!EnsureDirectoryExistsRecursive(ParentDirectoryOf(tempFilePath))) {
      cleanup_temp();
      return false;
    }

    std::ofstream tempFile(tempFilePath, std::ios::binary);
    if (tempFile.is_open()) {
      tempFile << content;
      tempFile.close();
    } else {
      cleanup_temp();
      return false;
    }
  }

  auto ensure_parent_dir = [](const std::string& path) {
    return EnsureDirectoryExistsRecursive(ParentDirectoryOf(path));
  };
  if (!ensure_parent_dir(zipPath)) {
    cleanup_temp();
    return false;
  }

  char shortZipPath[MAX_PATH] = {0};
  char shortTempPath[MAX_PATH] = {0};

  DWORD zipResult = GetShortPathNameA(zipPath.c_str(), shortZipPath, MAX_PATH);
  DWORD tempResult =
      GetShortPathNameA(tempPath.c_str(), shortTempPath, MAX_PATH);

  std::string finalZipPath =
      (zipResult > 0) ? std::string(shortZipPath) : zipPath;
  std::string finalTempPath =
      (tempResult > 0) ? std::string(shortTempPath) : tempPath;

  DWORD attrs = GetFileAttributesA(finalZipPath.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    DeleteFileA(finalZipPath.c_str());
  }

  std::string finalCmd =
      "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass "
      "-Command \"& {";
  finalCmd += "Add-Type -AssemblyName System.IO.Compression.FileSystem; ";
  finalCmd += "try { [System.IO.Compression.ZipFile]::CreateFromDirectory('";
  finalCmd += finalTempPath + "', '" + finalZipPath + "'); exit 0; } ";
  finalCmd += "catch { Write-Error $_.Exception.Message; exit 1; } }\"";

  printf("[DEBUG] PowerShell CMD: %s\n", finalCmd.c_str());
  int result = ExecuteHiddenPowerShell(finalCmd);
  printf("[DEBUG] PowerShell Result: %d\n", result);

  cleanup_temp();

  return (result == 0);
}

bool ExtractZipFileWindows(const std::string& zipPath,
                           std::map<std::string, std::string>& files) {
  const std::string extractPath = CreateUniqueTempDirectory("PCX");
  if (extractPath.empty()) {
    return false;
  }

  auto cleanup_extract = [&extractPath]() {
    DeleteDirectoryTreeRecursive(extractPath);
  };

  char shortZipPath[MAX_PATH] = {0};
  char shortExtractPath[MAX_PATH] = {0};

  DWORD zipResult = GetShortPathNameA(zipPath.c_str(), shortZipPath, MAX_PATH);
  DWORD extractResult =
      GetShortPathNameA(extractPath.c_str(), shortExtractPath, MAX_PATH);

  std::string finalZipPath =
      (zipResult > 0) ? std::string(shortZipPath) : zipPath;
  std::string finalExtractPath =
      (extractResult > 0) ? std::string(shortExtractPath) : extractPath;

  std::string powershellCmd =
      "powershell.exe -ExecutionPolicy Bypass -Command \"& {";
  powershellCmd += "Add-Type -AssemblyName System.IO.Compression.FileSystem; ";
  powershellCmd +=
      "try { [System.IO.Compression.ZipFile]::ExtractToDirectory('";
  powershellCmd += finalZipPath + "', '" + finalExtractPath + "'); exit 0; } ";
  powershellCmd += "catch { Write-Error $_.Exception.Message; exit 1; } }\"";

  printf("[DEBUG] Extract CMD: %s\n", powershellCmd.c_str());
  int result = ExecuteHiddenPowerShell(powershellCmd);
  printf("[DEBUG] Extract Result: %d\n", result);

  if (result == 0) {
    printf("[DEBUG] PowerShell extraction successful, reading files from: %s\n",
            extractPath.c_str());
    ReadDirectoryTreeRecursive(extractPath, "", files);

    printf("[DEBUG] Total files extracted: %zu\n", files.size());
    cleanup_extract();
    return true;
  } else {
    printf("[DEBUG] PowerShell extraction failed with code: %d\n", result);
  }

  cleanup_extract();
  return false;
}
#endif

namespace plc {

namespace {
std::string EscapeCSVField(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 2);
  for (char ch : value) {
    if (ch == '"') {
      out.push_back('"');
    }
    out.push_back(ch);
  }
  return out;
}

std::string JoinCSVLine(const std::vector<std::string>& fields) {
  std::ostringstream line;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i > 0) {
      line << "\t";
    }
    line << '"' << EscapeCSVField(fields[i]) << '"';
  }
  return line.str();
}

std::string TrimField(const std::string& value) {
  size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

std::vector<std::string> SplitCSVLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  bool inQuotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char ch = line[i];
    if (ch == '"') {
      if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
        field.push_back('"');
        ++i;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (ch == '\t' && !inQuotes) {
      fields.push_back(field);
      field.clear();
    } else {
      field.push_back(ch);
    }
  }

  fields.push_back(field);
  for (auto& f : fields) {
    f = TrimField(f);
    if (f.size() >= 2 && f.front() == '"' && f.back() == '"') {
      f = f.substr(1, f.size() - 2);
    }
  }

  return fields;
}

bool TryParseInt(const std::string& text, int* value) {
  if (!value || text.empty()) {
    return false;
  }
  char* end = nullptr;
  long parsed = std::strtol(text.c_str(), &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  if (parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

std::string FormatDeviceAddress(const std::string& address) {
  if (address.size() < 2) {
    return address;
  }

  char type = address[0];
  std::string digits = address.substr(1);
  int value = 0;
  if ((type == 'X' || type == 'Y') && TryParseInt(digits, &value)) {
    std::ostringstream oss;
    oss << type << std::setw(3) << std::setfill('0') << value;
    return oss.str();
  }

  return address;
}

std::string NormalizePreset(const std::string& preset) {
  if (preset.empty()) {
    return "";
  }
  if (preset[0] == 'K' || preset[0] == 'k') {
    return preset.substr(1);
  }
  return preset;
}

std::string BuildRungMemoNote(const std::string& memo) {
  if (memo.empty()) {
    return "";
  }
  return "RUNG_MEMO=" + memo;
}

std::string BuildOrGroupNote(int x, int outputBranchIndex) {
  std::ostringstream oss;
  oss << "VCX=" << x;
  if (outputBranchIndex >= 0) {
    oss << ";OUTB=" << outputBranchIndex;
  }
  return oss.str();
}

bool ParseRungMemoNote(const std::string& note, std::string* memo) {
  static const std::string kMemoPrefix = "RUNG_MEMO=";
  if (note.rfind(kMemoPrefix, 0) != 0) {
    return false;
  }
  if (memo) {
    *memo = note.substr(kMemoPrefix.size());
  }
  return true;
}

void ParseOrGroupNote(const std::string& note,
                      int* x,
                      int* outputBranchIndex) {
  if (x) {
    *x = -1;
  }
  if (outputBranchIndex) {
    *outputBranchIndex = 0;
  }
  if (note.empty()) {
    return;
  }

  std::stringstream ss(note);
  std::string token;
  while (std::getline(ss, token, ';')) {
    token = TrimField(token);
    if (token.rfind("VCX=", 0) == 0) {
      int value = 0;
      if (TryParseInt(token.substr(4), &value) && x) {
        *x = value;
      }
      continue;
    }
    if (token.rfind("OUTB=", 0) == 0) {
      int value = 0;
      if (TryParseInt(token.substr(5), &value) && outputBranchIndex) {
        *outputBranchIndex = std::max(0, value);
      }
    }
  }
}

std::string BuildGX2CSV(const plc::LadderProgram& program,
                        const std::string& projectName) {
  plc::LadderProgram canonical_program = program;
  plc::CanonicalizeLadderProgram(&canonical_program);

  std::vector<std::string> lines;
  std::string name = projectName.empty() ? "Untitled Project" : projectName;
  lines.push_back(JoinCSVLine({"(" + name + ")"}));
  lines.push_back(JoinCSVLine({"PLC Information:", "FXCPU FX3U/FX3UC"}));
  lines.push_back(JoinCSVLine({"Step No.", "Line Statement", "Instruction",
                               "I/O(Device)", "Blank", "PI Statement",
                               "Note"}));

  int step = 0;

  auto add_instruction = [&](const std::string& instruction,
                             const std::string& device) {
    lines.push_back(JoinCSVLine({std::to_string(step), "", instruction,
                                 device, "", "", ""}));
    step++;
  };

  auto add_instruction_with_note = [&](const std::string& instruction,
                                       const std::string& device,
                                       const std::string& note) {
    lines.push_back(JoinCSVLine({std::to_string(step), "", instruction,
                                 device, "", "", note}));
    step++;
  };

  auto add_blank = [&](const std::string& device) {
    lines.push_back(JoinCSVLine({"", "", "", device, "", "", ""}));
  };

  auto extract_rung = [&](const plc::Rung& rung,
                          std::vector<plc::LadderInstruction>& contacts,
                          plc::LadderInstruction& output,
                          bool& hasOutput) {
    contacts.clear();
    hasOutput = false;

    for (const auto& cell : rung.cells) {
      if (cell.type == plc::LadderInstructionType::XIC ||
          cell.type == plc::LadderInstructionType::XIO) {
        contacts.push_back(cell);
        continue;
      }

      if (cell.type == plc::LadderInstructionType::OTE ||
          cell.type == plc::LadderInstructionType::SET ||
          cell.type == plc::LadderInstructionType::RST ||
          cell.type == plc::LadderInstructionType::RST_TMR_CTR ||
          cell.type == plc::LadderInstructionType::TON ||
          cell.type == plc::LadderInstructionType::CTU ||
          cell.type == plc::LadderInstructionType::BKRST) {
        output = cell;
        hasOutput = true;
        break;
      }
    }
  };

  struct OrGroup {
    int x = 0;
    std::vector<int> rungs;
    size_t sourceIndex = 0;
  };

  std::vector<OrGroup> groups;
  groups.reserve(canonical_program.verticalConnections.size());
  for (const auto& conn : canonical_program.verticalConnections) {
    if (!conn.rungs.empty()) {
      OrGroup group;
      group.x = conn.x;
      group.rungs = conn.rungs;
      group.sourceIndex = groups.size();
      std::sort(group.rungs.begin(), group.rungs.end());
      group.rungs.erase(std::unique(group.rungs.begin(), group.rungs.end()),
                        group.rungs.end());
      groups.push_back(group);
    }
  }
  std::stable_sort(groups.begin(), groups.end(),
                   [](const OrGroup& lhs, const OrGroup& rhs) {
               int lhs_start = lhs.rungs.empty() ? std::numeric_limits<int>::max()
                                                 : lhs.rungs.front();
               int rhs_start = rhs.rungs.empty() ? std::numeric_limits<int>::max()
                                                 : rhs.rungs.front();
               if (lhs_start != rhs_start) {
                 return lhs_start < rhs_start;
               }
               const int lhs_end = lhs.rungs.empty() ? std::numeric_limits<int>::min()
                                                     : lhs.rungs.back();
               const int rhs_end = rhs.rungs.empty() ? std::numeric_limits<int>::min()
                                                     : rhs.rungs.back();
               if (lhs_end != rhs_end) {
                 return lhs_end < rhs_end;
               }
               if (lhs.x != rhs.x) {
                 return lhs.x < rhs.x;
               }
               return lhs.sourceIndex < rhs.sourceIndex;
             });

  auto emit_contacts = [&](const std::vector<plc::LadderInstruction>& contacts,
                           bool isFirstRung,
                           const std::string& rungMemo) {
    std::string memoNote = BuildRungMemoNote(rungMemo);
    if (contacts.empty()) {
      if (memoNote.empty()) {
        add_instruction(isFirstRung ? "LD" : "OR", "M8000");
      } else {
        add_instruction_with_note(isFirstRung ? "LD" : "OR", "M8000",
                                  memoNote);
      }
      return;
    }

    for (size_t i = 0; i < contacts.size(); ++i) {
      const auto& contact = contacts[i];
      bool negated = (contact.type == plc::LadderInstructionType::XIO);
      if (i == 0) {
        std::string firstInstruction = "";
        if (isFirstRung) {
          firstInstruction = negated ? "LDI" : "LD";
        } else {
          firstInstruction = negated ? "ORI" : "OR";
        }
        std::string firstDevice = FormatDeviceAddress(contact.address);
        if (memoNote.empty()) {
          add_instruction(firstInstruction, firstDevice);
        } else {
          add_instruction_with_note(firstInstruction, firstDevice, memoNote);
        }
      } else {
        add_instruction(negated ? "ANI" : "AND",
                        FormatDeviceAddress(contact.address));
      }
    }
  };

  auto emit_output = [&](const plc::LadderInstruction& output) {
    std::string outAddress = FormatDeviceAddress(output.address);
    switch (output.type) {
      case plc::LadderInstructionType::BKRST: {
        std::string count = output.preset;
        if (count.empty()) {
          count = "K1";
        } else if (count[0] != 'K' && count[0] != 'k') {
          count = "K" + count;
        }
        add_instruction("BKRST", outAddress + " " + count);
        break;
      }
      case plc::LadderInstructionType::SET:
        add_instruction("SET", outAddress);
        break;
      case plc::LadderInstructionType::RST:
      case plc::LadderInstructionType::RST_TMR_CTR:
        add_instruction("RST", outAddress);
        break;
      case plc::LadderInstructionType::TON:
      case plc::LadderInstructionType::CTU: {
        add_instruction("OUT", outAddress);
        std::string preset = NormalizePreset(output.preset);
        if (!preset.empty()) {
          add_blank("K" + preset);
        }
        break;
      }
      case plc::LadderInstructionType::OTE:
      default:
        add_instruction("OUT", outAddress);
        if (!output.preset.empty() && !outAddress.empty() &&
            (outAddress[0] == 'T' || outAddress[0] == 'C')) {
          std::string preset = NormalizePreset(output.preset);
          if (!preset.empty()) {
            add_blank("K" + preset);
          }
        }
        break;
    }
  };

  auto emit_group = [&](const OrGroup& group) {
    plc::LadderInstruction output;
    bool hasOutput = false;
    bool firstRung = true;
    int outputBranchIndex = 0;
    int emittedBranchIndex = 0;

    for (int rungIndex : group.rungs) {
       if (rungIndex < 0 || rungIndex >=
                                static_cast<int>(canonical_program.rungs.size())) {
         continue;
       }
       const auto& rung =
           canonical_program.rungs[static_cast<size_t>(rungIndex)];
       if (rung.isEndRung) {
         continue;
       }

      std::vector<plc::LadderInstruction> contacts;
      plc::LadderInstruction rungOutput;
      bool rungHasOutput = false;
      extract_rung(rung, contacts, rungOutput, rungHasOutput);

      if (!hasOutput && rungHasOutput) {
        output = rungOutput;
        hasOutput = true;
        outputBranchIndex = emittedBranchIndex;
      }

      emit_contacts(contacts, firstRung, rung.memo);
      firstRung = false;
      ++emittedBranchIndex;
    }

    if (!hasOutput) {
      return;
    }

    if (group.rungs.size() > 1) {
      add_instruction_with_note("ORB", "",
                                BuildOrGroupNote(group.x, outputBranchIndex));
    }
    emit_output(output);
  };

  std::set<int> emittedGroupedRungs;
  size_t nextGroupIndex = 0;
  for (size_t i = 0; i < canonical_program.rungs.size(); ++i) {
    const auto& rung = canonical_program.rungs[i];
    if (rung.isEndRung) {
      break;
    }

    bool emittedGroupAtRung = false;
    while (nextGroupIndex < groups.size()) {
      const auto& group = groups[nextGroupIndex];
      if (group.rungs.empty()) {
        ++nextGroupIndex;
        continue;
      }
      const int groupStart = group.rungs.front();
      if (groupStart < static_cast<int>(i)) {
        ++nextGroupIndex;
        continue;
      }
      if (groupStart != static_cast<int>(i)) {
        break;
      }

      emit_group(group);
      emittedGroupAtRung = true;
      for (int rungIndex : group.rungs) {
        emittedGroupedRungs.insert(rungIndex);
      }
      ++nextGroupIndex;
    }
    if (emittedGroupAtRung || emittedGroupedRungs.count(static_cast<int>(i)) > 0) {
      continue;
    }

    std::vector<plc::LadderInstruction> contacts;
    plc::LadderInstruction output;
    bool hasOutput = false;
    extract_rung(rung, contacts, output, hasOutput);

    if (!hasOutput) {
      continue;
    }

    emit_contacts(contacts, true, rung.memo);
    emit_output(output);
  }

  lines.push_back(JoinCSVLine({std::to_string(step), "", "END", "", "", "", ""}));

  std::ostringstream out;
  for (size_t i = 0; i < lines.size(); ++i) {
    out << lines[i];
    if (i + 1 < lines.size()) {
      out << "\n";
    }
  }

  return out.str();
}

bool ParseGX2CSV(const std::string& csvContent, plc::LadderProgram& program) {
  program.rungs.clear();
  program.verticalConnections.clear();

  std::istringstream stream(csvContent);
  std::string line;
  bool dataSection = false;

  std::vector<std::vector<plc::LadderInstruction>> branches;
  std::vector<std::string> branchMemos;
  std::vector<plc::LadderInstruction> current;
  std::string currentBranchMemo;
  struct PendingPresetTarget {
    int rungIndex = -1;
    int cellIndex = -1;
  };
  std::vector<PendingPresetTarget> pendingPresetTargets;
  int pending_or_x = -1;
  int pending_output_branch_index = 0;

  auto flush_output = [&](const plc::LadderInstruction& output,
                          int xHint,
                          int outputBranchIndex) {
    if (!current.empty() || !currentBranchMemo.empty()) {
      branches.push_back(current);
      branchMemos.push_back(currentBranchMemo);
    }
    if (branches.empty()) {
      branches.push_back({});
      branchMemos.push_back("");
    }

    int outputCol = 11;
    int maxContacts = 0;
    for (const auto& branch : branches) {
      int count = static_cast<int>(branch.size());
      if (count > maxContacts) {
        maxContacts = count;
      }
    }
    if (maxContacts > outputCol) {
      outputCol = std::min(maxContacts, 11);
    }

    const int clampedOutputBranchIndex = std::max(
        0, std::min(outputBranchIndex, static_cast<int>(branches.size()) - 1));

    std::vector<int> rungIndices;
    for (size_t branchIndex = 0; branchIndex < branches.size();
         ++branchIndex) {
      const auto& branch = branches[branchIndex];
      plc::Rung rung;
      int rungIndex = static_cast<int>(program.rungs.size());
      rung.number = rungIndex;
      rung.cells.assign(12, plc::LadderInstruction());
      if (branchIndex < branchMemos.size()) {
        rung.memo = branchMemos[branchIndex];
      }

      int cellIndex = 0;
      for (const auto& contact : branch) {
        if (cellIndex >= outputCol) {
          break;
        }
        rung.cells[cellIndex] = contact;
        cellIndex++;
      }

      if (static_cast<int>(branchIndex) == clampedOutputBranchIndex) {
        for (int col = cellIndex; col < outputCol; ++col) {
          rung.cells[col].type = plc::LadderInstructionType::HLINE;
        }
      }

      if (static_cast<int>(branchIndex) == clampedOutputBranchIndex &&
          outputCol < 12) {
        rung.cells[outputCol] = output;
      }

      program.rungs.push_back(rung);
      rungIndices.push_back(rung.number);

      if (static_cast<int>(branchIndex) == clampedOutputBranchIndex &&
          outputCol < 12) {
        auto& storedCell = program.rungs.back().cells[outputCol];
        if ((storedCell.type == plc::LadderInstructionType::TON ||
             storedCell.type == plc::LadderInstructionType::CTU) &&
            storedCell.preset.empty()) {
          pendingPresetTargets.push_back(
              {static_cast<int>(program.rungs.size()) - 1, outputCol});
        }
      }
    }

    if (rungIndices.size() > 1) {
      plc::VerticalConnection vc;
      vc.x = xHint >= 0 ? xHint : 0;
      vc.rungs = rungIndices;
      program.verticalConnections.push_back(vc);
    }

    branches.clear();
    branchMemos.clear();
    current.clear();
    currentBranchMemo.clear();
    pending_or_x = -1;
    pending_output_branch_index = 0;
  };

  while (std::getline(stream, line)) {
    auto fields = SplitCSVLine(line);
    if (fields.empty()) {
      continue;
    }

    if (!dataSection) {
      if (fields.size() > 0 && fields[0] == "Step No.") {
        dataSection = true;
      }
      continue;
    }

    if (fields.size() < 4) {
      continue;
    }

    std::string instr = TrimField(fields[2]);
    std::string device = TrimField(fields[3]);
    std::string note = (fields.size() > 6) ? TrimField(fields[6]) : "";

    if (instr.empty()) {
      if (!device.empty() && (device[0] == 'K' || device[0] == 'k') &&
          !pendingPresetTargets.empty()) {
        std::string preset = "K" + device.substr(1);
        for (const auto& target : pendingPresetTargets) {
          if (target.rungIndex >= 0 &&
              target.rungIndex < static_cast<int>(program.rungs.size()) &&
              target.cellIndex >= 0 && target.cellIndex < 12) {
            program.rungs[static_cast<size_t>(target.rungIndex)]
                .cells[static_cast<size_t>(target.cellIndex)]
                .preset = preset;
          }
        }
        pendingPresetTargets.clear();
      }
      continue;
    }

    if (instr == "END") {
      break;
    }

    if (instr == "LD" || instr == "LDI" || instr == "AND" || instr == "ANI" ||
        instr == "OR" || instr == "ORI") {
      plc::LadderInstruction contact;
      contact.type =
          (instr == "LDI" || instr == "ANI" || instr == "ORI")
              ? plc::LadderInstructionType::XIO
              : plc::LadderInstructionType::XIC;
      contact.address = device;

      std::string parsedMemo;
      bool hasMemo = ParseRungMemoNote(note, &parsedMemo);

      if (instr == "OR" || instr == "ORI") {
        if (!current.empty()) {
          branches.push_back(current);
          branchMemos.push_back(currentBranchMemo);
          current.clear();
          currentBranchMemo.clear();
        }
      }

      if (current.empty() && hasMemo) {
        currentBranchMemo = parsedMemo;
      }

      current.push_back(contact);
      continue;
    }

    if (instr == "ORB") {
      ParseOrGroupNote(note, &pending_or_x, &pending_output_branch_index);
      continue;
    }

    if (instr == "BKRST" || instr == "BKRSTP") {
      plc::LadderInstruction output;
      output.type = plc::LadderInstructionType::BKRST;
      std::string base = device;
      std::string count_token;

      size_t split = device.find_first_of(" \t");
      if (split != std::string::npos) {
        base = TrimField(device.substr(0, split));
        count_token = TrimField(device.substr(split + 1));
      }

      if (count_token.empty() && fields.size() > 4) {
        count_token = TrimField(fields[4]);
      }

      if (count_token.empty()) {
        count_token = "K1";
      } else if (count_token[0] != 'K' && count_token[0] != 'k') {
        count_token = "K" + count_token;
      }

      output.address = base.empty() ? device : base;
      output.preset = count_token;

      flush_output(output, pending_or_x, pending_output_branch_index);
      continue;
    }

    if (instr == "OUT" || instr == "SET" || instr == "RST" || instr == "TON" ||
        instr == "CTU") {
      plc::LadderInstruction output;
      output.address = device;
      if ((instr == "OUT" || instr == "TON" || instr == "CTU") &&
          !device.empty() && device[0] == 'T') {
        output.type = plc::LadderInstructionType::TON;
      } else if ((instr == "OUT" || instr == "CTU") && !device.empty() &&
                 device[0] == 'C') {
        output.type = plc::LadderInstructionType::CTU;
      } else if (instr == "SET") {
        output.type = plc::LadderInstructionType::SET;
      } else if (instr == "RST") {
        output.type = (!device.empty() && (device[0] == 'T' || device[0] == 'C'))
                          ? plc::LadderInstructionType::RST_TMR_CTR
                          : plc::LadderInstructionType::RST;
      } else {
        output.type = plc::LadderInstructionType::OTE;
      }

      flush_output(output, pending_or_x, pending_output_branch_index);
      continue;
    }
  }

  plc::Rung endRung;
  endRung.isEndRung = true;
  program.rungs.push_back(endRung);
  plc::CanonicalizeLadderProgram(&program);

  return !program.rungs.empty();
}
}  // namespace

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

  LogDebug("[CSV] Starting project save to: " + filePath);

  std::string resolvedName =
      projectName.empty() ? ExtractFileName(filePath) : projectName;
  std::string csvContent = BuildGX2CSV(program, resolvedName);
  if (csvContent.empty()) {
    result.success = false;
    result.errorMessage = "Failed to serialize ladder program to CSV";
    SetError(result.errorMessage);
    return result;
  }

  std::ofstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    result.success = false;
    result.errorMessage = "Cannot open output file: " + filePath;
    SetError(result.errorMessage);
    return result;
  }

  file << csvContent;
  file.flush();
  std::streampos endPos = file.tellp();
  file.close();

  result.success = true;
  result.savedPath = filePath;
  result.compressedSize = endPos > 0 ? static_cast<size_t>(endPos) : 0;

  result.info.projectName = resolvedName;
  result.info.version = "1.0";
  result.info.createdDate = GetCurrentDateTime();
  result.info.lastModified = result.info.createdDate;
  result.info.toolVersion = "PLC Simulator v1.0.4";
  result.info.schemaVersion = "1.0";
  result.info.layoutChecksum = CalculateChecksum(csvContent);
  result.info.programChecksum = result.info.layoutChecksum;

  LogDebug("[CSV] Project saved successfully: " +
           std::to_string(result.compressedSize) + " bytes");

  return result;
}

ProjectFileManager::SaveResult ProjectFileManager::SaveProjectPackage(
    const LadderProgram& program, const std::string& layoutJson,
    const std::string& rtlLibraryJson,
    const std::map<std::string, std::map<std::string, std::string>>& rtlArtifacts,
    const std::string& filePath, const std::string& projectName) {
  SaveResult result;
  last_error_.clear();

  LogDebug("[PKG] Starting project package save to: " + filePath);

  std::string resolvedName =
      projectName.empty() ? ExtractFileName(filePath) : projectName;
  std::string csvContent = BuildGX2CSV(program, resolvedName);
  if (csvContent.empty()) {
    result.success = false;
    result.errorMessage = "Failed to serialize ladder program to CSV";
    SetError(result.errorMessage);
    return result;
  }
  if (layoutJson.empty()) {
    result.success = false;
    result.errorMessage = "Layout JSON is empty";
    SetError(result.errorMessage);
    return result;
  }

  result.info.projectName = resolvedName;
  result.info.version = "2.0";
  result.info.createdDate = GetCurrentDateTime();
  result.info.lastModified = result.info.createdDate;
  result.info.toolVersion = "PLC Simulator v1.0.4";
  result.info.schemaVersion = "2.0";
  result.info.layoutChecksum = CalculateChecksum(layoutJson);
  result.info.programChecksum = CalculateChecksum(csvContent);

  std::map<std::string, std::string> files;
  files["program.csv"] = csvContent;
  files["layout.json"] = layoutJson;
  if (!rtlLibraryJson.empty()) {
    files["rtl_library.json"] = rtlLibraryJson;
  }
  
  // Pack RTL artifacts
  for (const auto& [moduleId, artifactFiles] : rtlArtifacts) {
    for (const auto& [fileName, data] : artifactFiles) {
      if (moduleId.empty() || fileName.empty() || data.empty()) {
        continue;
      }
      files["rtl_artifacts/" + moduleId + "/" + fileName] = data;
    }
  }

  files["manifest.json"] = GenerateManifest(result.info);

  if (!CreateZipFile(filePath, files)) {
    result.success = false;
    result.errorMessage = GetLastError().empty()
                              ? "Failed to create project package"
                              : GetLastError();
    return result;
  }

  std::ifstream saved_file(filePath, std::ios::binary | std::ios::ate);
  size_t saved_size = 0;
  if (saved_file.is_open()) {
    std::streampos end_pos = saved_file.tellg();
    if (end_pos > 0) {
      saved_size = static_cast<size_t>(end_pos);
    }
  }

  result.success = true;
  result.savedPath = filePath;
  result.compressedSize = saved_size;
  LogDebug("[PKG] Project package saved successfully");
  return result;
}

ProjectFileManager::SaveResult ProjectFileManager::SaveRtlComponentPackage(
    const std::string& entryJson,
    const std::map<std::string, std::string>& artifactFiles,
    const std::string& filePath,
    const std::string& componentName) {
  SaveResult result;
  last_error_.clear();

  LogDebug("[RTLCOMP] Starting RTL component package save to: " + filePath);

  if (entryJson.empty()) {
    result.success = false;
    result.errorMessage = "RTL component JSON is empty";
    SetError(result.errorMessage);
    return result;
  }

  result.info.projectName =
      componentName.empty() ? ExtractFileName(filePath) : componentName;
  result.info.version = "1.0";
  result.info.createdDate = GetCurrentDateTime();
  result.info.lastModified = result.info.createdDate;
  result.info.toolVersion = "PLC Simulator v1.0.4";
  result.info.schemaVersion = "rtl-component-1.0";
  result.info.programChecksum = CalculateChecksum(entryJson);

  std::map<std::string, std::string> files;
  files["rtl_component.json"] = entryJson;
  for (const auto& [fileName, data] : artifactFiles) {
    if (!fileName.empty() && !data.empty()) {
      files["rtl_artifacts/" + fileName] = data;
    }
  }
  files["manifest.json"] = GenerateManifest(result.info);

  if (!CreateZipFile(filePath, files)) {
    result.success = false;
    result.errorMessage = GetLastError().empty()
                              ? "Failed to create RTL component package"
                              : GetLastError();
    return result;
  }

  std::ifstream saved_file(filePath, std::ios::binary | std::ios::ate);
  if (saved_file.is_open()) {
    const std::streampos end_pos = saved_file.tellg();
    if (end_pos > 0) {
      result.compressedSize = static_cast<size_t>(end_pos);
    }
  }

  result.success = true;
  result.savedPath = filePath;
  LogDebug("[RTLCOMP] RTL component package saved successfully");
  return result;
}

ProjectFileManager::LoadResult ProjectFileManager::LoadProject(
    const std::string& filePath) {
  LoadResult result;
  last_error_.clear();

  LogDebug("[CSV] Starting project load from: " + filePath);

  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    result.success = false;
    result.errorMessage = "Cannot open CSV file: " + filePath;
    SetError(result.errorMessage);
    return result;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string csvContent = buffer.str();
  file.close();

  LadderProgram program;
  if (!ParseGX2CSV(csvContent, program)) {
    result.success = false;
    result.errorMessage = "Failed to parse GXWORKS2 CSV content";
    SetError(result.errorMessage);
    return result;
  }

  result.success = true;
  result.program = program;
  result.info.projectName = ExtractFileName(filePath);
  result.info.version = "1.0";
  result.info.createdDate = "";
  result.info.lastModified = "";
  result.info.toolVersion = "PLC Simulator v1.0.4";
  result.info.schemaVersion = "1.0";
  result.info.layoutChecksum = CalculateChecksum(csvContent);
  result.info.programChecksum = result.info.layoutChecksum;

  LogDebug("[CSV] Project loaded successfully: " +
           std::to_string(result.program.rungs.size()) + " rungs");

  return result;
}

ProjectFileManager::LoadResult ProjectFileManager::LoadProjectPackage(
    const std::string& filePath) {
  LoadResult result;
  last_error_.clear();

  LogDebug("[PKG] Starting project package load from: " + filePath);

  std::map<std::string, std::string> files;
  if (!ExtractZipFile(filePath, files)) {
    result.success = false;
    result.errorMessage = GetLastError().empty()
                              ? "Failed to extract project package"
                              : GetLastError();
    SetError(result.errorMessage);
    return result;
  }

  auto csv_it = files.find("program.csv");
  auto layout_it = files.find("layout.json");
  if (csv_it == files.end() || layout_it == files.end()) {
    result.success = false;
    result.errorMessage =
        "Project package is missing required files: program.csv/layout.json";
    SetError(result.errorMessage);
    return result;
  }

  LadderProgram program;
  if (!ParseGX2CSV(csv_it->second, program)) {
    result.success = false;
    result.errorMessage = "Failed to parse packaged GXWORKS2 CSV content";
    SetError(result.errorMessage);
    return result;
  }

  result.success = true;
  result.program = program;
  result.layoutJson = layout_it->second;
  auto rtl_it = files.find("rtl_library.json");
  if (rtl_it != files.end()) {
    result.rtlLibraryJson = rtl_it->second;
  }
  
  // Extract RTL artifacts
  for (const auto& [fileName, content] : files) {
    if (fileName.rfind("rtl_artifacts/", 0) == 0) {
      std::string relativePath = fileName.substr(14); // remove "rtl_artifacts/"
      size_t slash_pos = relativePath.find_first_of("/\\");
      if (slash_pos == std::string::npos) {
        std::string moduleId = relativePath;
        size_t dot_pos = moduleId.find_last_of('.');
        if (dot_pos != std::string::npos) {
          moduleId = moduleId.substr(0, dot_pos);
        }
        if (!moduleId.empty()) {
          result.rtlArtifacts[moduleId]["rtl_worker.exe"] = content;
        }
        continue;
      }

      std::string moduleId = relativePath.substr(0, slash_pos);
      std::string artifactName = relativePath.substr(slash_pos + 1);
      if (!moduleId.empty() && !artifactName.empty()) {
        result.rtlArtifacts[moduleId][artifactName] = content;
      }
    }
  }

  result.info.projectName = ExtractFileName(filePath);
  result.info.version = "2.0";
  result.info.toolVersion = "PLC Simulator v1.0.4";
  result.info.schemaVersion = "2.0";
  result.info.layoutChecksum = CalculateChecksum(result.layoutJson);
  result.info.programChecksum = CalculateChecksum(csv_it->second);

  auto manifest_it = files.find("manifest.json");
  if (manifest_it != files.end()) {
    ParseManifest(manifest_it->second, result.info);
  }

  LogDebug("[PKG] Project package loaded successfully");
  return result;
}

ProjectFileManager::RtlComponentLoadResult
ProjectFileManager::LoadRtlComponentPackage(const std::string& filePath) {
  RtlComponentLoadResult result;
  last_error_.clear();

  LogDebug("[RTLCOMP] Starting RTL component package load from: " + filePath);

  std::map<std::string, std::string> files;
  if (!ExtractZipFile(filePath, files)) {
    result.errorMessage = GetLastError().empty()
                              ? "Failed to extract RTL component package"
                              : GetLastError();
    SetError(result.errorMessage);
    return result;
  }

  auto entry_it = files.find("rtl_component.json");
  if (entry_it == files.end()) {
    result.errorMessage =
        "RTL component package is missing required file: rtl_component.json";
    SetError(result.errorMessage);
    return result;
  }

  result.success = true;
  result.entryJson = entry_it->second;
  for (const auto& [fileName, content] : files) {
    if (fileName.rfind("rtl_artifacts/", 0) == 0) {
      const std::string artifactName = fileName.substr(14);
      if (!artifactName.empty()) {
        result.artifactFiles[artifactName] = content;
      }
    }
  }

  result.info.projectName = ExtractFileName(filePath);
  result.info.version = "1.0";
  result.info.toolVersion = "PLC Simulator v1.0.4";
  result.info.schemaVersion = "rtl-component-1.0";
  result.info.programChecksum = CalculateChecksum(result.entryJson);

  auto manifest_it = files.find("manifest.json");
  if (manifest_it != files.end()) {
    ParseManifest(manifest_it->second, result.info);
  }

  LogDebug("[RTLCOMP] RTL component package loaded successfully");
  return result;
}

ProjectFileManager::ProjectInfo ProjectFileManager::GetProjectInfo(
    const std::string& filePath) {
  ProjectInfo info;
  info.projectName = ExtractFileName(filePath);
  info.version = "1.0";
  info.toolVersion = "PLC Simulator v1.0.4";
  info.schemaVersion = "1.0";

  std::ifstream file(filePath, std::ios::binary);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string csvContent = buffer.str();
    info.layoutChecksum = CalculateChecksum(csvContent);
    info.programChecksum = info.layoutChecksum;
  }

  return info;
}

bool ProjectFileManager::ValidateProjectFile(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string csvContent = buffer.str();

  LadderProgram program;
  return ParseGX2CSV(csvContent, program);
}

// =============================================================================
// =============================================================================

bool ProjectFileManager::CreateZipFile(
    const std::string& filePath,
    const std::map<std::string, std::string>& files) {
  LogDebug("[INFO] Creating ZIP file using Windows Shell API: " + filePath);

#ifdef _WIN32
  // Windows Shell API ????
  if (CreateZipFileWindows(filePath, files)) {
    LogDebug("[INFO] ZIP file created successfully with Windows Shell API");
    return true;
  } else {
    SetError("Failed to create ZIP file using Windows Shell API");
    return false;
  }
#else
  SetError("Windows-only project: Shell API not available on this platform");
  return false;
#endif
}

bool ProjectFileManager::ExtractZipFile(
    const std::string& filePath, std::map<std::string, std::string>& files) {
  LogDebug("[INFO] Extracting ZIP file using Windows Shell API: " + filePath);

#ifdef _WIN32
  // Windows Shell API ????
  if (ExtractZipFileWindows(filePath, files)) {
    LogDebug("[INFO] ZIP file extracted successfully with Windows Shell API");
    LogDebug("[INFO] Files extracted: " + std::to_string(files.size()));
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
  SetError("Windows-only project: Shell API not available on this platform");
  return false;
#endif
}

// =============================================================================
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
// =============================================================================

size_t ProjectFileManager::CalculateChecksum(const std::string& content) {
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
  // Avoid std::filesystem::path conversions to prevent locale issues on
  // non-ASCII paths. Use a manual parse instead.
  size_t lastSlash = filePath.find_last_of("/\\");
  size_t lastDot = filePath.find_last_of('.');

  std::string filename = (lastSlash != std::string::npos)
                             ? filePath.substr(lastSlash + 1)
                             : filePath;

  if (lastDot != std::string::npos && lastDot > lastSlash) {
    filename = filename.substr(
        0, lastDot - (lastSlash != std::string::npos ? lastSlash + 1 : 0));
  }

  return filename;
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
