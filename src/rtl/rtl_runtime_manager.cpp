#include "plc_emulator/rtl/rtl_runtime_manager.h"
#include "plc_emulator/core/windows_power_utils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// 워커 응답 대기 타임아웃 (ms). 조합 회로는 수μs, 순차 회로도 수ms 이내.
// 초과 시 해당 프레임은 실패로 처리하고 다음 프레임에서 재시도합니다.
static constexpr DWORD kRtlWorkerResponseTimeoutMs = 500;
static constexpr DWORD kRtlWorkerEfficiencyModeTimeoutMs = 1200;

namespace plc {
namespace {

#ifdef _WIN32
DWORD GetRtlWorkerResponseTimeoutMs() {
  return ShouldUseWindowsEfficiencyMode()
             ? kRtlWorkerEfficiencyModeTimeoutMs
             : kRtlWorkerResponseTimeoutMs;
}
#endif

float GetRtlInputHighThreshold(RtlLogicFamily family) {
  switch (family) {
    case RtlLogicFamily::CMOS_5V:
      return 3.5f;
    case RtlLogicFamily::TTL_5V:
      return 2.0f;
    case RtlLogicFamily::INDUSTRIAL_24V:
    default:
      return 12.0f;
  }
}

float GetRtlGroundMaxVoltage(RtlLogicFamily family) {
  switch (family) {
    case RtlLogicFamily::CMOS_5V:
      return 1.0f;
    case RtlLogicFamily::TTL_5V:
      return 0.8f;
    case RtlLogicFamily::INDUSTRIAL_24V:
    default:
      return 2.0f;
  }
}

bool IsVoltageHigh(float voltage, RtlLogicFamily family) {
  return voltage >= GetRtlInputHighThreshold(family);
}

bool TryGetPinVoltage(const PlacedComponent& comp,
                      const std::map<PortRef, float>& voltages,
                      const std::string& pin_name,
                      float* out_voltage) {
  if (!out_voltage || pin_name.empty()) {
    return false;
  }
  for (const auto& binding : comp.rtlPinBindings) {
    if (binding.pinName != pin_name) {
      continue;
    }
    auto it = voltages.find(std::make_pair(comp.instanceId, binding.portId));
    if (it == voltages.end()) {
      return false;
    }
    *out_voltage = it->second;
    return true;
  }
  return false;
}

bool IsRtlComponentPowered(const PlacedComponent& comp,
                          const std::map<PortRef, float>& voltages) {
  if (comp.rtlPowerPinName.empty() && comp.rtlGroundPinName.empty()) {
    return true;
  }
  bool power_ok = true;
  if (!comp.rtlPowerPinName.empty()) {
    float power_voltage = -1.0f;
    power_ok = TryGetPinVoltage(comp, voltages, comp.rtlPowerPinName,
                                &power_voltage) &&
               IsVoltageHigh(power_voltage, comp.rtlLogicFamily);
  }
  bool ground_ok = true;
  if (!comp.rtlGroundPinName.empty()) {
    float ground_voltage = -1.0f;
    ground_ok = TryGetPinVoltage(comp, voltages, comp.rtlGroundPinName,
                                 &ground_voltage) &&
                ground_voltage >= 0.0f &&
                ground_voltage <= GetRtlGroundMaxVoltage(comp.rtlLogicFamily);
  }
  return power_ok && ground_ok;
}

void SetAllRtlOutputs(const PlacedComponent& comp,
                      RtlLogicValue value,
                      std::map<int, RtlLogicValue>* output_values) {
  if (!output_values) {
    return;
  }
  output_values->clear();
  for (const auto& binding : comp.rtlPinBindings) {
    if (!binding.isInput) {
      (*output_values)[binding.portId] = value;
    }
  }
}

const char* kRtlClockLevelKey = "rtl_clock_level";
const char* kRtlResetActiveKey = "rtl_reset_active";

std::string GetBundledToolPathPrefix(const RtlToolchainStatus& status) {
#ifdef _WIN32
  if (status.mingwBinPath.empty()) {
    return "";
  }
  std::string result = status.mingwBinPath;
  // MSYS2 usr/bin: perl, python, sh 등 Verilator 런타임 의존성
  if (!status.msys2Root.empty()) {
    std::string usrBin = status.msys2Root + "\\usr\\bin";
    result += ";" + usrBin;
  }
  return result;
#else
  (void)status;
  return "";
#endif
}


}  // namespace

struct RtlRuntimeManager::WorkerProcess {
  int instanceId = -1;
  std::string moduleId;
  std::unordered_map<std::string, int> outputPortIdsByPinName;
#ifdef _WIN32
  HANDLE processHandle = nullptr;
  HANDLE threadHandle = nullptr;
  HANDLE stdinWrite = nullptr;
  HANDLE stdoutRead = nullptr;

