// event_system.h
// Copyright 2024 PLC Emulator Project
//
// Event dispatching and handling system.

// include/EventSystem.h
// C언어 스타일 이벤트 시스템
// 구조체 + 함수 포인터 기반 단순한 콜백 시스템

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_EVENT_SYSTEM_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_EVENT_SYSTEM_H_

namespace plc {

// 이벤트 타입 정의
typedef enum {
  EVENT_DATA_CHANGED = 0,    // 데이터 변경
  EVENT_MODE_CHANGED,        // 모드 변경
  EVENT_COMPONENT_ADDED,     // 컴포넌트 추가
  EVENT_COMPONENT_REMOVED,   // 컴포넌트 제거
  EVENT_WIRE_ADDED,          // 배선 추가
  EVENT_WIRE_REMOVED,        // 배선 제거
  EVENT_PLC_STATE_CHANGED,   // PLC 상태 변경
  EVENT_IO_MAPPING_UPDATED,  // Phase 2: I/O 매핑 업데이트
  EVENT_TYPE_COUNT           // 총 이벤트 타입 개수
} EventType;

// 이벤트 데이터 구조체 (C언어 스타일 union)
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

    struct {
      int mappingCount;  // 생성된 매핑 개수
      int inputCount;    // 입력 매핑 개수 (X)
      int outputCount;   // 출력 매핑 개수 (Y)
      bool success;      // 매핑 성공 여부
      void* mappingPtr;  // IOMapping* (타입 안전성을 위해 void*로)
    } ioMappingEvent;

    // 일반적인 정수/포인터 데이터
    int intValue;
    void* ptrValue;
  } data;
} EventData;

// 이벤트 콜백 함수 타입
typedef void (*EventCallback)(const EventData* eventData, void* userData);

// 이벤트 리스너 구조체
typedef struct EventListener {
  EventCallback callback;
  void* userData;
  struct EventListener* next;  // 연결 리스트
} EventListener;

// 이벤트 디스패처 구조체
typedef struct EventDispatcher {
  // 각 이벤트 타입별 리스너 리스트 (배열로 관리)
  EventListener* listeners[EVENT_TYPE_COUNT];

  // 함수 포인터들
  bool (*Subscribe)(struct EventDispatcher* dispatcher, EventType type,
                    EventCallback callback, void* userData);
  bool (*Unsubscribe)(struct EventDispatcher* dispatcher, EventType type,
                      EventCallback callback);
  void (*Dispatch)(struct EventDispatcher* dispatcher,
                   const EventData* eventData);
  void (*Clear)(struct EventDispatcher* dispatcher);
} EventDispatcher;

EventDispatcher* CreateEventDispatcher();
void DestroyEventDispatcher(EventDispatcher* dispatcher);

bool EventDispatcher_Subscribe(EventDispatcher* dispatcher, EventType type,
                               EventCallback callback, void* userData);
bool EventDispatcher_Unsubscribe(EventDispatcher* dispatcher, EventType type,
                                 EventCallback callback);
void EventDispatcher_Dispatch(EventDispatcher* dispatcher,
                              const EventData* eventData);
void EventDispatcher_Clear(EventDispatcher* dispatcher);

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
                                      void* mappingPtr);  // Phase 2

const char* EventTypeToString(EventType type);
void PrintEventData(const EventData* eventData);

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_DATA_EVENT_SYSTEM_H_
