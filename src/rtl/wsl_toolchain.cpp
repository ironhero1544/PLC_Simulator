// WSL process adapter adapted from AudioCircuitSimulator (GPL-3.0).
// See docs/RTL_WSL_TOOLCHAIN.md for source revisions and attribution.
#include "plc_emulator/rtl/wsl_toolchain.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// shellapi.h requires Windows SDK types defined by windows.h.
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

namespace plc_emulator {
namespace {

std::wstring QuoteWindowsArgument(const std::wstring& argument) {
  if (!argument.empty() &&
      argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    return argument;
  }
  std::wstring result = L"\"";
  std::size_t backslashes = 0;
  for (wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
      continue;
    }
    if (character == L'\"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'\"');
    } else {
      result.append(backslashes, L'\\');
      result.push_back(character);
    }
    backslashes = 0;
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}

std::string WideToUtf8(const std::wstring& text) {
#ifdef _WIN32
  if (text.empty())
    return {};
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0,
                                 nullptr, nullptr);
  if (size <= 0)
    return {};
  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
#else
  return std::string(text.begin(), text.end());
#endif
}

std::vector<std::wstring> BuildArguments(const WslCommand& command) {
  std::vector<std::wstring> arguments;
  if (command.distribution && !command.distribution->empty()) {
    arguments.emplace_back(L"--distribution");
    arguments.push_back(*command.distribution);
  }
  if (command.run_as_root) {
    arguments.emplace_back(L"--user");
    arguments.emplace_back(L"root");
  }
  arguments.emplace_back(L"--cd");
  arguments.push_back(command.working_directory.value_or(L"~"));
  arguments.emplace_back(L"--exec");
  if (!command.environment.empty()) {
    arguments.emplace_back(L"/usr/bin/env");
    for (const auto& [name, value] : command.environment) {
      arguments.push_back(name + L"=" + value);
    }
  }
  arguments.push_back(command.program);
  arguments.insert(arguments.end(), command.arguments.begin(),
                   command.arguments.end());
  return arguments;
}

std::string Trim(std::string text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  std::size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  return text.substr(start);
}

#ifdef _WIN32
std::wstring WslExecutablePath() {
  wchar_t directory[MAX_PATH] = {};
  const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return L"wsl.exe";
  }
  return std::wstring(directory, length) + L"\\wsl.exe";
}

WslProcessResult RunElevatedWslHostCommand(
    const std::vector<std::wstring>& arguments, std::uint32_t timeout_ms) {
  WslProcessResult result;

  std::wstring parameters;
  for (const auto& argument : arguments) {
    if (!parameters.empty())
      parameters.push_back(L' ');
    parameters += QuoteWindowsArgument(argument);
  }

  SHELLEXECUTEINFOW execute{};
  execute.cbSize = sizeof(execute);
  execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
  execute.lpVerb = L"runas";
  const std::wstring executable = WslExecutablePath();
  execute.lpFile = executable.c_str();
  execute.lpParameters = parameters.c_str();
  execute.nShow = SW_HIDE;

  if (!ShellExecuteExW(&execute) || execute.hProcess == nullptr) {
    const DWORD error = GetLastError();
    result.error =
        error == ERROR_CANCELLED
            ? "WSL installation was cancelled at the administrator prompt."
            : "Failed to start the WSL installer. Windows error " +
                  std::to_string(error) + ".";
    return result;
  }

  result.started = true;
  const DWORD wait = WaitForSingleObject(execute.hProcess, timeout_ms);
  if (wait == WAIT_TIMEOUT) {
    result.timed_out = true;
    result.error = "WSL installation timed out.";
    CloseHandle(execute.hProcess);
    return result;
  }
  if (wait == WAIT_FAILED) {
    result.error =
        "Failed while waiting for the WSL installer. Windows error " +
        std::to_string(GetLastError()) + ".";
    CloseHandle(execute.hProcess);
    return result;
  }

  DWORD exit_code = 1;
  if (!GetExitCodeProcess(execute.hProcess, &exit_code)) {
    result.error = "Cannot read the WSL installer exit code. Windows error " +
                   std::to_string(GetLastError()) + ".";
  }
  result.exit_code = exit_code;
  CloseHandle(execute.hProcess);

  if (exit_code != 0 && result.error.empty()) {
    result.error =
        "wsl.exe --install exited with code " + std::to_string(exit_code) + ".";
  }
  return result;
}
#endif

}  // namespace

