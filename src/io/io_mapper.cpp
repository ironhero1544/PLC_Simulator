// io_mapper.cpp
//
// Implementation of I/O mapper.

#include "plc_emulator/io/io_mapper.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>

namespace plc {


IOMapper* CreateIOMapper() {
  IOMapper* mapper = new IOMapper();
  if (!mapper) {
    std::cerr << "Error: Failed to allocate IOMapper" << std::endl;
    return nullptr;
  }

  // Wire up function pointers.
  mapper->ExtractMapping = IOMapper_ExtractMapping;
  mapper->FindPLCComponent = IOMapper_FindPLCComponent;
  mapper->TraceAllConnections = IOMapper_TraceAllConnections;
  mapper->TraceWireConnection = IOMapper_TraceWireConnection;
  mapper->GeneratePLCAddress = IOMapper_GeneratePLCAddress;
  mapper->ValidateConnection = IOMapper_ValidateConnection;
  mapper->ValidateMappingResult = IOMapper_ValidateMappingResult;

  // Initialize internal state.
  mapper->isInitialized = false;
  mapper->maxInputPorts = 16;   // FX3U defaults.
  mapper->maxOutputPorts = 16;  // FX3U defaults.

  // Allocate tracing state.
  mapper->visitedComponents = new std::vector<bool>();
  mapper->bfsQueue = new std::queue<int>();

  if (!InitializeIOMapper(mapper)) {
    DestroyIOMapper(mapper);
    return nullptr;
  }

  return mapper;
}

void DestroyIOMapper(IOMapper* mapper) {
  if (!mapper)
    return;

  ShutdownIOMapper(mapper);

  // Release tracing state.
  delete mapper->visitedComponents;
  delete mapper->bfsQueue;

  delete mapper;
}

bool InitializeIOMapper(IOMapper* mapper) {
  if (!mapper)
    return false;

  // Reset last result.
  mapper->lastResult = MappingResult();

  // Reset tracing state.
  mapper->visitedComponents->clear();
  while (!mapper->bfsQueue->empty()) {
    mapper->bfsQueue->pop();
  }

  mapper->isInitialized = true;
  std::cout << "IOMapper initialized successfully." << std::endl;
  return true;
}

void ShutdownIOMapper(IOMapper* mapper) {
  if (!mapper)
    return;

  // Clear tracing state.
  if (mapper->visitedComponents) {
    mapper->visitedComponents->clear();
  }
  if (mapper->bfsQueue) {
    while (!mapper->bfsQueue->empty()) {
      mapper->bfsQueue->pop();
    }
  }

  mapper->isInitialized = false;
  std::cout << "IOMapper shutdown completed." << std::endl;
}


MappingResult IOMapper_ExtractMapping(
    IOMapper* mapper, const std::vector<Wire>* wires,
    const std::vector<PlacedComponent>* components) {
  MappingResult result;

  if (!mapper || !wires || !components) {
    result.AddError("IOMapper_ExtractMapping: Invalid parameters");
    return result;
  }

  if (!mapper->isInitialized) {
    result.AddError("IOMapper_ExtractMapping: IOMapper not initialized");
    return result;
  }

  std::cout << "🔍 Starting I/O mapping extraction..." << std::endl;

  PlacedComponent* plcComponent = mapper->FindPLCComponent(mapper, components);
  if (!plcComponent) {
    result.AddError("No PLC component found in the circuit");
    return result;
  }

  result.mapping.plcComponentId = plcComponent->instanceId;
  std::cout << "✅ Found PLC component (ID: " << plcComponent->instanceId << ")"
            << std::endl;

  std::vector<ConnectionTrace> connections = mapper->TraceAllConnections(
      mapper, plcComponent->instanceId, wires, components);

  result.connections = connections;
  std::cout << "🔗 Found " << connections.size() << " connections from PLC"
            << std::endl;

  int inputCounter = 0;
  int outputCounter = 0;

  for (const auto& connection : connections) {
    // Map electrical connections only; pneumatic handled in Phase 3.
    if (!connection.isElectrical) {
      result.AddWarning(
          "Skipped pneumatic connection (will be handled in Phase 3)");
      continue;
    }

    // Determine PLC port direction.
    bool isInput = IOMapper_IsPLCInputPort(plcComponent, connection.fromPortId);
    std::string plcAddress;

    if (isInput && inputCounter < mapper->maxInputPorts) {
      plcAddress = mapper->GeneratePLCAddress(mapper, inputCounter, true,
                                              PortType::ELECTRIC);
      inputCounter++;
    } else if (!isInput && outputCounter < mapper->maxOutputPorts) {
      plcAddress = mapper->GeneratePLCAddress(mapper, outputCounter, false,
                                              PortType::ELECTRIC);
      outputCounter++;
    } else {
      result.AddError("Exceeded maximum port count (X: 16, Y: 16)");
      continue;
    }

    // Find the target component.
    auto targetComponent =
        std::find_if(components->begin(), components->end(),
                     [&](const PlacedComponent& comp) {
                       return comp.instanceId == connection.toComponentId;
                     });

    if (targetComponent != components->end()) {
      ComponentMapping mapping(
          connection.toComponentId, connection.toPortId, plcAddress,
          targetComponent->type,
          IOMapper_GetComponentRole(targetComponent->type));

      result.mapping.AddMapping(mapping);
      std::cout << "📍 Mapped "
                << IOMapper_GetComponentRole(targetComponent->type)
                << " (ID:" << connection.toComponentId << ") → " << plcAddress
                << std::endl;
    }
  }

  result.mapping.isValid = result.mapping.ValidateMapping();
  result.success = result.mapping.isValid && !result.HasErrors();

  // Set timestamp.
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();
  result.mapping.lastUpdated = timestamp;

  // Store result.
  mapper->lastResult = result;

  std::cout << "✅ I/O mapping extraction completed. Success: "
            << (result.success ? "Yes" : "No") << std::endl;
  std::cout << "📊 Generated mappings: " << result.mapping.GetMappingCount()
            << " (X: " << result.mapping.inputCount
            << ", Y: " << result.mapping.outputCount << ")" << std::endl;

  return result;
}


PlacedComponent* IOMapper_FindPLCComponent(
    IOMapper* mapper, const std::vector<PlacedComponent>* components) {
  if (!mapper || !components)
    return nullptr;

  for (auto& component : *components) {
    if (component.type == ComponentType::PLC) {
      return const_cast<PlacedComponent*>(&component);
    }
  }

  return nullptr;
}

bool IOMapper_IsValidPLCComponent(const PlacedComponent* component) {
  return component && component->type == ComponentType::PLC;
}


std::vector<ConnectionTrace> IOMapper_TraceAllConnections(
    IOMapper* mapper, int plcComponentId, const std::vector<Wire>* wires,
    const std::vector<PlacedComponent>* components) {
  std::vector<ConnectionTrace> connections;

  if (!mapper || !wires || !components)
    return connections;

  // Trace all PLC ports.
  int maxPorts = IOMapper_GetMaxPortCount(ComponentType::PLC);  // 32 ports.

  for (int portId = 0; portId < maxPorts; portId++) {
    ConnectionTrace trace = IOMapper_TraceSpecificConnection(
        mapper, plcComponentId, portId, wires, components);

    if (trace.toComponentId != -1) {
      connections.push_back(trace);
    }
  }

  return connections;
}

std::vector<int> IOMapper_TraceWireConnection(IOMapper* mapper,
                                              int fromComponentId,
                                              int fromPortId,
                                              const std::vector<Wire>* wires) {
  std::vector<int> connectedComponents;

  if (!mapper || !wires)
    return connectedComponents;

  (void)fromPortId;

  // Initialize BFS state.
  mapper->visitedComponents->clear();
  mapper->visitedComponents->resize(1000, false);  // Fixed traversal budget.

  while (!mapper->bfsQueue->empty()) {
    mapper->bfsQueue->pop();
  }

  // Start BFS.
  mapper->bfsQueue->push(fromComponentId);
  (*mapper->visitedComponents)[fromComponentId] = true;

  while (!mapper->bfsQueue->empty()) {
    int currentId = mapper->bfsQueue->front();
    mapper->bfsQueue->pop();

    // Visit wires from the current component.
    for (const auto& wire : *wires) {
      if (!wire.isElectric)
        continue;  // Track electrical wires only.

      int nextId = -1;

      if (wire.fromComponentId == currentId) {
        nextId = wire.toComponentId;
      } else if (wire.toComponentId == currentId) {
        nextId = wire.fromComponentId;
      }

      if (nextId != -1 && nextId < static_cast<int>(mapper->visitedComponents->size()) &&
          !(*mapper->visitedComponents)[nextId]) {
        (*mapper->visitedComponents)[nextId] = true;
        mapper->bfsQueue->push(nextId);
        connectedComponents.push_back(nextId);
      }
    }
  }

  return connectedComponents;
}

ConnectionTrace IOMapper_TraceSpecificConnection(
    IOMapper* mapper, int plcComponentId, int plcPortId,
    const std::vector<Wire>* wires,
    const std::vector<PlacedComponent>* components) {
  (void)mapper;
  (void)components;

  ConnectionTrace trace;
  trace.fromComponentId = plcComponentId;
  trace.fromPortId = plcPortId;

  // Find wires connected to the port.
  for (const auto& wire : *wires) {
    if (!wire.isElectric)
      continue;

    if (wire.fromComponentId == plcComponentId &&
        wire.fromPortId == plcPortId) {
      trace.toComponentId = wire.toComponentId;
      trace.toPortId = wire.toPortId;
      trace.wireIds.push_back(wire.id);
      trace.isElectrical = true;
      break;
    } else if (wire.toComponentId == plcComponentId &&
               wire.toPortId == plcPortId) {
      trace.toComponentId = wire.fromComponentId;
      trace.toPortId = wire.fromPortId;
      trace.wireIds.push_back(wire.id);
      trace.isElectrical = true;
      break;
    }
  }

  return trace;
}


std::string IOMapper_GeneratePLCAddress(IOMapper* mapper, int portId,
                                        bool isInput, PortType type) {
  if (!mapper)
    return "";

  (void)type;

  std::ostringstream oss;

  if (isInput) {
    oss << "X" << portId;
  } else {
    oss << "Y" << portId;
  }

  return oss.str();
}

bool IOMapper_IsValidPLCAddress(const std::string& address) {
  if (address.length() < 2)
    return false;

  char prefix = address[0];
  if (prefix != 'X' && prefix != 'Y')
    return false;

  std::string numberPart = address.substr(1);
  try {
    int portNum = std::stoi(numberPart);
    return portNum >= 0 && portNum < 16;
  } catch (...) {
    return false;
  }
}

int IOMapper_ParsePLCPortNumber(const std::string& address) {
  if (!IOMapper_IsValidPLCAddress(address))
    return -1;

  try {
    return std::stoi(address.substr(1));
  } catch (...) {
    return -1;
  }
}


bool IOMapper_ValidateConnection(
    IOMapper* mapper, const Wire* wire,
    const std::vector<PlacedComponent>* components) {
  if (!mapper || !wire || !components)
    return false;

  // Basic validation for component IDs.
  bool fromValid = false, toValid = false;

  for (const auto& comp : *components) {
    if (comp.instanceId == wire->fromComponentId)
      fromValid = true;
    if (comp.instanceId == wire->toComponentId)
      toValid = true;
  }

  return fromValid && toValid && wire->isElectric;
}

bool IOMapper_ValidateMappingResult(IOMapper* mapper,
                                    const MappingResult* result) {
  if (!mapper || !result)
    return false;

  return result->success && result->mapping.isValid && !result->HasErrors();
}

bool IOMapper_CheckCircularConnection(IOMapper* mapper, int startComponentId,
                                      const std::vector<Wire>* wires) {
  (void)mapper;
  (void)startComponentId;
  (void)wires;
  // Stub: circular detection is not implemented yet.
  return false;
}


std::vector<Wire*> IOMapper_FindWiresByComponent(const std::vector<Wire>* wires,
                                                 int componentId, int portId) {
  std::vector<Wire*> result;

  if (!wires)
    return result;

  for (auto& wire : *const_cast<std::vector<Wire>*>(wires)) {
    bool matches = false;

    if (portId == -1) {
      // All ports.
      matches = (wire.fromComponentId == componentId ||
                 wire.toComponentId == componentId);
    } else {
      // Specific port.
      matches =
          (wire.fromComponentId == componentId && wire.fromPortId == portId) ||
          (wire.toComponentId == componentId && wire.toPortId == portId);
    }

    if (matches) {
      result.push_back(&wire);
    }
  }

  return result;
}

int IOMapper_GetOtherComponentId(const Wire* wire, int currentComponentId) {
  if (!wire)
    return -1;

  if (wire->fromComponentId == currentComponentId) {
    return wire->toComponentId;
  } else if (wire->toComponentId == currentComponentId) {
    return wire->fromComponentId;
  }

  return -1;
}

int IOMapper_GetMaxPortCount(ComponentType type) {
  switch (type) {
    case ComponentType::PLC:
      return 32;
    case ComponentType::MANIFOLD:
      return 5;
    case ComponentType::VALVE_DOUBLE:
      return 7;
    case ComponentType::VALVE_SINGLE:
      return 5;
    case ComponentType::BUTTON_UNIT:
      return 15;
    case ComponentType::LIMIT_SWITCH:
      return 3;
    case ComponentType::SENSOR:
      return 3;
    case ComponentType::CYLINDER:
      return 2;
    case ComponentType::POWER_SUPPLY:
      return 2;
    case ComponentType::FRL:
      return 1;
    case ComponentType::WORKPIECE_METAL:
    case ComponentType::WORKPIECE_NONMETAL:
      return 0;
    case ComponentType::RING_SENSOR:
      return 3;
    case ComponentType::METER_VALVE:
      return 2;
    case ComponentType::INDUCTIVE_SENSOR:
      return 3;
    case ComponentType::CONVEYOR:
      return 2;
    case ComponentType::PROCESSING_CYLINDER:
      return 7;
    case ComponentType::BOX:
      return 0;
    case ComponentType::TOWER_LAMP:
      return 4;
    case ComponentType::EMERGENCY_STOP:
      return 4;
    case ComponentType::RTL_MODULE:
      return 0;
    default:
      return 1;
  }
}

std::string IOMapper_GetComponentRole(ComponentType type) {
  switch (type) {
    case ComponentType::PLC:
      return "PLC";
    case ComponentType::LIMIT_SWITCH:
      return "LIMIT_SWITCH";
    case ComponentType::SENSOR:
      return "SENSOR";
    case ComponentType::CYLINDER:
      return "ACTUATOR";
    case ComponentType::VALVE_SINGLE:
      return "VALVE";
    case ComponentType::VALVE_DOUBLE:
      return "VALVE";
    case ComponentType::BUTTON_UNIT:
      return "BUTTON_PANEL";
    case ComponentType::POWER_SUPPLY:
      return "POWER_SUPPLY";
    case ComponentType::MANIFOLD:
      return "MANIFOLD";
    case ComponentType::FRL:
      return "FRL";
    case ComponentType::WORKPIECE_METAL:
    case ComponentType::WORKPIECE_NONMETAL:
      return "WORKPIECE";
    case ComponentType::RING_SENSOR:
      return "SENSOR";
    case ComponentType::METER_VALVE:
      return "VALVE";
    case ComponentType::INDUCTIVE_SENSOR:
      return "SENSOR";
    case ComponentType::CONVEYOR:
      return "ACTUATOR";
    case ComponentType::PROCESSING_CYLINDER:
      return "ACTUATOR";
    case ComponentType::BOX:
      return "BOX";
    case ComponentType::TOWER_LAMP:
      return "LAMP";
    case ComponentType::EMERGENCY_STOP:
      return "EMERGENCY_STOP";
    case ComponentType::RTL_MODULE:
      return "RTL_MODULE";
    default:
      return "UNKNOWN";
  }
}

bool IOMapper_IsElectricPort(const PlacedComponent* component, int portId) {
  (void)component;
  (void)portId;
  // Assume all ports are electric (Phase 3 will refine this).
  return true;
}

bool IOMapper_IsPLCInputPort(const PlacedComponent* plcComponent, int portId) {
  (void)plcComponent;  // unused parameter
  // FX3U ports: 0-15 input (X), 16-31 output (Y).
  return portId >= 0 && portId < 16;
}


void IOMapper_PrintMappingResult(const MappingResult* result) {
  if (!result)
    return;

  std::cout << "=== I/O Mapping Result ===" << std::endl;
  std::cout << "Success: " << (result->success ? "Yes" : "No") << std::endl;
  std::cout << "Connections: " << result->connections.size() << std::endl;
  std::cout << "Mappings: " << result->mapping.GetMappingCount() << std::endl;

  for (const auto& mapping : result->mapping.mappings) {
    std::cout << "  " << mapping.plcAddress << " → Component "
              << mapping.componentId << " (" << mapping.componentRole << ")"
              << std::endl;
  }

  if (result->HasErrors()) {
    std::cout << "Errors:" << std::endl;
    for (const auto& error : result->errors) {
      std::cout << "  - " << error << std::endl;
    }
  }
}

void IOMapper_PrintConnectionTrace(const ConnectionTrace* trace) {
  if (!trace)
    return;

  std::cout << "Connection: Component " << trace->fromComponentId << " (Port "
            << trace->fromPortId << ") → Component " << trace->toComponentId
            << " (Port " << trace->toPortId << ")" << std::endl;
}

std::string IOMapper_MappingResultToString(const MappingResult* result) {
  if (!result)
    return "Invalid result";

  std::ostringstream oss;
  oss << "I/O Mapping: " << result->mapping.GetMappingCount() << " mappings, ";
  oss << "Success: " << (result->success ? "Yes" : "No");

  return oss.str();
}

}  // namespace plc
