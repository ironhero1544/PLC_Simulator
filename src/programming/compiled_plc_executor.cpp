// compiled_plc_executor.cpp
//
// Implementation of PLC executor.

#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

namespace plc {

// Initialize executor state.
CompiledPLCExecutor::CompiledPLCExecutor() {
  memory_ = PLCMemory();
  memory_.last_scan_time = std::chrono::steady_clock::now();
  debug_mode_ = false;
  is_running_ = false;
  continuous_mode_ = false;
  cycle_time_ms_ = 10;  // Default scan time
}

CompiledPLCExecutor::~CompiledPLCExecutor() {
  SetContinuousExecution(false);
}

// Cache generated code and parse it into the instruction list.
bool CompiledPLCExecutor::LoadCompiledCode(const std::string& compiledCode) {
  loaded_code_ = compiledCode;
  instructions_.clear();

  DebugLog("Loading compiled C++ code...");
  DebugLog("Code size: " + std::to_string(compiledCode.length()) +
           " characters");

  if (!ParseCompiledCode(compiledCode)) {
    SetError("Failed to parse compiled C++ code");
    return false;
  }

  DebugLog("Successfully loaded " + std::to_string(instructions_.size()) +
           " instructions");
  return true;
}

bool CompiledPLCExecutor::LoadFromCompilationResult(
    const OpenPLCCompilerIntegration::CompilationResult& result) {
  if (!result.success) {
    SetError("Cannot load failed compilation result: " + result.errorMessage);
    return false;
  }

  return LoadCompiledCode(result.generatedCode);
}

// Execute one PLC scan cycle and return timing/status.
CompiledPLCExecutor::ExecutionResult CompiledPLCExecutor::ExecuteScanCycle() {
  ExecutionResult result;
  auto startTime = std::chrono::high_resolution_clock::now();

  auto now = std::chrono::steady_clock::now();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - memory_.last_scan_time).count();
  if (elapsed_ms < 0) {
    elapsed_ms = 0;
  }
  int elapsed_ms_int = static_cast<int>(elapsed_ms);
  if (elapsed_ms_int < 0) {
    elapsed_ms_int = 0;
  }
  if (continuous_mode_) {
    current_elapsed_ms_ = std::max(elapsed_ms_int, cycle_time_ms_);
  } else {
    current_elapsed_ms_ = elapsed_ms_int;
  }

  if (instructions_.empty()) {
    result.success = false;
    result.errorMessage = "No compiled code loaded";
    return result;
  }

  is_running_ = true;

  // Run the scan cycle.
  try {
    // Stage 1: input scan (inputs set via SetInput()).
    // Stage 2: program scan (execute ladder logic).
    int executedInstructions = 0;
    for (const auto& instruction : instructions_) {
      if (!ExecuteInstruction(instruction)) {
        result.success = false;
        result.errorMessage = "Failed to execute instruction at line " +
                              std::to_string(instruction.lineNumber);
        is_running_ = false;
        return result;
      }
      executedInstructions++;
    }

    // Stage 3: output scan (Y outputs already set).

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);

    result.success = true;
    result.cycleTime_us = static_cast<int>(duration.count());
    result.instructionCount = executedInstructions;

    memory_.last_scan_time = std::chrono::steady_clock::now();
    memory_.scan_cycle_ms = static_cast<int>(duration.count() / 1000);

    if (debug_mode_) {
      DebugLog("Scan cycle completed: " + std::to_string(result.cycleTime_us) +
               "us, " + std::to_string(executedInstructions) + " instructions");
    }

  } catch (const std::exception& e) {
    // Keep state consistent after failures.
    result.success = false;
    result.errorMessage =
        "Exception during execution: " + std::string(e.what());

    is_running_ = false;

    if (debug_mode_) {
      DebugLog("CRITICAL: Scan cycle exception - " + std::string(e.what()));
    }
  }

  is_running_ = false;
  last_result_ = result;
  return result;
}