WslProcessResult WslToolchain::Run(const WslCommand& command,
                                   std::uint32_t timeout_ms) {
  WslProcessResult result;
  if (command.program.empty()) {
    result.error = "WSL program is empty.";
    return result;
  }
  WslCommand bounded = command;
  bounded.arguments.insert(bounded.arguments.begin(), bounded.program);
  bounded.arguments.insert(bounded.arguments.begin(),
                           std::to_wstring((timeout_ms + 999) / 1000) + L"s");
  bounded.arguments.insert(bounded.arguments.begin(), L"--kill-after=1s");
  bounded.program = L"/usr/bin/timeout";
  return RunHost(BuildArguments(bounded), timeout_ms + 2000);
}

WslProcessResult WslToolchain::RunHost(
    const std::vector<std::wstring>& arguments, std::uint32_t timeout_ms) {
  WslProcessResult result;
#ifdef _WIN32
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) {
    result.error = "Failed to create WSL output pipe.";
    return result;
  }
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
  HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ,
                                  &security, OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  startup.hStdInput = null_input;
  startup.hStdOutput = write_pipe;
  startup.hStdError = write_pipe;

  std::wstring command_line = L"wsl.exe";
  for (const auto& argument : arguments) {
    command_line.push_back(L' ');
    command_line += QuoteWindowsArgument(argument);
  }
  std::vector<wchar_t> mutable_command(command_line.begin(),
                                       command_line.end());
  mutable_command.push_back(L'\0');
  PROCESS_INFORMATION process{};
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                            sizeof(limits));
  }
  const std::wstring executable = WslExecutablePath();
  BOOL created = CreateProcessW(executable.c_str(), mutable_command.data(),
                                nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                nullptr, nullptr, &startup, &process);
  const DWORD launch_error = created ? ERROR_SUCCESS : GetLastError();
  CloseHandle(write_pipe);
  if (null_input != INVALID_HANDLE_VALUE)
    CloseHandle(null_input);
  if (!created) {
    result.error = "Failed to start wsl.exe. Windows error " +
                   std::to_string(launch_error) + ".";
    CloseHandle(read_pipe);
    if (job)
      CloseHandle(job);
    return result;
  }
  result.started = true;
  if (job)
    AssignProcessToJobObject(job, process.hProcess);
  const DWORD start_tick = GetTickCount();
  for (;;) {
    DWORD available = 0;
    while (PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &available, nullptr) &&
           available > 0) {
      char buffer[8192];
      DWORD read = 0;
      if (!ReadFile(read_pipe, buffer,
                    std::min<DWORD>(available, sizeof(buffer)), &read,
                    nullptr) ||
          read == 0) {
        break;
      }
      result.output.append(
          buffer,
          std::min<std::size_t>(read, 1024 * 1024 - result.output.size()));
    }
    if (WaitForSingleObject(process.hProcess, 20) != WAIT_TIMEOUT)
      break;
    if (GetTickCount() - start_tick > timeout_ms) {
      result.timed_out = true;
      if (job)
        TerminateJobObject(job, ERROR_TIMEOUT);
      else
        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
      WaitForSingleObject(process.hProcess, 2000);
      break;
    }
  }
  for (;;) {
    char buffer[8192];
    DWORD available = 0;
    DWORD read = 0;
    if (!PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &available, nullptr) ||
        available == 0 ||
        !ReadFile(read_pipe, buffer, std::min<DWORD>(available, sizeof(buffer)),
                  &read, nullptr) ||
        read == 0) {
      break;
    }
    result.output.append(buffer, std::min<std::size_t>(
                                     read, 1024 * 1024 - result.output.size()));
  }
  DWORD exit_code = 1;
  GetExitCodeProcess(process.hProcess, &exit_code);
  result.exit_code = exit_code;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  CloseHandle(read_pipe);
  if (job)
    CloseHandle(job);
  if (result.output.find('\0') != std::string::npos) {
    std::wstring decoded;
    for (std::size_t i = 0; i + 1 < result.output.size(); i += 2) {
      decoded.push_back(
          static_cast<unsigned char>(result.output[i]) |
          (static_cast<unsigned char>(result.output[i + 1]) << 8));
    }
    result.output = WideToUtf8(decoded);
  }
  result.output = Trim(result.output);
  if (result.timed_out)
    result.error = "WSL command timed out.";
