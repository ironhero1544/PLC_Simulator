# PLC Emulator 리팩토링 계획서
# Google C++ Style Guide 기준 전면 리팩토링

## 프로젝트 개요

**프로젝트명**: FX3U PLC Simulator  
**언어**: C++20  
**목적**: Mitsubishi FX3U PLC의 실배선 및 래더 프로그래밍 시뮬레이터  
**리팩토링 목표**: Google C++ Style Guide 완전 준수 및 코드 품질 개선

## 핵심 기능 분석

### 1. Wiring Mode (실배선 모드)
- 전기/공압/기계 컴포넌트 배치 및 연결
- 드래그 앤 드롭 인터페이스
- 스냅 그리드 시스템
- 와이어 라우팅 및 편집

### 2. Programming Mode (프로그래밍 모드)
- 래더 다이어그램 편집기
- OpenPLC 기반 컴파일 엔진
- 실시간 시뮬레이션
- 디바이스 모니터링

### 3. Physics Engine (물리 엔진)
- 전기 네트워크 시뮬레이션
- 공압 시스템 시뮬레이션
- 기계 시스템 동역학
- C 스타일 구조체 기반 설계

### 4. I/O Mapping System
- 물리 배선 자동 매핑
- PLC 주소 생성 (X0-X15, Y0-Y15)
- BFS 기반 연결 추적

### 5. Project Management
- .plcproj 파일 형식
- JSON 기반 직렬화
- 프로젝트 저장/로드

---

## 현재 디렉토리 구조 분석

### 현재 구조
```
PLC_Emulator/
├── .clang-format           # 포매팅 설정
├── .copilot/              # Copilot 문서
├── .git/                  # Git 저장소
├── .idea/                 # CLion 설정
├── cmake-build-debug/     # 빌드 출력
├── CMakeLists.txt         # 빌드 설정
├── include/               # 헤더 파일 (18개)
├── src/                   # 소스 파일 (25개)
├── main.exe              # 빌드 산출물 (루트에 위치 - 부적절)
└── unifont.ttf           # 폰트 리소스

문제점:
❌ 빌드 산출물이 프로젝트 루트에 위치
❌ 테스트 디렉토리 없음
❌ 문서 디렉토리 체계 없음
❌ 리소스 파일 (폰트) 루트에 위치
❌ 예제/샘플 프로젝트 없음
❌ 서드파티 라이브러리 관리 체계 없음
❌ 스크립트/도구 디렉토리 없음
```

---

## 리팩토링 규칙 (Google C++ Style Guide)

### R1. 네이밍 규칙 (Naming Conventions)

#### R1.1 파일명
```
✓ application.h / application.cc
✗ Application.h / Application.cpp

규칙:
- 모두 소문자
- 단어 구분: 언더스코어 (_)
- 헤더: .h
- 소스: .cc (Google 스타일) 또는 .cpp (허용)
```

#### R1.2 클래스/구조체명
```cpp
✓ class WiringManager { };
✓ struct PlacedComponent { };
✗ class wiringManager { };
✗ struct placed_component { };

규칙:
- PascalCase (각 단어 첫 글자 대문자)
- 명사 사용
- 약어 최소화
```

#### R1.3 변수명
```cpp
// 일반 변수
✓ int component_count = 0;
✗ int componentCount = 0;
✗ int ComponentCount = 0;

// 멤버 변수
✓ class Application {
    GLFWwindow* window_;          // trailing underscore
    Mode current_mode_;
    int selected_component_id_;
  };
✗ class Application {
    GLFWwindow* m_window;         // Hungarian notation
    Mode mCurrentMode;
    int _selectedId;              // leading underscore
  };

규칙:
- 모두 소문자
- 단어 구분: 언더스코어
- 멤버 변수: trailing underscore (_)
- 전역/정적 변수: g_ 접두사
```

#### R1.4 함수명
```cpp
✓ void InitializeWindow();      // 공개 API
✓ bool IsValid() const;
✓ void set_mode(Mode mode);     // setter
✓ Mode mode() const;            // getter

✗ void initialize_window();     // 공개 함수는 PascalCase
✗ void INITIALIZE();

규칙:
- 공개 함수: PascalCase
- 접근자: snake_case
- 동사로 시작
```

