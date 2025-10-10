// include/PLCSimulatorCore.h
// 중앙 집중식 PLC 시뮬레이터 코어
// 실배선 모드와 프로그래밍 모드 간의 데이터 동기화 및 통합 관리

#pragma once

#include "DataTypes.h"
#include "ProgrammingMode.h"  // 기존 타입들 사용 (하위 호환성)
#include "IOMapping.h"        // Phase 2: I/O 매핑 데이터 구조
#include <vector>
#include <string>
#include <map>
#include <memory>

namespace plc {

// 전방 선언 (Repository, IOMapper, EventSystem)
struct ComponentRepository;
struct WiringRepository; 
struct LadderRepository;
struct IORepository;    // Phase 2: I/O 매핑 리포지토리
struct IOMapper;        // Phase 2: I/O 매핑 엔진
struct EventDispatcher;

// PLCSimulatorCore - 중앙 집중식 데이터 모델 및 시뮬레이션 엔진
class PLCSimulatorCore {
public:
    PLCSimulatorCore();
    ~PLCSimulatorCore();

    // 기본 초기화 및 정리
    bool Initialize();
    void Shutdown();

    // === 데이터 접근자 함수들 (C언어 스타일) ===
    
    // 실배선 모드 데이터
    std::vector<PlacedComponent>& GetPlacedComponents() { return m_placedComponents; }
    const std::vector<PlacedComponent>& GetPlacedComponents() const { return m_placedComponents; }
    
    std::vector<Wire>& GetWires() { return m_wires; }
    const std::vector<Wire>& GetWires() const { return m_wires; }

    // 프로그래밍 모드 데이터
    LadderProgram& GetLadderProgram() { return m_ladderProgram; }
    const LadderProgram& GetLadderProgram() const { return m_ladderProgram; }
    
    std::map<std::string, bool>& GetDeviceStates() { return m_deviceStates; }
    const std::map<std::string, bool>& GetDeviceStates() const { return m_deviceStates; }

    // === 모드 간 동기화 함수들 ===
    
    // 프로그래밍 모드 → 실배선 모드 동기화
    void SyncFromProgrammingMode(const ProgrammingMode* programmingMode);
    
    // 실배선 모드 → 프로그래밍 모드 동기화 (향후 구현)
    void SyncToProgrammingMode(ProgrammingMode* programmingMode);

    // === I/O 매핑 시스템 (Phase 2) ===
    
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

    // === 이벤트 시스템 (기본 구조만) ===
    
    // 데이터 변경 알림
    void NotifyDataChanged();
    
    // 모드 전환 알림
    void NotifyModeChanged(Mode newMode);

private:
    // === 실배선 모드 데이터 ===
    std::vector<PlacedComponent> m_placedComponents;
    std::vector<Wire> m_wires;
    
    // === 프로그래밍 모드 데이터 ===
    LadderProgram m_ladderProgram;
    std::map<std::string, bool> m_deviceStates;
    std::map<std::string, TimerState> m_timerStates;
    std::map<std::string, CounterState> m_counterStates;

    // === 데이터 저장소들 (C언어 스타일 구조체) ===
    std::unique_ptr<ComponentRepository> m_componentRepo;
    std::unique_ptr<WiringRepository> m_wiringRepo;
    std::unique_ptr<LadderRepository> m_ladderRepo;
    std::unique_ptr<IORepository> m_ioRepo;            // Phase 2: I/O 매핑 리포지토리
    
    // === I/O 매핑 시스템 (Phase 2) ===
    std::unique_ptr<IOMapper> m_ioMapper;              // I/O 매핑 엔진
    IOMapping m_currentIOMapping;                      // 현재 I/O 매핑 정보
    MappingResult m_lastMappingResult;                // 마지막 매핑 결과
    
    // === 이벤트 시스템 (C언어 스타일 구조체) ===
    std::unique_ptr<EventDispatcher> m_eventDispatcher;

    // 내부 헬퍼 함수들
    void InitializeDefaultData();
    void ClearAllData();
};

} // namespace plc