void CompiledPLCExecutor::SetContinuousExecution(bool enable,
                                                 int cycleTime_ms) {
  continuous_mode_ = enable;
  cycle_time_ms_ = cycleTime_ms;

  if (enable) {
    DebugLog("Starting continuous execution mode with " +
             std::to_string(cycleTime_ms) + "ms cycle time");
  } else {
    DebugLog("Stopping continuous execution mode");
  }
}

void CompiledPLCExecutor::SetInput(int address, bool state) {
  if (address >= 0 && address < 16) {
    memory_.X[address] = state;
    if (debug_mode_ && state) {
      DebugLog("Input X" + std::to_string(address) + " = ON");
    }
  }
}

bool CompiledPLCExecutor::GetOutput(int address) const {
  if (address >= 0 && address < 16) {
    return memory_.Y[address];
  }
  return false;
}

void CompiledPLCExecutor::SetMemory(int address, bool state) {
  if (address >= 0 && address < 1000) {
    memory_.M[address] = state;
  }
}

bool CompiledPLCExecutor::GetMemory(int address) const {
  if (address >= 0 && address < 1000) {
    return memory_.M[address];
  }
  return false;
}

int CompiledPLCExecutor::GetTimerValue(int index) const {
  if (index >= 0 && index < 256) {
    return memory_.T[index];
  }
  return 0;
}

bool CompiledPLCExecutor::GetTimerEnabled(int index) const {
  if (index >= 0 && index < 256) {
    return timer_enabled_[index];
  }
  return false;
}

int CompiledPLCExecutor::GetCounterValue(int index) const {
  if (index >= 0 && index < 256) {
    return memory_.C[index];
  }
  return 0;
}

bool CompiledPLCExecutor::GetCounterLastPower(int index) const {
  if (index >= 0 && index < 256) {
    return counter_last_power_[index];
  }
  return false;
}

// Return device state for X/Y/M/T/C addresses.
bool CompiledPLCExecutor::GetDeviceState(const std::string& address) const {
  if (address.empty())
    return false;

  char deviceType = address[0];
  int deviceAddr = 0;

  // Parse numeric address safely.
  try {
    deviceAddr = std::stoi(address.substr(1));
  } catch (...) {
    return false;
  }

  auto timer_done = [&](int idx) -> bool {
    if (idx < 0 || idx >= 256)
      return false;
    int preset = timer_presets_[idx];
    if (preset > 0)
      return memory_.T[idx] >= preset;
    return timer_enabled_[idx];
  };

  auto counter_done = [&](int idx) -> bool {
    if (idx < 0 || idx >= 256)
      return false;
    int preset = counter_presets_[idx];
    if (preset > 0)
      return memory_.C[idx] >= preset;
    return memory_.C[idx] > 0;
  };

  // Bounds-checked device access.
  switch (deviceType) {
    case 'X':
      return (deviceAddr >= 0 && deviceAddr < 16) ? memory_.X[deviceAddr]
                                                  : false;
    case 'Y':
      return (deviceAddr >= 0 && deviceAddr < 16) ? memory_.Y[deviceAddr]
                                                  : false;
    case 'M':
      return (deviceAddr >= 0 && deviceAddr < 1000) ? memory_.M[deviceAddr]
                                                    : false;
    case 'T':
      return timer_done(deviceAddr);
    case 'C':
      return counter_done(deviceAddr);
    default:
      return false;
  }
}

// Update device state for X/Y/M addresses.
void CompiledPLCExecutor::SetDeviceState(const std::string& address,
                                         bool state) {
  if (address.empty())
    return;

  char deviceType = address[0];
  int deviceAddr = 0;

  // Parse numeric address safely.
  try {
    deviceAddr = std::stoi(address.substr(1));
  } catch (...) {
    return;
  }

  // Bounds-checked device writes.
  switch (deviceType) {
    case 'X':
      if (deviceAddr >= 0 && deviceAddr < 16) {
        memory_.X[deviceAddr] = state;
      }
      break;
    case 'Y':
      if (deviceAddr >= 0 && deviceAddr < 16) {
        memory_.Y[deviceAddr] = state;
      }
      break;
    case 'M':
      if (deviceAddr >= 0 && deviceAddr < 1000) {
        memory_.M[deviceAddr] = state;
      }
      break;
  }
}