#### R1.5 상수명
```cpp
✓ constexpr int kWindowWidth = 1440;
✓ const float kDefaultZoom = 1.0f;
✗ const int WINDOW_WIDTH = 1440;
✗ const float DefaultZoom = 1.0f;

규칙:
- k 접두사 + PascalCase
- constexpr 우선 사용
```

#### R1.6 열거형
```cpp
✓ enum class Mode {
    kWiring,
    kProgramming
  };
  
✓ enum class ComponentType {
    kPLC,
    kFRL,
    kManifold
  };

✗ enum Mode { WIRING, PROGRAMMING };  // old-style enum
✗ enum class Mode { Wiring, Programming };  // no k prefix

규칙:
- enum class 사용 (타입 안전성)
- 열거자: k 접두사 + PascalCase
```

#### R1.7 네임스페이스
```cpp
✓ namespace plc {
    namespace physics {
      class Engine { };
    }
  }

✗ namespace PLC { }
✗ namespace plc_simulator { }  // 너무 길면 약어 사용

규칙:
- 모두 소문자
- 짧고 명확하게
- 중첩 가능
```

---

### R2. 파일 구조 (File Organization)

#### R2.1 헤더 가드
```cpp
// application.h
#ifndef PLC_EMULATOR_INCLUDE_APPLICATION_H_
#define PLC_EMULATOR_INCLUDE_APPLICATION_H_

// ... 내용 ...

#endif  // PLC_EMULATOR_INCLUDE_APPLICATION_H_

규칙:
- #pragma once 사용 금지
- 형식: PROJECT_PATH_FILE_H_
- 마지막 주석으로 닫기
```

#### R2.2 Include 순서
```cpp
// application.cc 파일의 include 순서:

#include "application.h"          // 1. 대응 헤더 (자기 자신)

#include <stdio.h>                // 2. C 시스템 헤더
#include <stdlib.h>

#include <iostream>               // 3. C++ 시스템 헤더
#include <memory>
#include <string>
#include <vector>

#include <glad/glad.h>            // 4. 외부 라이브러리
#include <GLFW/glfw3.h>
#include "imgui.h"

#include "data_types.h"           // 5. 프로젝트 내부 헤더
#include "physics_engine.h"
#include "programming_mode.h"

규칙:
- 각 그룹 사이 빈 줄
- 각 그룹 내에서 알파벳 순
- 조건부 include 최소화
```

#### R2.3 헤더 파일 자급자족 (Self-contained)
```cpp
// data_types.h는 단독 컴파일 가능해야 함

#ifndef PLC_EMULATOR_INCLUDE_DATA_TYPES_H_
#define PLC_EMULATOR_INCLUDE_DATA_TYPES_H_

#include <string>  // std::string 사용 위해 필요
#include <vector>  // std::vector 사용 위해 필요

namespace plc {

struct Position {
  float x;
  float y;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_DATA_TYPES_H_

규칙:
- 모든 헤더는 단독 컴파일 가능
- 필요한 include 모두 명시
- 전방 선언 최대한 활용
```

---

### R3. 클래스 설계 (Class Design)

#### R3.1 클래스 선언 순서
```cpp
class Application {
 public:
  // 1. 타입 정의 (using, typedef, enum)
  using ComponentList = std::vector<PlacedComponent>;
  
  // 2. 정적 상수
  static constexpr int kDefaultWidth = 1440;
  
  // 3. 생성자/소멸자
  Application();
  ~Application();
  
  // 4. 복사/이동 (필요시)
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  
  // 5. 공개 메서드
  bool Initialize();
  void Run();
  void Shutdown();
  
  // 6. 접근자 (getter/setter)
  Mode mode() const { return current_mode_; }
  void set_mode(Mode mode) { current_mode_ = mode; }
  
 protected:
  // 7. Protected 메서드 (상속용)
  virtual void OnModeChanged();
  
 private:
  // 8. Private 메서드
  bool InitializeWindow();
  void ProcessInput();
  void Update();
  void Render();
  
  // 9. Private 데이터 멤버 (항상 마지막!)
  GLFWwindow* window_;
  Mode current_mode_;
  bool is_running_;
  
  std::vector<PlacedComponent> placed_components_;
  std::unique_ptr<ProgrammingMode> programming_mode_;
};

규칙:
- public → protected → private 순서
- 각 섹션 내에서도 위 순서 유지
- 데이터 멤버는 항상 마지막
- 빈 줄로 그룹 구분
```

