/*
 * data_repository.h
 *
 * Central repositories for simulation state and wiring.
 * 시뮬레이션 상태와 배선을 위한 중앙 리포지토리 모음.
 *
 * C-style repository interfaces backed by STL containers.
 * STL 컨테이너를 사용하는 C 스타일 리포지토리 인터페이스.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_DATA_REPOSITORY_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_DATA_REPOSITORY_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/io/io_mapping.h"
#include "plc_emulator/programming/programming_mode.h"

#include <map>
#include <string>
#include <vector>

namespace plc {

typedef struct ComponentRepository {
  std::vector<PlacedComponent>* components;
  int (*AddComponent)(struct ComponentRepository* repo,
                      const PlacedComponent* component);
  bool (*RemoveComponent)(struct ComponentRepository* repo, int instanceId);
  PlacedComponent* (*FindComponent)(struct ComponentRepository* repo,
                                    int instanceId);
  void (*ClearComponents)(struct ComponentRepository* repo);
  size_t (*GetComponentCount)(struct ComponentRepository* repo);
} ComponentRepository;

/*
 * Component repository lifecycle.
 * 컴포넌트 리포지토리 생성/해제.
 */
ComponentRepository* CreateComponentRepository(
    std::vector<PlacedComponent>* components);
void DestroyComponentRepository(ComponentRepository* repo);

typedef struct WiringRepository {
  std::vector<Wire>* wires;
  int (*AddWire)(struct WiringRepository* repo, const Wire* wire);
  bool (*RemoveWire)(struct WiringRepository* repo, int wireId);
  Wire* (*FindWire)(struct WiringRepository* repo, int wireId);
  void (*ClearWires)(struct WiringRepository* repo);
  size_t (*GetWireCount)(struct WiringRepository* repo);
  std::vector<Wire*> (*FindWiresByComponent)(struct WiringRepository* repo,
                                             int componentId);
  bool (*IsComponentConnected)(struct WiringRepository* repo, int componentId,
                               int portId);
} WiringRepository;

/*
 * Wiring repository lifecycle.
 * 배선 리포지토리 생성/해제.
 */
WiringRepository* CreateWiringRepository(std::vector<Wire>* wires);
void DestroyWiringRepository(WiringRepository* repo);

typedef struct LadderRepository {
  plc::LadderProgram* program;
  std::map<std::string, bool>* deviceStates;
  std::map<std::string, plc::TimerState>* timerStates;
  std::map<std::string, plc::CounterState>* counterStates;
  bool (*LoadProgram)(struct LadderRepository* repo,
                      const plc::LadderProgram* newProgram);
  void (*ClearProgram)(struct LadderRepository* repo);
  plc::Rung* (*GetRung)(struct LadderRepository* repo, int rungIndex);
  bool (*SetDeviceState)(struct LadderRepository* repo, const char* address,
                         bool state);
  bool (*GetDeviceState)(struct LadderRepository* repo, const char* address);
  void (*ClearDeviceStates)(struct LadderRepository* repo);
  bool (*SetTimerState)(struct LadderRepository* repo, const char* address,
                        const plc::TimerState* state);
  bool (*SetCounterState)(struct LadderRepository* repo, const char* address,
                          const plc::CounterState* state);
} LadderRepository;

/*
 * Ladder repository lifecycle.
 * 래더 리포지토리 생성/해제.
 */
LadderRepository* CreateLadderRepository(
    plc::LadderProgram* program, std::map<std::string, bool>* deviceStates,
    std::map<std::string, plc::TimerState>* timerStates,
    std::map<std::string, plc::CounterState>* counterStates);
void DestroyLadderRepository(LadderRepository* repo);

