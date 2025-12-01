// io_mapping.h
// Copyright 2024 PLC Emulator Project
//
// I/O mapping data structures.

// include/IOMapping.h
// Phase 2: I/O 매핑 시스템 데이터 구조
// 실배선 모드와 프로그래밍 모드를 연결하는 I/O 매핑 정보

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPING_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPING_H_

#include "plc_emulator/core/data_types.h"

#include <string>
#include <vector>

namespace plc {

// FX3U PLC의 물리적 포트 정보를 나타냄

struct PLCPortInfo {
  int portId;           // 물리적 포트 번호 (0-31)
  std::string address;  // PLC 주소 ("X0", "Y0", "X1", "Y1" 등)
  PortType type;        // ELECTRIC 또는 PNEUMATIC
  bool isInput;         // true: 입력 포트(X), false: 출력 포트(Y)

  PLCPortInfo()
      : portId(0), address(""), type(PortType::ELECTRIC), isInput(true) {}

  PLCPortInfo(int id, const std::string& addr, PortType portType, bool input)
      : portId(id), address(addr), type(portType), isInput(input) {}
};

// 실배선의 물리 컴포넌트와 PLC 주소 간의 매핑 관계

struct ComponentMapping {
  int componentId;              // PlacedComponent의 instanceId
  int componentPortId;          // 컴포넌트의 포트 번호
  std::string plcAddress;       // 매핑된 PLC 주소 ("X0", "Y5" 등)
  ComponentType componentType;  // 컴포넌트 타입 (SENSOR, CYLINDER 등)
  std::string componentRole;    // 컴포넌트 역할 ("LIMIT_SWITCH", "ACTUATOR" 등)

  ComponentMapping()
      : componentId(-1),
        componentPortId(-1),
        plcAddress(""),
        componentType(ComponentType::PLC),
        componentRole("") {}

  ComponentMapping(int compId, int portId, const std::string& addr,
                   ComponentType type, const std::string& role)
      : componentId(compId),
        componentPortId(portId),
        plcAddress(addr),
        componentType(type),
        componentRole(role) {}
};

// 실배선 모드에서 자동 생성되는 전체 I/O 매핑 정보

struct IOMapping {
  // 매핑 데이터
  std::vector<ComponentMapping> mappings;  // 컴포넌트 매핑 리스트
  std::vector<PLCPortInfo> plcPorts;       // PLC 포트 정보 리스트

  // 메타데이터
  int plcComponentId;  // PLC 컴포넌트의 instanceId
  bool isValid;        // 매핑 유효성 플래그
  int inputCount;      // 입력 매핑 개수 (X 포트)
  int outputCount;     // 출력 매핑 개수 (Y 포트)

  // 생성 정보
  long long lastUpdated;  // 마지막 업데이트 시각 (타임스탬프)

  IOMapping()
      : plcComponentId(-1),
        isValid(false),
        inputCount(0),
        outputCount(0),
        lastUpdated(0) {
    mappings.clear();
    plcPorts.clear();
  }


  // 특정 PLC 주소의 매핑 찾기
  ComponentMapping* FindMappingByAddress(const std::string& address) {
    for (auto& mapping : mappings) {
      if (mapping.plcAddress == address) {
        return &mapping;
      }
    }
    return nullptr;
  }

  // 특정 컴포넌트의 매핑 찾기
  ComponentMapping* FindMappingByComponent(int componentId, int portId = -1) {
    for (auto& mapping : mappings) {
      if (mapping.componentId == componentId) {
        if (portId == -1 || mapping.componentPortId == portId) {
          return &mapping;
        }
      }
    }
    return nullptr;
  }

  // 매핑 추가
  void AddMapping(const ComponentMapping& mapping) {
    mappings.push_back(mapping);

    // 입력/출력 카운트 업데이트
    if (mapping.plcAddress.length() > 0) {
      if (mapping.plcAddress[0] == 'X')
        inputCount++;
      else if (mapping.plcAddress[0] == 'Y')
        outputCount++;
    }
  }

  // 매핑 클리어
  void Clear() {
    mappings.clear();
    plcPorts.clear();
    plcComponentId = -1;
    isValid = false;
    inputCount = 0;
    outputCount = 0;
    lastUpdated = 0;
  }

  // 매핑 개수
  size_t GetMappingCount() const { return mappings.size(); }

  // 매핑 유효성 검증
  bool ValidateMapping() const {
    if (plcComponentId == -1)
      return false;
    if (mappings.empty())
      return false;

    // 중복 주소 검사
    for (size_t i = 0; i < mappings.size(); i++) {
      for (size_t j = i + 1; j < mappings.size(); j++) {
        if (mappings[i].plcAddress == mappings[j].plcAddress &&
            !mappings[i].plcAddress.empty()) {
          return false;  // 중복 주소 발견
        }
      }
    }

    return true;
  }
};

// 배선 추적 중 발견되는 연결 정보

struct ConnectionTrace {
  int fromComponentId;       // 시작 컴포넌트 ID
  int fromPortId;            // 시작 포트 ID
  int toComponentId;         // 도착 컴포넌트 ID
  int toPortId;              // 도착 포트 ID
  std::vector<int> wireIds;  // 연결에 사용된 와이어 ID들
  bool isElectrical;         // 전기 연결 여부

  ConnectionTrace()
      : fromComponentId(-1),
        fromPortId(-1),
        toComponentId(-1),
        toPortId(-1),
        isElectrical(true) {
    wireIds.clear();
  }
};

// IOMapper의 분석 결과를 담는 구조체

struct MappingResult {
  IOMapping mapping;                         // 생성된 I/O 매핑
  std::vector<ConnectionTrace> connections;  // 발견된 연결 정보들
  std::vector<std::string> errors;           // 발생한 오류 메시지들
  std::vector<std::string> warnings;         // 경고 메시지들
  bool success;                              // 매핑 생성 성공 여부

  MappingResult() : success(false) {
    connections.clear();
    errors.clear();
    warnings.clear();
  }

  void AddError(const std::string& error) {
    errors.push_back(error);
    success = false;
  }

  void AddWarning(const std::string& warning) { warnings.push_back(warning); }

  bool HasErrors() const { return !errors.empty(); }

  bool HasWarnings() const { return !warnings.empty(); }
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPING_H_
