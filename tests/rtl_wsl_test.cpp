#include "plc_emulator/rtl/rtl_project_manager.h"
#include "plc_emulator/rtl/rtl_runtime_manager.h"
#include "plc_emulator/rtl/wsl_toolchain.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>

namespace plc_emulator {
namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class IsolatedProfile {
 public:
  IsolatedProfile() {
    const wchar_t* previous = _wgetenv(L"LOCALAPPDATA");
    if (previous != nullptr) {
      previous_ = previous;
    }
    const auto path =
        std::filesystem::current_path() / "tmp" / "rtl-wsl-fixture";
    std::filesystem::create_directories(path);
    _wputenv_s(L"LOCALAPPDATA", path.c_str());
  }
  ~IsolatedProfile() { _wputenv_s(L"LOCALAPPDATA", previous_.c_str()); }

 private:
  std::wstring previous_;
};

void TestAdapter() {
  Require(WslToolchain::WindowsPathToWsl("C:\\Test Space\\코드.sv") ==
              "/mnt/c/Test Space/코드.sv",
          "Windows path conversion");
  const auto distributions = WslToolchain::ParseDistributionList(
      "  NAME STATE VERSION\n* Ubuntu Running 2\n"
      "  Custom Ubuntu Stopped 1\n");
  Require(distributions.size() == 2 && distributions[0].is_default &&
              distributions[0].version == 2 &&
              distributions[1].name == "Custom Ubuntu" &&
              distributions[1].version == 1,
          "WSL distribution parsing");
  WslCommand command;
  command.distribution = L"Custom Ubuntu";
  command.program = L"/usr/bin/printf";
  command.arguments = {L"", L"a\"b", L"space path\\", L"$;literal"};
  const std::string launch = WslToolchain::BuildLaunchCommand(command);
  Require(
      launch.find("--distribution \"Custom Ubuntu\"") != std::string::npos &&
          launch.find("\"\" \"a\\\"b\"") != std::string::npos &&
          launch.find("$;literal") != std::string::npos,
      "Structured argument quoting");
  Require(WslToolchain::SaveProfile({"Ubuntu", 3}), "Save profile");
  const WslProfile profile = WslToolchain::LoadProfile();
  Require(profile.distribution == "Ubuntu" && profile.cpu_budget == 3,
          "Profile round trip");
  Require(!WslToolchain::SaveProfile({"Ubuntu", 0}), "Reject invalid budget");
  plc::RtlProjectManager manager;
  auto* entry = manager.CreateEntry("Adapter unit test");
  const std::string id = entry->moduleId;
  Require(!manager.RestoreBuildArtifactBundle(id, {{"rtl_worker.exe", "MZ"}}),
          "Reject truncated Windows PE artifact");
  entry->testbenchTopModule = "tb";
  entry->sourceFiles = {{"top.sv", "abc"}};
  const std::string old_cache = manager.GetTestbenchCacheDirectory(*entry);
  entry->sourceFiles[0].content = "abd";
  Require(old_cache != manager.GetTestbenchCacheDirectory(*entry),
          "Same-size source changes must invalidate cache");
  manager.DeleteEntry(id);
}

void TestLibraryMerge() {
  plc::RtlProjectManager manager;
  Require(manager.DeserializeLibraryJson(R"({"entries":[]})"), "Empty fixture");
  auto* entry = manager.CreateEntry("RTL Test");
  const std::string id = entry->moduleId;
  entry->sourceFiles = {{"top.sv", "module top; endmodule"}};
  const std::string library = manager.SerializeLibraryJson();
  for (int repeat = 0; repeat < 3; ++repeat) {
    Require(manager.DeserializeLibraryJson(library, true), "Merge project RTL");
    Require(manager.GetLibrary().size() == 1 && manager.FindEntryById(id) &&
                manager.FindEntryById(id)->displayName == "RTL Test",
            "Reload must preserve ID/name without duplicating RTL Test");
  }
  auto updated = nlohmann::json::parse(library);
  updated["entries"][0]["source_files"][0]["content"] = "module updated; endmodule";
  Require(manager.DeserializeLibraryJson(updated.dump(), true) &&
              manager.FindEntryById(id)->sourceFiles[0].content ==
                  "module updated; endmodule" && manager.GetLibrary().size() == 1,
          "Same ID loads the project's source revision in place");
  const std::string other_id = manager.CreateEntry("Unrelated RTL")->moduleId;
  const auto selected = nlohmann::json::parse(manager.SerializeProjectRtlJson({id}));
  Require(selected["entries"].size() == 1 &&
              selected["entries"][0]["module_id"] == id,
          "Project export excludes unrelated global library entries");
  Require(nlohmann::json::parse(manager.SerializeProjectRtlJson({}))
              ["entries"].empty(), "Project without RTL exports no library");
  manager.DeleteEntry(id);
  manager.DeleteEntry(other_id);
}

void TestIntegration() {
  Require(WslToolchain::SaveProfile({"Ubuntu", 2}), "Integration profile");
  const WslDiagnosis diagnosis = WslToolchain::Diagnose({"Ubuntu", 2});
  for (const auto& check : diagnosis.checks) {
    std::cout << check.name << ": " << (check.passed ? "OK" : "FAIL") << '\n';
  }
  Require(diagnosis.ready, "Ubuntu WSL2 toolchain must be ready");
  WslCommand command;
  command.distribution = L"Ubuntu";
  command.program = L"/usr/bin/printf";
  command.arguments = {L"%s", L"spaces 한글 $; \"literal\""};
  const auto argument_result = WslToolchain::Run(command, 10000);
  Require(argument_result.Succeeded() &&
              argument_result.output == "spaces 한글 $; \"literal\"",
          "WSL Unicode and shell metacharacters: " + argument_result.output);
  command.program = L"/usr/bin/sleep";
  command.arguments = {L"5"};
  const auto timeout = WslToolchain::Run(command, 1000);
  Require(
      !timeout.Succeeded() && (timeout.exit_code == 124 || timeout.timed_out),
      "Linux process deadline");

  plc::RtlProjectManager manager;
  auto* entry = manager.CreateEntry("WSL integration");
  const std::string id = entry->moduleId;
  entry->topModule = "wsl_top";
  entry->topFile = "nested folder/상위.sv";
  entry->sourceFiles = {
      {entry->topFile,
       "module wsl_top(input logic a, input logic b, output logic y);\n"
       "wsl_child child(.a(a), .b(b), .y(y));\nendmodule\n"},
      {"nested folder/child.sv",
       "module wsl_child(input logic a, input logic b, output logic y);\n"
       "assign y = a & b;\nendmodule\n"}};
  Require(manager.Analyze(id), "Lint: " + entry->diagnostics);
  if (!manager.Build(id))
    throw std::runtime_error("Build: " + entry->buildDiagnostics);
  Require(manager.HasBuildArtifact(*entry), "Windows PE worker missing");
  const auto executable = manager.GetBuildArtifactData(id);
  Require(executable.size() > 64 && executable.substr(0, 2) == "MZ",
          "MinGW must emit Windows PE, not Linux ELF");
  const std::string launch = manager.GetWorkerLaunchCommand(*entry, {});
  Require(launch.find("wsl.exe") == std::string::npos &&
              launch.find("rtl_worker.exe") != std::string::npos,
          "Worker launch must be independent of WSL/toolchain state");
  const std::string old_hash = entry->buildHash;

  plc::PlacedComponent component;
  component.instanceId = 123;
  component.type = plc::ComponentType::RTL_MODULE;
  component.rtlModuleId = id;
  component.rtlPinBindings =
      plc::RtlProjectManager::BuildDefaultPinBindings(entry->ports);
  // A deliberately unavailable distribution must not affect native workers.
  Require(WslToolchain::SaveProfile({"PLC_TEST_NO_SUCH_DISTRIBUTION", 2}),
          "Select an unavailable WSL profile for runtime isolation test");
  plc::RtlRuntimeManager runtime;
  runtime.SetProjectManager(&manager);
  for (int pattern = 0; pattern < 4; ++pattern) {
    std::map<plc::PortRef, float> voltages;
    int output_id = -1;
    for (const auto& binding : component.rtlPinBindings) {
      if (binding.pinName == "a" || binding.pinName == "b") {
        const int bit = binding.pinName == "a" ? 0 : 1;
        voltages[{component.instanceId, binding.portId}] =
            (pattern & (1 << bit)) != 0 ? 24.0f : 0.0f;
      } else if (!binding.isInput) {
        output_id = binding.portId;
      }
    }
    std::map<int, plc::RtlLogicValue> outputs;
    std::string diagnostics;
    // Keep polling long enough to consume the new request rather than the
    // previous frame's cached outputs.
    for (int attempt = 0; attempt < 80; ++attempt) {
      Require(runtime.EvaluateComponent(component, voltages, &outputs,
                                        &diagnostics),
              "Runtime: " + diagnostics);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Require(outputs.contains(output_id) &&
                outputs[output_id] == (pattern == 3 ? plc::RtlLogicValue::ONE
                                                    : plc::RtlLogicValue::ZERO),
            "AND truth table pattern " + std::to_string(pattern));
  }
  runtime.ShutdownAll();
  Require(WslToolchain::SaveProfile({"Ubuntu", 2}), "Restore build profile");
  std::cout << "Native Windows worker truth table without WSL profile: OK\n";

  auto& child = entry->sourceFiles[1].content;
  child.replace(child.find("a & b"), 5, "a | b");
  entry->analyzeSuccess = false;
  Require(manager.Analyze(id), "Changed lint");
  if (!manager.Build(id))
    throw std::runtime_error("Changed build: " + entry->buildDiagnostics);
  Require(entry->buildHash != old_hash, "Worker source cache invalidation");
  entry->testbenchTopFile = "tb.sv";
  entry->testbenchTopModule = "wsl_tb";
  entry->sourceFiles.push_back(
      {"tb.sv",
       "module wsl_tb;\nlogic a = 0, b = 0; wire y;\n"
       "wsl_top dut(.a(a), .b(b), .y(y));\ninitial begin\n"
       "#1; if (y !== 0) $fatal(1, \"zero\");\n"
       "a = 1; #1; if (y !== 1) $fatal(1, \"one\");\n"
       "$display(\"WSL_TB_PASS\"); $finish;\nend\nendmodule\n"});
  Require(manager.RunTestbench(id), "TB: " + entry->testbenchDiagnostics +
                                        entry->testbenchBuildLog +
                                        entry->testbenchRunLog);
  Require(entry->testbenchRunLog.find("WSL_TB_PASS") != std::string::npos &&
              std::filesystem::exists(entry->testbenchVcdPath),
          "Testbench pass marker and VCD");
  std::cout << "WSL testbench and VCD: OK\n";
  const auto artifact = manager.GetBuildArtifactBundle(id);
  Require(artifact.size() == 1 && artifact.contains("rtl_worker.exe"),
          "Windows native artifact bundle");
  Require(manager.RestoreBuildArtifactBundle(id, artifact),
          "Restore Windows EXE");
  entry->sourceFiles[1].path = "../escape.sv";
  Require(!manager.Analyze(id) &&
              entry->diagnostics.find("escape") != std::string::npos,
          "Reject parent traversal before staging HDL");
  entry->sourceFiles[1].path = "/escape.sv";
  Require(!manager.Analyze(id) &&
              entry->diagnostics.find("relative") != std::string::npos,
          "Reject rooted source paths on Windows");
  manager.DeleteEntry(id);
}

}  // namespace
}  // namespace plc_emulator

int main(int argc, char** argv) {
  try {
    plc_emulator::IsolatedProfile isolated;
    plc_emulator::TestAdapter();
    plc_emulator::TestLibraryMerge();
    if (argc > 1 && std::string(argv[1]) == "--integration") {
      plc_emulator::TestIntegration();
    }
    std::cout << "RTL WSL tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