  bool evalPending = false;
  std::string pendingLine;
  std::map<int, RtlLogicValue> lastOutputs;
  DWORD startTick = 0;
#endif
};

RtlRuntimeManager::RtlRuntimeManager() = default;

RtlRuntimeManager::~RtlRuntimeManager() {
  ShutdownAll();
}

void RtlRuntimeManager::SetProjectManager(
    const RtlProjectManager* project_manager) {
  project_manager_ = project_manager;
}

void RtlRuntimeManager::SyncComponentInstances(
    const std::vector<PlacedComponent>& components) {
  std::set<int> live_ids;
  for (const auto& comp : components) {
    if (comp.type == ComponentType::RTL_MODULE) {
      live_ids.insert(comp.instanceId);
    }
  }

  for (auto it = processes_.begin(); it != processes_.end();) {
    if (live_ids.count(it->first) == 0) {
#ifdef _WIN32
      if (it->second.stdinWrite) {
        const char* quit = "QUIT\n";
        DWORD written = 0;
        WriteFile(it->second.stdinWrite, quit, 5, &written, nullptr);
      }
      if (it->second.stdinWrite) {
        CloseHandle(it->second.stdinWrite);
      }
      if (it->second.stdoutRead) {
        CloseHandle(it->second.stdoutRead);
      }
      if (it->second.threadHandle) {
        CloseHandle(it->second.threadHandle);
      }
      if (it->second.processHandle) {
        WaitForSingleObject(it->second.processHandle, 50);
        CloseHandle(it->second.processHandle);
      }
#endif
      it = processes_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string TrimCopy(const std::string& text) {
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }

  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }

  return text.substr(start, end - start);
}

void RtlRuntimeManager::InvalidateModule(const std::string& module_id) {
  for (auto it = processes_.begin(); it != processes_.end();) {
    if (it->second.moduleId == module_id) {
#ifdef _WIN32
      if (it->second.stdinWrite) {
        const char* quit = "QUIT\n";
        DWORD written = 0;
        WriteFile(it->second.stdinWrite, quit, 5, &written, nullptr);
      }
      if (it->second.stdinWrite) {
        CloseHandle(it->second.stdinWrite);
      }
      if (it->second.stdoutRead) {
        CloseHandle(it->second.stdoutRead);
      }
      if (it->second.threadHandle) {
        CloseHandle(it->second.threadHandle);
      }
      if (it->second.processHandle) {
        WaitForSingleObject(it->second.processHandle, 50);
        CloseHandle(it->second.processHandle);
      }
#endif
      it = processes_.erase(it);
    } else {
      ++it;
    }
  }
}

  void RtlRuntimeManager::ShutdownAll() {
#ifdef _WIN32
  for (auto& pair : processes_) {
    if (pair.second.stdinWrite) {
      const char* quit = "QUIT\n";
      DWORD written = 0;
      WriteFile(pair.second.stdinWrite, quit, 5, &written, nullptr);
      CloseHandle(pair.second.stdinWrite);
    }
    if (pair.second.stdoutRead) {
      CloseHandle(pair.second.stdoutRead);
    }
    if (pair.second.threadHandle) {
      CloseHandle(pair.second.threadHandle);
    }
    if (pair.second.processHandle) {
      WaitForSingleObject(pair.second.processHandle, 50);
      CloseHandle(pair.second.processHandle);
    }
  }
#endif
  processes_.clear();
}

bool RtlRuntimeManager::EvaluateComponent(
    const PlacedComponent& comp,
    const std::map<PortRef, float>& voltages,
    std::map<int, RtlLogicValue>* output_values,
    std::string* diagnostics) {
  if (output_values) {
    output_values->clear();
  }
  if (diagnostics) {
    diagnostics->clear();
  }
  if (comp.type != ComponentType::RTL_MODULE || comp.rtlModuleId.empty()) {
    return false;
  }
  if (!IsRtlComponentPowered(comp, voltages)) {
    SetAllRtlOutputs(comp, RtlLogicValue::HIGH_Z, output_values);
    if (diagnostics) {
      diagnostics->clear();
    }
    return true;
  }
  if (!EnsureProcessStarted(comp, diagnostics)) {
    return false;
  }
  auto it = processes_.find(comp.instanceId);
  if (it == processes_.end()) {
    if (diagnostics) {
      *diagnostics = "RTL worker process was not created.";
    }
    return false;
  }
  return SendEvalRequest(&it->second, comp, voltages, output_values,
                         diagnostics);
}

bool RtlRuntimeManager::EnsureProcessStarted(const PlacedComponent& comp,
                                             std::string* diagnostics) {
  if (processes_.count(comp.instanceId) != 0) {
    return true;
  }
  if (!project_manager_) {
    if (diagnostics) {
      *diagnostics = "RTL project manager is not attached.";
    }
    return false;
  }
  const RtlLibraryEntry* entry = project_manager_->FindEntryById(comp.rtlModuleId);
  if (!entry) {
    if (diagnostics) {
      *diagnostics = "RTL module entry was not found.";
    }
    return false;
  }
  RtlToolchainStatus status = project_manager_->DetectToolchain();
  std::string command = project_manager_->GetWorkerLaunchCommand(*entry, status);
  if (command.empty()) {
    if (diagnostics) {
      *diagnostics = "RTL worker launch command is not available. Build the module first.";
    }
    return false;
  }

#ifdef _WIN32
  SECURITY_ATTRIBUTES sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE child_stdout_read = nullptr;
  HANDLE child_stdout_write = nullptr;
  HANDLE child_stdin_read = nullptr;
  HANDLE child_stdin_write = nullptr;
  if (!CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
    if (diagnostics) {
      *diagnostics = "Failed to create RTL worker stdout pipe.";
    }
    return false;
  }
  if (!SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(child_stdout_read);
    CloseHandle(child_stdout_write);
    return false;
  }
  if (!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0)) {
    CloseHandle(child_stdout_read);
    CloseHandle(child_stdout_write);
    if (diagnostics) {
      *diagnostics = "Failed to create RTL worker stdin pipe.";
    }
    return false;
  }
  if (!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(child_stdout_read);
    CloseHandle(child_stdout_write);
    CloseHandle(child_stdin_read);
    CloseHandle(child_stdin_write);
    return false;
  }

  STARTUPINFOA startup_info;
  std::memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  startup_info.wShowWindow = SW_HIDE;
  startup_info.hStdInput = child_stdin_read;
  startup_info.hStdOutput = child_stdout_write;
  startup_info.hStdError = child_stdout_write;

  PROCESS_INFORMATION process_info;
  std::memset(&process_info, 0, sizeof(process_info));

  std::vector<char> command_buffer(command.begin(), command.end());
  command_buffer.push_back('\0');

  char originalPath[32767] = {0};
  DWORD originalPathLen = GetEnvironmentVariableA("PATH", originalPath,
                                                  static_cast<DWORD>(sizeof(originalPath)));
  std::string previous_path =
      (originalPathLen > 0 && originalPathLen < sizeof(originalPath))
          ? std::string(originalPath, originalPathLen)
          : std::string();
  std::string effective_path = GetBundledToolPathPrefix(status);
  if (!previous_path.empty()) {
    effective_path += ";" + previous_path;
  }
  SetEnvironmentVariableA("PATH", effective_path.c_str());
  if (!status.verilatorRoot.empty()) {
    SetEnvironmentVariableA("VERILATOR_ROOT", status.verilatorRoot.c_str());
  }

  BOOL created = CreateProcessA(nullptr, command_buffer.data(), nullptr, nullptr,
                                TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                &startup_info, &process_info);

  if (originalPathLen > 0 && originalPathLen < sizeof(originalPath)) {
    SetEnvironmentVariableA("PATH", previous_path.c_str());
  } else {
    SetEnvironmentVariableA("PATH", nullptr);
  }
  SetEnvironmentVariableA("VERILATOR_ROOT", nullptr);
  CloseHandle(child_stdin_read);
  CloseHandle(child_stdout_write);
  if (!created) {
    CloseHandle(child_stdout_read);
    CloseHandle(child_stdin_write);
    if (diagnostics) {
      *diagnostics = "Failed to launch RTL worker process.";
    }
    return false;
  }

  ApplyWindowsEfficiencyModeCompatibility(process_info.hProcess,
                                          process_info.hThread);

  WorkerProcess process;
  process.instanceId = comp.instanceId;
  process.moduleId = comp.rtlModuleId;
  for (const auto& binding : comp.rtlPinBindings) {
    if (!binding.isInput) {
      process.outputPortIdsByPinName[binding.pinName] = binding.portId;
    }
  }
  process.processHandle = process_info.hProcess;
  process.threadHandle = process_info.hThread;
  process.stdinWrite = child_stdin_write;
  process.stdoutRead = child_stdout_read;
  processes_.emplace(comp.instanceId, process);
  return true;
#else
  (void)comp;
  (void)command;
  if (diagnostics) {
    *diagnostics = "RTL runtime requires native Windows.";
  }
  return false;
#endif
}

bool RtlRuntimeManager::SendEvalRequest(
    WorkerProcess* process,
    const PlacedComponent& comp,
    const std::map<PortRef, float>& voltages,
    std::map<int, RtlLogicValue>* output_values,
    std::string* diagnostics) {
  if (!process) {
    return false;
  }
#ifdef _WIN32
  ApplyWindowsEfficiencyModeCompatibility(process->processHandle,
                                          process->threadHandle);
  if (!process->evalPending) {
    std::string command = BuildEvalCommand(comp, voltages);
    command.push_back('\n');
    DWORD written = 0;
    if (!WriteFile(process->stdinWrite, command.data(),
                   static_cast<DWORD>(command.size()), &written, nullptr)) {
      if (diagnostics) {
        *diagnostics = "Failed to send input values to the RTL worker.";
      }
      return false;
    }
    process->evalPending = true;
    process->startTick = GetTickCount();
    process->pendingLine.clear();
  }

  char ch = '\0';
  DWORD bytes_read = 0;
  while (true) {
    if (WaitForSingleObject(process->processHandle, 0) != WAIT_TIMEOUT) {
      if (diagnostics) {
        *diagnostics = "RTL worker terminated unexpectedly.";
      }
      return false;
    }
    DWORD available = 0;
    if (!PeekNamedPipe(process->stdoutRead, nullptr, 0, nullptr, &available,
                       nullptr)) {
      if (diagnostics) {
        *diagnostics = "RTL worker pipe error.";
      }
      return false;
    }
    if (available == 0) {
      if (GetTickCount() - process->startTick > GetRtlWorkerResponseTimeoutMs()) {
        if (diagnostics) {
          *diagnostics = "RTL worker response timed out.";
        }
        process->evalPending = false;
        return false;
      }
      if (output_values) {
        *output_values = process->lastOutputs;
      }
      return true; // Non-blocking exit
    }
    if (!ReadFile(process->stdoutRead, &ch, 1, &bytes_read, nullptr) ||
        bytes_read == 0) {
      if (diagnostics) {
        *diagnostics = "RTL worker terminated unexpectedly.";
      }
      return false;
    }
    if (ch == '\n') {
      process->evalPending = false;
      break;
    }
    if (ch != '\r') {
      process->pendingLine.push_back(ch);
    }
  }

  std::string line = process->pendingLine;
  line = TrimCopy(line);
  if (line.rfind("OUT", 0) != 0) {
    if (diagnostics) {
      *diagnostics = line.empty() ? "RTL worker returned no output."
                                  : ("RTL worker error: " + line);
    }
    return false;
  }

  std::map<int, RtlLogicValue> newOutputs;
  if (output_values || true) {
    // Bounds check: ensure line has at least 3 characters before calling substr(3)
    std::istringstream stream(line.size() >= 3 ? line.substr(3) : "");
    std::string token;
    while (stream >> token) {
      size_t eq = token.find('=');
      if (eq == std::string::npos) {
        continue;
      }
      std::string pin_name = token.substr(0, eq);
      std::string value_text = token.substr(eq + 1);
      RtlLogicValue value = RtlLogicValue::UNKNOWN;
      if (value_text == "0") {
        value = RtlLogicValue::ZERO;
      } else if (value_text == "1") {
        value = RtlLogicValue::ONE;
      } else if (value_text == "Z") {
        value = RtlLogicValue::HIGH_Z;
      }
      auto port_it = process->outputPortIdsByPinName.find(pin_name);
      if (port_it != process->outputPortIdsByPinName.end()) {
        newOutputs[port_it->second] = value;
        if (output_values) {
          (*output_values)[port_it->second] = value;
        }
      }
    }
  }
  process->lastOutputs = newOutputs;
  return true;
#else
  (void)comp;
  (void)voltages;
  (void)output_values;
  if (diagnostics) {
    *diagnostics = "RTL runtime requires native Windows.";
  }
  return false;
#endif
}

std::string RtlRuntimeManager::BuildEvalCommand(
    const PlacedComponent& comp,
    const std::map<PortRef, float>& voltages) {
  std::ostringstream command;
  command << "EVAL";
  for (const auto& binding : comp.rtlPinBindings) {
    if (!binding.isInput) {
      continue;
    }
    int bit_value = 0;
    if (comp.rtlUseInternalClock && binding.pinName == comp.rtlClockPinName) {
      auto level_it = comp.internalStates.find(kRtlClockLevelKey);
      bit_value =
          (level_it != comp.internalStates.end() && level_it->second > 0.5f)
              ? 1
              : 0;
    } else if (comp.rtlUseStartupReset &&
               binding.pinName == comp.rtlResetPinName) {
      auto reset_it = comp.internalStates.find(kRtlResetActiveKey);
      bool active =
          reset_it != comp.internalStates.end() && reset_it->second > 0.5f;
      bit_value = active ? (comp.rtlResetActiveLow ? 0 : 1)
                         : (comp.rtlResetActiveLow ? 1 : 0);
    } else {
      PortRef port_ref = std::make_pair(comp.instanceId, binding.portId);
      float voltage = -1.0f;
      auto it = voltages.find(port_ref);
      if (it != voltages.end()) {
        voltage = it->second;
      }
      bit_value = IsVoltageHigh(voltage, comp.rtlLogicFamily) ? 1 : 0;
    }
    command << " " << binding.pinName << "=" << bit_value;
  }
  return command.str();
}

}  // namespace plc