#else
  (void)timeout_ms;
  (void)arguments;
  result.error = "WSL toolchain is available only on Windows.";
#endif
  return result;
}

std::string WslToolchain::WindowsPathToWsl(const std::string& path) {
  if (path.size() < 3 || path[1] != ':')
    return path;
  std::string result = "/mnt/";
  result.push_back(static_cast<char>(
      std::tolower(static_cast<unsigned char>(path.front()))));
  for (std::size_t i = 2; i < path.size(); ++i) {
    result.push_back(path[i] == '\\' ? '/' : path[i]);
  }
  return result;
}

std::string WslToolchain::BuildLaunchCommand(const WslCommand& command) {
#ifdef _WIN32
  std::wstring line = QuoteWindowsArgument(WslExecutablePath());
#else
  std::wstring line = L"wsl.exe";
#endif
  for (const auto& argument : BuildArguments(command)) {
    line.push_back(L' ');
    line += QuoteWindowsArgument(argument);
  }
  return WideToUtf8(line);
}

std::wstring WslToolchain::ToWide(const std::string& text) {
#ifdef _WIN32
  if (text.empty())
    return {};
  const int size =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0)
    return {};
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), result.data(), size);
  return result;
#else
  return std::wstring(text.begin(), text.end());
#endif
}

std::vector<WslDistribution> WslToolchain::ParseDistributionList(
    const std::string& output) {
  std::vector<WslDistribution> distributions;
  std::istringstream lines(output);
  std::string line;
  while (std::getline(lines, line)) {
    line = Trim(line);
    if (line.empty())
      continue;
    const bool is_default = line.front() == '*';
    if (is_default)
      line = Trim(line.substr(1));
    const std::size_t version_start = line.find_last_of(" \t");
    if (version_start == std::string::npos)
      continue;
    const std::string version = Trim(line.substr(version_start + 1));
    if (version != "1" && version != "2")
      continue;
    line = Trim(line.substr(0, version_start));
    const std::size_t state_start = line.find_last_of(" \t");
    if (state_start == std::string::npos)
      continue;
    distributions.push_back({Trim(line.substr(0, state_start)),
                             Trim(line.substr(state_start + 1)),
                             version == "2" ? 2 : 1, is_default});
  }
  return distributions;
}

WslDiagnosis WslToolchain::Diagnose(const WslProfile& profile) {
  WslDiagnosis diagnosis;
  diagnosis.profile = profile;
  const WslProcessResult list = RunHost({L"--list", L"--verbose"}, 15000);
  diagnosis.checks.push_back({"WSL host", list.Succeeded(),
                              list.error.empty() ? list.output : list.error});
  if (!list.Succeeded())
    return diagnosis;
  diagnosis.distributions = ParseDistributionList(list.output);
  const auto selected = std::find_if(
      diagnosis.distributions.begin(), diagnosis.distributions.end(),
      [&profile](const WslDistribution& item) {
        return profile.distribution.empty() ? item.is_default
                                            : item.name == profile.distribution;
      });
  const bool found = selected != diagnosis.distributions.end();
  const bool wsl2 = found && selected->version == 2;
  diagnosis.checks.push_back(
      {"WSL2 distribution", wsl2,
       found
           ? selected->name + " (WSL" + std::to_string(selected->version) + ")"
           : "No matching installed distribution. Install WSL2/Ubuntu."});
  if (!wsl2)
    return diagnosis;
  diagnosis.profile.distribution = selected->name;
  WslCommand command;
  command.distribution = ToWide(selected->name);
  diagnosis.ready = true;
  for (const auto& [name, program] :
       std::vector<std::pair<std::string, std::wstring>>{
           {"Linux kernel", L"/usr/bin/uname"},
           {"Verilator", L"/usr/bin/verilator"},
           {"C++ compiler", L"/usr/bin/g++"},
           {"MinGW Windows compiler", L"/usr/bin/x86_64-w64-mingw32-g++-posix"},
           {"make", L"/usr/bin/make"}}) {
    command.program = program;
    command.arguments = {name == "Linux kernel" ? L"-r" : L"--version"};
    const WslProcessResult result = Run(command, 15000);
    bool compatible = result.Succeeded();
    std::string detail = result.error.empty() ? result.output : result.error;
    if (name == "Verilator" && compatible) {
      std::istringstream version_stream(result.output);
      std::string tool_name;
      int major_version = 0;
      version_stream >> tool_name >> major_version;
      compatible = tool_name == "Verilator" && major_version >= 5;
      if (!compatible) {
        detail += "\nVerilator 5.x or newer is required for timing support.";
      }
    }
    diagnosis.checks.push_back({name, compatible, detail});
    diagnosis.ready = diagnosis.ready && compatible;
  }
  return diagnosis;
}