// Reset PLC memory and execution state.
void CompiledPLCExecutor::ResetMemory() {
  // Reset device memory.
  for (int i = 0; i < 16; i++) {
    memory_.X[i] = false;
    memory_.Y[i] = false;
  }

  for (int i = 0; i < 1000; i++) {
    memory_.M[i] = false;
  }

  // Reset timers and counters.
  for (int i = 0; i < 256; i++) {
    memory_.T[i] = 0;
    memory_.C[i] = 0;
  }

  // Reset execution state.
  memory_.accumulator = false;
  memory_.stack_pointer = 0;

  for (int i = 0; i < 16; i++) {
    memory_.accumulator_stack[i] = false;
  }

  current_elapsed_ms_ = 0;
  timer_enabled_.fill(false);
  counter_last_power_.fill(false);
  timer_presets_.fill(0);
  counter_presets_.fill(0);

  DebugLog("Memory reset completed");
}

// Parse generated C++ into executable instructions.
bool CompiledPLCExecutor::ParseCompiledCode(const std::string& code) {
  std::istringstream stream(code);
  std::string line;
  int lineNumber = 0;

  while (std::getline(stream, line)) {
    lineNumber++;

    // Normalize whitespace for consistent parsing.
    line.erase(0, line.find_first_not_of(" \t"));
    if (!line.empty()) {
      size_t last = line.find_last_not_of(" \t\r\n");
      if (last != std::string::npos)
        line.erase(last + 1);
    }

    // Remove inline comments.
    size_t commentPos = line.find("//");
    if (commentPos != std::string::npos) {
      line = line.substr(0, commentPos);
      line.erase(0, line.find_first_not_of(" \t"));
      if (!line.empty()) {
        size_t last = line.find_last_not_of(" \t");
        if (last != std::string::npos)
          line.erase(last + 1);
      }
    }

    if (line.empty())
      continue;  // Skip empty lines
    if (line[0] == '/' || line[0] == '#')
      continue;  // Skip comments and preprocessor
    if (line == "{" || line == "}" || line == "};")
      continue;  // Skip braces only lines

    // Validate PLC target names.
    auto is_valid_target = [](const std::string& t) -> bool {
      static const std::regex re(R"(^(accumulator|[XYM]\[\d+\]|[XYM]\d+)$)");
      return std::regex_match(t, re);
    };

    // Analyze instruction types with error tolerance.
    // PLC helper instructions.
    if (line.rfind("PLC_", 0) == 0) {
      std::istringstream plcStream(line);
      std::string op;
      std::string addr;
      std::string presetStr;
      plcStream >> op >> addr >> presetStr;
      if (!addr.empty() && addr.back() == ';') {
        addr.pop_back();
      }
      if (!presetStr.empty() && presetStr.back() == ';') {
        presetStr.pop_back();
      }
      auto parse_index = [](const std::string& text) -> int {
        if (text.size() < 2) return -1;
        int idx = -1;
        try { idx = std::stoi(text.substr(1)); } catch (...) { return -1; }
        return idx;
      };

      ParsedInstruction instruction;
      instruction.originalLine = line;
      instruction.lineNumber = lineNumber;

      if (op == "PLC_TON") {
        instruction.type = ParsedInstruction::PLC_TON;
        instruction.index = parse_index(addr);
        instruction.preset = presetStr.empty() ? 0 : std::atoi(presetStr.c_str());
        instructions_.push_back(instruction);
        continue;
      }

      if (op == "PLC_CTU") {
        instruction.type = ParsedInstruction::PLC_CTU;
        instruction.index = parse_index(addr);
        instruction.preset = presetStr.empty() ? 0 : std::atoi(presetStr.c_str());
        instructions_.push_back(instruction);
        continue;
      }

      if (op == "PLC_RST") {
        int idx = parse_index(addr);
        instruction.index = idx;
        if (!addr.empty() && addr[0] == 'T') {
          instruction.type = ParsedInstruction::PLC_RST_T;
        } else if (!addr.empty() && addr[0] == 'C') {
          instruction.type = ParsedInstruction::PLC_RST_C;
        } else {
          instruction.type = ParsedInstruction::UNKNOWN;
        }
        instructions_.push_back(instruction);
        continue;
      }
    }

    // Conditional assignment: if (accumulator) X0 = true.
    {
      static const std::regex condRegex(
          R"(^if\s*\(\s*accumulator\s*\)\s*([A-Za-z0-9_\[\]]+)\s*=\s*(true|false)\s*;?$)");
      std::smatch condMatch;
      if (std::regex_match(line, condMatch, condRegex)) {
        ParsedInstruction instruction;
        instruction.originalLine = line;
        instruction.lineNumber = lineNumber;
        instruction.type = ParsedInstruction::COND_ASSIGN;
        instruction.target = condMatch[1].str();
        instruction.boolValue = (condMatch[2].str() == "true");
        instructions_.push_back(instruction);
        if (debug_mode_) {
          DebugLog("Parsed line " + std::to_string(lineNumber) + ": " + instruction.originalLine);
        }
        continue;
      }
    }

    if (line.find(" = ") != std::string::npos) {
      size_t equalPos = line.find(" = ");
      std::string lhs = line.substr(0, equalPos);
      std::string rhs = line.substr(equalPos + 3);

      // Trim lhs/rhs.
      auto trim = [](std::string s) {
        s.erase(0, s.find_first_not_of(" \t"));
        size_t last = s.find_last_not_of(" \t");
        if (last != std::string::npos)
          s.erase(last + 1);
        return s;
      };
      lhs = trim(lhs);

      // Remove inline comments from rhs and trailing semicolon.
      size_t rhsComment = rhs.find("//");
      if (rhsComment != std::string::npos)
        rhs = rhs.substr(0, rhsComment);
      rhs = trim(rhs);
      if (!rhs.empty() && rhs.back() == ';')
        rhs.pop_back();

      // Ignore non-PLC assignments (e.g., C++ variables).
      if (!is_valid_target(lhs)) {
        if (debug_mode_)
          DebugLog("Skipping non-PLC assignment: " + lhs);
        continue;
      }

      ParsedInstruction instruction;
      instruction.originalLine = line;
      instruction.lineNumber = lineNumber;
      instruction.type = ParsedInstruction::ASSIGNMENT;
      instruction.target = lhs;
      instruction.operand1 = rhs;

      instructions_.push_back(instruction);
      if (debug_mode_) {
        DebugLog("Parsed line " + std::to_string(lineNumber) + ": " +
                 instruction.originalLine);
      }
    } else {
      // Skip non-assignment lines.
      continue;
    }
  }

  return true;
}

