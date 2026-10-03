// compiled_plc_executor.cpp
//
//  Implementation of PLC executor.

#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>

namespace plc {

// Initialize executor state.
CompiledPLCExecutor::CompiledPLCExecutor() {
  extension_file_.fill(-1);
  memory_ = PLCMemory();
  memory_.last_scan_time = std::chrono::steady_clock::now();
  rtc_anchor_ = memory_.last_scan_time;
  const std::time_t wall_time = std::time(nullptr);
  std::tm local_time{};
#ifdef _WIN32
  localtime_s(&local_time, &wall_time);
#else
  localtime_r(&wall_time, &local_time);
#endif
  const std::chrono::year_month_day date{
      std::chrono::year{local_time.tm_year + 1900},
      std::chrono::month{static_cast<unsigned>(local_time.tm_mon + 1)},
      std::chrono::day{static_cast<unsigned>(local_time.tm_mday)}};
  rtc_time_ = std::chrono::sys_days{date} +
              std::chrono::hours{local_time.tm_hour} +
              std::chrono::minutes{local_time.tm_min} +
              std::chrono::seconds{local_time.tm_sec};
  debug_mode_ = false;
  is_running_ = false;
  continuous_mode_ = false;
  cycle_time_ms_ = 10;  // Default scan time
  memory_.D[8000] = 200;
  memory_.D[8020] = 10;
  memory_.D[8310] = 1;
  InitializeAxes(true);
  for (auto& module : modules_)
    module.buffer.resize(32767);
  module_transfer_owners_.fill(SIZE_MAX);
  namespace model = plc_emulator::programming;
  for (int index = 0; index < 16; ++index) {
    ResolveDeviceAddress(model::FormatIOAddress('X', index));
    ResolveDeviceAddress(model::FormatIOAddress('Y', index));
  }
  for (int index = 0; index < 1000; ++index) {
    ResolveDeviceAddress("M" + std::to_string(index));
  }
}

CompiledPLCExecutor::~CompiledPLCExecutor() {
  SetContinuousExecution(false);
}

bool CompiledPLCExecutor::LoadProgram(
    const plc_emulator::programming::ExecutionProgram& program) {
  std::string error;
  if (!plc_emulator::programming::ValidateProgram(program, &error)) {
    SetError(error);
    return false;
  }
  auto candidate = program;
  std::vector<size_t> flow_targets;
  if (!plc_emulator::programming::BuildFlowTargets(candidate, &flow_targets,
                                                   &error)) {
    SetError(error);
    return false;
  }
  size_t size = 0;
  for (const auto& instruction : candidate.instructions) {
    size = std::max(size, instruction.condition.size());
  }
  std::vector<uint8_t> values(size);
  std::vector<std::vector<uint8_t>> edges;
  std::vector<std::vector<uint8_t>> observations;
  std::vector<uint8_t> powers(candidate.instructions.size(), 0);
  std::vector<bool> pulses(candidate.instructions.size(), false);
  for (const auto& instruction : candidate.instructions) {
    edges.emplace_back(instruction.condition.size(), 0);
    observations.emplace_back(instruction.condition.size(), 0);
    for (const auto& gate : instruction.condition) {
      for (const auto& operand : {gate.first, gate.second}) {
        if (operand.kind ==
                plc_emulator::programming::OperandKind::kBitDevice ||
            operand.kind ==
                plc_emulator::programming::OperandKind::kWordDevice) {
          ResolveDeviceAddress(
              plc_emulator::programming::FormatDeviceAddress(operand.device));
        }
      }
    }
    for (size_t index = 0; index < instruction.operand_count; ++index) {
      const auto& operand = instruction.operands[index];
      if (operand.kind == plc_emulator::programming::OperandKind::kBitDevice ||
          operand.kind == plc_emulator::programming::OperandKind::kWordDevice) {
        ResolveDeviceAddress(
            plc_emulator::programming::FormatDeviceAddress(operand.device));
      }
    }
  }
  timer_enabled_.fill(false);
  ist_mode_ = -1;
  ist_start_previous_ = false;
  ist_auto_previous_ = false;
  counter_last_power_.fill(false);
  InitializeAxes(false);
  for (auto& port : serial_ports_) {
    if (port.owner != SIZE_MAX)
      memory_.special_m[port.request_relay] = false;
    port.owner = SIZE_MAX;
    port.sending = port.receiving = false;
  }
  program_ = std::move(candidate);
  resolved_program_ = program_;
  BuildSourceSteps();
  InitializeSteps();
  inverter_transfers_.assign(program_.instructions.size(), {});
  inverter_owners_.fill(SIZE_MAX);
  modbus_transfers_.assign(program_.instructions.size(), {});
  modbus_owner_ = SIZE_MAX;
  cf_transfers_.assign(program_.instructions.size(), {});
  for (auto& card : cf_cards_)
    if (card) {
      card->owner = SIZE_MAX;
      card->mixed.active = false;
    }
  pointer_targets_.fill(program_.instructions.size());
  main_program_end_ = program_.instructions.size();
  for (size_t index = 0; index < program_.instructions.size(); ++index) {
    const auto& instruction = program_.instructions[index];
    if (instruction.opcode == plc_emulator::programming::Opcode::kFend)
      main_program_end_ = std::min(main_program_end_, index);
    if (instruction.opcode == plc_emulator::programming::Opcode::kLabel &&
        instruction.operands[0].kind ==
            plc_emulator::programming::OperandKind::kPointer)
      pointer_targets_[instruction.operands[0].immediate] = index;
  }
  flow_targets_ = std::move(flow_targets);
  interrupt_targets_.fill(program_.instructions.size());
  pending_interrupts_.fill(false);
  interrupt_elapsed_ms_.fill(0);
  interrupts_enabled_ = false;
  sort_state_ = {};
  sort2_states_ = {};
  sort2_indices_.fill(SIZE_MAX);
  size_t sort2_slot = 0;
  for (size_t index = 0; index < program_.instructions.size(); ++index)
    if (program_.instructions[index].opcode ==
        plc_emulator::programming::Opcode::kTableSort2)
      sort2_indices_[sort2_slot++] = index;
  module_transfers_.assign(program_.instructions.size(), {});
  handy_states_.assign(program_.instructions.size(), {});
  speed_pulse_starts_.assign(program_.instructions.size(), 0);
  panel_states_.assign(program_.instructions.size(), {});
  absolute_transfers_.assign(program_.instructions.size(), {});
  pid_states_.assign(program_.instructions.size(), {});
  module_transfer_owners_.fill(SIZE_MAX);
  for (size_t index = 0; index < program_.instructions.size(); ++index) {
    const auto& instruction = program_.instructions[index];
    if (instruction.opcode == plc_emulator::programming::Opcode::kLabel &&
        instruction.operands[0].kind ==
            plc_emulator::programming::OperandKind::kInterruptPointer)
      interrupt_targets_[instruction.operands[0].immediate] = index;
  }
  gate_values_ = std::move(values);
  for (auto& frame : flow_frames_)
    frame.gates.resize(size);
  edge_states_ = std::move(edges);
  first_scan_ = true;
  pulse_states_ = std::move(pulses);
  operation_elapsed_ms_.assign(program_.instructions.size(), 0);
  for (size_t index = 0; index < program_.instructions.size(); ++index)
    if (program_.instructions[index].opcode ==
        plc_emulator::programming::Opcode::kDuty)
      operation_elapsed_ms_[index] = -1;
  observed_gates_ = std::move(observations);
  instruction_power_ = std::move(powers);
  has_scan_observations_ = false;
  return true;
}

std::optional<bool> CompiledPLCExecutor::GetCellPower(int rung,
                                                      int cell) const {
  if (!has_scan_observations_)
    return std::nullopt;
  bool found = false;
  bool power = false;
  for (size_t index = 0; index < program_.instructions.size(); ++index) {
    const auto& instruction = program_.instructions[index];
    if (instruction.rung == rung && instruction.cell == cell) {
      found = true;
      power = power || instruction_power_[index] != 0;
    }
    for (const auto& observation : instruction.observations) {
      if (observation.rung == rung && observation.cell == cell &&
          observation.gate < observed_gates_[index].size()) {
        found = true;
        bool active = observed_gates_[index][observation.gate] != 0;
        for (const uint32_t incoming : observation.incoming_gates) {
          active = active && incoming < observed_gates_[index].size() &&
                   observed_gates_[index][incoming] != 0;
        }
        power = power || active;
      }
    }
  }
  return found ? std::optional<bool>(power) : std::nullopt;
}

bool CompiledPLCExecutor::LoadFromCompilationResult(
    const OpenPLCCompilerIntegration::CompilationResult& result) {
  if (!result.success) {
    SetError("Cannot load failed compilation result: " + result.errorMessage);
    return false;
  }
  return LoadProgram(result.program);
}

// Execute one PLC scan cycle and return timing/status.
CompiledPLCExecutor::ExecutionResult CompiledPLCExecutor::ExecuteScanCycle() {
  ExecutionResult result;
  auto startTime = std::chrono::high_resolution_clock::now();

  auto now = std::chrono::steady_clock::now();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - memory_.last_scan_time)
                        .count();
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

