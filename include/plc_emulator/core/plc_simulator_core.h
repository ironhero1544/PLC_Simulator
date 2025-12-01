// plc_simulator_core.h
// Copyright 2024 PLC Emulator Project
//
// Core simulation logic coordinator.

// include/PLCSimulatorCore.h
// 중앙 집중식 PLC 시뮬레이터 코어
// 실배선 모드와 프로그래밍 모드 간의 데이터 동기화 및 통합 관리

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_PLC_SIMULATOR_CORE_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_PLC_SIMULATOR_CORE_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/io/io_mapping.h"  // Phase 2: I/O 매핑 데이터 구조
#include "plc_emulator/programming/programming_mode.h"  // 기존 타입들 사용 (하위 호환성)

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace plc {

// 전방 선언 (Repository, IOMapper, EventSystem)
struct ComponentRepository;
struct WiringRepository;
struct LadderRepository;
struct IORepository;  // Phase 2: I/O 매핑 리포지토리
struct IOMapper;      // Phase 2: I/O 매핑 엔진
struct EventDispatcher;

// PLCSimulatorCore - 중앙 집중식 데이터 모델 및 시뮬레이션 엔진
class PLCSimulatorCore {
 public:
  PLCSimulatorCore();
  ~PLCSimulatorCore();

  // 기본 초기화 및 정리
  bool Initialize();
  void Shutdown();


  // 실배선 모드 데이터
  std::vector<PlacedComponent>& GetPlacedComponents() {
    return placed_components_;
  }
  const std::vector<PlacedComponent>& GetPlacedComponents() const {
    return placed_components_;
  }

  std::vector<Wire>& GetWires() { return wires_; }
  const std::vector<Wire>& GetWires() const { return wires_; }

  // 프로그래밍 모드 데이터
  LadderProgram& GetLadderProgram() { return ladder_program_; }
  const LadderProgram& GetLadderProgram() const { return ladder_program_; }

  std::map<std::string, bool>& GetDeviceStates() { return device_states_; }
  const std::map<std::string, bool>& GetDeviceStates() const {
    return device_states_;
  }


  // 프로그래밍 모드 → 실배선 모드 동기화
  void SyncFromProgrammingMode(const ProgrammingMode* programmingMode);

  // 실배선 모드 → 프로그래밍 모드 동기화 (향후 구현)
  void SyncToProgrammingMode(ProgrammingMode* programmingMode);


  // I/O 매핑 업데이트 (배선 변경 시 자동 호출)
  bool UpdateIOMapping();

  // 현재 I/O 매핑 정보 가져오기
  const IOMapping& GetCurrentIOMapping() const;

  // I/O 매핑 결과 가져오기
  const MappingResult& GetLastMappingResult() const;

  // I/O 매핑 동기화 (프로그래밍 모드와 연동)
  void SyncIOMapping();

  // I/O 매핑 유효성 검증
  bool ValidateIOMapping() const;


  // 데이터 변경 알림
  void NotifyDataChanged();

  // 모드 전환 알림
  void NotifyModeChanged(Mode newMode);

 private:
  std::vector<PlacedComponent> placed_components_;
  std::vector<Wire> wires_;

  LadderProgram ladder_program_;
  std::map<std::string, bool> device_states_;
  std::map<std::string, TimerState> timer_states_;
  std::map<std::string, CounterState> counter_states_;

  std::unique_ptr<ComponentRepository> component_repo_;
  std::unique_ptr<WiringRepository> wiring_repo_;
  std::unique_ptr<LadderRepository> ladder_repo_;
  std::unique_ptr<IORepository> io_repo_;  // Phase 2: I/O 매핑 리포지토리

  std::unique_ptr<IOMapper> io_mapper_;  // I/O 매핑 엔진
  IOMapping current_io_mapping_;          // 현재 I/O 매핑 정보
  MappingResult last_mapping_result_;     // 마지막 매핑 결과

  std::unique_ptr<EventDispatcher> event_dispatcher_;

  // 내부 헬퍼 함수들
  void InitializeDefaultData();
  void ClearAllData();
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_PLC_SIMULATOR_CORE_H_
