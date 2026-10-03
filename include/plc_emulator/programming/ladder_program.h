#pragma once

#include "plc_emulator/programming/execution_program.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace plc {

/*
 * 래더 셀 명령 타입.
 * Ladder cell instruction types.
 */
enum class LadderInstructionType {
  EMPTY,
  XIC,
  XIO,
  OTE,
  HLINE,
  SET,
  RST,
  TON,
  CTU,
  RST_TMR_CTR,
  BKRST,
  APPLICATION,
  COMPARISON,
  RISING_CONTACT,
  FALLING_CONTACT,
  kWrappingSource,
  kWrappingDestination
};

/*
 * 래더 셀의 명령과 상태.
 * Instruction and state for a ladder cell.
 */
struct LadderInstruction {
  LadderInstructionType type = LadderInstructionType::EMPTY;
  std::string address;
  std::string preset;
  bool isActive = false;
  std::string mnemonic;
  std::vector<plc_emulator::programming::Operand> operands;
  bool pulse = false;
  bool wide = false;
};

/*
 * 타이머 상태 값.
 * Timer state values.
 */
struct TimerState {
  int value = 0;
  bool done = false;
  int preset = 0;
  bool enabled = false;
};

/*
 * 카운터 상태 값.
 * Counter state values.
 */
struct CounterState {
  int value = 0;
  bool done = false;
  int preset = 0;
  bool lastPower = false;
};

/*
 * 시믬레이터 실행 상태 스냅샷.
 * Snapshot of simulator execution state.
 */
struct SimulatorState {
  std::map<std::string, bool> deviceStates;
  std::map<std::string, TimerState> timerStates;
  std::map<std::string, CounterState> counterStates;
  uint64_t seqNo = 0;
  std::chrono::steady_clock::time_point timestamp;

  SimulatorState() : timestamp(std::chrono::steady_clock::now()) {}

  SimulatorState(const SimulatorState& other)
      : deviceStates(other.deviceStates),
        timerStates(other.timerStates),
        counterStates(other.counterStates),
        seqNo(other.seqNo),
        timestamp(std::chrono::steady_clock::now()) {}

  SimulatorState& operator=(const SimulatorState& other) {
    if (this != &other) {
      deviceStates = other.deviceStates;
      timerStates = other.timerStates;
      counterStates = other.counterStates;
      seqNo = other.seqNo;
      timestamp = std::chrono::steady_clock::now();
    }
    return *this;
  }

  void UpdateDeviceState(const std::string& address, bool state) {
    deviceStates[address] = state;
    seqNo++;
    timestamp = std::chrono::steady_clock::now();
  }
};

/*
 * 래더 룽 데이터.
 * Ladder rung data.
 */
struct Rung {
  int number = 0;
  std::vector<LadderInstruction> cells;
  std::string memo;
  bool isEndRung = false;
  Rung() : cells(12) {}
};

/*
 * 룽 간 세로 연결 정보.
 * Vertical connection between rungs.
 */
struct VerticalConnection {
  int x = 0;
  std::vector<int> rungs;

  VerticalConnection() = default;
  VerticalConnection(int x_pos, int start_rung, int end_rung) : x(x_pos) {
    for (int i = start_rung; i <= end_rung; i++) {
      rungs.push_back(i);
    }
  }

  int startRung() const { return rungs.empty() ? 0 : rungs.front(); }

  int endRung() const { return rungs.empty() ? 0 : rungs.back(); }
};

/*
 * 래더 프로그램 구성.
 * Ladder program container.
 */
struct LadderProgram {
  std::optional<plc_emulator::programming::ExecutionProgram> canonical_program;
  size_t canonical_fingerprint = 0;
  std::vector<Rung> rungs;
  std::vector<VerticalConnection> verticalConnections;
  LadderProgram() {
    rungs.emplace_back();
    rungs.back().number = 0;
    rungs.emplace_back();
    rungs.back().isEndRung = true;
  }
};

}  // namespace plc
