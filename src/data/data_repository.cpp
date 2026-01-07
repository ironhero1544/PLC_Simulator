// data_repository.cpp
//
// Implementation of data repository.

// src/DataRepository.cpp
// DataRepository 구현 - C언어 스타일 데이터 저장소

#include "plc_emulator/data/data_repository.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace plc {


ComponentRepository* CreateComponentRepository(
    std::vector<PlacedComponent>* components) {
  if (!components) {
    std::cerr << "Error: components vector is null" << std::endl;
    return nullptr;
  }

  ComponentRepository* repo = new ComponentRepository();
  repo->components = components;

  // 함수 포인터들 연결
  repo->AddComponent = ComponentRepo_AddComponent;
  repo->RemoveComponent = ComponentRepo_RemoveComponent;
  repo->FindComponent = ComponentRepo_FindComponent;
  repo->ClearComponents = ComponentRepo_ClearComponents;
  repo->GetComponentCount = ComponentRepo_GetComponentCount;

  return repo;
}

void DestroyComponentRepository(ComponentRepository* repo) {
  if (repo) {
    delete repo;
  }
}

int ComponentRepo_AddComponent(ComponentRepository* repo,
                               const PlacedComponent* component) {
  if (!repo || !repo->components || !component) {
    return -1;
  }

  PlacedComponent newComp = *component;
  repo->components->push_back(newComp);
  return newComp.instanceId;
}

bool ComponentRepo_RemoveComponent(ComponentRepository* repo, int instanceId) {
  if (!repo || !repo->components) {
    return false;
  }

  auto it = std::find_if(repo->components->begin(), repo->components->end(),
                         [instanceId](const PlacedComponent& comp) {
                           return comp.instanceId == instanceId;
                         });

  if (it != repo->components->end()) {
    repo->components->erase(it);
    return true;
  }

  return false;
}

PlacedComponent* ComponentRepo_FindComponent(ComponentRepository* repo,
                                             int instanceId) {
  if (!repo || !repo->components) {
    return nullptr;
  }

  auto it = std::find_if(repo->components->begin(), repo->components->end(),
                         [instanceId](const PlacedComponent& comp) {
                           return comp.instanceId == instanceId;
                         });

  return (it != repo->components->end()) ? &(*it) : nullptr;
}

void ComponentRepo_ClearComponents(ComponentRepository* repo) {
  if (repo && repo->components) {
    repo->components->clear();
  }
}

size_t ComponentRepo_GetComponentCount(ComponentRepository* repo) {
  if (!repo || !repo->components) {
    return 0;
  }
  return repo->components->size();
}


WiringRepository* CreateWiringRepository(std::vector<Wire>* wires) {
  if (!wires) {
    std::cerr << "Error: wires vector is null" << std::endl;
    return nullptr;
  }

  WiringRepository* repo = new WiringRepository();
  repo->wires = wires;

  // 함수 포인터들 연결
  repo->AddWire = WiringRepo_AddWire;
  repo->RemoveWire = WiringRepo_RemoveWire;
  repo->FindWire = WiringRepo_FindWire;
  repo->ClearWires = WiringRepo_ClearWires;
  repo->GetWireCount = WiringRepo_GetWireCount;
  repo->FindWiresByComponent = WiringRepo_FindWiresByComponent;
  repo->IsComponentConnected = WiringRepo_IsComponentConnected;

  return repo;
}

void DestroyWiringRepository(WiringRepository* repo) {
  if (repo) {
    delete repo;
  }
}

int WiringRepo_AddWire(WiringRepository* repo, const Wire* wire) {
  if (!repo || !repo->wires || !wire) {
    return -1;
  }

  Wire newWire = *wire;
  repo->wires->push_back(newWire);
  return newWire.id;
}

bool WiringRepo_RemoveWire(WiringRepository* repo, int wireId) {
  if (!repo || !repo->wires) {
    return false;
  }

  auto it =
      std::find_if(repo->wires->begin(), repo->wires->end(),
                   [wireId](const Wire& wire) { return wire.id == wireId; });

  if (it != repo->wires->end()) {
    repo->wires->erase(it);
    return true;
  }

  return false;
}