  if (program_.instructions.empty()) {
    result.success = false;
    result.errorMessage = "No compiled code loaded";
    return result;
  }

  is_running_ = true;
  memory_.special_m[0] = true;
  memory_.special_m[2] = first_scan_;
  memory_.special_m[411] = first_scan_;
  evaluated_group_ = -1;
  evaluated_gate_count_ = 0;
  has_scan_observations_ = false;
  last_result_.errorMessage.clear();

  // Run the scan cycle.
  try {
    for (int input = 0; input < 256; ++input)
      physical_input_age_[input] =
          std::min(60000, physical_input_age_[input] + current_elapsed_ms_);
    RefreshInputs(0, 256, std::clamp<int>(memory_.D[8020], 0, 60));
    AdvanceAxes();
    AdvanceSerialPorts();
    BeginStepScan();
    RefreshCfStatus();
    // Stage 2: program scan (execute ladder logic).
    int executedInstructions = 0;
    if (!ExecuteProgramScan(&executedInstructions)) {
      result.success = false;
      result.errorMessage = last_result_.errorMessage.empty()
                                ? "Failed to execute PLC program"
                                : last_result_.errorMessage;
      result.instructionCount = executedInstructions;
      is_running_ = false;
      last_result_ = result;
      return result;
    }

    FinishStepScan();
    if (memory_.special_m[49]) {
      int first_annunciator = 0;
      for (int relay = 900; relay <= 999; ++relay)
        if (memory_.S[relay]) {
          first_annunciator = relay;
          break;
        }
      memory_.special_m[48] = first_annunciator != 0;
      memory_.D[8049] = static_cast<int16_t>(first_annunciator);
    }
    std::copy_n(memory_.Y, 256, physical_outputs_.begin());

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);

