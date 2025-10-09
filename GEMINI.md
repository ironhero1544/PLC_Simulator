# ⚡ Gemini CLI 개발 가이드 - 구현 및 테스트 중심

## 📋 역할 정의

**Gemini CLI의 주요 책임**: 실제 코드 구현, 단위 테스트, 빌드 시스템, 성능 최적화

> ⚡ **중요**: Gemini CLI는 Claude Code가 설계한 아키텍처를 **실제로 구현**하는 역할입니다. 빠르고 정확한 코드 생성에 집중하세요.

---

## 🛠️ 현재 기술 스택 및 환경

### **빌드 환경**
- **컴파일러**: MSVC (Windows), GCC/Clang (Linux/macOS)
- **빌드 시스템**: CMake 3.16+
- **의존성 관리**: FetchContent (자동 다운로드)

### **핵심 라이브러리**
```cmake
# 현재 사용 중인 라이브러리들
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui REQUIRED)
```

### **프로젝트 구조 (2025년 1월 업데이트)**
```
src/                                    # 소스 파일 (17개)
├── main.cpp                           # 애플리케이션 진입점
│
├── Application.cpp                    # 메인 애플리케이션 (실배선 모드)
├── Application_Components.cpp         # 컴포넌트 관리 및 렌더링
├── Application_Physics.cpp            # ⭐ 물리 시뮬레이션 (Phase 3 완료)
├── Application_Ports.cpp              # 포트 연결 및 관리
├── Application_Snap.cpp               # 스냅 및 정렬 기능
├── Application_Wiring.cpp             # 배선 시스템
│
├── ProgrammingMode.cpp                # 프로그래밍 모드 메인
├── ProgrammingMode_Edit.cpp           # 래더 편집기
├── ProgrammingMode_Sim.cpp            # 래더 시뮬레이션
├── ProgrammingMode_UI.cpp             # 프로그래밍 UI
│
├── DataTypes.cpp                      # 기본 데이터 구조
├── DataRepository.cpp                 # 데이터 저장소 관리
├── EventSystem.cpp                    # 이벤트 시스템
├── IOMapper.cpp                       # I/O 매핑 시스템
├── PLCSimulatorCore.cpp               # ⭐ 시뮬레이터 코어 엔진
├── PhysicsEngine.cpp                  # ⭐ 물리 엔진 (Phase 3 완료)
└── PhysicsSolvers.cpp                 # ⭐ 물리 솔버 (전기/공압/기계)

include/                                # 헤더 파일 (11개)
├── Application.h                      # 메인 애플리케이션
├── DataRepository.h                   # 데이터 저장소
├── DataTypes.h                        # 기본 타입 정의
├── EventSystem.h                      # 이벤트 시스템
├── IOMapper.h                         # I/O 매핑
├── IOMapping.h                        # I/O 매핑 데이터
├── PLCSimulatorCore.h                 # ⭐ 시뮬레이터 코어
├── PhysicsEngine.h                    # ⭐ 물리 엔진 API
├── PhysicsNetwork.h                   # ⭐ 물리 네트워크 (전기/공압/기계)
├── PhysicsStates.h                    # ⭐ 물리 상태 정의
└── ProgrammingMode.h                  # 프로그래밍 모드

빌드 산출물/
├── cmake-build-debug/PLCSimulator.exe # ⭐ 메인 실행 파일
├── cmake-build-debug/libimgui_lib.a   # ImGui 정적 라이브러리
└── unifont.ttf                        # 폰트 리소스
```

### **⭐ Phase 3에서 추가된 핵심 파일들**
- **PhysicsEngine.cpp/h**: 고성능 물리 시뮬레이션 엔진
- **PhysicsSolvers.cpp**: C 함수 기반 수치 해석 솔버
- **PhysicsNetwork.h**: 전기/공압/기계 네트워크 구조
- **PhysicsStates.h**: 컴포넌트별 물리 상태 정의
- **Application_Physics.cpp**: 3단계 물리 시뮬레이션 파이프라인

---

## 🎯 **최근 완료된 주요 구현 작업 (2025년 1월)**

