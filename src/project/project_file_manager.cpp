// project_file_manager.cpp
//
// Implementation of project file manager.

#include "plc_emulator/project/project_file_manager.h"

#include "miniz.h"

#include <chrono>
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
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <windows.h>


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

bool CreateZipFileWindows(const std::string& zipPath,
                          const std::map<std::string, std::string>& files) {
  char tempDir[MAX_PATH];
  GetTempPathA(MAX_PATH, tempDir);
  std::string tempPath = std::string(tempDir) + "PLCProject_temp\\";
  CreateDirectoryA(tempPath.c_str(), nullptr);

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

  auto ensure_parent_dir = [](const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      std::string dir = path.substr(0, pos);
      if (!dir.empty()) {
        CreateDirectoryA(dir.c_str(), nullptr);
      }
    }
  };
  ensure_parent_dir(zipPath);

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

  for (const auto& [fileName, content] : files) {
    std::string tempFilePath = tempPath + fileName;
    DeleteFileA(tempFilePath.c_str());
  }
  RemoveDirectoryA(tempPath.c_str());

  return (result == 0);
}

bool ExtractZipFileWindows(const std::string& zipPath,
                           std::map<std::string, std::string>& files) {
  char tempDir[MAX_PATH];
  GetTempPathA(MAX_PATH, tempDir);
  std::string extractPath = std::string(tempDir) + "PLCProject_extract\\";
  CreateDirectoryA(extractPath.c_str(), nullptr);

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

  RemoveDirectoryA(extractPath.c_str());
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

std::string BuildGX2CSV(const plc::LadderProgram& program,
                        const std::string& projectName) {
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
          cell.type == plc::LadderInstructionType::TON ||
          cell.type == plc::LadderInstructionType::CTU) {
        output = cell;
        hasOutput = true;
        break;
      }
    }
  };

  struct OrGroup {
    int x = 0;
    std::vector<int> rungs;
  };

  std::set<int> groupedRungs;
  std::vector<OrGroup> groups;
  groups.reserve(program.verticalConnections.size());
  for (const auto& conn : program.verticalConnections) {
    if (!conn.rungs.empty()) {
      OrGroup group;
      group.x = conn.x;
      group.rungs = conn.rungs;
      groups.push_back(group);
      for (int rungIndex : conn.rungs) {
        groupedRungs.insert(rungIndex);
      }
    }
  }

  auto emit_contacts = [&](const std::vector<plc::LadderInstruction>& contacts,
                           bool isFirstRung) {
    if (contacts.empty()) {
      add_instruction(isFirstRung ? "LD" : "OR", "M8000");
      return;
    }

    for (size_t i = 0; i < contacts.size(); ++i) {
      const auto& contact = contacts[i];
      bool negated = (contact.type == plc::LadderInstructionType::XIO);
      if (i == 0) {
        if (isFirstRung) {
          add_instruction(negated ? "LDI" : "LD",
                          FormatDeviceAddress(contact.address));
        } else {
          add_instruction(negated ? "ORI" : "OR",
                          FormatDeviceAddress(contact.address));
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

  for (const auto& group : groups) {
    plc::LadderInstruction output;
    bool hasOutput = false;
    bool firstRung = true;

    for (int rungIndex : group.rungs) {
      if (rungIndex < 0 || rungIndex >=
                              static_cast<int>(program.rungs.size())) {
        continue;
      }
      const auto& rung = program.rungs[static_cast<size_t>(rungIndex)];
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
      }

      emit_contacts(contacts, firstRung);
      firstRung = false;
    }

    if (!hasOutput) {
      continue;
    }

    if (group.rungs.size() > 1) {
      add_instruction_with_note("ORB", "",
                                "VCX=" + std::to_string(group.x));
    }
    emit_output(output);
  }

  for (size_t i = 0; i < program.rungs.size(); ++i) {
    const auto& rung = program.rungs[i];
    if (rung.isEndRung) {
      break;
    }

    if (groupedRungs.count(static_cast<int>(i)) > 0) {
      continue;
    }

    std::vector<plc::LadderInstruction> contacts;
    plc::LadderInstruction output;
    bool hasOutput = false;
    extract_rung(rung, contacts, output, hasOutput);

    if (!hasOutput) {
      continue;
    }

    emit_contacts(contacts, true);
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
  std::vector<plc::LadderInstruction> current;
  std::vector<plc::LadderInstruction*> pendingPresetTargets;
  int pending_or_x = -1;

  auto flush_output = [&](const plc::LadderInstruction& output, int xHint) {
    if (!current.empty()) {
      branches.push_back(current);
    }
    if (branches.empty()) {
      branches.push_back({});
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

    std::vector<int> rungIndices;
    for (size_t branchIndex = 0; branchIndex < branches.size();
         ++branchIndex) {
      const auto& branch = branches[branchIndex];
      plc::Rung rung;
      int rungIndex = static_cast<int>(program.rungs.size());
      rung.number = rungIndex;
      rung.cells.assign(12, plc::LadderInstruction());

      int cellIndex = 0;
      for (const auto& contact : branch) {
        if (cellIndex >= outputCol) {
          break;
        }
        rung.cells[cellIndex] = contact;
        cellIndex++;
      }

      for (int col = cellIndex; col < outputCol; ++col) {
        rung.cells[col].type = plc::LadderInstructionType::HLINE;
      }

      if (branchIndex == 0 && outputCol < 12) {
        rung.cells[outputCol] = output;
      }

      program.rungs.push_back(rung);
      rungIndices.push_back(rung.number);

      if (branchIndex == 0 && outputCol < 12) {
        auto& storedCell = program.rungs.back().cells[outputCol];
        if ((storedCell.type == plc::LadderInstructionType::TON ||
             storedCell.type == plc::LadderInstructionType::CTU) &&
            storedCell.preset.empty()) {
          pendingPresetTargets.push_back(&storedCell);
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
    current.clear();
    pending_or_x = -1;
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

    if (instr.empty()) {
      if (!device.empty() && (device[0] == 'K' || device[0] == 'k') &&
          !pendingPresetTargets.empty()) {
        std::string preset = "K" + device.substr(1);
        for (auto* target : pendingPresetTargets) {
          if (target) {
            target->preset = preset;
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

      if (instr == "OR" || instr == "ORI") {
        if (!current.empty()) {
          branches.push_back(current);
          current.clear();
        }
      }

      current.push_back(contact);
      continue;
    }

    if (instr == "ORB") {
      pending_or_x = -1;
      if (fields.size() > 6) {
        std::string note = TrimField(fields[6]);
        const std::string prefix = "VCX=";
        if (note.rfind(prefix, 0) == 0) {
          int value = 0;
          if (TryParseInt(note.substr(prefix.size()), &value)) {
            pending_or_x = value;
          }
        }
      }
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
        output.type = plc::LadderInstructionType::RST;
      } else {
        output.type = plc::LadderInstructionType::OTE;
      }

      flush_output(output, pending_or_x);
      continue;
    }
  }

  plc::Rung endRung;
  endRung.isEndRung = true;
  program.rungs.push_back(endRung);

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
  result.info.toolVersion = "PLC Simulator v1.0";
  result.info.schemaVersion = "1.0";
  result.info.layoutChecksum = CalculateChecksum(csvContent);
  result.info.programChecksum = result.info.layoutChecksum;

  LogDebug("[CSV] Project saved successfully: " +
           std::to_string(result.compressedSize) + " bytes");

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
  result.info.toolVersion = "PLC Simulator v1.0";
  result.info.schemaVersion = "1.0";
  result.info.layoutChecksum = CalculateChecksum(csvContent);
  result.info.programChecksum = result.info.layoutChecksum;

  LogDebug("[CSV] Project loaded successfully: " +
           std::to_string(result.program.rungs.size()) + " rungs");

  return result;
}

ProjectFileManager::ProjectInfo ProjectFileManager::GetProjectInfo(
    const std::string& filePath) {
  ProjectInfo info;
  info.projectName = ExtractFileName(filePath);
  info.version = "1.0";
  info.toolVersion = "PLC Simulator v1.0";
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
