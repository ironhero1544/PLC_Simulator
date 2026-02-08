// programming_mode.cpp
// Implementation of programming mode.

#include "plc_emulator/programming/programming_mode.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/project/ladder_to_ld_converter.h"
#include "plc_emulator/project/openplc_compiler_integration.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace plc {

ProgrammingMode::ProgrammingMode(plc::Application* app)

    : application_(app),

      pending_action_(),

      selected_rung_(0),

      selected_cell_(0),

      is_monitor_mode_(false),

      use_compiled_engine_(false),

      has_compile_attempted_(false),

      show_address_popup_(false),
      show_rung_memo_popup_(false),

      show_vertical_dialog_(false),

      pending_instruction_type_(LadderInstructionType::EMPTY),

      vertical_line_count_(1) {

  temp_address_buffer_[0] = '\0';
  rung_memo_buffer_[0] = '\0';
  rung_memo_popup_buffer_[0] = '\0';
  rung_memo_popup_target_rung_ = -1;
  memo_edit_rung_ = -1;
  memo_edit_session_active_ = false;
  focus_rung_memo_next_frame_ = false;




  plc_executor_ = std::make_unique<CompiledPLCExecutor>();

  ld_converter_ = std::make_unique<LadderToLDConverter>();




  plc_executor_->SetDebugMode(false);
  plc_executor_->SetContinuousExecution(true, kDefaultScanStepMs);




  is_dirty_ = true;

  last_compiled_hash_ = 0;




  last_safe_update_ = std::chrono::steady_clock::now();
  scan_time_initialized_ = false;
  scan_accumulator_ = 0.0;



  std::cout << "[ProgrammingMode] OpenPLC engine initialized."

            << std::endl;

}



void ProgrammingMode::Initialize() {

  for (int i = 0; i < 16; ++i) {

    std::string i_str = std::to_string(i);

    device_states_["X" + i_str] = false;

    device_states_["Y" + i_str] = false;

    device_states_["M" + i_str] = false;

  }

}



void ProgrammingMode::Update() {

  ExecutePendingAction();




  ProcessPendingStateUpdates();



  if (is_monitor_mode_) {


    static int simulationCount = 0;

    if (simulationCount % 60 == 0) {

      std::vector<std::string> coils;

      std::vector<std::string> inputs;

      GetUsedCoils(coils);

      GetUsedInputs(inputs);



      auto sort_by_type_num = [](std::vector<std::string>& v) {

        auto key = [](const std::string& a) {

          char t = a.empty() ? '\0' : a[0];

          int n = 0;

          try {

            n = std::stoi(a.substr(1));

          } catch (...) {

            n = 0;

          }

          return std::tuple<char, int>(t, n);

        };

        std::sort(v.begin(), v.end(),

                  [&](const std::string& a, const std::string& b) {

                    return key(a) < key(b);

                  });

      };

      sort_by_type_num(coils);

      sort_by_type_num(inputs);



      std::ostringstream line;

      line << "[SIM] Simulation running - Count: " << simulationCount;




      if (inputs.empty()) {

        line << " | X:(none)";

      } else {

        line << " | X:";

        for (const auto& addr : inputs) {

          bool state = false;

          auto it = device_states_.find(addr);

          if (it != device_states_.end())

            state = it->second;

          line << " " << addr << ":" << (state ? "ON" : "OFF");

        }

      }




      if (coils.empty()) {

        line << " | Coils:(none)";

      } else {

        line << " | Coils:";

        for (const auto& addr : coils) {

          bool state = false;

          auto it = device_states_.find(addr);

          if (it != device_states_.end())

            state = it->second;

          line << " " << addr << ":" << (state ? "ON" : "OFF");

        }

      }



      std::cout << line.str() << std::endl;

    }

    simulationCount++;



    if (monitor_external_plc_) {
      SyncFromExternalPlc();
    } else {
      SimulateLadderProgram();
    }

  } else {



    //     }

    // }




    SimulatorState stopState;




    for (auto& pair : device_states_) {

      if (!pair.first.empty() && pair.first[0] == 'Y') {

        pair.second = false;

        stopState.deviceStates[pair.first] = false;

      } else {

        stopState.deviceStates[pair.first] = pair.second;

      }

    }




    stopState.timerStates = timer_states_;

    stopState.counterStates = counter_states_;




    SafeUpdateUI(stopState);

  }

}





void ProgrammingMode::UpdateWithPlcState(bool isPlcRunning) {

  // ?  ?**DEBUG**: PLC ?       ?       ?        ?

  static bool lastPlcState = false;

  if (lastPlcState != isPlcRunning) {

    std::cout << "[PLC] State changed: " << (isPlcRunning ? "RUN" : "STOP")

              << " -> Monitor Mode: " << (isPlcRunning ? "ON" : "OFF")

              << std::endl;

    lastPlcState = isPlcRunning;

  }




  monitor_external_plc_ = isPlcRunning;

  if (isPlcRunning) {

    if (!is_monitor_mode_) {

      std::cout << "[PLC] Activating monitor mode for simulation" << std::endl;
      scan_time_initialized_ = false;
      scan_accumulator_ = 0.0;
      InitializeTimersAndCountersFromProgram();

    }

    is_monitor_mode_ = true;

  }




  Update();

}