#### R3.2 생성자 초기화 리스트
```cpp
✓ Application::Application()
    : window_(nullptr),
      current_mode_(Mode::kWiring),
      is_running_(false),
      selected_component_id_(-1),
      next_instance_id_(0),
      programming_mode_(std::make_unique<ProgrammingMode>(this)) {
  // 생성자 본문
}

✗ Application::Application() {
    window_ = nullptr;
    current_mode_ = Mode::kWiring;
    is_running_ = false;
  }

규칙:
- 초기화 리스트 우선 사용
- 선언 순서대로 초기화
- 한 줄에 하나씩
- 들여쓰기 4칸
```

#### R3.3 상수 멤버 함수
```cpp
✓ class Component {
   public:
    Position GetPosition() const;
    bool IsSelected() const;
    
   private:
    bool ValidateState() const;  // private도 const 가능
  };

규칙:
- 상태 변경 없는 함수는 const
- getter는 항상 const
- const 키워드는 함수 끝에
```

#### R3.4 인라인 함수
```cpp
// 헤더 파일
class Component {
 public:
  // 짧은 함수만 인라인
  int id() const { return id_; }
  bool is_selected() const { return selected_; }
  
  // 긴 함수는 .cc 파일에
  void UpdatePhysics(float delta_time);
  
 private:
  int id_;
  bool selected_;
};

규칙:
- 10줄 이하만 헤더에 인라인
- getter/setter는 인라인 허용
- 복잡한 로직은 .cc 파일
```

---

### R4. 함수 설계 (Function Design)

#### R4.1 함수 길이
```
규칙:
- 최대 50줄 (화면 1페이지)
- 가독성을 위해 더 짧을수록 좋음
- 복잡한 함수는 하위 함수로 분할
```

#### R4.2 매개변수
```cpp
✓ void ProcessComponent(const Component& comp);
✓ bool CreateWire(int from_id, int to_id, const WireConfig& config);

✗ void ProcessComponent(Component comp);  // 불필요한 복사
✗ bool CreateWire(int from_id, int to_id, 
                  float thickness, Color color, 
                  bool electric, float voltage,
                  int waypoint_count);  // 매개변수 너무 많음

규칙:
- 매개변수 4개 이하
- 읽기 전용: const 참조
- 수정 필요: 비const 참조 또는 포인터
- 많으면 구조체로 그룹화
```

#### R4.3 반환값
```cpp
✓ std::optional<Component> FindComponent(int id);
✓ std::pair<bool, std::string> ValidateConfiguration();

struct FindResult {
  bool found;
  Component component;
  std::string error_message;
};
✓ FindResult FindComponentAdvanced(int id);

✗ bool FindComponent(int id, Component* out_comp);  // 출력 매개변수
✗ Component* FindComponent(int id);  // nullptr 가능성

규칙:
- 값 반환 우선 (RVO 최적화)
- 실패 가능: std::optional
- 여러 값: std::pair, std::tuple, 구조체
- 출력 매개변수 지양
```

#### R4.4 함수 오버로딩
```cpp
✓ class Canvas {
   public:
    void DrawLine(const Point& start, const Point& end);
    void DrawLine(float x1, float y1, float x2, float y2);
  };

✗ class Canvas {
   public:
    void DrawLine(const Point& start, const Point& end);
    void DrawLineCoords(float x1, float y1, float x2, float y2);  // 불필요한 이름 변형
  };

규칙:
- 같은 기능은 오버로딩
- 매개변수 타입으로 구분
- 기본 매개변수 활용
```

---

### R5. 포매팅 (Formatting)

#### R5.1 들여쓰기
```
규칙:
- 2칸 (탭 아님)
- 중괄호 안쪽 +2칸
- 연속 줄 +4칸
```

#### R5.2 줄 길이
```
규칙:
- 최대 80자 (권장)
- 절대 최대 100자
- 주석 포함
- 예외: #include 경로
```

#### R5.3 중괄호
```cpp
// 함수: 새 줄
void FunctionName() {
  // 내용
}

// 클래스/구조체: 새 줄
class ClassName {
 public:
  // 내용
};

// 제어문: 같은 줄
if (condition) {
  // 내용
} else {
  // 내용
}

// 짧은 함수: 한 줄 허용
void set_value(int v) { value_ = v; }

규칙:
- 일관성 유지
- 빈 블록: {}
- 한 줄 블록도 중괄호 사용 권장
```