bool CompiledPLCExecutor::ExecuteInstruction(
    const ParsedInstruction& instruction) {
  switch (instruction.type) {
    case ParsedInstruction::ASSIGNMENT:
    case ParsedInstruction::LOGIC_OP:
      return ExecuteAssignment(instruction);

    case ParsedInstruction::COND_ASSIGN: {
      if (memory_.accumulator) {
        bool* targetPtr = GetVariablePointer(instruction.target);
        if (!targetPtr) {
          SetError("Invalid target variable: " + instruction.target);
          return false;
        }
        *targetPtr = instruction.boolValue;
      }
      return true;
    }

    case ParsedInstruction::PLC_TON: {
      int idx = instruction.index;
      if (idx < 0 || idx >= 256) {
        SetError("Invalid timer index");
        return false;
      }
      int preset = instruction.preset;
      if (preset < 0) {
        preset = 0;
      }
      timer_presets_[idx] = preset;
      if (memory_.accumulator) {
        timer_enabled_[idx] = true;
        if (preset == 0) {
          memory_.T[idx] = 0;
          memory_.accumulator = true;
        } else {
          if (memory_.T[idx] < preset) {
            memory_.T[idx] += current_elapsed_ms_;
            if (memory_.T[idx] > preset) {
              memory_.T[idx] = preset;
            }
          }
          memory_.accumulator = (memory_.T[idx] >= preset);
        }
      } else {
        timer_enabled_[idx] = false;
        memory_.T[idx] = 0;
        memory_.accumulator = false;
      }
      return true;
    }

    case ParsedInstruction::PLC_CTU: {
      int idx = instruction.index;
      if (idx < 0 || idx >= 256) {
        SetError("Invalid counter index");
        return false;
      }
      int preset = instruction.preset;
      if (preset < 0) {
        preset = 0;
      }
      counter_presets_[idx] = preset;
      bool power = memory_.accumulator;
      if (power && !counter_last_power_[idx]) {
        memory_.C[idx] += 1;
      }
      counter_last_power_[idx] = power;
      bool done = (preset > 0) ? (memory_.C[idx] >= preset) : power;
      memory_.accumulator = done;
      return true;
    }

    case ParsedInstruction::PLC_RST_T: {
      int idx = instruction.index;
      if (idx < 0 || idx >= 256) {
        SetError("Invalid timer index");
        return false;
      }
      if (memory_.accumulator) {
        memory_.T[idx] = 0;
        timer_enabled_[idx] = false;
      }
      return true;
    }

    case ParsedInstruction::PLC_RST_C: {
      int idx = instruction.index;
      if (idx < 0 || idx >= 256) {
        SetError("Invalid counter index");
        return false;
      }
      if (memory_.accumulator) {
        memory_.C[idx] = 0;
        counter_last_power_[idx] = false;
      }
      return true;
    }

    case ParsedInstruction::COMMENT:
      return true;  // Comment line.

    case ParsedInstruction::UNKNOWN:
      return true;  // Ignore unknown lines.

    default:
      return false;
  }
}