void ProgrammingMode::SyncFromExternalPlc() {
  if (!application_) {
    return;
  }
  const CompiledPLCExecutor* executor = application_->GetCompiledPlcExecutor();
  if (!executor) {
    return;
  }

  for (int i = 0; i < 16; ++i) {
    std::string x_addr = "X" + std::to_string(i);
    device_states_[x_addr] = executor->GetDeviceState(x_addr);
  }
  for (int i = 0; i < 16; ++i) {
    std::string y_addr = "Y" + std::to_string(i);
    device_states_[y_addr] = executor->GetDeviceState(y_addr);
  }
  for (int i = 0; i < 1000; ++i) {
    std::string m_addr = "M" + std::to_string(i);
    device_states_[m_addr] = executor->GetDeviceState(m_addr);
  }

  auto parse_index = [](const std::string& address, int* out) -> bool {
    if (!out || address.size() < 2) {
      return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(address.c_str() + 1, &end, 10);
    if (!end || *end != '\0') {
      return false;
    }
    if (parsed < 0 || parsed > std::numeric_limits<int>::max()) {
      return false;
    }
    *out = static_cast<int>(parsed);
    return true;
  };

  for (auto& pair : timer_states_) {
    int idx = -1;
    if (!parse_index(pair.first, &idx) || idx < 0 || idx >= 256) {
      continue;
    }
    TimerState& timer = pair.second;
    timer.value = executor->GetTimerValue(idx);
    timer.enabled = executor->GetTimerEnabled(idx);
    if (timer.preset > 0) {
      int preset_ms = timer.preset * 100;
      timer.done = (timer.value >= preset_ms);
    } else {
      timer.done = timer.enabled;
    }
  }

  for (auto& pair : counter_states_) {
    int idx = -1;
    if (!parse_index(pair.first, &idx) || idx < 0 || idx >= 256) {
      continue;
    }
    CounterState& counter = pair.second;
    counter.value = executor->GetCounterValue(idx);
    counter.lastPower = executor->GetCounterLastPower(idx);
    if (counter.preset > 0) {
      counter.done = (counter.value >= counter.preset);
    } else {
      counter.done = (counter.value > 0);
    }
  }

  SimulatorState snapshot = GetCurrentStateSnapshot();
  UpdateUIFromSimulatorState(snapshot);
}



void ProgrammingMode::ExecutePendingAction() {

  if (pending_action_.type == PendingActionType::NONE) {

    return;

  }



  switch (pending_action_.type) {

    case PendingActionType::NONE:

      // No action to execute

      break;

    case PendingActionType::ADD_INSTRUCTION:

      HandleEditAction(pending_action_.instructionType);

      break;

    case PendingActionType::DELETE_INSTRUCTION:

      DeleteCurrentInstruction();

      break;

    case PendingActionType::ADD_NEW_RUNG:

      AddNewRung();

      break;

    case PendingActionType::DELETE_RUNG:

      if (pending_action_.rungIndex != -1) {

        DeleteRung(pending_action_.rungIndex);

      }

      break;

  }

  pending_action_.type = PendingActionType::NONE;

}



void ProgrammingMode::GetUsedDevices(std::vector<std::string>& M,

                                     std::vector<std::string>& T,

                                     std::vector<std::string>& C) const {

  std::set<std::string> usedM, usedT, usedC;

  auto try_parse_index = [](const std::string& text, int* value) -> bool {
    if (!value || text.empty()) {
      return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (!end || *end != '\0') {
      return false;
    }
    if (parsed < 0 || parsed > std::numeric_limits<int>::max()) {
      return false;
    }
    *value = static_cast<int>(parsed);
    return true;
  };

  auto try_parse_bkrst = [&](const LadderInstruction& cell, char* type,
                             int* start, int* count) -> bool {
    if (!type || !start || !count) {
      return false;
    }
    if (cell.type != LadderInstructionType::BKRST ||
        cell.address.size() < 2) {
      return false;
    }
    char device_type = cell.address[0];
    if (device_type != 'M' && device_type != 'T' && device_type != 'C') {
      return false;
    }
    int base = 0;
    if (!try_parse_index(cell.address.substr(1), &base)) {
      return false;
    }
    int parsed_count = 0;
    std::string raw = cell.preset;
    if (!raw.empty() && (raw[0] == 'K' || raw[0] == 'k')) {
      raw = raw.substr(1);
    }
    if (!raw.empty() && !try_parse_index(raw, &parsed_count)) {
      return false;
    }
    if (parsed_count <= 0) {
      parsed_count = 1;
    }
    *type = device_type;
    *start = base;
    *count = parsed_count;
    return true;
  };



  for (const auto& rung : ladder_program_.rungs) {

    if (rung.isEndRung)

      continue;

    for (const auto& cell : rung.cells) {

      if (!cell.address.empty()) {

        if (cell.type == LadderInstructionType::BKRST) {
          char type = '\0';
          int start = 0;
          int count = 0;
          if (try_parse_bkrst(cell, &type, &start, &count)) {
            for (int i = 0; i < count; ++i) {
              std::string addr = std::string(1, type) +
                                 std::to_string(start + i);
              if (type == 'M') {
                usedM.insert(addr);
              } else if (type == 'T') {
                usedT.insert(addr);
              } else if (type == 'C') {
                usedC.insert(addr);
              }
            }
            continue;
          }
        }

        switch (cell.address[0]) {

          case 'M':

            usedM.insert(cell.address);

            break;

          case 'T':

            usedT.insert(cell.address);

            break;

          case 'C':

            usedC.insert(cell.address);

            break;

        }

      }

    }

  }



  M.assign(usedM.begin(), usedM.end());

  T.assign(usedT.begin(), usedT.end());

  C.assign(usedC.begin(), usedC.end());



  auto sort_logic = [](const std::string& a, const std::string& b) {

    try {

      return std::stoi(a.substr(1)) < std::stoi(b.substr(1));

    } catch (const std::exception&) {

      return a < b;

    }

  };



  std::sort(M.begin(), M.end(), sort_logic);

  std::sort(T.begin(), T.end(), sort_logic);

  std::sort(C.begin(), C.end(), sort_logic);

}



std::string ProgrammingMode::InstructionTypeToString(

    LadderInstructionType type) const {

  switch (type) {

    case LadderInstructionType::XIC:

      return "XIC";

    case LadderInstructionType::XIO:

      return "XIO";

    case LadderInstructionType::OTE:

      return "OTE";

    case LadderInstructionType::SET:

      return "SET";

    case LadderInstructionType::RST:

      return "RST";

    case LadderInstructionType::TON:

      return "TON";

    case LadderInstructionType::CTU:

      return "CTU";

    case LadderInstructionType::RST_TMR_CTR:

      return "RST_TMR_CTR";

    case LadderInstructionType::BKRST:

      return "BKRST";

    case LadderInstructionType::HLINE:

      return "HLINE";

    default:

      return "EMPTY";

  }

}



const LadderProgram& ProgrammingMode::GetLadderProgram() const {

  return ladder_program_;

}



void ProgrammingMode::SetLadderProgram(const LadderProgram& program) {

  std::cout << "[DEBUG] SetLadderProgram called with "

            << program.rungs.size() << " rungs" << std::endl;




  ladder_program_ = program;




  selected_rung_ = 0;

  selected_cell_ = 0;
  memo_edit_rung_ = -1;
  memo_edit_session_active_ = false;
  rung_memo_buffer_[0] = '\0';
  focus_rung_memo_next_frame_ = false;




  pending_action_.type = PendingActionType::NONE;
  show_rung_memo_popup_ = false;
  rung_memo_popup_buffer_[0] = '\0';
  rung_memo_popup_target_rung_ = -1;




  InitializeTimersAndCountersFromProgram();

  programming_undo_stack_.clear();
  programming_redo_stack_.clear();




  MarkDirty();



  std::cout << "[DEBUG] Ladder program set; marked dirty for recompilation."

            << std::endl;

}



const std::map<std::string, bool>& ProgrammingMode::GetDeviceStates() const {

  return device_states_;

}



const std::map<std::string, TimerState>& ProgrammingMode::GetTimerStates()

    const {

  return timer_states_;

}



const std::map<std::string, CounterState>& ProgrammingMode::GetCounterStates()

    const {

  return counter_states_;

}



LadderInstructionType ProgrammingMode::StringToInstructionType(

    const std::string& str) {

  if (str == "XIC")

    return LadderInstructionType::XIC;

  if (str == "XIO")

    return LadderInstructionType::XIO;

  if (str == "OTE")

    return LadderInstructionType::OTE;

  if (str == "SET")

    return LadderInstructionType::SET;

  if (str == "RST")

    return LadderInstructionType::RST;

  if (str == "TON")

    return LadderInstructionType::TON;

  if (str == "CTU")

    return LadderInstructionType::CTU;

  if (str == "RST_TMR_CTR")

    return LadderInstructionType::RST_TMR_CTR;

  if (str == "BKRST")

    return LadderInstructionType::BKRST;

  if (str == "HLINE")

    return LadderInstructionType::HLINE;

  return LadderInstructionType::EMPTY;

}




void ProgrammingMode::SaveLadderProgramToLD(const std::string& filepath) {

  LadderToLDConverter converter;

  converter.SetDebugMode(true);



  bool success = converter.ConvertToLDFile(ladder_program_, filepath);



  if (success) {

    std::cout << "[INFO] Successfully saved ladder program to .ld file: "

              << filepath << std::endl;

  } else {

    std::cout << "[ERROR] Failed to save ladder program to .ld file: " << filepath

              << std::endl;

    std::cout << "Error: " << converter.GetLastError() << std::endl;

  }

}




void ProgrammingMode::TestCompileLDFile(const std::string& ldFilepath) {

  has_compile_attempted_ = true;

  std::cout << "\n[INFO] Testing .ld file compilation..." << std::endl;

  std::cout << "Input file: " << ldFilepath << std::endl;



  OpenPLCCompilerIntegration compiler;

  compiler.SetDebugMode(true);

  compiler.SetIOConfiguration(16, 16);  // FX3U-32M: 16 inputs, 16 outputs




  auto result = compiler.CompileLDFile(ldFilepath);



  if (result.success) {

    std::cout << "[INFO] Compilation successful!" << std::endl;

    std::cout << "[INFO] Statistics:" << std::endl;

    std::cout << "   - Inputs: " << result.inputCount << std::endl;

    std::cout << "   - Outputs: " << result.outputCount << std::endl;

    std::cout << "   - Memory: " << result.memoryCount << std::endl;

    std::cout << "   - Generated code size: " << result.generatedCode.length()

              << " chars" << std::endl;




    std::string cppFilepath = ldFilepath + ".cpp";

    if (compiler.SaveGeneratedCode(result, cppFilepath)) {

      std::cout << "[INFO] Generated C++ code saved to: " << cppFilepath

                << std::endl;

    }




    std::cout << "\n[INFO] Generated C++ Code Preview:" << std::endl;

    std::cout << "=====================================\n";

    std::string preview = result.generatedCode.substr(

        0, std::min(static_cast<size_t>(500), result.generatedCode.length()));

    std::cout << preview;

    if (result.generatedCode.length() > 500) {

      std::cout << "\n... (truncated, see full code in " << cppFilepath << ")";

    }

    std::cout << "\n=====================================\n" << std::endl;



  } else {

    std::cout << "[ERROR] Compilation failed!" << std::endl;

    std::cout << "Error: " << result.errorMessage << std::endl;

  }

}



// =============================================================================


// =============================================================================



LadderProgram ProgrammingMode::DeepCopyLadderProgram(

    const LadderProgram& source) const {

  LadderProgram copy;



  // rungs       ?         ?

  copy.rungs.clear();

  copy.rungs.reserve(source.rungs.size());



  for (const auto& rung : source.rungs) {

    Rung copyRung;

    copyRung.number = rung.number;
    copyRung.memo = rung.memo;

    copyRung.isEndRung = rung.isEndRung;



    // cells         ?

    copyRung.cells.clear();

    copyRung.cells.reserve(rung.cells.size());

    for (const auto& cell : rung.cells) {

      LadderInstruction copyCell;

      copyCell.type = cell.type;

      copyCell.address = cell.address;

      copyCell.preset = cell.preset;

      copyCell.isActive = cell.isActive;

      copyRung.cells.push_back(copyCell);

    }



    copy.rungs.push_back(copyRung);

  }



  // verticalConnections       ?         ?

  copy.verticalConnections.clear();

  copy.verticalConnections.reserve(source.verticalConnections.size());

  for (const auto& conn : source.verticalConnections) {

    VerticalConnection copyConn;

    copyConn.x = conn.x;

    copyConn.rungs = conn.rungs;

    copy.verticalConnections.push_back(copyConn);

  }




  static int deepcopyLogCounter = 0;

  if ((++deepcopyLogCounter % 200) == 0) {

    std::cout << "[DEBUG] DeepCopy: Copied " << copy.rungs.size() << " rungs and "

              << copy.verticalConnections.size() << " vertical connections"

              << std::endl;

  }



  return copy;

}



SimulatorState ProgrammingMode::GetCurrentStateSnapshot() const {

  SimulatorState snapshot;




  snapshot.deviceStates = device_states_;

  snapshot.timerStates = timer_states_;

  snapshot.counterStates = counter_states_;




  static uint64_t seqCounter = 0;

  snapshot.seqNo = ++seqCounter;



  return snapshot;

}



void ProgrammingMode::UpdateUIFromSimulatorState(const SimulatorState& state) {
  for (const auto& [address, deviceState] : state.deviceStates) {
    device_states_[address] = deviceState;
  }

  for (const auto& [address, timerState] : state.timerStates) {
    timer_states_[address] = timerState;
  }

  for (const auto& [address, counterState] : state.counterStates) {
    counter_states_[address] = counterState;
  }

  for (auto& rung : ladder_program_.rungs) {
    for (auto& cell : rung.cells) {
      if (cell.address.empty())
        continue;
      if (cell.type == LadderInstructionType::XIC) {
        cell.isActive = GetDeviceState(cell.address);
        continue;
      }
      if (cell.type == LadderInstructionType::XIO) {
        cell.isActive = !GetDeviceState(cell.address);
        continue;
      }
      if (cell.type == LadderInstructionType::OTE ||
          cell.type == LadderInstructionType::SET ||
          cell.type == LadderInstructionType::RST ||
          cell.type == LadderInstructionType::TON ||
          cell.type == LadderInstructionType::CTU ||
          cell.type == LadderInstructionType::RST_TMR_CTR ||
          cell.type == LadderInstructionType::BKRST) {
        cell.isActive = GetDeviceState(cell.address);
        continue;
      }
      auto it = state.deviceStates.find(cell.address);
      if (it != state.deviceStates.end()) {
        cell.isActive = it->second;
      }
    }
  }

  static int updateCounter = 0;
  if ((++updateCounter % 100) == 0) {
    std::cout << "[UI] State Update #" << updateCounter
              << " (seq: " << state.seqNo << ")" << std::endl;
  }
}

void ProgrammingMode::UpdateInputsFromSystem(

    const std::map<std::string, bool>& inputs) {

  for (const auto& kv : inputs) {

    const std::string& addr = kv.first;

    if (!addr.empty() && addr[0] == 'X') {

      device_states_[addr] = kv.second;
      if (plc_executor_) {
        plc_executor_->SetDeviceState(addr, kv.second);
      }

    }

  }

}



size_t ProgrammingMode::ComputeProgramHash(const LadderProgram& program) const {


  auto hash_combine = [](size_t& seed, size_t v) {

    seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);

  };

  std::hash<int> hi;

  std::hash<std::string> hs;

  size_t seed = 0;



  hash_combine(seed, hi(static_cast<int>(program.rungs.size())));

  for (const auto& rung : program.rungs) {

    hash_combine(seed, hi(rung.number));

    hash_combine(seed, hi(rung.isEndRung ? 1 : 0));

    hash_combine(seed, hi(static_cast<int>(rung.cells.size())));

    for (const auto& cell : rung.cells) {

      hash_combine(seed, hi(static_cast<int>(cell.type)));

      hash_combine(seed, hs(cell.address));

      hash_combine(seed, hs(cell.preset));

    }

  }

  hash_combine(seed, hi(static_cast<int>(program.verticalConnections.size())));

  for (const auto& vc : program.verticalConnections) {

    hash_combine(seed, hi(vc.x));

    hash_combine(seed, hi(static_cast<int>(vc.rungs.size())));

    for (int r : vc.rungs)

      hash_combine(seed, hi(r));

  }

  return seed;

}
void ProgrammingMode::MarkDirty() {
  is_dirty_ = true;
  UpdateCompileErrorRungsOnEdit();
}

void ProgrammingMode::PushProgrammingUndoState() {
  ProgrammingUndoState state;
  state.program = DeepCopyLadderProgram(ladder_program_);
  state.selected_rung = selected_rung_;
  state.selected_cell = selected_cell_;
  programming_undo_stack_.push_back(state);
  if (programming_undo_stack_.size() > kProgrammingUndoLimit) {
    programming_undo_stack_.erase(programming_undo_stack_.begin());
  }
  programming_redo_stack_.clear();
}

void ProgrammingMode::UndoProgrammingState() {
  if (is_monitor_mode_ || programming_undo_stack_.empty()) {
    return;
  }
  ProgrammingUndoState current;
  current.program = DeepCopyLadderProgram(ladder_program_);
  current.selected_rung = selected_rung_;
  current.selected_cell = selected_cell_;
  programming_redo_stack_.push_back(current);

  ProgrammingUndoState state = programming_undo_stack_.back();
  programming_undo_stack_.pop_back();
  ladder_program_ = DeepCopyLadderProgram(state.program);
  selected_rung_ = std::max(
      0, std::min(state.selected_rung,
                  static_cast<int>(ladder_program_.rungs.size()) - 1));
  selected_cell_ = std::max(0, std::min(state.selected_cell, 11));
  memo_edit_rung_ = -1;
  memo_edit_session_active_ = false;
  rung_memo_buffer_[0] = '\0';
  focus_rung_memo_next_frame_ = false;
  show_rung_memo_popup_ = false;
  rung_memo_popup_buffer_[0] = '\0';
  rung_memo_popup_target_rung_ = -1;
  InitializeTimersAndCountersFromProgram();
  MarkDirty();
}

void ProgrammingMode::RedoProgrammingState() {
  if (is_monitor_mode_ || programming_redo_stack_.empty()) {
    return;
  }
  ProgrammingUndoState current;
  current.program = DeepCopyLadderProgram(ladder_program_);
  current.selected_rung = selected_rung_;
  current.selected_cell = selected_cell_;
  programming_undo_stack_.push_back(current);

  ProgrammingUndoState state = programming_redo_stack_.back();
  programming_redo_stack_.pop_back();
  ladder_program_ = DeepCopyLadderProgram(state.program);
  selected_rung_ = std::max(
      0, std::min(state.selected_rung,
                  static_cast<int>(ladder_program_.rungs.size()) - 1));
  selected_cell_ = std::max(0, std::min(state.selected_cell, 11));
  memo_edit_rung_ = -1;
  memo_edit_session_active_ = false;
  rung_memo_buffer_[0] = '\0';
  focus_rung_memo_next_frame_ = false;
  show_rung_memo_popup_ = false;
  rung_memo_popup_buffer_[0] = '\0';
  rung_memo_popup_target_rung_ = -1;
  InitializeTimersAndCountersFromProgram();
  MarkDirty();
}

size_t ProgrammingMode::ComputeRungHash(const Rung& rung) const {
  auto hash_combine = [](size_t& seed, size_t v) {
    seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  };
  std::hash<int> hi;
  std::hash<std::string> hs;
  size_t seed = 0;

  hash_combine(seed, hi(rung.number));
  hash_combine(seed, hi(rung.isEndRung ? 1 : 0));
  hash_combine(seed, hi(static_cast<int>(rung.cells.size())));
  for (const auto& cell : rung.cells) {
    hash_combine(seed, hi(static_cast<int>(cell.type)));
    hash_combine(seed, hs(cell.address));
    hash_combine(seed, hs(cell.preset));
  }
  return seed;
}

void ProgrammingMode::UpdateCompileErrorRungsOnEdit() {
  if (!compile_failed_ || compile_error_rungs_.empty()) {
    return;
  }

  std::vector<int> remaining;
  std::map<int, size_t> remainingHashes;

  for (int rungIndex : compile_error_rungs_) {
    if (rungIndex < 0 || rungIndex >= static_cast<int>(ladder_program_.rungs.size())) {
      continue;
    }
    size_t currentHash = ComputeRungHash(ladder_program_.rungs[rungIndex]);
    auto it = compile_error_rung_hashes_.find(rungIndex);
    if (it != compile_error_rung_hashes_.end() && it->second == currentHash) {
      remaining.push_back(rungIndex);
      remainingHashes[rungIndex] = it->second;
    }
  }

  compile_error_rungs_ = remaining;
  compile_error_rung_hashes_ = remainingHashes;

  if (compile_error_rungs_.empty()) {
    compile_failed_ = false;
    last_failed_hash_ = 0;
    last_compile_error_.clear();
  }
}

bool ProgrammingMode::IsRungCompileError(int rungIndex) const {
  return std::find(compile_error_rungs_.begin(), compile_error_rungs_.end(),
                   rungIndex) != compile_error_rungs_.end();
}

void ProgrammingMode::UpdateCompileErrorRungsOnCompileFailure(
    const std::string& ldCode, const std::string& errorMessage) {
  compile_failed_ = true;
  last_failed_hash_ = ComputeProgramHash(ladder_program_);
  last_compile_error_ = errorMessage;

  compile_error_rungs_.clear();
  compile_error_rung_hashes_.clear();

  int errorLine = -1;
  std::regex lineRegex(R"(line\s*(\d+))", std::regex_constants::icase);
  std::smatch lineMatch;
  if (std::regex_search(errorMessage, lineMatch, lineRegex)) {
    try {
      errorLine = std::stoi(lineMatch[1].str());
    } catch (...) {
      errorLine = -1;
    }
  }

  if (!ldCode.empty() && errorLine > 0) {
    std::istringstream ldStream(ldCode);
    std::string line;
    int currentRung = -1;
    int lineNo = 0;
    std::regex rungRegex(R"(\(\*\s*Rung\s+(\d+)\s*\*\))");
    while (std::getline(ldStream, line)) {
      lineNo++;
      std::smatch rungMatch;
      if (std::regex_search(line, rungMatch, rungRegex)) {
        try {
          currentRung = std::stoi(rungMatch[1].str());
        } catch (...) {
          currentRung = -1;
        }
      }
      if (lineNo == errorLine && currentRung >= 0) {
        compile_error_rungs_.push_back(currentRung);
        break;
      }
    }
  }

  if (compile_error_rungs_.empty()) {
    for (size_t i = 0; i + 1 < ladder_program_.rungs.size(); ++i) {
      compile_error_rungs_.push_back(static_cast<int>(i));
    }
  }

  for (int rungIndex : compile_error_rungs_) {
    if (rungIndex < 0 || rungIndex >= static_cast<int>(ladder_program_.rungs.size())) {
      continue;
    }
    compile_error_rung_hashes_[rungIndex] =
        ComputeRungHash(ladder_program_.rungs[rungIndex]);
  }
}



void ProgrammingMode::InitializeTimersAndCountersFromProgram() {


  std::set<std::string> explicit_timers;
  std::set<std::string> explicit_counters;

  auto ensure_timer = [&](const std::string& addr,

                          const std::string& presetStr,
                          bool override_preset) {

    int preset = 0;

    if (!presetStr.empty()) {

      std::string raw = presetStr;
      if (!raw.empty() && (raw[0] == 'K' || raw[0] == 'k')) {
        raw = raw.substr(1);
      }

      try {

        preset = std::stoi(raw);

        if (preset < 0) {

          std::cerr << "[WARN] TON preset < 0 for " << addr << ": " << presetStr

                    << ". Clamping to 0." << std::endl;

          preset = 0;

        }

      } catch (const std::exception& e) {

        std::cerr << "[WARN] Invalid TON preset for " << addr << ": '"

                  << presetStr << "' (" << e.what() << "). Using 0."

                  << std::endl;

        preset = 0;

      }

    }

    auto it = timer_states_.find(addr);

    if (it == timer_states_.end()) {

      TimerState cs;

      cs.value = 0;

      cs.done = false;

      cs.preset = preset;

      cs.enabled = false;

      timer_states_.emplace(addr, cs);

    } else {

      it->second.value = 0;
      it->second.done = false;
      it->second.enabled = false;
      if (override_preset) {
        it->second.preset = preset;
      }

    }

  };



    auto ensure_counter = [&](const std::string& addr,

                            const std::string& presetStr,
                            bool override_preset) {

    int preset = 0;

    if (!presetStr.empty()) {

      std::string raw = presetStr;
      if (!raw.empty() && (raw[0] == 'K' || raw[0] == 'k')) {
        raw = raw.substr(1);
      }

      try {

        preset = std::stoi(raw);

        if (preset < 0) {

          std::cerr << "[WARN] CTU preset < 0 for " << addr << ": " << presetStr

                    << ". Clamping to 0." << std::endl;

          preset = 0;

        }

      } catch (const std::exception& e) {

        std::cerr << "[WARN] Invalid CTU preset for " << addr << ": '"

                  << presetStr << "' (" << e.what() << "). Using 0."

                  << std::endl;

        preset = 0;

      }

    }

    auto it = counter_states_.find(addr);

    if (it == counter_states_.end()) {

      CounterState cs;

      cs.value = 0;

      cs.done = false;

      cs.preset = preset;

      cs.lastPower = false;

      counter_states_.emplace(addr, cs);

    } else {

      it->second.value = 0;
      it->second.done = false;
      it->second.lastPower = false;
      if (override_preset) {
        it->second.preset = preset;
      }

    }

  };

  auto parse_count = [&](const std::string& presetStr) -> int {
    if (presetStr.empty()) {
      return 1;
    }
    std::string raw = presetStr;
    if (!raw.empty() && (raw[0] == 'K' || raw[0] == 'k')) {
      raw = raw.substr(1);
    }
    if (raw.empty()) {
      return 1;
    }
    char* end = nullptr;
    long parsed = std::strtol(raw.c_str(), &end, 10);
    if (!end || *end != '\0') {
      return 1;
    }
    if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) {
      return 1;
    }
    return static_cast<int>(parsed);
  };
for (const auto& rung : ladder_program_.rungs) {

    if (rung.isEndRung)

      continue;

    for (const auto& cell : rung.cells) {

      if (cell.address.empty())

        continue;

      switch (cell.type) {

        case LadderInstructionType::TON:

          ensure_timer(cell.address, cell.preset, true);
          explicit_timers.insert(cell.address);

          break;

        case LadderInstructionType::CTU:

          ensure_counter(cell.address, cell.preset, true);
          explicit_counters.insert(cell.address);

          break;

        case LadderInstructionType::RST_TMR_CTR:

          if (!cell.address.empty()) {

            if (cell.address[0] == 'T') {

              bool override_preset =
                  (explicit_timers.find(cell.address) ==
                   explicit_timers.end());
              ensure_timer(cell.address, std::string(), override_preset);

            } else if (cell.address[0] == 'C') {

              bool override_preset =
                  (explicit_counters.find(cell.address) ==
                   explicit_counters.end());
              ensure_counter(cell.address, std::string(), override_preset);

            }

          }

          break;

        case LadderInstructionType::BKRST: {

          if (cell.address.empty()) {
            break;
          }
          char deviceType = cell.address[0];
          if (deviceType != 'T' && deviceType != 'C') {
            break;
          }
          char* end = nullptr;
          long base = std::strtol(cell.address.substr(1).c_str(), &end, 10);
          if (!end || *end != '\0' || base < 0 ||
              base > std::numeric_limits<int>::max()) {
            break;
          }
          int count = parse_count(cell.preset);
          for (int i = 0; i < count; ++i) {
            std::string addr = std::string(1, deviceType) +
                               std::to_string(static_cast<int>(base) + i);
            if (deviceType == 'T') {
              bool override_preset =
                  (explicit_timers.find(addr) == explicit_timers.end());
              ensure_timer(addr, std::string(), override_preset);
            } else if (deviceType == 'C') {
              bool override_preset =
                  (explicit_counters.find(addr) ==
                   explicit_counters.end());
              ensure_counter(addr, std::string(), override_preset);
            }
          }
          break;
        }

        default:

          break;

      }

    }

  }

}