#### R5.4 공백
```cpp
// 이항 연산자 양쪽
int sum = a + b;
bool result = (x == y) && (z != w);

// 단항 연산자 붙여쓰기
i++;
!flag;
*ptr;

// 함수 호출: 괄호 앞 공백 없음
FunctionName(arg1, arg2);

// 제어문: 괄호 앞 공백
if (condition) {
for (int i = 0; i < n; ++i) {
while (running) {

// 쉼표 뒤 공백
Function(arg1, arg2, arg3);

// 포인터/참조: * 와 & 는 타입에 붙임
int* ptr;    // ✓
int *ptr;    // ✗
int* a, *b;  // ✓ (단, 한 줄에 여러 포인터 선언 지양)
```

#### R5.5 빈 줄
```cpp
// 파일 시작: 라이선스/헤더 뒤 빈 줄

// 함수 사이: 빈 줄 1개
void Function1() {
  // ...
}

void Function2() {
  // ...
}

// 논리적 블록 구분: 빈 줄
void ComplexFunction() {
  // 초기화
  int x = 0;
  int y = 0;
  
  // 계산
  x = Calculate1();
  y = Calculate2();
  
  // 결과 처리
  ProcessResult(x, y);
}

규칙:
- 빈 줄 2개 이상 금지
- 파일 끝 빈 줄 1개
- 중괄호 직후/직전 빈 줄 불필요
```

---

### R6. 주석 (Comments)

#### R6.1 파일 주석
```cpp
// application.h
// Copyright 2024 PLC Emulator Project
//
// Main application class for PLC simulator.
// Manages window, rendering, and mode switching.

#ifndef PLC_EMULATOR_INCLUDE_APPLICATION_H_
#define PLC_EMULATOR_INCLUDE_APPLICATION_H_

규칙:
- 파일 시작에 저작권/라이선스
- 파일 목적 간단 설명
- 작성자/날짜는 버전 관리 도구 활용
```

#### R6.2 클래스 주석
```cpp
// Manages the main application lifecycle and coordinates
// between wiring mode and programming mode.
//
// This class is the central coordinator for:
// - Window management (GLFW)
// - Rendering (ImGui + OpenGL)
// - Mode switching (Wiring ↔ Programming)
// - Physics engine integration
//
// Example:
//   Application app;
//   if (app.Initialize()) {
//     app.Run();
//     app.Shutdown();
//   }
class Application {
  // ...
};

규칙:
- 클래스 목적 설명
- 주요 책임 나열
- 사용 예시 (필요시)
- 간결하게 (5-10줄)
```

#### R6.3 함수 주석
```cpp
// Initializes the application window and graphics context.
//
// Creates a GLFW window with OpenGL 3.3 context and initializes
// ImGui for UI rendering. Must be called before Run().
//
// Returns:
//   true if initialization successful, false otherwise.
//
// Example:
//   if (!app.Initialize()) {
//     std::cerr << "Failed to initialize\n";
//     return -1;
//   }
bool Initialize();

// 간단한 함수는 한 줄로
// Returns the current simulation mode.
Mode mode() const;

규칙:
- 공개 API만 상세 주석
- 자명한 함수는 주석 생략
- Doxygen 스타일 선택적
- 매개변수/반환값 설명
```

#### R6.4 구현 주석
```cpp
void Application::UpdatePhysics() {
  // Early exit if physics disabled
  if (!physics_enabled_) {
    return;
  }
  
  // Update electrical network simulation
  // Using modified nodal analysis for circuit solving
  physics_engine_->SolveElectricalSystem(delta_time_);
  
  // Sync results back to components
  for (auto& component : placed_components_) {
    SyncPhysicsToComponent(component);
  }
  
  // TODO(username): Optimize for large networks (>100 components)
  // FIXME: Voltage calculation unstable at high frequencies
}

규칙:
- // 사용 (/* */ 지양)
- 왜(Why)를 설명, 무엇(What)은 코드로
- TODO/FIXME/HACK 등 태그 활용
- 영어 사용 권장 (팀 정책 따름)
```

