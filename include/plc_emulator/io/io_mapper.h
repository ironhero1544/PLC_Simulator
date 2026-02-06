/*
 * io_mapper.h
 *
 * Maps physical wiring to PLC I/O addresses.
 * 실배선 정보를 PLC I/O 주소로 매핑합니다.
 *
 * C-style I/O mapping interface (Phase 2).
 * C 스타일 I/O 매핑 인터페이스 (Phase 2).
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPER_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/io/io_mapping.h"

#include <queue>
#include <string>
#include <vector>

namespace plc {

/*
 * C-style I/O mapping interface with function pointers.
 * 함수 포인터 기반 C 스타일 I/O 매핑 인터페이스.
 */
typedef struct IOMapper {
  MappingResult (*ExtractMapping)(
      struct IOMapper* mapper, const std::vector<Wire>* wires,
      const std::vector<PlacedComponent>* components);

  PlacedComponent* (*FindPLCComponent)(
      struct IOMapper* mapper, const std::vector<PlacedComponent>* components);

  std::vector<ConnectionTrace> (*TraceAllConnections)(
      struct IOMapper* mapper, int plcComponentId,
      const std::vector<Wire>* wires,
      const std::vector<PlacedComponent>* components);

  std::vector<int> (*TraceWireConnection)(struct IOMapper* mapper,
                                          int fromComponentId, int fromPortId,
                                          const std::vector<Wire>* wires);

  std::string (*GeneratePLCAddress)(struct IOMapper* mapper, int portId,
                                    bool isInput, PortType type);

  bool (*ValidateConnection)(struct IOMapper* mapper, const Wire* wire,
                             const std::vector<PlacedComponent>* components);

  bool (*ValidateMappingResult)(struct IOMapper* mapper,
                                const MappingResult* result);

  MappingResult lastResult;
  bool isInitialized;
  int maxInputPorts;
  int maxOutputPorts;

  std::vector<bool>* visitedComponents;
  std::queue<int>* bfsQueue;

} IOMapper;


/*
 * IOMapper lifecycle.
 * IOMapper 생성/해제.
 */
IOMapper* CreateIOMapper();
void DestroyIOMapper(IOMapper* mapper);

/*
 * IOMapper initialization.
 * IOMapper 초기화/정리.
 */
bool InitializeIOMapper(IOMapper* mapper);
void ShutdownIOMapper(IOMapper* mapper);


/*
 * Mapping extraction and tracing.
 * 매핑 추출 및 추적.
 */
MappingResult IOMapper_ExtractMapping(
    IOMapper* mapper, const std::vector<Wire>* wires,
    const std::vector<PlacedComponent>* components);

PlacedComponent* IOMapper_FindPLCComponent(
    IOMapper* mapper, const std::vector<PlacedComponent>* components);

bool IOMapper_IsValidPLCComponent(const PlacedComponent* component);

std::vector<ConnectionTrace> IOMapper_TraceAllConnections(
    IOMapper* mapper, int plcComponentId, const std::vector<Wire>* wires,
    const std::vector<PlacedComponent>* components);

std::vector<int> IOMapper_TraceWireConnection(IOMapper* mapper,
                                              int fromComponentId,
                                              int fromPortId,
                                              const std::vector<Wire>* wires);

ConnectionTrace IOMapper_TraceSpecificConnection(
    IOMapper* mapper, int plcComponentId, int plcPortId,
    const std::vector<Wire>* wires,
    const std::vector<PlacedComponent>* components);

/*
 * Address helpers.
 * 주소 헬퍼 함수.
 */
std::string IOMapper_GeneratePLCAddress(IOMapper* mapper, int portId,
                                        bool isInput, PortType type);

bool IOMapper_IsValidPLCAddress(const std::string& address);

int IOMapper_ParsePLCPortNumber(const std::string& address);

/*
 * Validation helpers.
 * 검증 헬퍼 함수.
 */
bool IOMapper_ValidateConnection(
    IOMapper* mapper, const Wire* wire,
    const std::vector<PlacedComponent>* components);

bool IOMapper_ValidateMappingResult(IOMapper* mapper,
                                    const MappingResult* result);

bool IOMapper_CheckCircularConnection(IOMapper* mapper, int startComponentId,
                                      const std::vector<Wire>* wires);


/*
 * Wiring query helpers.
 * 배선 조회 헬퍼 함수.
 */
std::vector<Wire*> IOMapper_FindWiresByComponent(const std::vector<Wire>* wires,
                                                 int componentId,
                                                 int portId = -1);

int IOMapper_GetOtherComponentId(const Wire* wire, int currentComponentId);

int IOMapper_GetMaxPortCount(ComponentType type);

std::string IOMapper_GetComponentRole(ComponentType type);

bool IOMapper_IsElectricPort(const PlacedComponent* component, int portId);

bool IOMapper_IsPLCInputPort(const PlacedComponent* plcComponent, int portId);

/*
 * Diagnostics helpers.
 * 진단 헬퍼 함수.
 */
void IOMapper_PrintMappingResult(const MappingResult* result);

void IOMapper_PrintConnectionTrace(const ConnectionTrace* trace);

std::string IOMapper_MappingResultToString(const MappingResult* result);

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPER_H_ */