Wire* WiringRepo_FindWire(WiringRepository* repo, int wireId) {
  if (!repo || !repo->wires) {
    return nullptr;
  }

  auto it =
      std::find_if(repo->wires->begin(), repo->wires->end(),
                   [wireId](const Wire& wire) { return wire.id == wireId; });

  return (it != repo->wires->end()) ? &(*it) : nullptr;
}

void WiringRepo_ClearWires(WiringRepository* repo) {
  if (repo && repo->wires) {
    repo->wires->clear();
  }
}

size_t WiringRepo_GetWireCount(WiringRepository* repo) {
  if (!repo || !repo->wires) {
    return 0;
  }
  return repo->wires->size();
}

std::vector<Wire*> WiringRepo_FindWiresByComponent(WiringRepository* repo,
                                                   int componentId) {
  std::vector<Wire*> result;

  if (!repo || !repo->wires) {
    return result;
  }

  for (auto& wire : *repo->wires) {
    if (wire.fromComponentId == componentId ||
        wire.toComponentId == componentId) {
      result.push_back(&wire);
    }
  }

  return result;
}

bool WiringRepo_IsComponentConnected(WiringRepository* repo, int componentId,
                                     int portId) {
  if (!repo || !repo->wires) {
    return false;
  }

  for (const auto& wire : *repo->wires) {
    if ((wire.fromComponentId == componentId && wire.fromPortId == portId) ||
        (wire.toComponentId == componentId && wire.toPortId == portId)) {
      return true;
    }
  }

  return false;
}


LadderRepository* CreateLadderRepository(
    plc::LadderProgram* program, std::map<std::string, bool>* deviceStates,
    std::map<std::string, plc::TimerState>* timerStates,
    std::map<std::string, plc::CounterState>* counterStates) {
  if (!program || !deviceStates || !timerStates || !counterStates) {
    std::cerr << "Error: LadderRepository parameters cannot be null"
              << std::endl;
    return nullptr;
  }

  LadderRepository* repo = new LadderRepository();
  repo->program = program;
  repo->deviceStates = deviceStates;
  repo->timerStates = timerStates;
  repo->counterStates = counterStates;

  // 함수 포인터들 연결
  repo->LoadProgram = LadderRepo_LoadProgram;
  repo->ClearProgram = LadderRepo_ClearProgram;
  repo->GetRung = LadderRepo_GetRung;
  repo->SetDeviceState = LadderRepo_SetDeviceState;
  repo->GetDeviceState = LadderRepo_GetDeviceState;
  repo->ClearDeviceStates = LadderRepo_ClearDeviceStates;
  repo->SetTimerState = LadderRepo_SetTimerState;
  repo->SetCounterState = LadderRepo_SetCounterState;

  return repo;
}

void DestroyLadderRepository(LadderRepository* repo) {
  if (repo) {
    delete repo;
  }
}

bool LadderRepo_LoadProgram(LadderRepository* repo,
                            const plc::LadderProgram* newProgram) {
  if (!repo || !repo->program || !newProgram) {
    return false;
  }

  *repo->program = *newProgram;
  return true;
}

void LadderRepo_ClearProgram(LadderRepository* repo) {
  if (repo && repo->program) {
    *repo->program = plc::LadderProgram();  // 기본 생성자로 초기화
  }
}

plc::Rung* LadderRepo_GetRung(LadderRepository* repo, int rungIndex) {
  if (!repo || !repo->program) {
    return nullptr;
  }

  if (rungIndex >= 0 &&
      rungIndex < static_cast<int>(repo->program->rungs.size())) {
    return &repo->program->rungs[rungIndex];
  }

  return nullptr;
}

bool LadderRepo_SetDeviceState(LadderRepository* repo, const char* address,
                               bool state) {
  if (!repo || !repo->deviceStates || !address) {
    return false;
  }

  (*repo->deviceStates)[std::string(address)] = state;
  return true;
}

bool LadderRepo_GetDeviceState(LadderRepository* repo, const char* address) {
  if (!repo || !repo->deviceStates || !address) {
    return false;
  }

  std::string addr(address);
  auto it = repo->deviceStates->find(addr);
  return (it != repo->deviceStates->end()) ? it->second : false;
}

void LadderRepo_ClearDeviceStates(LadderRepository* repo) {
  if (repo && repo->deviceStates) {
    repo->deviceStates->clear();
  }
}