#### R6.5 주석 금지 사항
```cpp
✗ // 변수 i를 선언한다
  int i = 0;

✗ // 루프를 돈다
  for (int i = 0; i < n; ++i) {

✗ // 함수를 호출한다
  ProcessData();

규칙:
- 코드 그대로 읽는 주석 금지
- 자명한 내용 설명 금지
- 주석으로 나쁜 코드 감추기 금지
```

---

### R7. 타입 및 캐스팅 (Types & Casting)

#### R7.1 정수 타입
```cpp
✓ int count = 0;              // 일반 정수
✓ int32_t precise_int = 0;    // 크기 보장 필요시
✓ size_t index = 0;           // 컨테이너 인덱스
✓ uint8_t byte = 0xFF;        // 바이트 데이터

✗ short s = 0;                // 애매한 크기
✗ long l = 0;                 // 플랫폼 의존적

규칙:
- int를 기본으로
- 크기 중요하면 <cstdint>
- 부호 없는 정수 신중히 사용
```

#### R7.2 부동소수점
```cpp
✓ float position = 0.0f;      // 단정도 (그래픽스)
✓ double precise = 0.0;       // 배정도 (과학 계산)

규칙:
- float 리터럴: 0.0f
- double 리터럴: 0.0
- 정밀도 필요에 따라 선택
```

#### R7.3 auto 키워드
```cpp
✓ auto window = CreateWindow();  // 타입이 명확
✓ auto it = map.begin();         // 반복자
✓ auto lambda = [](int x) { return x * 2; };

✗ auto x = GetValue();           // 반환 타입 불명확
✗ auto v = {1, 2, 3};            // std::initializer_list<int> (의도 불명)

규칙:
- 타입 명확할 때만 사용
- 반복자, 람다에 적극 활용
- 가독성 해치면 명시적 타입
```

#### R7.4 캐스팅
```cpp
✓ auto* ptr = static_cast<Derived*>(base_ptr);
✓ const auto& comp = const_cast<const Component&>(mutable_comp);
✓ auto result = reinterpret_cast<uint64_t>(ptr);  // 최후 수단

✗ Derived* ptr = (Derived*)base_ptr;  // C-style cast

규칙:
- C++ 스타일 캐스트만 사용
- static_cast: 타입 변환
- const_cast: const 추가/제거
- reinterpret_cast: 위험, 최소화
- dynamic_cast: RTTI 필요시
```

#### R7.5 타입 별칭
```cpp
✓ using ComponentList = std::vector<PlacedComponent>;
✓ using WireMap = std::map<int, Wire>;

✗ typedef std::vector<PlacedComponent> ComponentList;

규칙:
- using 사용 (C++11+)
- typedef 지양
- 의미 있는 이름
```

---

### R8. 메모리 관리 (Memory Management)

#### R8.1 스마트 포인터
```cpp
✓ class Application {
   private:
    std::unique_ptr<ProgrammingMode> programming_mode_;
    std::shared_ptr<PhysicsEngine> physics_engine_;
  };

✗ class Application {
   private:
    ProgrammingMode* programming_mode_;  // 수동 관리
  };

규칙:
- 동적 할당: 스마트 포인터 필수
- unique_ptr 기본
- shared_ptr 필요시만
- weak_ptr 순환 참조 방지
```

#### R8.2 원시 포인터
```cpp
✓ void ProcessComponent(const Component* comp) {
    if (comp == nullptr) return;
    // comp는 소유권 없음, 읽기만
  }

규칙:
- 비소유 참조만 사용
- nullptr 체크 필수
- 소유권은 스마트 포인터
```

#### R8.3 참조 vs 포인터
```cpp
// 항상 유효: 참조
✓ void Process(const Component& comp);

// nullptr 가능: 포인터
✓ void Process(const Component* comp);

규칙:
- 항상 유효: 참조
- 선택적: 포인터
- 출력: 비const 참조 또는 포인터
```

#### R8.4 RAII 패턴
```cpp
✓ class ResourceManager {
   public:
    ResourceManager() {
      resource_ = AcquireResource();
    }
    ~ResourceManager() {
      ReleaseResource(resource_);
    }
   private:
    Resource* resource_;
  };

규칙:
- 생성자에서 획득
- 소멸자에서 해제
- 예외 안전성 보장
```

---

### R9. 현대적 C++ 기능 (Modern C++)