### ✅ **Phase 3: 물리엔진 안정성 및 성능 최적화 완료**

#### 🚨 **메모리 안전성 강화 완료**
**구현**: PhysicsEngine 메모리 관리 및 초기화 완전 재구현

```cpp
// PhysicsEngine.cpp - CreatePhysicsEngine 안전성 강화
PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires) {
    // [구현] std::nothrow를 사용한 안전한 메모리 할당
    PhysicsEngine* engine = new(std::nothrow) PhysicsEngine;
    if (!engine) {
        std::cerr << "[PhysicsEngine] Failed to allocate memory" << std::endl;
        return nullptr;
    }
    
    // [핵심] 함수 포인터 초기화 순서 최적화
    engine->lifecycle.Create = LifecycleFunctions::Create;
    
    // [안전성] Create 실패 시 즉시 정리
    if (engine->lifecycle.Create(engine, maxComponents, maxWires) != PHYSICS_ENGINE_SUCCESS) {
        delete engine;
        return nullptr;
    }
    
    // [메모리 안전] memset 이후 모든 함수 포인터 재설정
    // 총 15개 함수 포인터 안전 초기화 완료
    return engine;
}
```

#### ⚡ **물리 시뮬레이션 성능 최적화**
**구현**: 3단계 파이프라인으로 60FPS 실시간 시뮬레이션

```cpp
// Application_Physics.cpp - UpdatePhysics 최적화 구현
void Application::UpdatePhysics() {
    // STEP 1: 기본 물리 (항상 실행 - 60FPS)
    SimulateElectrical();    // 전기 네트워크 O(E) 
    UpdateComponentLogic();  // 컴포넌트 상태 O(N)
    SimulatePneumatic();     // 공압 네트워크 O(P)  
    UpdateActuators();       // 센서/실린더 O(N*M)
    
    // STEP 2: PLC 로직 (조건부 실행)
    if (m_isPlcRunning) {
        SimulateLoadedLadder(); // 래더 실행 O(R*C)
        
        // STEP 3: 고급 물리 (선택적)
        if (m_physicsEngine && m_physicsEngine->isInitialized) {
            // C 함수 호출로 최대 성능
            SolveElectricalNetworkC(m_physicsEngine->electricalNetwork, deltaTime);
            SolvePneumaticNetworkC(m_physicsEngine->pneumaticNetwork, deltaTime);
            SolveMechanicalSystemC(m_physicsEngine->mechanicalSystem, deltaTime);
        }
    }
}
```

#### 🎯 **센서 감지 알고리즘 최적화**
**구현**: 거리 기반 감지 + 디버그 시스템

```cpp
// UpdateActuators() - 최적화된 감지 로직
for (auto& sensor : m_placedComponents) {
    if (sensor.type == ComponentType::LIMIT_SWITCH || sensor.type == ComponentType::SENSOR) {
        for (const auto& cylinder : m_placedComponents) {
            if (cylinder.type == ComponentType::CYLINDER) {
                // [최적화] 빠른 거리 계산
                float dx = sensor.position.x - cylinderRodPos.x;
                float dy = sensor.position.y - cylinderRodPos.y;
                float distance = std::sqrt(dx*dx + dy*dy);
                
                // [구현] 컴포넌트별 최적 감지 범위
                float detectionRange = (sensor.type == ComponentType::LIMIT_SWITCH) ? 
                                     150.0f : 100.0f;
                
                if (distance <= detectionRange) {
                    sensor.internalStates["is_pressed"] = 1.0f;
                    
                    // [디버그] 실시간 상태 로깅
                    if (m_enableDebugLogging) {
                        std::cout << "[" << (sensor.type == ComponentType::LIMIT_SWITCH ? 
                                           "LimitSwitch" : "Sensor") 
                                 << "] Detected at distance: " << distance 
                                 << "px (range: " << detectionRange << "px)" << std::endl;
                    }
                }
            }
        }
    }
}
```

#### 🛠️ **컴파일 시스템 안정성 강화**
**구현**: C++20 호환성 + 타입 안전성 보장

