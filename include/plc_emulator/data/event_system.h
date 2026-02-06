/*
 * event_system.h
 *
 * Event dispatching and handling system.
 * 이벤트 디스패치 및 처리 시스템.
 *
 * C-style callbacks and listener lists.
 * C 스타일 콜백과 리스너 리스트.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_EVENT_SYSTEM_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_EVENT_SYSTEM_H_

namespace plc {

/*
 * Event type identifiers.
 * 이벤트 타입 식별자.
 */
typedef enum {
  EVENT_DATA_CHANGED = 0,
  EVENT_MODE_CHANGED,
  EVENT_COMPONENT_ADDED,
  EVENT_COMPONENT_REMOVED,
  EVENT_WIRE_ADDED,
  EVENT_WIRE_REMOVED,
  EVENT_PLC_STATE_CHANGED,
  EVENT_IO_MAPPING_UPDATED,
  EVENT_TYPE_COUNT
} EventType;

/*
 * Event payload data.
 * 이벤트 페이로드 데이터.
 */
typedef struct EventData {
  EventType type;
  union {
    struct {
      int componentId;
      int componentType;
    } componentEvent;

    struct {
      int wireId;
      int fromComponent;
      int toComponent;
    } wireEvent;

    struct {
      int oldMode;
      int newMode;
    } modeEvent;

    struct {
      bool isRunning;
    } plcStateEvent;

    /*
     * I/O mapping update payload (mappingPtr is IOMapping*).
     * I/O 매핑 업데이트 페이로드 (mappingPtr는 IOMapping*).
     */
    struct {
      int mappingCount;
      int inputCount;
      int outputCount;
      bool success;
      void* mappingPtr;
    } ioMappingEvent;

    int intValue;
    void* ptrValue;
  } data;
} EventData;

/*
 * Event callback signature.
 * 이벤트 콜백 시그니처.
 */
typedef void (*EventCallback)(const EventData* eventData, void* userData);

/*
 * Event listener node.
 * 이벤트 리스너 노드.
 */
typedef struct EventListener {
  EventCallback callback;
  void* userData;
  struct EventListener* next;
} EventListener;

/*
 * Event dispatcher with per-type listener lists.
 * 타입별 리스너 리스트를 관리하는 디스패처.
 */
typedef struct EventDispatcher {
  EventListener* listeners[EVENT_TYPE_COUNT];
  bool (*Subscribe)(struct EventDispatcher* dispatcher, EventType type,
                    EventCallback callback, void* userData);
  bool (*Unsubscribe)(struct EventDispatcher* dispatcher, EventType type,
                      EventCallback callback);
  void (*Dispatch)(struct EventDispatcher* dispatcher,
                   const EventData* eventData);
  void (*Clear)(struct EventDispatcher* dispatcher);
} EventDispatcher;

/*
 * Dispatcher lifecycle.
 * 디스패처 생성/해제.
 */
EventDispatcher* CreateEventDispatcher();
void DestroyEventDispatcher(EventDispatcher* dispatcher);

/*
 * Dispatcher operations.
 * 디스패처 동작 함수.
 */
bool EventDispatcher_Subscribe(EventDispatcher* dispatcher, EventType type,
                               EventCallback callback, void* userData);
bool EventDispatcher_Unsubscribe(EventDispatcher* dispatcher, EventType type,
                                 EventCallback callback);
void EventDispatcher_Dispatch(EventDispatcher* dispatcher,
                              const EventData* eventData);
void EventDispatcher_Clear(EventDispatcher* dispatcher);

/*
 * Event data builders.
 * 이벤트 데이터 생성 함수.
 */
EventData CreateDataChangedEvent();
EventData CreateModeChangedEvent(int oldMode, int newMode);
EventData CreateComponentAddedEvent(int componentId, int componentType);
EventData CreateComponentRemovedEvent(int componentId, int componentType);
EventData CreateWireAddedEvent(int wireId, int fromComponent, int toComponent);
EventData CreateWireRemovedEvent(int wireId, int fromComponent,
                                 int toComponent);
EventData CreatePlcStateChangedEvent(bool isRunning);
EventData CreateIOMappingUpdatedEvent(int mappingCount, int inputCount,
                                      int outputCount, bool success,
                                      void* mappingPtr);

/*
 * Event diagnostics helpers.
 * 이벤트 진단 헬퍼 함수.
 */
const char* EventTypeToString(EventType type);
void PrintEventData(const EventData* eventData);

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_EVENT_SYSTEM_H_ */
