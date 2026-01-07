// plc_simulator_core.cpp
//
// Implementation of core simulator.

// src/PLCSimulatorCore.cpp
// PLCSimulatorCore 구현 - 중앙 집중식 데이터 관리 및 모드 간 동기화

#include "plc_emulator/core/plc_simulator_core.h"

#include "plc_emulator/data/data_repository.h"
#include "plc_emulator/data/event_system.h"
#include "plc_emulator/io/io_mapper.h"   // Phase 2: I/O 매핑 시스템
#include "plc_emulator/io/io_mapping.h"  // Phase 2: I/O 매핑 데이터 구조

#include <algorithm>
#include <iostream>

namespace plc {

PLCSimulatorCore::PLCSimulatorCore()
    : component_repo_(nullptr),
      wiring_repo_(nullptr),
      ladder_repo_(nullptr),
      io_repo_(nullptr),    // Phase 2: IORepository 초기화
      io_mapper_(nullptr),  // Phase 2: IOMapper 초기화
      event_dispatcher_(nullptr) {
  // 기본 데이터 초기화
  InitializeDefaultData();
}

PLCSimulatorCore::~PLCSimulatorCore() {
  Shutdown();
}

bool PLCSimulatorCore::Initialize() {
  // 기본 데이터 초기화
  InitializeDefaultData();


  // ComponentRepository 생성
  ComponentRepository* compRepo =
      CreateComponentRepository(&placed_components_);
  if (compRepo) {
    component_repo_ = std::unique_ptr<ComponentRepository>(compRepo);
    std::cout << "ComponentRepository initialized successfully." << std::endl;
  }

  // WiringRepository 생성
  WiringRepository* wireRepo = CreateWiringRepository(&wires_);
  if (wireRepo) {
    wiring_repo_ = std::unique_ptr<WiringRepository>(wireRepo);
    std::cout << "WiringRepository initialized successfully." << std::endl;
  }

  // LadderRepository 생성
  LadderRepository* ladderRepo = CreateLadderRepository(
      &ladder_program_, &device_states_, &timer_states_, &counter_states_);
  if (ladderRepo) {
    ladder_repo_ = std::unique_ptr<LadderRepository>(ladderRepo);
    std::cout << "LadderRepository initialized successfully." << std::endl;
  }


  // IOMapper 생성
  IOMapper* ioMapper = CreateIOMapper();
  if (ioMapper) {
    io_mapper_ = std::unique_ptr<IOMapper>(ioMapper);
    std::cout << "IOMapper initialized successfully." << std::endl;
  }

  // IORepository 생성
  IORepository* ioRepo =
      CreateIORepository(&current_io_mapping_, &last_mapping_result_);
  if (ioRepo) {
    io_repo_ = std::unique_ptr<IORepository>(ioRepo);
    std::cout << "IORepository initialized successfully." << std::endl;
  }

  // EventDispatcher 생성
  EventDispatcher* eventDisp = CreateEventDispatcher();
  if (eventDisp) {
    event_dispatcher_ = std::unique_ptr<EventDispatcher>(eventDisp);
    std::cout << "EventDispatcher initialized successfully." << std::endl;
  }

  std::cout << "PLCSimulatorCore initialized with all repositories and "
               "event system."
            << std::endl;
  return true;
}

void PLCSimulatorCore::Shutdown() {
  ClearAllData();


  // DataRepository들 정리
  if (component_repo_) {
    component_repo_.reset();  // unique_ptr이 자동으로 delete 호출
    std::cout << "ComponentRepository destroyed." << std::endl;
  }

  if (wiring_repo_) {
    wiring_repo_.reset();
    std::cout << "WiringRepository destroyed." << std::endl;
  }

  if (ladder_repo_) {
    ladder_repo_.reset();
    std::cout << "LadderRepository destroyed." << std::endl;
  }


  if (io_mapper_) {
    io_mapper_.reset();
    std::cout << "IOMapper destroyed." << std::endl;
  }

  if (io_repo_) {
    io_repo_.reset();
    std::cout << "IORepository destroyed." << std::endl;
  }

  // EventDispatcher 정리
  if (event_dispatcher_) {
    event_dispatcher_.reset();
    std::cout << "EventDispatcher destroyed." << std::endl;
  }

  std::cout << "PLCSimulatorCore shutdown completed." << std::endl;
}


void PLCSimulatorCore::SyncFromProgrammingMode(
    const ProgrammingMode* programmingMode) {
  if (!programmingMode) {
    std::cerr << "Error: ProgrammingMode is null, cannot sync data."
              << std::endl;
    return;
  }

  // 래더 프로그램 동기화
  ladder_program_ = programmingMode->GetLadderProgram();

  // 디바이스 상태 동기화
  device_states_ = programmingMode->GetDeviceStates();

  // 타이머 상태 동기화
  timer_states_ = programmingMode->GetTimerStates();

  // 카운터 상태 동기화
  counter_states_ = programmingMode->GetCounterStates();

  std::cout << "Data synchronized from ProgrammingMode to PLCSimulatorCore."
            << std::endl;

  // 데이터 변경 알림
  NotifyDataChanged();
}

void PLCSimulatorCore::SyncToProgrammingMode(ProgrammingMode* programmingMode) {
  // TODO: 향후 구현 - 실배선 모드에서 프로그래밍 모드로 I/O 매핑 정보 전달
  if (!programmingMode) {
    std::cerr << "Error: ProgrammingMode is null, cannot sync data."
              << std::endl;
    return;
  }

  std::cout << "SyncToProgrammingMode - Feature will be implemented in Phase 2."
            << std::endl;
}


void PLCSimulatorCore::NotifyDataChanged() {
  if (event_dispatcher_) {
    EventData event = CreateDataChangedEvent();
    event_dispatcher_->Dispatch(event_dispatcher_.get(), &event);
  }
  std::cout << "PLCSimulatorCore: Data changed event dispatched."
            << std::endl;
}

void PLCSimulatorCore::NotifyModeChanged(Mode newMode) {
  if (event_dispatcher_) {
    // Mode enum을 int로 변환 (기존 코드와의 호환성)
    int oldModeInt = 0;  // 현재는 이전 모드를 추적하지 않음
    int newModeInt = (newMode == Mode::WIRING) ? 0 : 1;

    EventData event = CreateModeChangedEvent(oldModeInt, newModeInt);
    event_dispatcher_->Dispatch(event_dispatcher_.get(), &event);
  }

  const char* modeStr = (newMode == Mode::WIRING) ? "WIRING" : "PROGRAMMING";
  std::cout << "PLCSimulatorCore: Mode changed event dispatched to "
            << modeStr << std::endl;
}


void PLCSimulatorCore::InitializeDefaultData() {
  // 기본 디바이스 상태 초기화 (기존 Application과 동일)
  device_states_.clear();
  timer_states_.clear();
  counter_states_.clear();

  // X, Y, M 디바이스 기본 초기화 (0-15)
  for (int i = 0; i < 16; ++i) {
    std::string i_str = std::to_string(i);
    device_states_["X" + i_str] = false;
    device_states_["Y" + i_str] = false;
    device_states_["M" + i_str] = false;
  }

  // 실배선 컴포넌트 및 배선 리스트 초기화
  placed_components_.clear();
  wires_.clear();

  std::cout << "PLCSimulatorCore: Default data initialized." << std::endl;
}

void PLCSimulatorCore::ClearAllData() {
  placed_components_.clear();
  wires_.clear();
  device_states_.clear();
  timer_states_.clear();
  counter_states_.clear();

  // 래더 프로그램 초기화
  ladder_program_ = LadderProgram();  // 기본 생성자로 초기화

  std::cout << "PLCSimulatorCore: All data cleared." << std::endl;
}


bool PLCSimulatorCore::UpdateIOMapping() {
  if (!io_mapper_) {
    std::cerr << "Error: IOMapper not initialized" << std::endl;
    return false;
  }

  std::cout << "Updating I/O mapping from current wiring..." << std::endl;

  // IOMapper를 사용하여 현재 배선 정보로부터 I/O 매핑 추출
  MappingResult result = io_mapper_->ExtractMapping(io_mapper_.get(), &wires_,
                                                    &placed_components_);

  // 결과 저장
  last_mapping_result_ = result;

  if (result.success) {
    current_io_mapping_ = result.mapping;

    // IORepository에 업데이트
    if (io_repo_) {
      io_repo_->UpdateMapping(io_repo_.get(), &current_io_mapping_);
      io_repo_->SetMappingResult(io_repo_.get(), &last_mapping_result_);
    }

    std::cout << "I/O mapping updated successfully" << std::endl;
    std::cout << "Generated: " << current_io_mapping_.inputCount
              << " inputs (X), " << current_io_mapping_.outputCount
              << " outputs (Y)" << std::endl;

    // 이벤트 발송 (향후 구현)
    // NotifyIOMappingUpdated();

    return true;
  }

  return false;
}

const IOMapping& PLCSimulatorCore::GetCurrentIOMapping() const {
  return current_io_mapping_;
}

const MappingResult& PLCSimulatorCore::GetLastMappingResult() const {
  return last_mapping_result_;
}

void PLCSimulatorCore::SyncIOMapping() {
  // 현재는 기본적인 동기화만 수행
  // Phase 3에서 프로그래밍 모드와 실제 연동 구현 예정

  if (!UpdateIOMapping()) {
    return;
  }

  std::cout << "I/O mapping synchronized with current wiring state"
            << std::endl;

  // TODO Phase 3: 프로그래밍 모드의 X, Y 디바이스와 실제 매핑 연동
  // - m_deviceStates와 실제 컴포넌트 상태 연결
  // - 매핑된 주소에 따른 자동 디바이스 상태 업데이트
}

bool PLCSimulatorCore::ValidateIOMapping() const {
  if (!io_repo_) {
    return false;
  }

  return io_repo_->IsValidMapping(io_repo_.get());
}

}  // namespace plc