    result.success = true;
    has_scan_observations_ = true;
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
  first_scan_ = false;
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
  if (!SetPhysicalInput(address, state))
    return;
  memory_.X[address] = state;
}

bool CompiledPLCExecutor::RequestInterrupt(int pointer) {
  if (pointer < 0 || pointer >= 900 ||
      interrupt_targets_[pointer] >= program_.instructions.size())
    return false;
  pending_interrupts_[pointer] = true;
  return true;
}

bool CompiledPLCExecutor::GetOutput(int address) const {
  if (address >= 0 && address < 256) {
    return memory_.Y[address];
  }
  return false;
}

void CompiledPLCExecutor::SetMemory(int address, bool state) {
  if (address >= 0 && address < 7680) {
    memory_.M[address] = state;
  }
}

bool CompiledPLCExecutor::GetMemory(int address) const {
  if (address >= 0 && address < 7680) {
    return memory_.M[address];
  }
  return false;
}

int CompiledPLCExecutor::GetTimerValue(int index) const {
  if (index >= 0 && index < 512) {
    const int unit = index < 200 || (index >= 250 && index < 256) ? 100
                     : index < 246                                ? 10
                                                                  : 1;
    return memory_.T[index] * unit + timer_remainders_ms_[index];
  }
  return 0;
}

