// event_system.cpp
//
// Implementation of event system.

#include "plc_emulator/data/event_system.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace plc {


EventDispatcher* CreateEventDispatcher() {
  EventDispatcher* dispatcher = new EventDispatcher();

  // Initialize listener slots.
  for (int i = 0; i < EVENT_TYPE_COUNT; i++) {
    dispatcher->listeners[i] = nullptr;
  }

  // Wire up function pointers.
  dispatcher->Subscribe = EventDispatcher_Subscribe;
  dispatcher->Unsubscribe = EventDispatcher_Unsubscribe;
  dispatcher->Dispatch = EventDispatcher_Dispatch;
  dispatcher->Clear = EventDispatcher_Clear;

  return dispatcher;
}

void DestroyEventDispatcher(EventDispatcher* dispatcher) {
  if (dispatcher) {
    // Release all listeners.
    dispatcher->Clear(dispatcher);
    delete dispatcher;
  }
}


bool EventDispatcher_Subscribe(EventDispatcher* dispatcher, EventType type,
                               EventCallback callback, void* userData) {
  if (!dispatcher || !callback || type < 0 || type >= EVENT_TYPE_COUNT) {
    std::cerr << "Error: Invalid parameters for Subscribe" << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(dispatcher->listenersMutex);

  // Create a new listener node.
  EventListener* newListener = new EventListener();
  newListener->callback = callback;
  newListener->userData = userData;
  newListener->next = nullptr;

  // Insert at the list head.
  if (dispatcher->listeners[type] == nullptr) {
    dispatcher->listeners[type] = newListener;
  } else {
    newListener->next = dispatcher->listeners[type];
    dispatcher->listeners[type] = newListener;
  }

  return true;
}

bool EventDispatcher_Unsubscribe(EventDispatcher* dispatcher, EventType type,
                                 EventCallback callback) {
  if (!dispatcher || !callback || type < 0 || type >= EVENT_TYPE_COUNT) {
    return false;
  }

  std::lock_guard<std::mutex> lock(dispatcher->listenersMutex);

  EventListener* current = dispatcher->listeners[type];
  EventListener* prev = nullptr;

  // Remove the matching callback from the list.
  while (current) {
    if (current->callback == callback) {
      if (prev) {
        prev->next = current->next;
      } else {
        dispatcher->listeners[type] = current->next;
      }
      delete current;
      return true;
    }
    prev = current;
    current = current->next;
  }

  return false;  // Callback not found.
}

void EventDispatcher_Dispatch(EventDispatcher* dispatcher,
                              const EventData* eventData) {
  if (!dispatcher || !eventData) {
    return;
  }

  EventType type = eventData->type;
  if (type < 0 || type >= EVENT_TYPE_COUNT) {
    std::cerr << "Error: Invalid event type in Dispatch: " << type << std::endl;
    return;
  }

  std::vector<std::pair<EventCallback, void*>> listeners_snapshot;
  {
    std::lock_guard<std::mutex> lock(dispatcher->listenersMutex);
    EventListener* current = dispatcher->listeners[type];
    while (current) {
      listeners_snapshot.emplace_back(current->callback, current->userData);
      current = current->next;
    }
  }

  // Invoke callbacks outside lock to avoid deadlock/re-entrancy hazards.
  for (const auto& listener : listeners_snapshot) {
    if (listener.first) {
      listener.first(eventData, listener.second);
    }
  }
}

void EventDispatcher_Clear(EventDispatcher* dispatcher) {
  if (!dispatcher) {
    return;
  }

  std::lock_guard<std::mutex> lock(dispatcher->listenersMutex);

  // Clear listeners for all event types.
  for (int i = 0; i < EVENT_TYPE_COUNT; i++) {
    EventListener* current = dispatcher->listeners[i];
    while (current) {
      EventListener* next = current->next;
      delete current;
      current = next;
    }
    dispatcher->listeners[i] = nullptr;
  }
}


EventData CreateDataChangedEvent() {
  EventData event;
  event.type = EVENT_DATA_CHANGED;
  event.data.intValue = 0;
  return event;
}

EventData CreateModeChangedEvent(int oldMode, int newMode) {
  EventData event;
  event.type = EVENT_MODE_CHANGED;
  event.data.modeEvent.oldMode = oldMode;
  event.data.modeEvent.newMode = newMode;
  return event;
}

EventData CreateComponentAddedEvent(int componentId, int componentType) {
  EventData event;
  event.type = EVENT_COMPONENT_ADDED;
  event.data.componentEvent.componentId = componentId;
  event.data.componentEvent.componentType = componentType;
  return event;
}

EventData CreateComponentRemovedEvent(int componentId, int componentType) {
  EventData event;
  event.type = EVENT_COMPONENT_REMOVED;
  event.data.componentEvent.componentId = componentId;
  event.data.componentEvent.componentType = componentType;
  return event;
}

EventData CreateWireAddedEvent(int wireId, int fromComponent, int toComponent) {
  EventData event;
  event.type = EVENT_WIRE_ADDED;
  event.data.wireEvent.wireId = wireId;
  event.data.wireEvent.fromComponent = fromComponent;
  event.data.wireEvent.toComponent = toComponent;
  return event;
}

EventData CreateWireRemovedEvent(int wireId, int fromComponent,
                                 int toComponent) {
  EventData event;
  event.type = EVENT_WIRE_REMOVED;
  event.data.wireEvent.wireId = wireId;
  event.data.wireEvent.fromComponent = fromComponent;
  event.data.wireEvent.toComponent = toComponent;
  return event;
}

EventData CreatePlcStateChangedEvent(bool isRunning) {
  EventData event;
  event.type = EVENT_PLC_STATE_CHANGED;
  event.data.plcStateEvent.isRunning = isRunning;
  return event;
}

EventData CreateIOMappingUpdatedEvent(int mappingCount, int inputCount,
                                      int outputCount, bool success,
                                      void* mappingPtr) {
  EventData event;
  event.type = EVENT_IO_MAPPING_UPDATED;
  event.data.ioMappingEvent.mappingCount = mappingCount;
  event.data.ioMappingEvent.inputCount = inputCount;
  event.data.ioMappingEvent.outputCount = outputCount;
  event.data.ioMappingEvent.success = success;
  event.data.ioMappingEvent.mappingPtr = mappingPtr;
  return event;
}


const char* EventTypeToString(EventType type) {
  switch (type) {
    case EVENT_DATA_CHANGED:
      return "DATA_CHANGED";
    case EVENT_MODE_CHANGED:
      return "MODE_CHANGED";
    case EVENT_COMPONENT_ADDED:
      return "COMPONENT_ADDED";
    case EVENT_COMPONENT_REMOVED:
      return "COMPONENT_REMOVED";
    case EVENT_WIRE_ADDED:
      return "WIRE_ADDED";
    case EVENT_WIRE_REMOVED:
      return "WIRE_REMOVED";
    case EVENT_PLC_STATE_CHANGED:
      return "PLC_STATE_CHANGED";
    case EVENT_IO_MAPPING_UPDATED:
      return "IO_MAPPING_UPDATED";
    default:
      return "UNKNOWN";
  }
}

void PrintEventData(const EventData* eventData) {
  if (!eventData) {
    std::cout << "EventData: null" << std::endl;
    return;
  }

  std::cout << "Event: " << EventTypeToString(eventData->type);

  switch (eventData->type) {
    case EVENT_MODE_CHANGED:
      std::cout << " (Old: " << eventData->data.modeEvent.oldMode
                << ", New: " << eventData->data.modeEvent.newMode << ")";
      break;
    case EVENT_COMPONENT_ADDED:
    case EVENT_COMPONENT_REMOVED:
      std::cout << " (ID: " << eventData->data.componentEvent.componentId
                << ", Type: " << eventData->data.componentEvent.componentType
                << ")";
      break;
    case EVENT_WIRE_ADDED:
    case EVENT_WIRE_REMOVED:
      std::cout << " (Wire ID: " << eventData->data.wireEvent.wireId
                << ", From: " << eventData->data.wireEvent.fromComponent
                << ", To: " << eventData->data.wireEvent.toComponent << ")";
      break;
    case EVENT_PLC_STATE_CHANGED:
      std::cout << " (Running: "
                << (eventData->data.plcStateEvent.isRunning ? "true" : "false")
                << ")";
      break;
    case EVENT_IO_MAPPING_UPDATED:
      std::cout << " (Mappings: " << eventData->data.ioMappingEvent.mappingCount
                << ", Inputs: " << eventData->data.ioMappingEvent.inputCount
                << ", Outputs: " << eventData->data.ioMappingEvent.outputCount
                << ", Success: "
                << (eventData->data.ioMappingEvent.success ? "true" : "false")
                << ")";
      break;
    default:
      break;
  }

  std::cout << std::endl;
}

}  // namespace plc