```cpp
// [구현] 모든 brace-enclosed initializer 오류 해결 (70+ 개)
// 수정 전: 컴파일 에러 발생
PortRef p1 = {wire.fromComponentId, wire.fromPortId};

// 수정 후: 타입 안전성 보장
PortRef p1 = std::make_pair(wire.fromComponentId, wire.fromPortId);

// [구현] namespace 명시적 선언
using plc::TimerState;
using plc::CounterState;
using plc::ComponentType;

// [구현] lambda 함수 타입 추론 최적화
auto comp_it = std::find_if(m_placedComponents.begin(), m_placedComponents.end(), 
                           [compId](const auto& c){ return c.instanceId == compId; });
```

#### 📊 **구현 성과 요약**
- ✅ **메모리 안전성**: ACCESS VIOLATION 0회, 안전한 포인터 관리
- ✅ **실시간 성능**: 60FPS 물리 시뮬레이션, 3단계 파이프라인  
- ✅ **컴파일 안정성**: 70+ 타입 오류 해결, C++20 완전 호환
- ✅ **센서 정밀도**: 리밋스위치 150px, 센서 100px 최적화
- ✅ **디버그 시스템**: Ctrl+F12 실시간 로깅, 상태 추적

#### 🔧 **다음 구현 우선순위**
1. **I/O 매핑 자동화**: 실배선 → 래더 프로그램 연결
2. **네트워크 시뮬레이션**: 고급 전기/공압 솔버 성능 향상  
3. **UI 응답성**: ImGui 렌더링 최적화
4. **테스트 자동화**: 단위 테스트 및 통합 테스트 구축

---

## 🎯 Gemini CLI 작업 영역

### 1. **핵심 클래스 구현 (Phase 1)** - ⚠️ **Phase 3에서 일부 완료됨**

#### 🔧 **PLCSimulatorCore 클래스 구현**

```cpp
// PLCSimulatorCore.h
#pragma once
#include "DataTypes.h"
#include <memory>
#include <functional>

namespace plc {

class PhysicsEngine;
class IOMapper;
class LadderInterpreter;
class EventDispatcher;

class PLCSimulatorCore {
private:
    // 데이터 저장소
    std::vector<PlacedComponent> m_components;
    std::vector<Wire> m_wires;
    std::vector<LadderRung> m_ladderRungs;
    
    // 엔진들
    std::unique_ptr<PhysicsEngine> m_physics;
    std::unique_ptr<IOMapper> m_ioMapper;
    std::unique_ptr<LadderInterpreter> m_interpreter;
    std::unique_ptr<EventDispatcher> m_eventDispatcher;
    
    // 실행 상태
    bool m_isRunning = false;
    float m_simulationTime = 0.0f;
    
public:
    PLCSimulatorCore();
    ~PLCSimulatorCore();
    
    // 컴포넌트 관리
    int AddComponent(ComponentType type, Position pos);
    void RemoveComponent(int instanceId);
    void MoveComponent(int instanceId, Position newPos);
    
    // 배선 관리
    int AddWire(int fromCompId, int fromPortId, int toCompId, int toPortId);
    void RemoveWire(int wireId);
    void UpdateWireWaypoints(int wireId, const std::vector<Position>& waypoints);
    
    // 래더 프로그램 관리
    void SetLadderProgram(const std::vector<LadderRung>& rungs);
    const std::vector<LadderRung>& GetLadderProgram() const;
    
    // 시뮬레이션 제어
    void StartSimulation();
    void StopSimulation();
    void StepSimulation(float deltaTime);
    
    // 상태 조회
    const std::vector<PlacedComponent>& GetComponents() const { return m_components; }
    const std::vector<Wire>& GetWires() const { return m_wires; }
    bool IsRunning() const { return m_isRunning; }
    
    // 이벤트 등록
    void RegisterComponentUpdateCallback(std::function<void(int)> callback);
    void RegisterSimulationStateCallback(std::function<void(bool)> callback);
};

} // namespace plc
```

#### 🔧 **IOMapper 클래스 구현**

