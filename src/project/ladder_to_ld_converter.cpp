// ladder_to_ld_converter.cpp
// Copyright 2024 PLC Emulator Project
//
// Implementation of LD converter.

#include "plc_emulator/project/ladder_to_ld_converter.h"

#include "plc_emulator/programming/programming_mode.h"  // 🔥 **IMPORTANT**: 실제 타입 정의 포함
#include "plc_emulator/project/ladder_ir.h"  // 🔥 **NEW**: LadderIR 지원

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iomanip>  // 🔥 **NEW**: setw 지원
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>

namespace plc {
namespace {

bool IsContactType(LadderInstructionType type) {
  return type == LadderInstructionType::XIC ||
         type == LadderInstructionType::XIO;
}

bool IsPassableType(LadderInstructionType type) {
  return IsContactType(type) || type == LadderInstructionType::HLINE;
}

bool IsOutputType(LadderInstructionType type) {
  return type == LadderInstructionType::OTE ||
         type == LadderInstructionType::SET ||
         type == LadderInstructionType::RST ||
         type == LadderInstructionType::TON ||
         type == LadderInstructionType::CTU ||
         type == LadderInstructionType::RST_TMR_CTR;
}

bool TryParseDeviceAddress(const std::string& address, char* type,
                           int* index) {
  if (!type || !index || address.size() < 2) {
    return false;
  }

  char device_type = address[0];
  if (device_type != 'X' && device_type != 'Y' && device_type != 'M' &&
      device_type != 'T' && device_type != 'C') {
    return false;
  }

  std::string digits = address.substr(1);
  if (digits.empty()) {
    return false;
  }

  char* end = nullptr;
  long parsed = std::strtol(digits.c_str(), &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  if (parsed < 0 || parsed > std::numeric_limits<int>::max()) {
    return false;
  }

  *type = device_type;
  *index = static_cast<int>(parsed);
  return true;
}

std::string NormalizeAddress(const std::string& address) {
  char type = '\0';
  int index = 0;
  if (!TryParseDeviceAddress(address, &type, &index)) {
    return address;
  }
  std::string normalized;
  normalized.push_back(type);
  normalized += std::to_string(index);
  return normalized;
}

int FindOutputColumn(const Rung& rung, LadderInstruction* output) {
  for (size_t i = 0; i < rung.cells.size(); ++i) {
    const auto& cell = rung.cells[i];
    if (IsOutputType(cell.type)) {
      if (output) {
        *output = cell;
      }
      return static_cast<int>(i);
    }
  }
  return -1;
}

using ConnectionMap = std::map<int, std::vector<std::vector<int>>>;

void BuildVerticalConnectionsByColumn(const LadderProgram& program,
                                      ConnectionMap* connections) {
  if (!connections) {
    return;
  }
  connections->clear();

  for (const auto& conn : program.verticalConnections) {
    if (conn.rungs.empty()) {
      continue;
    }
    (*connections)[conn.x].push_back(conn.rungs);
  }
}

std::set<int> BuildRungComponent(const LadderProgram& program, int start_rung) {
  std::set<int> component;
  if (start_rung < 0 || start_rung >= static_cast<int>(program.rungs.size())) {
    return component;
  }

  std::vector<std::vector<int>> adjacency(program.rungs.size());
  for (const auto& conn : program.verticalConnections) {
    const auto& rungs = conn.rungs;
    for (size_t i = 0; i < rungs.size(); ++i) {
      int a = rungs[i];
      if (a < 0 || a >= static_cast<int>(program.rungs.size())) {
        continue;
      }
      for (size_t j = i + 1; j < rungs.size(); ++j) {
        int b = rungs[j];
        if (b < 0 || b >= static_cast<int>(program.rungs.size())) {
          continue;
        }
        adjacency[a].push_back(b);
        adjacency[b].push_back(a);
      }
    }
  }

  std::queue<int> pending;
  pending.push(start_rung);
  component.insert(start_rung);

  while (!pending.empty()) {
    int current = pending.front();
    pending.pop();
    for (int next : adjacency[current]) {
      if (component.insert(next).second) {
        pending.push(next);
      }
    }
  }

  return component;
}

std::vector<int> GetCandidateRungs(int rung, int column,
                                   const std::set<int>& component,
                                   const ConnectionMap& connections) {
  std::set<int> candidates;
  if (component.count(rung) > 0) {
    candidates.insert(rung);
  }

  auto it = connections.find(column);
  if (it != connections.end()) {
    for (const auto& group : it->second) {
      if (std::find(group.begin(), group.end(), rung) == group.end()) {
        continue;
      }
      for (int entry : group) {
        if (component.count(entry) > 0) {
          candidates.insert(entry);
        }
      }
    }
  }

  return std::vector<int>(candidates.begin(), candidates.end());
}

std::string BuildPathKey(const std::vector<LadderInstruction>& contacts) {
  std::string key;
  for (const auto& contact : contacts) {
    key.push_back(contact.type == LadderInstructionType::XIO ? 'N' : 'P');
    key.push_back(':');
    key.append(contact.address);
    key.push_back('|');
  }
  return key;
}

void DeduplicatePaths(std::vector<std::vector<LadderInstruction>>* paths) {
  if (!paths) {
    return;
  }
  std::set<std::string> seen;
  std::vector<std::vector<LadderInstruction>> unique;
  for (const auto& path : *paths) {
    std::string key = BuildPathKey(path);
    if (seen.insert(key).second) {
      unique.push_back(path);
    }
  }
  paths->swap(unique);
}

void AppendContacts(const std::vector<LadderInstruction>& contacts,
                    bool first_branch, std::string* output) {
  if (!output) {
    return;
  }

  if (contacts.empty()) {
    *output += first_branch ? "LD TRUE" : "\nOR TRUE";
    return;
  }

  bool first = true;
  for (const auto& contact : contacts) {
    bool negated = (contact.type == LadderInstructionType::XIO);
    if (first) {
      if (first_branch) {
        *output += negated ? "LDN " : "LD ";
      } else {
        *output += negated ? "\nORN " : "\nOR ";
      }
      *output += NormalizeAddress(contact.address);
      first = false;
    } else {
      *output += negated ? "\nANDN " : "\nAND ";
      *output += NormalizeAddress(contact.address);
    }
  }
}

void CollectPathsForOutput(const LadderProgram& program, int output_rung,
                           int output_column,
                           const std::set<int>& component,
                           const ConnectionMap& connections,
                           std::vector<std::vector<LadderInstruction>>* paths) {
  if (!paths) {
    return;
  }
  paths->clear();

  if (output_column < 0 ||
      output_rung < 0 ||
      output_rung >= static_cast<int>(program.rungs.size())) {
    return;
  }

  if (output_column == 0) {
    if (component.count(output_rung) > 0) {
      paths->push_back({});
    }
    return;
  }

  std::vector<int> start_rungs;
  for (int rung : component) {
    if (rung < 0 || rung >= static_cast<int>(program.rungs.size())) {
      continue;
    }
    const auto& cells = program.rungs[rung].cells;
    if (!cells.empty() && IsPassableType(cells[0].type)) {
      start_rungs.push_back(rung);
    }
  }
  std::sort(start_rungs.begin(), start_rungs.end());

  std::vector<LadderInstruction> current;
  std::function<void(int, int)> dfs = [&](int rung, int column) {
    if (column == output_column) {
      if (rung == output_rung) {
        paths->push_back(current);
      }
      return;
    }

    if (column < 0 ||
        column >= static_cast<int>(program.rungs[rung].cells.size())) {
      return;
    }

    auto candidates = GetCandidateRungs(rung, column, component, connections);
    for (int candidate : candidates) {
      if (candidate < 0 ||
          candidate >= static_cast<int>(program.rungs.size())) {
        continue;
      }
      const auto& cell = program.rungs[candidate].cells[column];
      if (!IsPassableType(cell.type)) {
        continue;
      }
      bool added = false;
      if (IsContactType(cell.type)) {
        current.push_back(cell);
        added = true;
      }
      dfs(candidate, column + 1);
      if (added) {
        current.pop_back();
      }
    }
  };

  for (int start_rung : start_rungs) {
    dfs(start_rung, 0);
  }

  DeduplicatePaths(paths);
}

}  // namespace

LadderToLDConverter::LadderToLDConverter() {
  debug_mode_ = false;
}

LadderToLDConverter::~LadderToLDConverter() {}

bool LadderToLDConverter::ConvertToLDFile(const LadderProgram& program,
                                          const std::string& outputPath) {
  std::string ldContent = ConvertToLDString(program);

  if (ldContent.empty()) {
    last_error_ = "Failed to convert ladder program to LD format";
    return false;
  }

  std::ofstream file(outputPath);
  if (!file.is_open()) {
    last_error_ = "Cannot open output file: " + outputPath;
    return false;
  }

  file << ldContent;
  file.close();

  DebugLog("Successfully saved .ld file to: " + outputPath);
  return true;
}

std::string LadderToLDConverter::ConvertToLDString(
    const LadderProgram& program) {
  last_error_.clear();
  DeviceSet devices = CollectUsedDevices(program);
  std::string ldContent = GenerateLDHeader(devices);
  ldContent += "\n";

  ConnectionMap connections;
  BuildVerticalConnectionsByColumn(program, &connections);

  for (size_t i = 0; i < program.rungs.size(); ++i) {
    const auto& rung = program.rungs[i];
    if (rung.isEndRung) {
      break;
    }

    LadderInstruction output;
    int output_column = FindOutputColumn(rung, &output);
    if (output_column < 0) {
      continue;
    }

    std::set<int> component =
        BuildRungComponent(program, static_cast<int>(i));
    std::vector<std::vector<LadderInstruction>> paths;
    CollectPathsForOutput(program, static_cast<int>(i), output_column,
                          component, connections, &paths);

    if (paths.empty()) {
      ConvertSingleRung(rung, static_cast<int>(i), ldContent);
      continue;
    }

    ldContent += "(* Rung " + std::to_string(i) + " *)\n";
    bool first_branch = true;
    for (const auto& path : paths) {
      AppendContacts(path, first_branch, &ldContent);
      first_branch = false;
    }

    AppendOutputInstruction(output, ldContent);
    ldContent += "\n\n";
  }

  ldContent += "\n(* End of Program *)\n";
  ldContent += "END_PROGRAM\n";

  return ldContent;
}

std::string LadderToLDConverter::GenerateLDHeader(const DeviceSet& devices) {
  std::string header;

  header += "(* Generated by FX3U PLC Simulator *)\n";
  header += "(* Compatible with OpenPLC Runtime *)\n";
  header += "\n";
  header += "PROGRAM PLC_PRG\n";
  header += "VAR\n";

  if (!devices.x_inputs.empty()) {
    header += "  (* Input Variables *)\n";
    for (int i : devices.x_inputs) {
      header += "  X" + std::to_string(i) + " AT %IX0." + std::to_string(i) +
                " : BOOL;\n";
    }
    header += "\n";
  }

  if (!devices.y_outputs.empty()) {
    header += "  (* Output Variables *)\n";
    for (int i : devices.y_outputs) {
      header += "  Y" + std::to_string(i) + " AT %QX0." + std::to_string(i) +
                " : BOOL;\n";
    }
    header += "\n";
  }

  if (!devices.m_bits.empty()) {
    header += "  (* Internal Memory *)\n";
    for (int i : devices.m_bits) {
      header += "  M" + std::to_string(i) + " : BOOL;\n";
    }
    header += "\n";
  }

  if (!devices.t_timers.empty()) {
    header += "  (* Timer Variables *)\n";
    for (int i : devices.t_timers) {
      header += "  T" + std::to_string(i) + " : TON;\n";
    }
    header += "\n";
  }

  if (!devices.c_counters.empty()) {
    header += "  (* Counter Variables *)\n";
    for (int i : devices.c_counters) {
      header += "  C" + std::to_string(i) + " : CTU;\n";
    }
    header += "\n";
  }

  header += "END_VAR\n\n";

  return header;
}

void LadderToLDConverter::ConvertSingleRung(const Rung& rung, int rungIndex,
                                            std::string& ldContent) {
  ldContent += "(* Rung " + std::to_string(rungIndex) + " *)\n";

  bool hasCondition = false;
  bool firstCondition = true;
  LadderInstruction output;
  bool hasOutput = false;

  // 조건 부분 처리
  for (const auto& cell : rung.cells) {
    if (cell.type == LadderInstructionType::XIC ||
        cell.type == LadderInstructionType::XIO) {
      bool negated = (cell.type == LadderInstructionType::XIO);
      if (firstCondition) {
        ldContent += negated ? "LDN " : "LD ";
        firstCondition = false;
      } else {
        ldContent += negated ? "\nANDN " : "\nAND ";
      }

      ldContent += ConvertAddress(cell.address);
      hasCondition = true;
    } else if (IsOutputType(cell.type)) {
      output = cell;
      hasOutput = true;
      break;
    }
  }

  // 출력 부분 처리
  if (hasOutput && !output.address.empty()) {
    if (!hasCondition) {
      ldContent += "LD TRUE";  // 조건이 없으면 항상 참
    }
    AppendOutputInstruction(output, ldContent);
  }

  ldContent += "\n\n";
}

LadderToLDConverter::DeviceSet LadderToLDConverter::CollectUsedDevices(
    const LadderProgram& program) const {
  DeviceSet devices;

  for (const auto& rung : program.rungs) {
    if (rung.isEndRung) {
      break;
    }
    for (const auto& cell : rung.cells) {
      if (!IsContactType(cell.type) && !IsOutputType(cell.type)) {
        continue;
      }
      char type = '\0';
      int index = 0;
      if (!TryParseDeviceAddress(cell.address, &type, &index)) {
        continue;
      }
      switch (type) {
        case 'X':
          devices.x_inputs.insert(index);
          break;
        case 'Y':
          devices.y_outputs.insert(index);
          break;
        case 'M':
          devices.m_bits.insert(index);
          break;
        case 'T':
          devices.t_timers.insert(index);
          break;
        case 'C':
          devices.c_counters.insert(index);
          break;
        default:
          break;
      }
    }
  }

  return devices;
}

void LadderToLDConverter::AppendOutputInstruction(
    const LadderInstruction& output, std::string& ldContent) const {
  std::string address = ConvertAddress(output.address);
  std::string preset = output.preset;
  if (!preset.empty() && (preset[0] == 'K' || preset[0] == 'k')) {
    preset = preset.substr(1);
  }

  switch (output.type) {
    case LadderInstructionType::SET:
      ldContent += "\nS " + address;
      break;
    case LadderInstructionType::RST:
    case LadderInstructionType::RST_TMR_CTR:
      ldContent += "\nR " + address;
      break;
    case LadderInstructionType::TON:
      ldContent += "\nTON " + address;
      if (!preset.empty()) {
        ldContent += " K" + preset;
      }
      break;
    case LadderInstructionType::CTU:
      ldContent += "\nCTU " + address;
      if (!preset.empty()) {
        ldContent += " K" + preset;
      }
      break;
    case LadderInstructionType::OTE:
    default:
      ldContent += "\nST " + address;
      break;
  }
}

std::string LadderToLDConverter::ConvertAddress(
    const std::string& address) const {
  if (address.empty())
    return "";

  char deviceType = address[0];
  int deviceNumber = 0;

  try {
    deviceNumber = std::stoi(address.substr(1));
  } catch (...) {
    return address;  // 변환 실패 시 원래 주소 반환
  }

  switch (deviceType) {
    case 'X':
    case 'Y':
    case 'M':
    case 'T':
    case 'C':
      return std::string(1, deviceType) + std::to_string(deviceNumber);
    default:
      break;
  }

  return address;  // 변환할 수 없는 주소는 그대로 반환
}

std::string LadderToLDConverter::GetLDInstructionName(
    LadderInstructionType type) {
  switch (type) {
    case LadderInstructionType::XIC:
      return "LD";
    case LadderInstructionType::XIO:
      return "LDN";
    case LadderInstructionType::OTE:
      return "ST";
    case LadderInstructionType::SET:
      return "S";
    case LadderInstructionType::RST:
      return "R";
    case LadderInstructionType::TON:
      return "TON";
    case LadderInstructionType::CTU:
      return "CTU";
    default:
      return "";
  }
}

void LadderToLDConverter::DebugLog(const std::string& message) {
  if (debug_mode_) {
    std::cout << "[LadderToLDConverter] " << message << std::endl;
  }
}

// ============================================================================
// 🔥 **NEW**: IR 기반 고정밀도 변환 함수들 (Phase 3)
// ============================================================================

std::string LadderToLDConverter::ConvertToLDStringWithIR(
    const LadderProgram& program) {
  DebugLog("🔥 Starting IR-based conversion...");
  last_error_.clear();

  // 1. LadderProgram → LadderIR 변환
  DebugLog("Step 1: Converting to IR...");
  LadderIRProgram irProgram = LadderToIRConverter::ConvertToIR(program);

  // IR 구조 디버그 출력
  if (debug_mode_) {
    irProgram.PrintIRStructure();
  }

  // 2. LadderIR → 스택 명령어 변환
  DebugLog("Step 2: Generating stack instructions...");
  std::vector<std::string> stackInstructions =
      GenerateStackInstructions(irProgram);

  // 3. .ld 파일 형식으로 조합
  DebugLog("Step 3: Building .ld file content...");
  std::stringstream ldContent;

  // .ld 파일 헤더
  DeviceSet devices = CollectUsedDevices(program);
  ldContent << GenerateLDHeader(devices) << "\n\n";

  // 메인 PROGRAM 섹션
  ldContent << "PROGRAM main\n";
  ldContent << "VAR\n";
  ldContent << "END_VAR\n\n";

  // 스택 명령어들을 .ld 형식으로 변환
  for (const auto& instruction : stackInstructions) {
    if (!instruction.empty()) {
      ldContent << "    " << instruction << ";\n";
    }
  }

  ldContent << "\nEND_PROGRAM\n";

  DebugLog("✅ IR-based conversion completed successfully!");
  return ldContent.str();
}

std::vector<std::string> LadderToLDConverter::GenerateStackInstructions(
    const LadderIRProgram& irProgram) {
  DebugLog(
      "Generating advanced stack instructions with MPS/ORB/MPP support...");

  // IR → 스택 명령어 변환 (고급 병렬 처리 포함)
  std::vector<std::string> instructions =
      IRToStackConverter::ConvertToStackInstructions(irProgram);

  // 📊 **변환 통계 로깅**
  DebugLog("Generated " + std::to_string(instructions.size()) +
           " stack instructions");

  if (debug_mode_) {
    std::cout << "\n=== Generated Stack Instructions ===\n";
    for (size_t i = 0; i < instructions.size(); ++i) {
      std::cout << std::setw(3) << i + 1 << ": " << instructions[i] << "\n";
    }
    std::cout << "=====================================\n\n";
  }

  // 🧠 **추론**: 스택 명령어 후처리
  // OpenPLC는 특정 형식을 요구하므로 명령어 형식 조정
  for (auto& instruction : instructions) {
    // LOAD → LD 변환
    if (instruction.find("LOAD ") == 0) {
      instruction.replace(0, 5, "LD ");
    }
    // LOAD_NOT → LDN 변환
    else if (instruction.find("LOAD_NOT ") == 0) {
      instruction.replace(0, 9, "LDN ");
    }
    // OUT → ST 변환
    else if (instruction.find("OUT ") == 0) {
      instruction.replace(0, 4, "ST ");
    }
    // AND_NOT → ANDN 변환
    else if (instruction.find("AND_NOT ") == 0) {
      instruction.replace(0, 8, "ANDN ");
    }
    // OR_NOT → ORN 변환
    else if (instruction.find("OR_NOT ") == 0) {
      instruction.replace(0, 7, "ORN ");
    }
  }

  DebugLog("Stack instruction post-processing completed");
  return instructions;
}

}  // namespace plc