void ProgrammingMode::GetUsedCoils(std::vector<std::string>& coils) const {

  std::set<std::string> used;

  auto try_parse_index = [](const std::string& text, int* value) -> bool {
    if (!value || text.empty()) {
      return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (!end || *end != '\0') {
      return false;
    }
    if (parsed < 0 || parsed > std::numeric_limits<int>::max()) {
      return false;
    }
    *value = static_cast<int>(parsed);
    return true;
  };

  auto parse_count = [&](const std::string& presetStr) -> int {
    if (presetStr.empty()) {
      return 1;
    }
    std::string raw = presetStr;
    if (!raw.empty() && (raw[0] == 'K' || raw[0] == 'k')) {
      raw = raw.substr(1);
    }
    if (raw.empty()) {
      return 1;
    }
    int value = 0;
    if (!try_parse_index(raw, &value) || value <= 0) {
      return 1;
    }
    return value;
  };

  for (const auto& rung : ladder_program_.rungs) {

    if (rung.isEndRung)

      continue;

    for (const auto& cell : rung.cells) {

      switch (cell.type) {

        case LadderInstructionType::OTE:

        case LadderInstructionType::SET:

        case LadderInstructionType::RST:

        case LadderInstructionType::BKRST:

          if (!cell.address.empty()) {
            if (cell.type == LadderInstructionType::BKRST) {
              char type = cell.address[0];
              int base = 0;
              if (cell.address.size() >= 2 &&
                  try_parse_index(cell.address.substr(1), &base)) {
                int count = parse_count(cell.preset);
                for (int i = 0; i < count; ++i) {
                  used.insert(std::string(1, type) +
                              std::to_string(base + i));
                }
              } else {
                used.insert(cell.address);
              }
            } else {
              used.insert(cell.address);
            }
          }

          break;

        default:

          break;

      }

    }

  }

  coils.assign(used.begin(), used.end());

}




void ProgrammingMode::GetUsedInputs(std::vector<std::string>& inputs) const {

  std::set<std::string> used;

  for (const auto& rung : ladder_program_.rungs) {

    if (rung.isEndRung)

      continue;

    for (const auto& cell : rung.cells) {

      if ((cell.type == LadderInstructionType::XIC ||

           cell.type == LadderInstructionType::XIO) &&

          !cell.address.empty() && cell.address[0] == 'X') {

        used.insert(cell.address);

      }

    }

  }

  inputs.assign(used.begin(), used.end());

}



// =============================================================================


// =============================================================================

bool ProgrammingMode::IsSafeToUpdateUI() const {


  if (is_editing_in_progress_)

    return false;


  auto now = std::chrono::steady_clock::now();

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(

      now - last_safe_update_);

  return elapsed.count() >= 10;

}



void ProgrammingMode::SetEditingState(bool isEditing) {

  is_editing_in_progress_ = isEditing;

}



void ProgrammingMode::ProcessPendingStateUpdates() {

  if (!has_pending_state_update_)

    return;

  if (IsSafeToUpdateUI()) {

    UpdateUIFromSimulatorState(pending_state_update_);

    has_pending_state_update_ = false;

    last_safe_update_ = std::chrono::steady_clock::now();

  }

}



void ProgrammingMode::SafeUpdateUI(const SimulatorState& state) {

  if (IsSafeToUpdateUI()) {

    UpdateUIFromSimulatorState(state);

    last_safe_update_ = std::chrono::steady_clock::now();

  } else {


    pending_state_update_ = state;

    has_pending_state_update_ = true;

  }

}




bool ProgrammingMode::IsRecompileNeeded() const {

  return NeedsRecompilation();

}



LadderProgram ProgrammingMode::NormalizeLadderGX2(const LadderProgram& src) {

  LadderProgram norm = DeepCopyLadderProgram(src);

  last_normalization_fix_count_ = 0;

  std::ostringstream oss;



  auto ensure_hline = [&](int rungIdx, int col) {

    if (rungIdx < 0 || rungIdx >= static_cast<int>(norm.rungs.size()))

      return;

    auto& rung = norm.rungs[static_cast<size_t>(rungIdx)];

    if (static_cast<int>(rung.cells.size()) <= col || col < 0)

      return;

    auto& cell = rung.cells[static_cast<size_t>(col)];

    if (cell.type == LadderInstructionType::EMPTY) {

      cell.type = LadderInstructionType::HLINE;

      cell.address.clear();

      cell.isActive = false;

      ++last_normalization_fix_count_;

      oss << "HLINE inserted at rung=" << rungIdx << ", x=" << col << "\n";

    }

  };



  [[maybe_unused]] auto has_vc_at = [&](int x) -> bool {

    for (const auto& vc : norm.verticalConnections) {

      if (vc.x == x)

        return true;

    }

    return false;

  };



  auto add_or_merge_vc = [&](int x, const std::vector<int>& rungs) {

    //         ?

    for (auto& vc : norm.verticalConnections) {

      if (vc.x == x) {

        // rungs          

        for (int r : rungs) {

          if (std::find(vc.rungs.begin(), vc.rungs.end(), r) ==

              vc.rungs.end()) {

            vc.rungs.push_back(r);

          }

        }

        return;

      }

    }


    VerticalConnection newVc;

    newVc.x = x;

    newVc.rungs = rungs;

    norm.verticalConnections.push_back(newVc);

  };




  auto find_rightmost_output_col = [&](const Rung& rung) -> int {

    int right = -1;

    for (int c = static_cast<int>(rung.cells.size()) - 1; c >= 0; --c) {

      switch (rung.cells[static_cast<size_t>(c)].type) {

        case LadderInstructionType::OTE:

        case LadderInstructionType::SET:

        case LadderInstructionType::RST:

          return c;

        default:

          break;

      }

    }

    return right;  // -1

  };




  for (const auto& vc : src.verticalConnections) {

    int x = vc.x;

    if (x < 0)

      continue;


    for (int rungIdx : vc.rungs) {

      if (rungIdx < 0 || rungIdx >= static_cast<int>(norm.rungs.size()))

        continue;

      ensure_hline(rungIdx, x);

    }





    int x_end = -1;

    int anchorRung = -1;

    if (!vc.rungs.empty()) {

      anchorRung = *std::min_element(vc.rungs.begin(), vc.rungs.end());

      if (anchorRung >= 0 && anchorRung < static_cast<int>(norm.rungs.size())) {

        x_end = find_rightmost_output_col(norm.rungs[static_cast<size_t>(anchorRung)]);

      }

    }

    if (x_end < 0)

      x_end = static_cast<int>(norm.rungs.front().cells.size()) - 1;




    int l = std::min(x, x_end), r = std::max(x, x_end);

    for (int rungIdx : vc.rungs) {

      for (int col = l + 1; col < r; ++col)

        ensure_hline(rungIdx, col);

    }




    add_or_merge_vc(x_end, vc.rungs);

  }




  for (size_t i = 0; i < norm.rungs.size(); ++i) {

    if (norm.rungs[i].isEndRung && norm.rungs[i].number != static_cast<int>(i)) {

      oss << "EndRung number normalized hint (rung=" << i << ")\n";

    }

  }



  last_normalization_summary_ = oss.str();

  return norm;

}



}  // namespace plc