typedef struct IORepository {
  IOMapping* mapping;
  MappingResult* lastResult;

  bool (*UpdateMapping)(struct IORepository* repo, const IOMapping* newMapping);
  const IOMapping* (*GetMapping)(struct IORepository* repo);
  void (*ClearMapping)(struct IORepository* repo);
  bool (*IsValidMapping)(struct IORepository* repo);
  bool (*SetMappingResult)(struct IORepository* repo,
                           const MappingResult* result);
  const MappingResult* (*GetMappingResult)(struct IORepository* repo);
  void (*ClearMappingResult)(struct IORepository* repo);
  ComponentMapping* (*FindMappingByAddress)(struct IORepository* repo,
                                            const char* address);
  ComponentMapping* (*FindMappingByComponent)(struct IORepository* repo,
                                              int componentId, int portId);
  std::vector<ComponentMapping> (*GetAllMappings)(struct IORepository* repo);
  int (*GetInputCount)(struct IORepository* repo);
  int (*GetOutputCount)(struct IORepository* repo);
  size_t (*GetMappingCount)(struct IORepository* repo);

} IORepository;

/*
 * I/O repository lifecycle.
 * I/O 리포지토리 생성/해제.
 */
IORepository* CreateIORepository(IOMapping* mapping, MappingResult* result);
void DestroyIORepository(IORepository* repo);

/*
 * Component repository implementation functions.
 * 컴포넌트 리포지토리 구현 함수.
 */
int ComponentRepo_AddComponent(ComponentRepository* repo,
                               const PlacedComponent* component);
bool ComponentRepo_RemoveComponent(ComponentRepository* repo, int instanceId);
PlacedComponent* ComponentRepo_FindComponent(ComponentRepository* repo,
                                             int instanceId);
void ComponentRepo_ClearComponents(ComponentRepository* repo);
size_t ComponentRepo_GetComponentCount(ComponentRepository* repo);

/*
 * Wiring repository implementation functions.
 * 배선 리포지토리 구현 함수.
 */
int WiringRepo_AddWire(WiringRepository* repo, const Wire* wire);
bool WiringRepo_RemoveWire(WiringRepository* repo, int wireId);
Wire* WiringRepo_FindWire(WiringRepository* repo, int wireId);
void WiringRepo_ClearWires(WiringRepository* repo);
size_t WiringRepo_GetWireCount(WiringRepository* repo);
std::vector<Wire*> WiringRepo_FindWiresByComponent(WiringRepository* repo,
                                                   int componentId);
bool WiringRepo_IsComponentConnected(WiringRepository* repo, int componentId,
                                      int portId);

/*
 * Ladder repository implementation functions.
 * 래더 리포지토리 구현 함수.
 */
bool LadderRepo_LoadProgram(LadderRepository* repo,
                            const plc::LadderProgram* newProgram);
void LadderRepo_ClearProgram(LadderRepository* repo);
plc::Rung* LadderRepo_GetRung(LadderRepository* repo, int rungIndex);
bool LadderRepo_SetDeviceState(LadderRepository* repo, const char* address,
                               bool state);
bool LadderRepo_GetDeviceState(LadderRepository* repo, const char* address);
void LadderRepo_ClearDeviceStates(LadderRepository* repo);
bool LadderRepo_SetTimerState(LadderRepository* repo, const char* address,
                              const plc::TimerState* state);
bool LadderRepo_SetCounterState(LadderRepository* repo, const char* address,
                                const plc::CounterState* state);

/*
 * I/O repository implementation functions (Phase 2).
 * I/O 리포지토리 구현 함수 (Phase 2).
 */
bool IORepo_UpdateMapping(IORepository* repo, const IOMapping* newMapping);
const IOMapping* IORepo_GetMapping(IORepository* repo);
void IORepo_ClearMapping(IORepository* repo);
bool IORepo_IsValidMapping(IORepository* repo);
bool IORepo_SetMappingResult(IORepository* repo, const MappingResult* result);
const MappingResult* IORepo_GetMappingResult(IORepository* repo);
void IORepo_ClearMappingResult(IORepository* repo);
ComponentMapping* IORepo_FindMappingByAddress(IORepository* repo,
                                              const char* address);
ComponentMapping* IORepo_FindMappingByComponent(IORepository* repo,
                                                int componentId, int portId);
std::vector<ComponentMapping> IORepo_GetAllMappings(IORepository* repo);
int IORepo_GetInputCount(IORepository* repo);
int IORepo_GetOutputCount(IORepository* repo);
size_t IORepo_GetMappingCount(IORepository* repo);

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_DATA_REPOSITORY_H_ */