```cpp
// IOMapper.h
#pragma once
#include "DataTypes.h"
#include <unordered_map>

namespace plc {

struct IOMapping {
    // PLC 포트 → 물리 컴포넌트 매핑
    std::unordered_map<std::string, int> inputMapping;   // "X0" → component_id
    std::unordered_map<std::string, int> outputMapping;  // "Y0" → component_id
    
    // 매핑 정보 조회
    bool HasInputMapping(const std::string& plcPort) const;
    bool HasOutputMapping(const std::string& plcPort) const;
    int GetInputComponent(const std::string& plcPort) const;
    int GetOutputComponent(const std::string& plcPort) const;
};

class IOMapper {
public:
    // 배선 정보에서 I/O 매핑 자동 추출
    IOMapping ExtractMapping(const std::vector<Wire>& wires, 
                           const std::vector<PlacedComponent>& components);
    
private:
    // PLC 컴포넌트 찾기
    const PlacedComponent* FindPLCComponent(const std::vector<PlacedComponent>& components);
    
    // 특정 포트에서 시작하여 연결된 컴포넌트 추적
    int TraceWireConnection(const PlacedComponent* plc, int portId,
                          const std::vector<Wire>& wires,
                          const std::vector<PlacedComponent>& components);
    
    // 포트 역할 판별 (입력/출력)
    bool IsInputPort(const std::string& portName);
    bool IsOutputPort(const std::string& portName);
};

} // namespace plc
```

### 2. **물리 엔진 구현 (Phase 2)**

#### 🔧 **PhysicsEngine 클래스 구현**

```cpp
// PhysicsEngine.h
#pragma once
#include "DataTypes.h"
#include <unordered_map>

namespace plc {

struct PhysicsState {
    // 전기 상태
    std::unordered_map<int, float> voltages;      // component_id → voltage
    std::unordered_map<int, float> currents;     // component_id → current
    
    // 공압 상태  
    std::unordered_map<int, float> pressures;    // component_id → pressure
    std::unordered_map<int, float> flowRates;    // component_id → flow_rate
    
    // 기구 상태
    std::unordered_map<int, float> positions;    // component_id → position
    std::unordered_map<int, float> velocities;   // component_id → velocity
    
    // 센서 상태
    std::unordered_map<int, bool> sensorStates;  // component_id → activated
};

class PhysicsEngine {
private:
    PhysicsState m_currentState;
    PhysicsState m_previousState;
    
    // 네트워크 그래프
    std::vector<std::vector<int>> m_electricalConnections;
    std::vector<std::vector<int>> m_pneumaticConnections;
    
public:
    PhysicsEngine();
    ~PhysicsEngine();
    
    // 네트워크 구성
    void BuildNetworks(const std::vector<Wire>& wires, 
                      const std::vector<PlacedComponent>& components);
    
    // 물리 시뮬레이션 스텝
    PhysicsState SimulateStep(const PhysicsState& currentState,
                             const std::unordered_map<std::string, bool>& inputs,
                             float deltaTime);
    
    // 상태 조회
    const PhysicsState& GetCurrentState() const { return m_currentState; }
    
private:
    // 전기 시뮬레이션
    void SolveElectricalNetwork(PhysicsState& state, 
                               const std::unordered_map<std::string, bool>& inputs);
    
    // 공압 시뮬레이션
    void SolvePneumaticNetwork(PhysicsState& state, float deltaTime);
    
    // 기구 시뮬레이션
    void SolveMechanicalSystem(PhysicsState& state, float deltaTime);
    
    // 센서 업데이트
    void UpdateSensors(PhysicsState& state);
};

} // namespace plc
```

### 3. **래더 인터프리터 구현 (Phase 3)**

#### 🔧 **LadderInterpreter 클래스 구현**

