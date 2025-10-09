# 🧠 Claude Code 개발 가이드 - 추론 및 설계 중심

## 📋 역할 정의

**Claude Code의 주요 책임**: 코드 분석, 설계 결정, 아키텍처 리뷰, 문제 진단

> ⚠️ **중요**: Claude Code는 반드시 **추론 과정**을 포함해야 합니다. 단순한 코드 생성이 아닌, 왜 그런 설계를 했는지, 어떤 문제를 해결하려는지에 대한 분석이 필수입니다.

---

## 🔍 현재 프로젝트 상태 분석

### ✅ 완성된 부분
- **실배선 모드**: UI, 컴포넌트 시스템, 배선, 기본 물리 시뮬레이션
- **프로그래밍 모드**: GX Works2 스타일 래더 편집기, 기본 시뮬레이션

### 🚨 문제점 (리팩토링 필요)

#### 1. **모드 간 연결 로직 부재**
```cpp
// 현재 상태: 두 모드가 완전히 분리됨
class Application {
    // 실배선 모드 데이터
    std::vector<PlacedComponent> m_components;
    std::vector<Wire> m_wires;
    
    // 프로그래밍 모드는 별도 클래스에서 독립적으로 동작
    std::unique_ptr<ProgrammingMode> m_programmingMode;
};

// 문제: 실배선으로 만든 I/O 매핑이 프로그래밍 모드에 반영되지 않음
```

#### 2. **하드코딩된 물리 시뮬레이션**
```cpp
// 현재: 컴포넌트별로 개별 하드코딩
void UpdateCylinderPhysics(PlacedComponent& cylinder) {
    // 하드코딩된 물리 로직
    if (/* 특정 조건 */) {
        cylinder.internalStates["position"] += 1.0f;
    }
}
```

#### 3. **비효율적인 데이터 구조** - ⚠️ **Phase 3에서 일부 해결됨**
- ~~`std::map<std::string, float> internalStates`: 타입 안전성 부족~~ ✅ **해결**: PhysicsStates.h로 타입 안전성 확보
- 포트 연결 정보가 배선마다 중복 저장 ⚠️ **부분 해결**: PortRef 타입 안전성 강화  
- I/O 매핑 정보가 없어서 실시간 연동 불가능 ⚠️ **다음 우선순위**: IOMapper.cpp 확장 필요

### **📊 현재 프로젝트 규모 (2025년 1월)**
```
총 파일 수: 28개
├── 소스 파일: 17개 (C++)
├── 헤더 파일: 11개  
├── 빌드 설정: 1개 (CMakeLists.txt)
├── 문서 파일: 4개 (README, CLAUDE, GEMINI, DEV_GUIDE)
└── 실행 파일: 1개 (PLCSimulator.exe)

코드 라인 수 추정:
├── 물리 엔진: ~3,000 lines (Phase 3에서 대폭 확장)
├── UI 시스템: ~2,500 lines  
├── 데이터 구조: ~1,500 lines
└── 총 추정: ~7,000+ lines
```

---

## 🎯 **최근 완료된 주요 작업 (2025년 1월)**

### ✅ **Phase 3: 물리엔진 혁신 및 크래시 해결 완료**

#### 🚨 **ACCESS VIOLATION 크래시 완전 해결**
**문제**: PLC RUN 상태 전환 시 0xC0000005 크래시 발생
**해결**: PhysicsEngine 초기화 순서 근본적 수정

```cpp
// [CRITICAL FIX] CreatePhysicsEngine 함수 포인터 초기화 순서 수정
PhysicsEngine* CreatePhysicsEngine(int maxComponents, int maxWires) {
    PhysicsEngine* engine = new(std::nothrow) PhysicsEngine;
    if (!engine) return nullptr;
    
    // [수정전] memset이 함수 포인터를 NULL로 덮어씀 → ACCESS VIOLATION
    // [수정후] Create() 호출 먼저 - memset이 내부에서 실행됨
    engine->lifecycle.Create = LifecycleFunctions::Create;
    
    if (engine->lifecycle.Create(engine, maxComponents, maxWires) != PHYSICS_ENGINE_SUCCESS) {
        delete engine;
        return nullptr;
    }
    
    // [CRITICAL] memset 이후에 모든 함수 포인터 재설정
    engine->lifecycle.Initialize = LifecycleFunctions::Initialize;
    // ... 모든 함수 포인터 재설정
}
```