namespace {

std::mutex g_profile_mutex;

std::filesystem::path ProfilePath() {
#ifdef _WIN32
  const wchar_t* local_app_data = _wgetenv(L"LOCALAPPDATA");
  if (local_app_data == nullptr)
    return {};
  return std::filesystem::path(local_app_data) / L"PLCSimulator" /
         L"rtl-wsl.json";
#else
  return {};
#endif
}

}  // namespace

WslProfile WslToolchain::LoadProfile() {
  std::scoped_lock lock(g_profile_mutex);
  WslProfile profile;
  std::ifstream file(ProfilePath());
  if (!file)
    return profile;
  const auto document = nlohmann::json::parse(file, nullptr, false);
  if (!document.is_object())
    return profile;
  if (document.contains("distribution") && document["distribution"].is_string())
    profile.distribution = document["distribution"].get<std::string>();
  if (document.contains("cpu_budget") &&
      document["cpu_budget"].is_number_integer())
    profile.cpu_budget = static_cast<int>(
        std::clamp(document["cpu_budget"].get<double>(), 1.0, 64.0));
  return profile;
}

bool WslToolchain::SaveProfile(const WslProfile& profile) {
  if (profile.cpu_budget < 1 || profile.cpu_budget > 64 ||
      profile.distribution.find_first_of("\r\n") != std::string::npos)
    return false;
  std::scoped_lock lock(g_profile_mutex);
  const auto path = ProfilePath();
  if (path.empty())
    return false;
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return false;
  const auto temporary = std::filesystem::path(path.wstring() + L".tmp");
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  if (!file)
    return false;
  file << nlohmann::json{{"distribution", profile.distribution},
                         {"cpu_budget", profile.cpu_budget}}
              .dump(2);
  file.close();
  if (!file.good()) {
    return false;
  }
#ifdef _WIN32
  return MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::filesystem::rename(temporary, path, error);
  return !error;
#endif
}

WslProcessResult WslToolchain::ExecuteAction(WslAction action,
                                             const WslProfile& profile) {
  if (action == WslAction::kInstallWsl) {
#ifdef _WIN32
    return RunElevatedWslHostCommand(
        {L"--install", L"--distribution", L"Ubuntu", L"--no-launch"}, 1800000);
#else
    return {};
#endif
  }
  if (action == WslAction::kUpdateWsl)
    return RunHost({L"--update"}, 600000);
  if (profile.distribution.empty())
    return {false, false, 1, {}, "Select an installed distribution first."};
  const std::wstring distribution = ToWide(profile.distribution);
  if (action == WslAction::kStop)
    return RunHost({L"--terminate", distribution}, 30000);
  if (action == WslAction::kConvertToWsl2)
    return RunHost({L"--set-version", distribution, L"2"}, 1800000);
  WslCommand command;
  command.distribution = distribution;
  command.program = L"/usr/bin/true";
  if (action == WslAction::kStart)
    return Run(command, 30000);
  const auto diagnosis = Diagnose(profile);
  const auto selected = std::find_if(
      diagnosis.distributions.begin(), diagnosis.distributions.end(),
      [&profile](const WslDistribution& item) {
        return item.name == profile.distribution && item.version == 2;
      });
  if (selected == diagnosis.distributions.end()) {
    return {false,
            false,
            1,
            {},
            "Select a WSL2 distribution before installing RTL tools."};
  }
  command.run_as_root = true;
  command.program = L"/usr/bin/apt-get";
  command.environment = {{L"DEBIAN_FRONTEND", L"noninteractive"}};
  command.arguments = {L"update"};
  WslProcessResult update = Run(command, 600000);
  if (!update.Succeeded())
    return update;
  command.arguments = {L"install", L"-y",   L"verilator",
                       L"g++",     L"make", L"g++-mingw-w64-x86-64-posix"};
  WslProcessResult install = Run(command, 1200000);
  install.output = update.output + "\n" + install.output;
  return install;
}

}  // namespace plc_emulator