```cpp
// LadderInterpreter.h
#pragma once
#include "DataTypes.h"
#include <unordered_map>

namespace plc {

enum class LadderInstructionType {
    CONTACT_NO,    // A접점 (Normally Open)
    CONTACT_NC,    // B접점 (Normally Closed)  
    COIL,          // 코일 (출력)
    TIMER,         // 타이머
    COUNTER,       // 카운터
    HLINE,         // 가로선
    VLINE          // 세로선
};

struct LadderInstruction {
    LadderInstructionType type;
    std::string address;        // "X0", "Y0", "M0", "T0", "C0" 등
    int preset = 0;            // 타이머/카운터 설정값
    bool isActive = false;     // 시뮬레이션 중 활성화 상태
};

struct LadderRung {
    int rungNumber;
    std::vector<LadderInstruction> instructions;  // 12개 셀
    std::string comment;
};

struct VerticalConnection {
    int x;                     // 세로선 X 위치 (0~11)
    std::vector<int> rungs;    // 연결된 룽들
};

class LadderInterpreter {
private:
    // 디바이스 메모리
    std::unordered_map<std::string, bool> m_deviceStates;  // X0, Y0, M0 등
    std::unordered_map<std::string, int> m_timerValues;    // T0, T1 등의 현재값
    std::unordered_map<std::string, int> m_counterValues;  // C0, C1 등의 현재값
    
    // 프로그램
    std::vector<LadderRung> m_program;
    std::vector<VerticalConnection> m_verticalConnections;
    
public:
    LadderInterpreter();
    ~LadderInterpreter();
    
    // 프로그램 관리
    void SetProgram(const std::vector<LadderRung>& rungs,
                   const std::vector<VerticalConnection>& vConnections);
    
    // 실행
    void ExecuteProgram(const std::unordered_map<std::string, bool>& inputs);
    
    // 상태 조회
    bool GetDeviceState(const std::string& address) const;
    int GetTimerValue(const std::string& address) const;
    int GetCounterValue(const std::string& address) const;
    
    // 출력 상태 조회
    std::unordered_map<std::string, bool> GetOutputStates() const;
    
private:
    // 룽 실행
    bool ExecuteRung(const LadderRung& rung);
    
    // 명령어 실행
    bool ExecuteInstruction(const LadderInstruction& instruction, bool powerFlow);
    
    // 세로선 처리 (OR 로직)
    void ProcessVerticalConnections();
    
    // 타이머/카운터 업데이트
    void UpdateTimers();
    void UpdateCounters();
};

} // namespace plc
```

### 4. **이벤트 시스템 구현 (Phase 4)**

#### 🔧 **EventDispatcher 클래스 구현**

```cpp
// EventDispatcher.h
#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>

namespace plc {

enum class EventType {
    COMPONENT_ADDED,
    COMPONENT_REMOVED,
    COMPONENT_MOVED,
    WIRE_ADDED,
    WIRE_REMOVED,
    SIMULATION_STARTED,
    SIMULATION_STOPPED,
    DEVICE_STATE_CHANGED,
    PHYSICS_UPDATED
};

struct Event {
    EventType type;
    int componentId = -1;
    std::string deviceAddress;
    bool boolValue = false;
    float floatValue = 0.0f;
    void* data = nullptr;
};

using EventCallback = std::function<void(const Event&)>;

class EventDispatcher {
private:
    std::unordered_map<EventType, std::vector<EventCallback>> m_callbacks;
    
public:
    // 이벤트 리스너 등록
    void Subscribe(EventType type, EventCallback callback);
    
    // 이벤트 발송
    void Dispatch(const Event& event);
    
    // 편의 함수들
    void DispatchComponentAdded(int componentId);
    void DispatchComponentRemoved(int componentId);
    void DispatchSimulationStateChanged(bool isRunning);
    void DispatchDeviceStateChanged(const std::string& address, bool state);
};

} // namespace plc
```

---

## 🧪 테스트 구현 가이드

### **단위 테스트 구조**