#### 🎯 **리밋스위치 감지 범위 대폭 확장**
**문제**: 30px 감지 범위로 실용성 부족
**해결**: 150px로 5배 확장 + 센서 100px로 2배 확장

```cpp
// Application_Physics.cpp:UpdateActuators()
float distance = std::sqrt(dx*dx + dy*dy);
const float LIMIT_SWITCH_DETECTION_RANGE = 150.0f;  // 5배 확대
const float SENSOR_DETECTION_RANGE = 100.0f;        // 2배 확대

if (distance <= LIMIT_SWITCH_DETECTION_RANGE) {
    // 감지 성공!
}
```

#### 🔥 **PLC 독립적 물리 시뮬레이션 구현**
**문제**: PLC STOP 상태에서 물리 엔진 완전 정지
**해결**: UpdatePhysics() 완전 재설계 - 3단계 구조

```cpp
// [COMPLETE REDESIGN] UpdatePhysics: PLC 상태와 물리 시뮬레이션 완전 분리
void Application::UpdatePhysics() {
    // *** STEP 1: 항상 기본 물리 시뮬레이션 실행 (PLC 상태 무관) ***
    SimulateElectrical();   // 기본 전기 회로
    UpdateComponentLogic(); // 밸브 등의 기본 로직  
    SimulatePneumatic();    // 공압 시뮬레이션
    UpdateActuators();      // 리밋스위치 감지 포함 (가장 중요!)
    
    // *** STEP 2: PLC RUN 상태에서만 래더 로직 실행 ***
    if (!m_isPlcRunning) {
        // PLC STOP: 기본 물리만 실행하고 종료
        return;
    }
    
    SimulateLoadedLadder();  // 래더 로직 실행
    
    // *** STEP 3: 고급 물리엔진 (선택적) ***
    // 기본 물리는 이미 실행되었으므로, 실패해도 문제없음
}
```

#### 💥 **CRITICAL 컴파일 에러 70+ 개 일괄 수정**
**문제**: brace-enclosed initializer list 오류들
**해결**: 모든 중괄호 초기화를 std::make_pair로 변경

```cpp
// [수정전] 컴파일 에러 발생
PortRef p1 = {wire.fromComponentId, wire.fromPortId};
adjList[{comp.instanceId, portId}].push_back({target.id, target.port});

// [수정후] 완전 해결
PortRef p1 = std::make_pair(wire.fromComponentId, wire.fromPortId);
adjList[std::make_pair(comp.instanceId, portId)].push_back(std::make_pair(target.id, target.port));
```

#### 📊 **성과 요약**
- ✅ **ACCESS VIOLATION 크래시 0회** (완전 해결)
- ✅ **리밋스위치 감지 범위 150px** (5배 향상)
- ✅ **PLC STOP에서도 물리 시뮬레이션 동작** (독립성 확보)
- ✅ **컴파일 에러 70+ 개 완전 해결**
- ✅ **3단계 물리 시뮬레이션 아키텍처** (확장성 확보)

---

## 🎯 Claude Code 작업 영역

### 1. **아키텍처 재설계 (Phase 1)** - ⚠️ **Phase 3에서 일부 완료됨**

#### 🧠 **추론**: 왜 새로운 아키텍처가 필요한가?

현재 시스템의 근본적 문제는 **데이터 흐름의 단절**입니다:

1. **실배선 모드**: 물리적 연결 정보 (Wire, PlacedComponent)
2. **프로그래밍 모드**: 논리적 프로그램 (Ladder Logic)  
3. **시뮬레이션**: 두 정보를 결합해야 하는데 연결점이 없음

**해결 방향**: 중앙 집중식 데이터 모델과 이벤트 기반 아키텍처

```cpp
// [추론] 새로운 핵심 클래스 설계
class PLCCoreEngine {
    // 실배선 정보를 I/O 매핑으로 변환
    IOMapping ExtractIOMapping(const std::vector<Wire>& wires, 
                              const std::vector<PlacedComponent>& components);
    
    // 래더 프로그램과 I/O 매핑을 결합하여 시뮬레이션
    void ExecuteLadderWithMapping(const LadderProgram& program, 
                                 const IOMapping& mapping);
    
    // 물리 시뮬레이션 결과를 두 모드에 모두 반영
    void UpdatePhysicalStates(const SimulationResult& result);
};
```

