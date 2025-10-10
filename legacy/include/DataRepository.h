// include/DataRepository.h
// C언어 스타일 데이터 저장소 패턴 구현
// 구조체 + 함수 포인터 기반으로 데이터 접근 계층 분리

#pragma once

#include "DataTypes.h"
#include "ProgrammingMode.h"  // 기존 타입들 사용 (하위 호환성)
#include "IOMapping.h"        // Phase 2: I/O 매핑 시스템
#include <vector>
#include <string>
#include <map>

namespace plc {

// === ComponentRepository: 실배선 컴포넌트 관리 ===

typedef struct ComponentRepository {
    // 데이터 저장소 (STL 컨테이너 사용)
    std::vector<PlacedComponent>* components;
    
    // 함수 포인터들 - C언어 스타일
    int (*AddComponent)(struct ComponentRepository* repo, const PlacedComponent* component);
    bool (*RemoveComponent)(struct ComponentRepository* repo, int instanceId);
    PlacedComponent* (*FindComponent)(struct ComponentRepository* repo, int instanceId);
    void (*ClearComponents)(struct ComponentRepository* repo);
    size_t (*GetComponentCount)(struct ComponentRepository* repo);
} ComponentRepository;

// ComponentRepository 생성/초기화 함수들
ComponentRepository* CreateComponentRepository(std::vector<PlacedComponent>* components);
void DestroyComponentRepository(ComponentRepository* repo);

// === WiringRepository: 배선 정보 관리 ===

typedef struct WiringRepository {
    // 데이터 저장소
    std::vector<Wire>* wires;
    
    // 함수 포인터들
    int (*AddWire)(struct WiringRepository* repo, const Wire* wire);
    bool (*RemoveWire)(struct WiringRepository* repo, int wireId);
    Wire* (*FindWire)(struct WiringRepository* repo, int wireId);
    void (*ClearWires)(struct WiringRepository* repo);
    size_t (*GetWireCount)(struct WiringRepository* repo);
    
    // 배선 관련 특수 기능들
    std::vector<Wire*> (*FindWiresByComponent)(struct WiringRepository* repo, int componentId);
    bool (*IsComponentConnected)(struct WiringRepository* repo, int componentId, int portId);
} WiringRepository;

// WiringRepository 생성/초기화 함수들
WiringRepository* CreateWiringRepository(std::vector<Wire>* wires);
void DestroyWiringRepository(WiringRepository* repo);

// === LadderRepository: 래더 프로그램 관리 ===

typedef struct LadderRepository {
    // 데이터 저장소들 (plc 네임스페이스 명시)
    plc::LadderProgram* program;
    std::map<std::string, bool>* deviceStates;
    std::map<std::string, plc::TimerState>* timerStates;
    std::map<std::string, plc::CounterState>* counterStates;
    
    // 래더 프로그램 관리 함수들
    bool (*LoadProgram)(struct LadderRepository* repo, const plc::LadderProgram* newProgram);
    void (*ClearProgram)(struct LadderRepository* repo);
    plc::Rung* (*GetRung)(struct LadderRepository* repo, int rungIndex);
    
    // 디바이스 상태 관리 함수들
    bool (*SetDeviceState)(struct LadderRepository* repo, const char* address, bool state);
    bool (*GetDeviceState)(struct LadderRepository* repo, const char* address);
    void (*ClearDeviceStates)(struct LadderRepository* repo);
    
    // 타이머/카운터 관리 함수들
    bool (*SetTimerState)(struct LadderRepository* repo, const char* address, const plc::TimerState* state);
    bool (*SetCounterState)(struct LadderRepository* repo, const char* address, const plc::CounterState* state);
} LadderRepository;

// LadderRepository 생성/초기화 함수들
LadderRepository* CreateLadderRepository(plc::LadderProgram* program, 
                                        std::map<std::string, bool>* deviceStates,
                                        std::map<std::string, plc::TimerState>* timerStates,
                                        std::map<std::string, plc::CounterState>* counterStates);
void DestroyLadderRepository(LadderRepository* repo);

// === IORepository: I/O 매핑 정보 관리 (Phase 2) ===

typedef struct IORepository {
    // 데이터 저장소
    IOMapping* mapping;                        // 현재 I/O 매핑 정보
    MappingResult* lastResult;                // 마지막 매핑 결과
    
    // I/O 매핑 관리 함수들
    bool (*UpdateMapping)(struct IORepository* repo, const IOMapping* newMapping);
    const IOMapping* (*GetMapping)(struct IORepository* repo);
    void (*ClearMapping)(struct IORepository* repo);
    bool (*IsValidMapping)(struct IORepository* repo);
    
    // 매핑 결과 관리 함수들
    bool (*SetMappingResult)(struct IORepository* repo, const MappingResult* result);
    const MappingResult* (*GetMappingResult)(struct IORepository* repo);
    void (*ClearMappingResult)(struct IORepository* repo);
    
    // 매핑 검색 함수들
    ComponentMapping* (*FindMappingByAddress)(struct IORepository* repo, const char* address);
    ComponentMapping* (*FindMappingByComponent)(struct IORepository* repo, int componentId, int portId);
    std::vector<ComponentMapping> (*GetAllMappings)(struct IORepository* repo);
    
    // 통계 및 정보 함수들
    int (*GetInputCount)(struct IORepository* repo);
    int (*GetOutputCount)(struct IORepository* repo);
    size_t (*GetMappingCount)(struct IORepository* repo);
    
} IORepository;

// IORepository 생성/초기화 함수들
IORepository* CreateIORepository(IOMapping* mapping, MappingResult* result);
void DestroyIORepository(IORepository* repo);

// === 전역 구현 함수들 (C언어 스타일) ===

// ComponentRepository 구현 함수들
int ComponentRepo_AddComponent(ComponentRepository* repo, const PlacedComponent* component);
bool ComponentRepo_RemoveComponent(ComponentRepository* repo, int instanceId);
PlacedComponent* ComponentRepo_FindComponent(ComponentRepository* repo, int instanceId);
void ComponentRepo_ClearComponents(ComponentRepository* repo);
size_t ComponentRepo_GetComponentCount(ComponentRepository* repo);

// WiringRepository 구현 함수들  
int WiringRepo_AddWire(WiringRepository* repo, const Wire* wire);
bool WiringRepo_RemoveWire(WiringRepository* repo, int wireId);
Wire* WiringRepo_FindWire(WiringRepository* repo, int wireId);
void WiringRepo_ClearWires(WiringRepository* repo);
size_t WiringRepo_GetWireCount(WiringRepository* repo);
std::vector<Wire*> WiringRepo_FindWiresByComponent(WiringRepository* repo, int componentId);
bool WiringRepo_IsComponentConnected(WiringRepository* repo, int componentId, int portId);

// LadderRepository 구현 함수들
bool LadderRepo_LoadProgram(LadderRepository* repo, const plc::LadderProgram* newProgram);
void LadderRepo_ClearProgram(LadderRepository* repo);
plc::Rung* LadderRepo_GetRung(LadderRepository* repo, int rungIndex);
bool LadderRepo_SetDeviceState(LadderRepository* repo, const char* address, bool state);
bool LadderRepo_GetDeviceState(LadderRepository* repo, const char* address);
void LadderRepo_ClearDeviceStates(LadderRepository* repo);
bool LadderRepo_SetTimerState(LadderRepository* repo, const char* address, const plc::TimerState* state);
bool LadderRepo_SetCounterState(LadderRepository* repo, const char* address, const plc::CounterState* state);

// IORepository 구현 함수들 (Phase 2)
bool IORepo_UpdateMapping(IORepository* repo, const IOMapping* newMapping);
const IOMapping* IORepo_GetMapping(IORepository* repo);
void IORepo_ClearMapping(IORepository* repo);
bool IORepo_IsValidMapping(IORepository* repo);
bool IORepo_SetMappingResult(IORepository* repo, const MappingResult* result);
const MappingResult* IORepo_GetMappingResult(IORepository* repo);
void IORepo_ClearMappingResult(IORepository* repo);
ComponentMapping* IORepo_FindMappingByAddress(IORepository* repo, const char* address);
ComponentMapping* IORepo_FindMappingByComponent(IORepository* repo, int componentId, int portId);
std::vector<ComponentMapping> IORepo_GetAllMappings(IORepository* repo);
int IORepo_GetInputCount(IORepository* repo);
int IORepo_GetOutputCount(IORepository* repo);
size_t IORepo_GetMappingCount(IORepository* repo);

} // namespace plc