#### R9.1 범위 기반 for 루프
```cpp
✓ for (const auto& component : placed_components_) {
    ProcessComponent(component);
  }

✗ for (size_t i = 0; i < placed_components_.size(); ++i) {
    ProcessComponent(placed_components_[i]);
  }

규칙:
- 컨테이너 전체 순회시 사용
- const auto& 로 불필요한 복사 방지
- 인덱스 필요시 전통적 for 사용
```

#### R9.2 람다 표현식
```cpp
✓ std::sort(components.begin(), components.end(),
           [](const auto& a, const auto& b) {
             return a.position.x < b.position.x;
           });

규칙:
- 짧은 콜백에 적극 활용
- 캡처 최소화 [&], [=] 지양
- 명시적 캡처 [ptr, value]
```

#### R9.3 std::optional
```cpp
✓ std::optional<Component> FindComponent(int id) {
    auto it = components_.find(id);
    if (it != components_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

// 사용
if (auto comp = FindComponent(id)) {
  ProcessComponent(*comp);
}

규칙:
- 값 없을 수 있는 경우
- nullptr 대신 안전
```

#### R9.4 구조화된 바인딩
```cpp
✓ for (const auto& [id, component] : component_map_) {
    std::cout << "ID: " << id << "\n";
  }

✓ auto [success, error] = ValidateConfig();

규칙:
- pair, tuple 반환시 유용
- 가독성 향상
- C++17 필요
```

---

### R10. 에러 처리 (Error Handling)

#### R10.1 예외 vs 에러 코드
```cpp
// 복구 가능 에러: std::optional
✓ std::optional<Config> LoadConfig(const std::string& path);

// 프로그램 에러: 예외
✓ void Initialize() {
    if (!CreateWindow()) {
      throw std::runtime_error("Failed to create window");
    }
  }

// 성능 critical: 에러 코드
✓ ErrorCode UpdatePhysics(float dt);

규칙:
- 예외: 복구 불가능한 에러
- optional: 정상 범위 내 실패
- 에러 코드: 성능 critical 경로
```

#### R10.2 예외 클래스
```cpp
✓ class PLCException : public std::runtime_error {
   public:
    explicit PLCException(const std::string& msg)
        : std::runtime_error(msg) {}
  };

class InitializationException : public PLCException {
  // ...
};

규칙:
- std 예외 상속
- 계층 구조 설계
- 상세 정보 포함
```

#### R10.3 noexcept
```cpp
✓ void Cleanup() noexcept {
    // 절대 예외 던지지 않음
  }

✓ ~Application() noexcept {
    // 소멸자는 기본 noexcept
  }

규칙:
- 예외 안 던지는 함수 명시
- 이동 연산 noexcept
- 소멸자 항상 noexcept
```

---

### R11. STL 사용 (Standard Library)

#### R11.1 컨테이너 선택
```cpp
// 순차 접근: vector
✓ std::vector<Component> components;

// 빠른 조회: unordered_map
✓ std::unordered_map<int, Component> component_map;

// 정렬 필요: map
✓ std::map<std::string, Value> sorted_map;

// FIFO: queue
✓ std::queue<Event> event_queue;

규칙:
- vector 기본
- map/set은 정렬 필요시
- unordered_* 는 해시 기반 빠름
```

#### R11.2 알고리즘
```cpp
✓ #include <algorithm>

// 찾기
auto it = std::find_if(vec.begin(), vec.end(), predicate);

// 정렬
std::sort(vec.begin(), vec.end());

// 변환
std::transform(in.begin(), in.end(), out.begin(), func);

규칙:
- STL 알고리즘 우선
- 수동 루프는 최후 수단
- 가독성과 성능 모두 향상
```

#### R11.3 문자열
```cpp
✓ std::string name = "Component";
✓ std::string_view view = name;  // 복사 없는 뷰

// 문자열 연결
✓ std::string full = name + "_" + std::to_string(id);

// 포맷팅 (C++20)
✓ std::string msg = std::format("ID: {}", id);

규칙:
- std::string 기본
- string_view 읽기 전용
- C 문자열 지양
```

---

## 리팩토링 우선순위 및 단계

### Phase 0: 디렉토리 구조 재구성 ⭐ NEW
**목표**: 표준 C++ 프로젝트 구조로 전환