bool CompiledPLCExecutor::GetTimerEnabled(int index) const {
  if (index >= 0 && index < 512) {
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

bool CompiledPLCExecutor::GetDeviceState(const std::string& address) const {
  const auto device = ResolveDeviceAddress(address);
  return device && ReadBit(*device);
}

void CompiledPLCExecutor::SetDeviceState(const std::string& address,
                                         bool state) {
  const auto device = ResolveDeviceAddress(address);
  if (device && device->kind == plc_emulator::programming::DeviceKind::kX) {
    SetInput(device->index, state);
  } else if (device) {
    WriteBit(*device, state);
  }
}

std::optional<plc_emulator::programming::DeviceAddress>
CompiledPLCExecutor::ResolveDeviceAddress(const std::string& address) const {
  const auto found = resolved_devices_.find(address);
  if (found != resolved_devices_.end())
    return found->second;
  const auto device = plc_emulator::programming::ParseDeviceAddress(address);
  resolved_devices_.emplace(address, device);
  return device;
}

int32_t CompiledPLCExecutor::GetWordValue(
    plc_emulator::programming::DeviceAddress address, bool wide) const {
  return ReadValue(
      {plc_emulator::programming::OperandKind::kWordDevice, address, 0}, wide);
}

// Reset PLC memory and execution state.
void CompiledPLCExecutor::ResetMemory() {
  cf_transfers_.assign(program_.instructions.size(), {});
  for (auto& card : cf_cards_)
    if (card) {
      card->owner = SIZE_MAX;
      card->mixed.active = false;
    }
  modbus_transfers_.assign(program_.instructions.size(), {});
  modbus_owner_ = SIZE_MAX;
  inverter_transfers_.assign(program_.instructions.size(), {});
  inverter_owners_.fill(SIZE_MAX);
  ist_mode_ = -1;
  ist_start_previous_ = false;
  ist_auto_previous_ = false;
  InitializeSteps();
  physical_outputs_.fill(false);
  // Reset device memory.
  for (int i = 0; i < 256; i++) {
    memory_.X[i] = false;
    memory_.Y[i] = false;
  }

  for (int i = 0; i < 7680; i++) {
    memory_.M[i] = false;
  }

  // Reset timers and counters.
  for (int i = 0; i < 256; i++) {
    memory_.T[i] = 0;
    memory_.C[i] = 0;
  }

  std::fill(std::begin(memory_.T), std::end(memory_.T), 0);

  // Reset execution state.
  memory_.accumulator = false;
  memory_.stack_pointer = 0;

  for (int i = 0; i < 16; i++) {
    memory_.accumulator_stack[i] = false;
  }

  first_scan_ = true;
  current_elapsed_ms_ = 0;
  sort_state_ = {};
  sort2_states_ = {};
  sort2_indices_.fill(SIZE_MAX);
  size_t sort2_slot = 0;
  for (size_t index = 0; index < program_.instructions.size(); ++index)
    if (program_.instructions[index].opcode ==
        plc_emulator::programming::Opcode::kTableSort2)
      sort2_indices_[sort2_slot++] = index;
  module_transfers_.assign(program_.instructions.size(), {});
  handy_states_.assign(program_.instructions.size(), {});
  speed_pulse_starts_.assign(program_.instructions.size(), 0);
  panel_states_.assign(program_.instructions.size(), {});
  absolute_transfers_.assign(program_.instructions.size(), {});
  pid_states_.assign(program_.instructions.size(), {});
  module_transfer_owners_.fill(SIZE_MAX);
  pending_interrupts_.fill(false);
  interrupt_elapsed_ms_.fill(0);
  interrupts_enabled_ = false;
  timer_enabled_.fill(false);
  counter_last_power_.fill(false);
  timer_presets_.fill(0);
  timer_remainders_ms_.fill(0);
  timer_contacts_.fill(false);
  counter_contacts_.fill(false);
  counter_presets_.fill(0);

  std::fill(std::begin(memory_.S), std::end(memory_.S), false);
  std::fill(std::begin(memory_.D), std::end(memory_.D), 0);
  std::fill(std::begin(memory_.R), std::end(memory_.R), 0);
  std::fill(std::begin(memory_.V), std::end(memory_.V), 0);
  std::fill(std::begin(memory_.Z), std::end(memory_.Z), 0);
  std::fill(std::begin(memory_.special_m), std::end(memory_.special_m), false);
  memory_.D[8000] = 200;
  memory_.D[8020] = 10;
  memory_.D[8310] = 1;
  InitializeAxes(true);
  for (auto& port : serial_ports_) {
    port.owner = SIZE_MAX;
    port.sending = port.receiving = false;
    port.incoming_count = port.outgoing_count = 0;
  }
  std::fill(operation_elapsed_ms_.begin(), operation_elapsed_ms_.end(), 0);
  for (size_t index = 0; index < program_.instructions.size(); ++index)
    if (program_.instructions[index].opcode ==
        plc_emulator::programming::Opcode::kDuty)
      operation_elapsed_ms_[index] = -1;
  for (auto& edges : edge_states_)
    std::fill(edges.begin(), edges.end(), 0);
  std::fill(pulse_states_.begin(), pulse_states_.end(), false);
  has_scan_observations_ = false;
  DebugLog("Memory reset completed");
}

// Parse generated C++ into executable instructions.
bool CompiledPLCExecutor::ExecuteInstruction(
    const ParsedInstruction& instruction) {
  switch (instruction.type) {
    case ParsedInstruction::PLC_TON: {
      const int index = instruction.index;
      if (index < 0 || index >= 512) {
        SetError("Invalid timer index");
        return false;
      }
      const int preset = instruction.preset;
      const int unit = index < 200 || (index >= 250 && index < 256) ? 100
                       : index < 246                                ? 10
                                                                    : 1;
      const bool retentive = index >= 246 && index < 256;
      const bool was_enabled = timer_enabled_[index];
      timer_presets_[index] = preset;
      timer_enabled_[index] = memory_.accumulator;
      if (memory_.accumulator) {
        if (was_enabled && memory_.T[index] < preset) {
          const int64_t elapsed = static_cast<int64_t>(current_elapsed_ms_) +
                                  timer_remainders_ms_[index];
          memory_.T[index] = static_cast<int>(
              std::min<int64_t>(preset, memory_.T[index] + elapsed / unit));
          timer_remainders_ms_[index] = static_cast<int>(elapsed % unit);
        }
        if (was_enabled && memory_.T[index] >= preset)
          timer_contacts_[index] = true;
      } else if (!retentive) {
        memory_.T[index] = 0;
        timer_remainders_ms_[index] = 0;
        timer_contacts_[index] = false;
      }
      memory_.accumulator = timer_contacts_[index];
      return true;
    }
    case ParsedInstruction::PLC_CTU: {
      const int index = instruction.index;
      if (index < 0 || index >= 256) {
        SetError("Invalid counter index");
        return false;
      }
      const int preset =
          index < 200 ? std::max(1, instruction.preset) : instruction.preset;
      counter_presets_[index] = preset;
      const bool power = memory_.accumulator;
      if (index < 235 && power && !counter_last_power_[index]) {
        if (index < 200) {
          if (!counter_contacts_[index]) {
            memory_.C[index] = std::min(preset, memory_.C[index] + 1);
            counter_contacts_[index] = memory_.C[index] >= preset;
          }
        } else {
          const int32_t previous = memory_.C[index];
          const bool down = memory_.special_m[index];
          const uint32_t bits = static_cast<uint32_t>(previous) +
                                (down ? UINT32_MAX : uint32_t{1});
          const int32_t current = std::bit_cast<int32_t>(bits);
          memory_.C[index] = current;
          if (!down && previous < preset && current >= preset)
            counter_contacts_[index] = true;
          if (down && previous >= preset && current < preset)
            counter_contacts_[index] = false;
        }
      }
      counter_last_power_[index] = power;
      memory_.accumulator = counter_contacts_[index];
      return true;
    }

    case ParsedInstruction::PLC_RST_T: {
      int idx = instruction.index;
      if (idx < 0 || idx >= 512) {
        SetError("Invalid timer index");
        return false;
      }
      if (memory_.accumulator) {
        memory_.T[idx] = 0;
        timer_enabled_[idx] = false;
        timer_remainders_ms_[idx] = 0;
        timer_contacts_[idx] = false;
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
        counter_contacts_[idx] = false;
        // Keep the powered state latched so the next scan does not treat
        // a still-true rung as a fresh rising edge immediately after reset.
        counter_last_power_[idx] = true;
      }
      return true;
    }

    case ParsedInstruction::COMMENT:
      return true;  // Comment line.

    default:
      return false;
  }
}

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