```cpp
// tests/test_ioMapper.cpp
#include <gtest/gtest.h>
#include "../include/IOMapper.h"

class IOMapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 데이터 준비
        components = CreateTestComponents();
        wires = CreateTestWires();
    }
    
    std::vector<PlacedComponent> components;
    std::vector<Wire> wires;
};

TEST_F(IOMapperTest, ExtractBasicIOMapping) {
    IOMapper mapper;
    auto mapping = mapper.ExtractMapping(wires, components);
    
    // X0가 button에 연결되었는지 확인
    EXPECT_TRUE(mapping.HasInputMapping("X0"));
    EXPECT_EQ(mapping.GetInputComponent("X0"), BUTTON_COMPONENT_ID);
    
    // Y0가 LED에 연결되었는지 확인
    EXPECT_TRUE(mapping.HasOutputMapping("Y0"));
    EXPECT_EQ(mapping.GetOutputComponent("Y0"), LED_COMPONENT_ID);
}

TEST_F(IOMapperTest, HandleComplexWiring) {
    // 복잡한 배선 시나리오 테스트
    // 여러 컴포넌트가 연결된 경우
    // 배선이 없는 포트들
    // 잘못된 연결 등
}
```

### **통합 테스트 구조**

```cpp
// tests/test_integration.cpp
TEST(IntegrationTest, WiringToLadderFlow) {
    PLCSimulatorCore core;
    
    // 1. 실배선 모드에서 컴포넌트 배치
    int plcId = core.AddComponent(ComponentType::PLC, {100, 100});
    int buttonId = core.AddComponent(ComponentType::BUTTON_UNIT, {200, 100});
    
    // 2. 배선 연결
    int wireId = core.AddWire(buttonId, 0, plcId, 0); // button → X0
    
    // 3. I/O 매핑 확인
    auto mapping = core.GetIOMapping();
    EXPECT_TRUE(mapping.HasInputMapping("X0"));
    
    // 4. 래더 프로그램 설정
    std::vector<LadderRung> program = CreateTestLadderProgram();
    core.SetLadderProgram(program);
    
    // 5. 시뮬레이션 실행
    core.StartSimulation();
    core.StepSimulation(0.1f);
    
    // 6. 결과 확인
    // 물리적 버튼 누름 → X0 활성화 → 래더 실행 → Y0 출력
}
```

---

## ⚡ 성능 최적화 가이드

### **1. 메모리 최적화**

```cpp
// 메모리 풀 사용
class ComponentPool {
private:
    std::vector<PlacedComponent> m_pool;
    std::vector<bool> m_used;
    size_t m_nextFree = 0;
    
public:
    int Allocate() {
        // O(1) 할당
        if (m_nextFree < m_pool.size()) {
            int id = m_nextFree++;
            m_used[id] = true;
            return id;
        }
        // 확장 필요시
        m_pool.resize(m_pool.size() * 2);
        m_used.resize(m_used.size() * 2);
        return Allocate();
    }
    
    void Deallocate(int id) {
        m_used[id] = false;
        // 간단한 free list 관리
    }
};
```

### **2. 시뮬레이션 최적화**

```cpp
// 더티 플래그 패턴
class OptimizedPhysicsEngine {
private:
    bool m_networkDirty = true;
    bool m_statesDirty = true;
    
public:
    void MarkNetworkDirty() { m_networkDirty = true; }
    void MarkStatesDirty() { m_statesDirty = true; }
    
    PhysicsState SimulateStep(float deltaTime) {
        if (m_networkDirty) {
            RebuildNetworks();
            m_networkDirty = false;
        }
        
        if (m_statesDirty) {
            RecalculateStates();
            m_statesDirty = false;
        }
        
        // 실제 시뮬레이션은 변화가 있을 때만
        return m_currentState;
    }
};
```

---

## 🔧 Gemini CLI 실행 명령어

### **1. 코드 생성**
```bash
# 클래스 구현 생성
gemini generate class --name=PLCSimulatorCore --interface=PLCSimulatorCore.h --output=src/

# 테스트 코드 생성  
gemini generate tests --class=IOMapper --type=unit --output=tests/

# CMake 파일 업데이트
gemini update cmake --add-sources=src/PLCSimulatorCore.cpp --add-tests=tests/
```

### **2. 빌드 및 테스트**
```bash
# 빠른 빌드
gemini build --target=PLCSimulator --config=Debug

# 테스트 실행
gemini test --filter=IOMapper* --verbose

# 성능 프로파일링
gemini profile --target=PhysicsEngine --duration=10s
```