상세 내용은 `DIRECTORY_STRUCTURE.md` 참조

---

### Phase 1: 기본 포매팅
**목표**: 코드 포맷 통일, 빌드 깨지지 않도록

1. [ ] clang-format 설정 파일 생성
2. [ ] 전체 코드베이스 자동 포매팅
3. [ ] 빌드 및 기본 테스트 확인
4. [ ] Git commit: "style: Apply clang-format"

**영향**: 모든 파일 (비기능적 변경)

---

### Phase 2: 헤더 가드 및 Include
**목표**: 헤더 파일 구조 개선

1. [ ] 모든 `#pragma once` → 전통적 헤더 가드
2. [ ] Include 순서 표준화
3. [ ] 불필요한 include 제거
4. [ ] 전방 선언 추가로 의존성 감소
5. [ ] Git commit: "refactor: Standardize header guards and includes"

**영향**: 모든 .h 파일

---

### Phase 3: 네이밍 규칙
**목표**: Google 스타일 네이밍 적용

#### Phase 3.1: 멤버 변수
```
m_window → window_
m_currentMode → current_mode_
m_isRunning → is_running_
```

1. [ ] 모든 클래스 멤버 변수 이름 변경
2. [ ] 빌드 확인 및 테스트
3. [ ] Git commit: "refactor: Rename member variables to trailing underscore"

**영향**: 모든 클래스 (.h/.cpp)

#### Phase 3.2: 상수
```
WINDOW_WIDTH → kWindowWidth
DEFAULT_ZOOM → kDefaultZoom
```

1. [ ] 모든 상수 k 접두사 추가
2. [ ] Git commit: "refactor: Add k prefix to constants"

**영향**: 상수 정의 파일들

#### Phase 3.3: 함수 (선택적)
```
현재 대부분 PascalCase (유지 가능)
내부 함수만 snake_case 고려
```

---

### Phase 4: 클래스 구조 재정렬
**목표**: 클래스 선언 순서 표준화

1. [ ] 각 클래스 섹션 순서 재정렬
   - public → protected → private
   - 타입/생성자/메서드/데이터 순
2. [ ] 데이터 멤버를 private 끝으로 이동
3. [ ] Git commit: "refactor: Reorganize class member order"

**영향**: 모든 클래스 선언

---

### Phase 5: Application 클래스 분할
**목표**: 거대한 Application 클래스를 기능별로 분리

#### 새로운 클래스 구조:
```
Application (코어)
├── WindowManager (창 관리)
├── RenderManager (렌더링)
├── InputHandler (입력 처리)
├── WiringManager (배선 모드)
├── ComponentManager (컴포넌트 관리)
└── SimulationController (시뮬레이션 제어)
```

1. [ ] WindowManager 추출
2. [ ] RenderManager 추출
3. [ ] InputHandler 추출
4. [ ] WiringManager 추출
5. [ ] ComponentManager 추출
6. [ ] Application을 컨트롤러로 축소
7. [ ] Git commits: 각 매니저별

**영향**: Application.h/cpp 전체 재구성

---

### Phase 6: 함수 리팩토링
**목표**: 긴 함수 분할, 매개변수 정리

1. [ ] 50줄 이상 함수 분할
2. [ ] 매개변수 4개 이상 함수 구조체화
3. [ ] 출력 매개변수 제거 (반환값 사용)
4. [ ] Git commit: "refactor: Split long functions and reduce parameters"

**영향**: 복잡한 함수들 (Update, Render 등)

---

### Phase 7: 타입 현대화
**목표**: 현대적 C++ 타입 사용

1. [ ] `typedef struct` → `struct`
2. [ ] `enum` → `enum class`
3. [ ] 원시 포인터 → 스마트 포인터 (가능한 곳)
4. [ ] Git commits: 타입별

**영향**: DataTypes.h, 각종 구조체

---

### Phase 8: STL 현대화
**목표**: 현대적 C++ 기능 활용

1. [ ] 전통적 for → 범위 기반 for
2. [ ] 함수 포인터 → 람다 (적절한 곳)
3. [ ] nullptr 가능 → std::optional
4. [ ] Git commit: "refactor: Use modern C++ features"

**영향**: 전체 코드베이스

---

### Phase 9: 주석 정리
**목표**: 불필요한 주석 제거, 필요한 주석 추가

