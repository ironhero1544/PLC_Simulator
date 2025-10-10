// src/PLCSimulatorCore.cpp
// PLCSimulatorCore 구현 - 중앙 집중식 데이터 관리 및 모드 간 동기화

#include "plc_emulator/core/plc_simulator_core.h"
#include "plc_emulator/data/data_repository.h"
#include "plc_emulator/data/event_system.h"
#include "plc_emulator/io/io_mapper.h"        // Phase 2: I/O 매핑 시스템
#include "plc_emulator/io/io_mapping.h"       // Phase 2: I/O 매핑 데이터 구조
#include <iostream>
#include <algorithm>

namespace plc {

PLCSimulatorCore::PLCSimulatorCore()
    : m_componentRepo(nullptr),
      m_wiringRepo(nullptr),
      m_ladderRepo(nullptr),
      m_ioRepo(nullptr),           // Phase 2: IORepository 초기화
      m_ioMapper(nullptr),         // Phase 2: IOMapper 초기화
      m_eventDispatcher(nullptr)
{
    // 기본 데이터 초기화
    InitializeDefaultData();
}

PLCSimulatorCore::~PLCSimulatorCore() {
    Shutdown();
}

bool PLCSimulatorCore::Initialize() {
    // 기본 데이터 초기화
    InitializeDefaultData();
    
    // === Phase 1: DataRepository와 EventSystem 실제 생성 ===
    
    // ComponentRepository 생성 (C언어 스타일)
    ComponentRepository* compRepo = CreateComponentRepository(&m_placedComponents);
    if (compRepo) {
        m_componentRepo = std::unique_ptr<ComponentRepository>(compRepo);
        std::cout << "ComponentRepository initialized successfully." << std::endl;
    }
    
    // WiringRepository 생성
    WiringRepository* wireRepo = CreateWiringRepository(&m_wires);
    if (wireRepo) {
        m_wiringRepo = std::unique_ptr<WiringRepository>(wireRepo);
        std::cout << "WiringRepository initialized successfully." << std::endl;
    }
    
    // LadderRepository 생성
    LadderRepository* ladderRepo = CreateLadderRepository(&m_ladderProgram, &m_deviceStates, 
                                                         &m_timerStates, &m_counterStates);
    if (ladderRepo) {
        m_ladderRepo = std::unique_ptr<LadderRepository>(ladderRepo);
        std::cout << "LadderRepository initialized successfully." << std::endl;
    }
    
    // === Phase 2: IOMapper 및 IORepository 생성 ===
    
    // IOMapper 생성
    IOMapper* ioMapper = CreateIOMapper();
    if (ioMapper) {
        m_ioMapper = std::unique_ptr<IOMapper>(ioMapper);
        std::cout << "IOMapper initialized successfully." << std::endl;
    }
    
    // IORepository 생성
    IORepository* ioRepo = CreateIORepository(&m_currentIOMapping, &m_lastMappingResult);
    if (ioRepo) {
        m_ioRepo = std::unique_ptr<IORepository>(ioRepo);
        std::cout << "IORepository initialized successfully." << std::endl;
    }
    
    // EventDispatcher 생성
    EventDispatcher* eventDisp = CreateEventDispatcher();
    if (eventDisp) {
        m_eventDispatcher = std::unique_ptr<EventDispatcher>(eventDisp);
        std::cout << "EventDispatcher initialized successfully." << std::endl;
    }
    
    std::cout << "✅ PLCSimulatorCore initialized with all repositories and event system." << std::endl;
    return true;
}

void PLCSimulatorCore::Shutdown() {
    ClearAllData();
    
    // === Phase 1: Repository들 정리 (C언어 스타일 소멸자 호출) ===
    
    // DataRepository들 정리
    if (m_componentRepo) {
        m_componentRepo.reset(); // unique_ptr이 자동으로 delete 호출
        std::cout << "ComponentRepository destroyed." << std::endl;
    }
    
    if (m_wiringRepo) {
        m_wiringRepo.reset();
        std::cout << "WiringRepository destroyed." << std::endl;
    }
    
    if (m_ladderRepo) {
        m_ladderRepo.reset();
        std::cout << "LadderRepository destroyed." << std::endl;
    }
    
    // === Phase 2: IOMapper 및 IORepository 정리 ===
    
    if (m_ioMapper) {
        m_ioMapper.reset();
        std::cout << "IOMapper destroyed." << std::endl;
    }
    
    if (m_ioRepo) {
        m_ioRepo.reset();
        std::cout << "IORepository destroyed." << std::endl;
    }
    
    // EventDispatcher 정리
    if (m_eventDispatcher) {
        m_eventDispatcher.reset();
        std::cout << "EventDispatcher destroyed." << std::endl;
    }
    
    std::cout << "✅ PLCSimulatorCore shutdown completed." << std::endl;
}

// === 모드 간 동기화 함수들 ===

void PLCSimulatorCore::SyncFromProgrammingMode(const ProgrammingMode* programmingMode) {
    if (!programmingMode) {
        std::cerr << "Error: ProgrammingMode is null, cannot sync data." << std::endl;
        return;
    }

    // 래더 프로그램 동기화
    m_ladderProgram = programmingMode->GetLadderProgram();
    
    // 디바이스 상태 동기화
    m_deviceStates = programmingMode->GetDeviceStates();
    
    // 타이머 상태 동기화
    m_timerStates = programmingMode->GetTimerStates();
    
    // 카운터 상태 동기화
    m_counterStates = programmingMode->GetCounterStates();

    std::cout << "Data synchronized from ProgrammingMode to PLCSimulatorCore." << std::endl;
    
    // 데이터 변경 알림
    NotifyDataChanged();
}

void PLCSimulatorCore::SyncToProgrammingMode(ProgrammingMode* programmingMode) {
    // TODO: 향후 구현 - 실배선 모드에서 프로그래밍 모드로 I/O 매핑 정보 전달
    if (!programmingMode) {
        std::cerr << "Error: ProgrammingMode is null, cannot sync data." << std::endl;
        return;
    }
    
    std::cout << "SyncToProgrammingMode - Feature will be implemented in Phase 2." << std::endl;
}

// === 이벤트 시스템 (기본 구조) ===

void PLCSimulatorCore::NotifyDataChanged() {
    // === Phase 1: EventDispatcher를 통한 실제 이벤트 발송 ===
    if (m_eventDispatcher) {
        EventData event = CreateDataChangedEvent();
        m_eventDispatcher->Dispatch(m_eventDispatcher.get(), &event);
    }
    std::cout << "📡 PLCSimulatorCore: Data changed event dispatched." << std::endl;
}

void PLCSimulatorCore::NotifyModeChanged(Mode newMode) {
    // === Phase 1: EventDispatcher를 통한 실제 이벤트 발송 ===
    if (m_eventDispatcher) {
        // Mode enum을 int로 변환 (기존 코드와의 호환성)
        int oldModeInt = 0; // 현재는 이전 모드를 추적하지 않음
        int newModeInt = (newMode == Mode::WIRING) ? 0 : 1;
        
        EventData event = CreateModeChangedEvent(oldModeInt, newModeInt);
        m_eventDispatcher->Dispatch(m_eventDispatcher.get(), &event);
    }
    
    const char* modeStr = (newMode == Mode::WIRING) ? "WIRING" : "PROGRAMMING";
    std::cout << "📡 PLCSimulatorCore: Mode changed event dispatched to " << modeStr << std::endl;
}

// === 내부 헬퍼 함수들 ===

void PLCSimulatorCore::InitializeDefaultData() {
    // 기본 디바이스 상태 초기화 (기존 Application과 동일)
    m_deviceStates.clear();
    m_timerStates.clear();
    m_counterStates.clear();
    
    // X, Y, M 디바이스 기본 초기화 (0-15)
    for (int i = 0; i < 16; ++i) {
        std::string i_str = std::to_string(i);
        m_deviceStates["X" + i_str] = false;
        m_deviceStates["Y" + i_str] = false;
        m_deviceStates["M" + i_str] = false;
    }
    
    // 실배선 컴포넌트 및 배선 리스트 초기화
    m_placedComponents.clear();
    m_wires.clear();
    
    std::cout << "PLCSimulatorCore: Default data initialized." << std::endl;
}

void PLCSimulatorCore::ClearAllData() {
    m_placedComponents.clear();
    m_wires.clear();
    m_deviceStates.clear();
    m_timerStates.clear();  
    m_counterStates.clear();
    
    // 래더 프로그램 초기화
    m_ladderProgram = LadderProgram(); // 기본 생성자로 초기화
    
    std::cout << "PLCSimulatorCore: All data cleared." << std::endl;
}

// === Phase 2: I/O 매핑 시스템 구현 ===

bool PLCSimulatorCore::UpdateIOMapping() {
    if (!m_ioMapper) {
        std::cerr << "Error: IOMapper not initialized" << std::endl;
        return false;
    }
    
    std::cout << "🔄 Updating I/O mapping from current wiring..." << std::endl;
    
    // IOMapper를 사용하여 현재 배선 정보로부터 I/O 매핑 추출
    MappingResult result = m_ioMapper->ExtractMapping(m_ioMapper.get(), &m_wires, &m_placedComponents);
    
    // 결과 저장
    m_lastMappingResult = result;
    
    if (result.success) {
        m_currentIOMapping = result.mapping;
        
        // IORepository에 업데이트
        if (m_ioRepo) {
            m_ioRepo->UpdateMapping(m_ioRepo.get(), &m_currentIOMapping);
            m_ioRepo->SetMappingResult(m_ioRepo.get(), &m_lastMappingResult);
        }
        
        std::cout << "✅ I/O mapping updated successfully" << std::endl;
        std::cout << "📊 Generated: " << m_currentIOMapping.inputCount << " inputs (X), " 
                 << m_currentIOMapping.outputCount << " outputs (Y)" << std::endl;
        
        // 이벤트 발송 (향후 구현)
        // NotifyIOMappingUpdated();
        
        return true;
    } else {
        std::cerr << "❌ Failed to update I/O mapping" << std::endl;
        if (result.HasErrors()) {
            for (const auto& error : result.errors) {
                std::cerr << "  Error: " << error << std::endl;
            }
        }
        return false;
    }
}

const IOMapping& PLCSimulatorCore::GetCurrentIOMapping() const {
    return m_currentIOMapping;
}

const MappingResult& PLCSimulatorCore::GetLastMappingResult() const {
    return m_lastMappingResult;
}

void PLCSimulatorCore::SyncIOMapping() {
    // 현재는 기본적인 동기화만 수행
    // Phase 3에서 프로그래밍 모드와 실제 연동 구현 예정
    
    if (!UpdateIOMapping()) {
        std::cerr << "Warning: I/O mapping sync failed" << std::endl;
        return;
    }
    
    std::cout << "🔄 I/O mapping synchronized with current wiring state" << std::endl;
    
    // TODO Phase 3: 프로그래밍 모드의 X, Y 디바이스와 실제 매핑 연동
    // - m_deviceStates와 실제 컴포넌트 상태 연결
    // - 매핑된 주소에 따른 자동 디바이스 상태 업데이트
}

bool PLCSimulatorCore::ValidateIOMapping() const {
    if (!m_ioRepo) {
        return false;
    }
    
    return m_ioRepo->IsValidMapping(m_ioRepo.get());
}

} // namespace plc