### **3. 코드 검증**
```bash
# 정적 분석
gemini analyze --tool=cppcheck --tool=clang-tidy --path=src/

# 메모리 검사  
gemini memcheck --tool=valgrind --target=PLCSimulator

# 코드 커버리지
gemini coverage --target=tests --format=html
```

---

## 📋 Gemini CLI 작업 체크리스트

### **Phase 1: 핵심 클래스 (1주)**
- [ ] PLCSimulatorCore 클래스 구현
- [ ] EventDispatcher 구현  
- [ ] 기본 단위 테스트 작성
- [ ] CMake 빌드 시스템 업데이트

### **Phase 2: I/O 매핑 (1주)**  
- [ ] IOMapper 클래스 구현
- [ ] 배선 추적 알고리즘 구현
- [ ] IOMapping 데이터 구조 구현
- [ ] I/O 매핑 테스트 작성

### **Phase 3: 물리 엔진 (2주)**
- [ ] PhysicsEngine 클래스 구현
- [ ] 전기 네트워크 솔버 구현  
- [ ] 공압 네트워크 솔버 구현
- [ ] 기구 시뮬레이션 구현
- [ ] 물리 엔진 성능 테스트

### **Phase 4: 래더 인터프리터 (1주)**
- [ ] LadderInterpreter 클래스 구현
- [ ] 래더 명령어 실행 엔진
- [ ] 세로선 OR 로직 구현
- [ ] 타이머/카운터 기능 구현

### **Phase 5: 통합 및 최적화 (1주)**
- [ ] 모든 컴포넌트 통합
- [ ] 통합 테스트 작성
- [ ] 성능 최적화
- [ ] 메모리 누수 검사
- [ ] 배포 준비

---

## 🚀 빠른 시작 템플릿

### **새 클래스 생성 템플릿**
```cpp
// Template: NewClass.h
#pragma once
#include "DataTypes.h"

namespace plc {

class NewClass {
private:
    // 멤버 변수
    
public:
    NewClass();
    ~NewClass();
    
    // 복사/이동 생성자 (필요시)
    NewClass(const NewClass&) = delete;
    NewClass& operator=(const NewClass&) = delete;
    NewClass(NewClass&&) = default;
    NewClass& operator=(NewClass&&) = default;
    
    // 주요 인터페이스
    
private:
    // 내부 구현
};

} // namespace plc
```

### **테스트 템플릿**
```cpp
// Template: test_NewClass.cpp
#include <gtest/gtest.h>
#include "../include/NewClass.h"

using namespace plc;

class NewClassTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 데이터 준비
    }
    
    void TearDown() override {
        // 정리
    }
    
    // 테스트 헬퍼 함수들
};

TEST_F(NewClassTest, BasicFunctionality) {
    // 기본 기능 테스트
    EXPECT_TRUE(true);
}

TEST_F(NewClassTest, EdgeCases) {
    // 경계 조건 테스트
}

TEST_F(NewClassTest, ErrorHandling) {
    // 에러 처리 테스트
}
```

---

## ⚠️ 주의사항

### **컴파일 에러 방지**
1. **헤더 가드**: 모든 헤더에 `#pragma once` 사용
2. **전방 선언**: `.h` 파일에서 불필요한 include 최소화
3. **네임스페이스**: 모든 클래스를 `plc` 네임스페이스에 배치

### **성능 고려사항**  
1. **실시간 제약**: 60FPS 유지를 위해 16ms 내 처리 완료
2. **메모리 할당**: 실시간 루프에서 동적 할당 최소화
3. **STL 최적화**: reserve() 사용으로 불필요한 재할당 방지

### **디버깅 지원**
1. **로깅**: 모든 중요한 상태 변화를 로그로 기록
2. **어설션**: 개발 모드에서 전제 조건 검증
3. **디버그 정보**: 릴리즈 빌드에서도 기본적인 디버그 정보 유지

**⚡ 기억하세요**: Gemini CLI는 빠르고 정확한 구현에 집중합니다. 설계는 Claude Code가 했으니, 그 설계를 충실히 구현하는 것이 목표입니다.
