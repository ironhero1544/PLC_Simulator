#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace plc_emulator {

struct WslCommand {
  std::optional<std::wstring> distribution;
  std::optional<std::wstring> working_directory;
  std::wstring program;
  std::vector<std::wstring> arguments;
  std::vector<std::pair<std::wstring, std::wstring>> environment;
  bool run_as_root = false;
};

struct WslProcessResult {
  bool started = false;
  bool timed_out = false;
  std::uint32_t exit_code = 1;
  std::string output;
  std::string error;
  bool Succeeded() const { return started && !timed_out && exit_code == 0; }
};

struct WslDistribution {
  std::string name;
  std::string state;
  int version = 0;
  bool is_default = false;
};

struct WslProfile {
  std::string distribution;
  int cpu_budget = 2;
};

struct WslCheck {
  std::string name;
  bool passed = false;
  std::string detail;
};

struct WslDiagnosis {
  WslProfile profile;
  std::vector<WslDistribution> distributions;
  std::vector<WslCheck> checks;
  bool ready = false;
};

enum class WslAction {
  kInstallWsl,
  kInstallTools,
  kStart,
  kStop,
  kConvertToWsl2,
  kUpdateWsl
};

class WslToolchain final {
 public:
  static WslProcessResult Run(const WslCommand& command,
                              std::uint32_t timeout_ms);
  static WslProcessResult RunHost(const std::vector<std::wstring>& arguments,
                                  std::uint32_t timeout_ms);
  static WslDiagnosis Diagnose(const WslProfile& profile);
  static WslProcessResult ExecuteAction(WslAction action,
                                        const WslProfile& profile);
  static WslProfile LoadProfile();
  static bool SaveProfile(const WslProfile& profile);
  static std::vector<WslDistribution> ParseDistributionList(
      const std::string& output);
  static std::string WindowsPathToWsl(const std::string& path);
  static std::string BuildLaunchCommand(const WslCommand& command);
  static std::wstring ToWide(const std::string& text);
};

}  // namespace plc_emulator
