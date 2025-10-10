// include/IOMapper.h
// Phase 2: I/O 매핑 시스템 - C언어 스타일 구조체
// 실배선 정보를 분석하여 자동으로 I/O 매핑을 생성하는 시스템

#pragma once

#include "DataTypes.h"
#include "IOMapping.h"
#include <vector>
#include <string>
#include <queue>

namespace plc {

// === IOMapper: I/O 매핑 생성 엔진 ===
// C언어 스타일 구조체 + 함수 포인터 패턴

typedef struct IOMapper {
    // === 핵심 함수 포인터들 ===
    
    // I/O 매핑 추출 (메인 함수)
    MappingResult (*ExtractMapping)(struct IOMapper* mapper,
                                   const std::vector<Wire>* wires,
                                   const std::vector<PlacedComponent>* components);
    
    // PLC 컴포넌트 찾기
    PlacedComponent* (*FindPLCComponent)(struct IOMapper* mapper,
                                        const std::vector<PlacedComponent>* components);
    
    // 배선 추적 (BFS 기반)
    std::vector<ConnectionTrace> (*TraceAllConnections)(struct IOMapper* mapper,
                                                       int plcComponentId,
                                                       const std::vector<Wire>* wires,
                                                       const std::vector<PlacedComponent>* components);
    
    // 특정 포트에서 연결된 컴포넌트 추적
    std::vector<int> (*TraceWireConnection)(struct IOMapper* mapper,
                                          int fromComponentId, int fromPortId,
                                          const std::vector<Wire>* wires);
    
    // PLC 주소 생성 (X0, Y0, X1, Y1 등)
    std::string (*GeneratePLCAddress)(struct IOMapper* mapper,
                                    int portId, bool isInput, PortType type);
    
    // 연결 검증
    bool (*ValidateConnection)(struct IOMapper* mapper,
                              const Wire* wire,
                              const std::vector<PlacedComponent>* components);
    
    // 매핑 결과 검증
    bool (*ValidateMappingResult)(struct IOMapper* mapper,
                                 const MappingResult* result);
    
    // === 내부 데이터 ===
    MappingResult lastResult;           // 마지막 매핑 결과
    bool isInitialized;                // 초기화 여부
    int maxInputPorts;                 // 최대 입력 포트 수 (기본 16)
    int maxOutputPorts;                // 최대 출력 포트 수 (기본 16)
    
    // 추적용 임시 데이터
    std::vector<bool>* visitedComponents;    // 방문한 컴포넌트 (순환 감지)
    std::queue<int>* bfsQueue;              // BFS 탐색용 큐
    
} IOMapper;

// === IOMapper 생성/초기화 함수들 ===

// IOMapper 생성
IOMapper* CreateIOMapper();

// IOMapper 소멸
void DestroyIOMapper(IOMapper* mapper);

// IOMapper 초기화
bool InitializeIOMapper(IOMapper* mapper);

// IOMapper 정리
void ShutdownIOMapper(IOMapper* mapper);

// === IOMapper 구현 함수들 (C언어 스타일 전역 함수) ===

// === 메인 매핑 추출 함수 ===
MappingResult IOMapper_ExtractMapping(IOMapper* mapper,
                                     const std::vector<Wire>* wires,
                                     const std::vector<PlacedComponent>* components);

// === PLC 컴포넌트 관련 함수들 ===
PlacedComponent* IOMapper_FindPLCComponent(IOMapper* mapper,
                                          const std::vector<PlacedComponent>* components);

bool IOMapper_IsValidPLCComponent(const PlacedComponent* component);

// === 배선 추적 관련 함수들 ===
std::vector<ConnectionTrace> IOMapper_TraceAllConnections(IOMapper* mapper,
                                                         int plcComponentId,
                                                         const std::vector<Wire>* wires,
                                                         const std::vector<PlacedComponent>* components);

std::vector<int> IOMapper_TraceWireConnection(IOMapper* mapper,
                                            int fromComponentId, int fromPortId,
                                            const std::vector<Wire>* wires);

ConnectionTrace IOMapper_TraceSpecificConnection(IOMapper* mapper,
                                               int plcComponentId, int plcPortId,
                                               const std::vector<Wire>* wires,
                                               const std::vector<PlacedComponent>* components);

// === PLC 주소 생성 함수들 ===
std::string IOMapper_GeneratePLCAddress(IOMapper* mapper,
                                       int portId, bool isInput, PortType type);

bool IOMapper_IsValidPLCAddress(const std::string& address);

int IOMapper_ParsePLCPortNumber(const std::string& address);

// === 검증 함수들 ===
bool IOMapper_ValidateConnection(IOMapper* mapper,
                                const Wire* wire,
                                const std::vector<PlacedComponent>* components);

bool IOMapper_ValidateMappingResult(IOMapper* mapper,
                                   const MappingResult* result);

bool IOMapper_CheckCircularConnection(IOMapper* mapper,
                                     int startComponentId,
                                     const std::vector<Wire>* wires);

// === 유틸리티 함수들 ===

// 특정 컴포넌트에 연결된 모든 와이어 찾기
std::vector<Wire*> IOMapper_FindWiresByComponent(const std::vector<Wire>* wires,
                                               int componentId, int portId = -1);

// 특정 와이어의 반대편 컴포넌트 찾기
int IOMapper_GetOtherComponentId(const Wire* wire, int currentComponentId);

// 컴포넌트 타입별 포트 개수 계산
int IOMapper_GetMaxPortCount(ComponentType type);

// 컴포넌트 타입별 역할 문자열 생성
std::string IOMapper_GetComponentRole(ComponentType type);

// 포트 타입이 전기인지 확인
bool IOMapper_IsElectricPort(const PlacedComponent* component, int portId);

// PLC 포트가 입력 포트인지 확인 (0-15: 입력, 16-31: 출력)
bool IOMapper_IsPLCInputPort(const PlacedComponent* plcComponent, int portId);

// === 디버그 및 로깅 함수들 ===
void IOMapper_PrintMappingResult(const MappingResult* result);

void IOMapper_PrintConnectionTrace(const ConnectionTrace* trace);

std::string IOMapper_MappingResultToString(const MappingResult* result);

} // namespace plc