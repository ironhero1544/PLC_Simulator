/*
 * io_mapping.h
 *
 * I/O mapping data structures.
 * I/O 매핑 데이터 구조.
 *
 * Mapping data for physical wiring and PLC addresses (Phase 2).
 * 실배선과 PLC 주소를 연결하는 매핑 데이터 (Phase 2).
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPING_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPING_H_

#include "plc_emulator/core/data_types.h"

#include <string>
#include <vector>

namespace plc {

/*
 * FX3U PLC physical port information.
 * FX3U PLC 물리 포트 정보.
 */
struct PLCPortInfo {
  int portId;
  std::string address;
  PortType type;
  bool isInput;

  PLCPortInfo()
      : portId(0), address(""), type(PortType::ELECTRIC), isInput(true) {}

  PLCPortInfo(int id, const std::string& addr, PortType portType, bool input)
      : portId(id), address(addr), type(portType), isInput(input) {}
};

/*
 * Mapping between a physical component port and a PLC address.
 * 물리 컴포넌트 포트와 PLC 주소 간의 매핑.
 */
struct ComponentMapping {
  int componentId;
  int componentPortId;
  std::string plcAddress;
  ComponentType componentType;
  std::string componentRole;

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

/*
 * Aggregate I/O mapping data with helper methods.
 * 전체 I/O 매핑 데이터 및 헬퍼 함수.
 */
struct IOMapping {
  std::vector<ComponentMapping> mappings;
  std::vector<PLCPortInfo> plcPorts;
  int plcComponentId;
  bool isValid;
  int inputCount;
  int outputCount;
  long long lastUpdated;

  IOMapping()
      : plcComponentId(-1),
        isValid(false),
        inputCount(0),
        outputCount(0),
        lastUpdated(0) {
    mappings.clear();
    plcPorts.clear();
  }


  ComponentMapping* FindMappingByAddress(const std::string& address) {
    for (auto& mapping : mappings) {
      if (mapping.plcAddress == address) {
        return &mapping;
      }
    }
    return nullptr;
  }

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

  void AddMapping(const ComponentMapping& mapping) {
    mappings.push_back(mapping);

    if (mapping.plcAddress.length() > 0) {
      if (mapping.plcAddress[0] == 'X')
        inputCount++;
      else if (mapping.plcAddress[0] == 'Y')
        outputCount++;
    }
  }

  void Clear() {
    mappings.clear();
    plcPorts.clear();
    plcComponentId = -1;
    isValid = false;
    inputCount = 0;
    outputCount = 0;
    lastUpdated = 0;
  }

  size_t GetMappingCount() const { return mappings.size(); }

  bool ValidateMapping() const {
    if (plcComponentId == -1)
      return false;
    if (mappings.empty())
      return false;

    for (size_t i = 0; i < mappings.size(); i++) {
      for (size_t j = i + 1; j < mappings.size(); j++) {
        if (mappings[i].plcAddress == mappings[j].plcAddress &&
            !mappings[i].plcAddress.empty()) {
          return false;
        }
      }
    }

    return true;
  }
};

/*
 * Connection trace discovered during wiring analysis.
 * 배선 분석 중 발견되는 연결 정보.
 */
struct ConnectionTrace {
  int fromComponentId;
  int fromPortId;
  int toComponentId;
  int toPortId;
  std::vector<int> wireIds;
  bool isElectrical;

  ConnectionTrace()
      : fromComponentId(-1),
        fromPortId(-1),
        toComponentId(-1),
        toPortId(-1),
        isElectrical(true) {
    wireIds.clear();
  }
};

/*
 * Result of IOMapper analysis.
 * IOMapper 분석 결과.
 */
struct MappingResult {
  IOMapping mapping;
  std::vector<ConnectionTrace> connections;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  bool success;

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

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_IO_IO_MAPPING_H_ */