1. [ ] 자명한 주석 제거
2. [ ] 공개 API 주석 추가/개선
3. [ ] 파일 헤더 주석 표준화
4. [ ] Git commit: "docs: Clean up and standardize comments"

**영향**: 전체 파일

---

### Phase 10: 테스트 추가
**목표**: 단위 테스트 프레임워크 도입

1. [ ] Google Test 통합
2. [ ] 핵심 클래스 단위 테스트 작성
3. [ ] CI/CD 통합
4. [ ] Git commits: 테스트별

**영향**: 새 test/ 디렉토리

---

## 리팩토링 체크리스트

### 파일 레벨
- [ ] 헤더 가드 형식: `PROJECT_PATH_FILE_H_`
- [ ] Include 순서: 자신 → C → C++ → 외부 → 내부
- [ ] 파일 끝 빈 줄 1개
- [ ] 줄 길이 80자 이하 (최대 100자)
- [ ] 들여쓰기 2칸 (공백)

### 클래스 레벨
- [ ] 섹션 순서: public → protected → private
- [ ] 데이터 멤버 마지막 (private)
- [ ] 복사/이동 의미론 명시 (Rule of Five or Zero)
- [ ] 생성자 초기화 리스트 사용
- [ ] const 멤버 함수 적절히 사용

### 함수 레벨
- [ ] 함수 길이 50줄 이하
- [ ] 매개변수 4개 이하
- [ ] const 참조로 전달 (읽기 전용)
- [ ] 출력 매개변수 지양 (반환값 사용)
- [ ] noexcept 적절히 사용

### 네이밍 레벨
- [ ] 클래스/구조체: PascalCase
- [ ] 함수: PascalCase (public), snake_case (private 선택적)
- [ ] 변수: snake_case
- [ ] 멤버 변수: trailing underscore (_)
- [ ] 상수: k + PascalCase
- [ ] 열거자: k + PascalCase

### 타입 레벨
- [ ] enum → enum class
- [ ] typedef → using
- [ ] 원시 포인터 → 스마트 포인터 (소유권)
- [ ] nullptr 가능 → std::optional

### STL 레벨
- [ ] 벡터 기본 사용
- [ ] 알고리즘 라이브러리 활용
- [ ] 범위 기반 for 루프
- [ ] 람다 표현식 활용

---

## 도구 설정

### clang-format 설정
`.clang-format` 파일 생성:
```yaml
---
Language: Cpp
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 80
AccessModifierOffset: -1
AlignAfterOpenBracket: Align
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
BreakBeforeBraces: Attach
DerivePointerAlignment: false
PointerAlignment: Left
SortIncludes: true
```

### CMake 통합
```cmake
# .clang-format 파일을 빌드 디렉토리에 복사
configure_file(.clang-format ${CMAKE_BINARY_DIR}/.clang-format COPYONLY)

# 포맷 체크 타겟
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOURCE_FILES}
    COMMENT "Running clang-format"
  )
endif()
```

---

## 참고 자료

1. **Google C++ Style Guide** (공식)
   - https://google.github.io/styleguide/cppguide.html

2. **C++ Core Guidelines** (참고)
   - https://isocpp.github.io/CppCoreGuidelines/

3. **clang-format 문서**
   - https://clang.llvm.org/docs/ClangFormat.html

4. **Modern C++ 가이드**
   - https://github.com/isocpp/CppCoreGuidelines

---

## 주의사항

1. **점진적 리팩토링**: 한 번에 모든 것을 바꾸지 말 것
2. **빌드 유지**: 각 단계마다 빌드 성공 확인
3. **Git 활용**: 작은 단위로 커밋, 되돌리기 쉽게
4. **테스트**: 기능 변경 없음을 확인
5. **코드 리뷰**: 팀원과 리뷰 필수
6. **문서 업데이트**: README 등 문서도 함께 업데이트

---

## 완료 기준

이 리팩토링이 완료되면:
- ✅ 모든 코드가 Google C++ Style Guide 준수
- ✅ clang-format으로 자동 포매팅 가능
- ✅ 일관된 네이밍과 구조
- ✅ 유지보수 용이한 코드베이스
- ✅ 단위 테스트 커버리지 확보
- ✅ 빌드 경고 0개

---

*이 문서는 리팩토링 진행 중 지속적으로 업데이트됩니다.*