bool CompiledPLCExecutor::ExecuteAssignment(
    const ParsedInstruction& instruction) {

  bool result = EvaluateExpression(instruction.operand1);

  bool* targetPtr = GetVariablePointer(instruction.target);
  if (targetPtr) {
    *targetPtr = result;

    if (debug_mode_) {
      DebugLog("Executed: " + instruction.target + " = " +
               (result ? "true" : "false"));
    }

    return true;
  } else {
    SetError("Invalid target variable: " + instruction.target);
    return false;
  }
}

// Map a variable token to PLC memory storage.
bool* CompiledPLCExecutor::GetVariablePointer(const std::string& varName) {
  // Remove whitespace and trailing semicolon.
  std::string name = varName;
  name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());
  if (!name.empty() && name.back() == ';')
    name.pop_back();

  // Accumulator access.
  if (name == "accumulator") {
    return &memory_.accumulator;
  }

  // Handle X[index], Y[index], M[index].
  std::regex arrayRegex(R"(([XYM])\[(\d+)\])");
  std::smatch match;

  if (std::regex_match(name, match, arrayRegex)) {
    char deviceType = match[1].str()[0];
    int index = std::stoi(match[2].str());

    // Bounds-check pointer return.
    switch (deviceType) {
      case 'X':
        return (index >= 0 && index < 16) ? &memory_.X[index] : nullptr;
      case 'Y':
        return (index >= 0 && index < 16) ? &memory_.Y[index] : nullptr;
      case 'M':
        return (index >= 0 && index < 1000) ? &memory_.M[index] : nullptr;
    }
  }

  // Handle X1, Y2, M3.
  std::regex plainRegex(R"(([XYM])(\d+))");
  if (std::regex_match(name, match, plainRegex)) {
    char deviceType = match[1].str()[0];
    int index = std::stoi(match[2].str());
    switch (deviceType) {
      case 'X':
        return (index >= 0 && index < 16) ? &memory_.X[index] : nullptr;
      case 'Y':
        return (index >= 0 && index < 16) ? &memory_.Y[index] : nullptr;
      case 'M':
        return (index >= 0 && index < 1000) ? &memory_.M[index] : nullptr;
    }
  }

  // Return nullptr for invalid variable names.
  return nullptr;
}