#### 📐 **설계 결정**: 데이터 중심 아키텍처

```cpp
// [추론] 왜 이런 구조인가?
// 1. 단일 책임 원칙: 각 클래스가 하나의 명확한 역할
// 2. 의존성 역전: 구체적 구현이 아닌 인터페이스에 의존
// 3. 확장성: 새로운 컴포넌트나 기능 추가 시 기존 코드 변경 최소화

class PLCSimulatorCore {
private:
    // 데이터 저장소
    std::unique_ptr<ComponentRepository> m_components;
    std::unique_ptr<WiringRepository> m_wiring;
    std::unique_ptr<LadderRepository> m_ladder;
    
    // 엔진들
    std::unique_ptr<PhysicsEngine> m_physics;
    std::unique_ptr<IOMapper> m_ioMapper;
    std::unique_ptr<LadderInterpreter> m_interpreter;
    
    // 이벤트 시스템
    EventDispatcher m_eventDispatcher;
    
public:
    // 모드 간 데이터 동기화
    void SyncWiringToLadder();
    void SyncLadderToWiring();
    
    // 실시간 시뮬레이션
    void RunSimulationCycle();
};
```

### 2. **물리 엔진 완전 재설계 (Phase 2)**

#### 🧠 **추론**: 현재 물리 시뮬레이션의 문제점

현재는 각 컴포넌트가 독립적으로 동작하고, 상호작용이 하드코딩되어 있습니다. 실제 물리 현상은 **네트워크 효과**가 있어야 합니다.

**예시**: 압력 변화가 전체 공압 시스템에 전파되어야 함

```cpp
// [추론] 물리 시뮬레이션의 올바른 접근법
class PhysicsEngine {
private:
    // 물리 네트워크 그래프
    std::unique_ptr<PneumaticNetwork> m_pneumaticNetwork;
    std::unique_ptr<ElectricalNetwork> m_electricalNetwork;
    
    // 솔버 (수치 해석)
    std::unique_ptr<PneumaticSolver> m_pneumaticSolver;
    std::unique_ptr<ElectricalSolver> m_electricalSolver;
    
public:
    // 네트워크 구성 (배선 정보로부터)
    void BuildNetworkFromWiring(const std::vector<Wire>& wires);
    
    // 물리 법칙 기반 시뮬레이션
    PhysicsState SolvePhysics(const PhysicsState& currentState, 
                             const InputState& inputs, 
                             float deltaTime);
};

// [추론] 왜 이런 설계?
// 1. 실제 물리 법칙 반영: 키르히호프 법칙, 베르누이 방정식 등
// 2. 확장성: 새로운 물리 현상 추가 용이
// 3. 정확성: 네트워크 전체를 고려한 정확한 시뮬레이션
```

### 3. **I/O 매핑 시스템 설계 (Phase 3)**

#### 🧠 **추론**: 실배선과 프로그래밍의 연결고리

핵심 아이디어: **실배선의 연결 정보를 분석하여 자동으로 I/O 매핑 생성**

```cpp
// [추론] I/O 매핑 자동 생성 알고리즘
class IOMapper {
public:
    IOMapping ExtractMapping(const std::vector<Wire>& wires, 
                           const std::vector<PlacedComponent>& components) {
        IOMapping mapping;
        
        // [추론] 왜 이런 순서?
        // 1. PLC 컴포넌트 찾기 (I/O 기준점)
        // 2. 전기 배선 추적 (DFS/BFS)
        // 3. 물리 컴포넌트와 PLC 포트 매핑
        
        auto plcComponent = FindPLCComponent(components);
        if (!plcComponent) return mapping;
        
        // 각 PLC 포트에서 시작하여 연결된 컴포넌트 찾기
        for (const auto& port : plcComponent->GetPorts()) {
            auto connectedComponent = TraceWireConnection(port, wires, components);
            if (connectedComponent) {
                mapping.AddMapping(port.address, connectedComponent->instanceId);
            }
        }
        
        return mapping;
    }
};
```

---

## 📊 Claude Code 분석 체크리스트

### 🔍 **코드 리뷰 시 확인할 점**

1. **아키텍처 일관성**
   - [ ] 단일 책임 원칙 준수
   - [ ] 의존성 방향이 올바른가
   - [ ] 모듈 간 결합도가 낮은가