bool LadderRepo_SetTimerState(LadderRepository* repo, const char* address,
                              const plc::TimerState* state) {
  if (!repo || !repo->timerStates || !address || !state) {
    return false;
  }

  (*repo->timerStates)[std::string(address)] = *state;
  return true;
}

bool LadderRepo_SetCounterState(LadderRepository* repo, const char* address,
                                const plc::CounterState* state) {
  if (!repo || !repo->counterStates || !address || !state) {
    return false;
  }

  (*repo->counterStates)[std::string(address)] = *state;
  return true;
}


IORepository* CreateIORepository(IOMapping* mapping, MappingResult* result) {
  if (!mapping || !result) {
    std::cerr << "Error: IORepository parameters cannot be null" << std::endl;
    return nullptr;
  }

  IORepository* repo = new IORepository();
  repo->mapping = mapping;
  repo->lastResult = result;

  // 함수 포인터들 연결
  repo->UpdateMapping = IORepo_UpdateMapping;
  repo->GetMapping = IORepo_GetMapping;
  repo->ClearMapping = IORepo_ClearMapping;
  repo->IsValidMapping = IORepo_IsValidMapping;
  repo->SetMappingResult = IORepo_SetMappingResult;
  repo->GetMappingResult = IORepo_GetMappingResult;
  repo->ClearMappingResult = IORepo_ClearMappingResult;
  repo->FindMappingByAddress = IORepo_FindMappingByAddress;
  repo->FindMappingByComponent = IORepo_FindMappingByComponent;
  repo->GetAllMappings = IORepo_GetAllMappings;
  repo->GetInputCount = IORepo_GetInputCount;
  repo->GetOutputCount = IORepo_GetOutputCount;
  repo->GetMappingCount = IORepo_GetMappingCount;

  return repo;
}

void DestroyIORepository(IORepository* repo) {
  if (repo) {
    delete repo;
  }
}

bool IORepo_UpdateMapping(IORepository* repo, const IOMapping* newMapping) {
  if (!repo || !repo->mapping || !newMapping) {
    return false;
  }

  *repo->mapping = *newMapping;
  return true;
}

const IOMapping* IORepo_GetMapping(IORepository* repo) {
  if (!repo || !repo->mapping) {
    return nullptr;
  }

  return repo->mapping;
}

void IORepo_ClearMapping(IORepository* repo) {
  if (repo && repo->mapping) {
    repo->mapping->Clear();
  }
}

bool IORepo_IsValidMapping(IORepository* repo) {
  if (!repo || !repo->mapping) {
    return false;
  }

  return repo->mapping->ValidateMapping();
}

bool IORepo_SetMappingResult(IORepository* repo, const MappingResult* result) {
  if (!repo || !repo->lastResult || !result) {
    return false;
  }

  *repo->lastResult = *result;
  return true;
}

const MappingResult* IORepo_GetMappingResult(IORepository* repo) {
  if (!repo || !repo->lastResult) {
    return nullptr;
  }

  return repo->lastResult;
}

void IORepo_ClearMappingResult(IORepository* repo) {
  if (repo && repo->lastResult) {
    *repo->lastResult = MappingResult();
  }
}

ComponentMapping* IORepo_FindMappingByAddress(IORepository* repo,
                                              const char* address) {
  if (!repo || !repo->mapping || !address) {
    return nullptr;
  }

  return repo->mapping->FindMappingByAddress(std::string(address));
}

ComponentMapping* IORepo_FindMappingByComponent(IORepository* repo,
                                                int componentId, int portId) {
  if (!repo || !repo->mapping) {
    return nullptr;
  }

  return repo->mapping->FindMappingByComponent(componentId, portId);
}

std::vector<ComponentMapping> IORepo_GetAllMappings(IORepository* repo) {
  std::vector<ComponentMapping> result;

  if (!repo || !repo->mapping) {
    return result;
  }

  return repo->mapping->mappings;
}

int IORepo_GetInputCount(IORepository* repo) {
  if (!repo || !repo->mapping) {
    return 0;
  }

  return repo->mapping->inputCount;
}

int IORepo_GetOutputCount(IORepository* repo) {
  if (!repo || !repo->mapping) {
    return 0;
  }

  return repo->mapping->outputCount;
}

size_t IORepo_GetMappingCount(IORepository* repo) {
  if (!repo || !repo->mapping) {
    return 0;
  }

  return repo->mapping->GetMappingCount();
}

}  // namespace plc