// Evaluate boolean expressions used by compiled ladder logic.
bool CompiledPLCExecutor::EvaluateExpression(const std::string& expression) {
  std::string expr = expression;

  // Remove inline comments and trailing semicolon.
  size_t cpos = expr.find("//");
  if (cpos != std::string::npos)
    expr = expr.substr(0, cpos);

  // Normalize whitespace for consistent parsing.
  expr.erase(std::remove_if(expr.begin(), expr.end(), ::isspace), expr.end());
  if (!expr.empty() && expr.back() == ';')
    expr.pop_back();

  // Normalize textual operators to C++ style.
  auto replace_all = [](std::string& s, const std::string& from,
                        const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, from.length(), to);
      pos += to.length();
    }
  };
  replace_all(expr, "AND", "&&");
  replace_all(expr, "OR", "||");
  replace_all(expr, "NOT", "!");

  // Treat bare NOT as accumulator negation.
  if (expr == "!") {
    return !memory_.accumulator;
  }

  auto timer_done = [&](int idx) -> bool {
    if (idx < 0 || idx >= 256)
      return false;
    int preset = timer_presets_[idx];
    if (preset > 0)
      return memory_.T[idx] >= preset;
    return timer_enabled_[idx];
  };

  auto counter_done = [&](int idx) -> bool {
    if (idx < 0 || idx >= 256)
      return false;
    int preset = counter_presets_[idx];
    if (preset > 0)
      return memory_.C[idx] >= preset;
    return memory_.C[idx] > 0;
  };

  auto parse_tc_index = [](const std::string& s, char type, int* out) -> bool {
    if (s.size() < 2 || s[0] != type)
      return false;
    std::string number;
    if (s[1] == '[' && s.back() == ']') {
      if (s.size() <= 3)
        return false;
      number = s.substr(2, s.size() - 3);
    } else {
      number = s.substr(1);
    }
    try {
      *out = std::stoi(number);
    } catch (...) {
      return false;
    }
    return true;
  };

  // Direct variable access.
  if (expr.find("&&") == std::string::npos &&
      expr.find("||") == std::string::npos &&
      (expr.empty() || expr[0] != '!')) {
    int idx = -1;
    if (parse_tc_index(expr, 'T', &idx)) {
      return timer_done(idx);
    }
    if (parse_tc_index(expr, 'C', &idx)) {
      return counter_done(idx);
    }
    bool* varPtr = GetVariablePointer(expr);
    return varPtr ? *varPtr : false;
  }

  // NOT has highest precedence.
  if (!expr.empty() && expr[0] == '!') {
    std::string innerExpr = expr.substr(1);
    return !EvaluateExpression(innerExpr);
  }

  // AND has medium precedence.
  size_t andPos = expr.find("&&");
  if (andPos != std::string::npos) {
    std::string left = expr.substr(0, andPos);
    std::string right = expr.substr(andPos + 2);
    return EvaluateExpression(left) && EvaluateExpression(right);
  }

  // OR has lowest precedence.
  size_t orPos = expr.find("||");
  if (orPos != std::string::npos) {
    std::string left = expr.substr(0, orPos);
    std::string right = expr.substr(orPos + 2);
    return EvaluateExpression(left) || EvaluateExpression(right);
  }

  // Return false for unparseable expressions.
  return false;
}

int CompiledPLCExecutor::ExtractNumber(const std::string& str) {
  std::regex numberRegex(R"(\d+)");
  std::smatch match;

  if (std::regex_search(str, match, numberRegex)) {
    return std::stoi(match[0].str());
  }

  return -1;
}

// Record the last error and log when debugging is enabled.
void CompiledPLCExecutor::SetError(const std::string& error) {
  last_result_.success = false;
  last_result_.errorMessage = error;

  if (debug_mode_) {
    std::cerr << "[CompiledPLCExecutor] ERROR: " << error << std::endl;
  }
}

void CompiledPLCExecutor::DebugLog(const std::string& message) {
  if (debug_mode_) {
    std::cout << "[CompiledPLCExecutor] " << message << std::endl;
  }
}

}  // namespace plc