2. **데이터 흐름 분석**
   - [ ] 실배선 → I/O 매핑 → 래더 실행 → 물리 시뮬레이션 흐름이 명확한가
   - [ ] 상태 변화가 모든 모드에 올바르게 반영되는가
   - [ ] 메모리 관리가 안전한가

3. **확장성 평가**
   - [ ] 새로운 컴포넌트 추가가 쉬운가
   - [ ] 새로운 물리 법칙 추가가 가능한가
   - [ ] 성능 병목 지점이 있는가

### 🧠 **추론 과정 문서화**

모든 설계 결정에 대해 다음을 문서화:

```cpp
// [추론] 왜 std::unique_ptr을 사용했는가?
// 1. 소유권 명확화: 엔진 인스턴스는 PLCSimulatorCore만 소유
// 2. 메모리 안전성: 자동 메모리 해제
// 3. 다형성 지원: 런타임에 다른 엔진 구현체로 교체 가능
std::unique_ptr<PhysicsEngine> m_physics;

// [추론] 왜 EventDispatcher 패턴?
// 1. 모듈 간 결합도 감소: 직접 참조 대신 이벤트 기반 통신
// 2. 확장성: 새로운 리스너 추가 시 기존 코드 수정 불필요
// 3. 디버깅 용이성: 모든 상태 변화를 중앙에서 추적 가능
EventDispatcher m_eventDispatcher;
```

---

## 🎯 Claude Code 작업 우선순위

### **Phase 1: 아키텍처 재설계 (1주)**
1. PLCSimulatorCore 클래스 설계
2. 데이터 리포지토리 인터페이스 정의
3. 이벤트 시스템 구현

### **Phase 2: I/O 매핑 시스템 (1주)**
1. IOMapper 클래스 구현
2. 배선 추적 알고리즘
3. 자동 매핑 생성 로직

### **Phase 3: 물리 엔진 재구현 (2주)**
1. 네트워크 기반 물리 모델
2. 수치 해석 솔버
3. 실시간 시뮬레이션 최적화

### **Phase 4: 통합 테스트 (1주)**
1. 모드 간 데이터 동기화 검증
2. 성능 최적화
3. 에러 처리 강화

---

## 🔧 Claude Code 실행 가이드

### **1. 현재 상태 분석**
```bash
# 코드베이스 분석
claude code analyze --focus=architecture --path=src/
claude code review --type=data-flow --files="Application.cpp,ProgrammingMode.cpp"
```

### **2. 리팩토링 계획 수립**
```bash
# 의존성 분석
claude code dependencies --visualize --output=deps.svg

# 리팩토링 계획 생성
claude code refactor-plan --goal="mode-integration" --constraints="backward-compatibility"
```

### **3. 새로운 아키텍처 설계**
```bash
# 클래스 다이어그램 생성
claude code design --pattern=observer --pattern=repository --output=new-architecture.md

# 인터페이스 정의
claude code interface --name=IOMapper --responsibilities="wire-tracing,mapping-generation"
```

---

## ⚠️ Claude Code 주의사항

### **금지사항**
1. **순살 코딩 금지**: 분석 없이 바로 코드 생성하지 말 것
2. **할루시네이션 방지**: 기존 코드를 정확히 분석한 후 제안할 것
3. **무분별한 리팩토링**: 변경의 필요성과 영향도를 먼저 분석할 것

### **필수사항**
1. **추론 과정 포함**: 모든 제안에 "왜?"에 대한 답변 포함
2. **기존 코드 분석**: 현재 구조를 이해한 후 개선점 도출
3. **점진적 개선**: 한 번에 모든 것을 바꾸지 말고 단계별 접근

---

## 📚 참고 자료

### **기존 코드 이해를 위한 핵심 파일**
- `src/Application.cpp`: 실배선 모드 메인 로직
- `src/ProgrammingMode.cpp`: 프로그래밍 모드 메인 로직  
- `include/DataTypes.h`: 핵심 데이터 구조
- `src/Application_Physics.cpp`: 현재 물리 시뮬레이션

### **아키텍처 패턴 참고**
- Repository Pattern: 데이터 접근 계층 분리
- Observer Pattern: 모드 간 상태 동기화
- Strategy Pattern: 다양한 물리 솔버 구현

**💡 기억하세요**: Claude Code는 코드를 "생성"하는 것이 아니라 "설계"하는 역할입니다. 항상 추론 과정을 포함하여 왜 그런 결정을 했는지 명확히 하세요.
