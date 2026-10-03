# PLC Emulator - C++ 코딩 가이드라인 (Google C++ Style Guide 기반)

이 문서는 `PLC_Simulator` (PLC Emulator) 프로젝트의 유지보수 및 확장에 참여하는 모든 개발자와 에이전트가 준수해야 하는 C++20 코딩 표준 및 규칙입니다.
프로젝트 루트의 `.clang-format` 설정(`BasedOnStyle: Google`)과 완벽히 연동됩니다.

---

## 1. 네이밍 규칙 (Naming Conventions)

| 대상 | 표기법 | 예시 | 비고 |
| :--- | :--- | :--- | :--- |
| **클래스 / 구조체 / 타입** | `PascalCase` | `PhysicsEngine`, `ComponentRegistry` | 명사 또는 명사구 |
| **함수 / 메서드** | `PascalCase` | `Initialize()`, `CalculatePressure()` | 동사 또는 동사구 |
| **단순 접근자/설정자** | `snake_case` | `size()`, `set_size()` | 프로퍼티 스타일 허용 |
| **일반 변수 / 함수 매개변수** | `snake_case` | `wire_index`, `target_port` | 명확한 의미, 축약어 지양 |
| **클래스 멤버 변수** | `trailing_underscore_` | `is_running_`, `max_connections_` | 구조체(POD)는 밑줄 생략 가능 |
| **상수 (const / constexpr)**| `kPascalCase` | `kMaxBufferSize`, `kDefaultPressure` | 파일 스코프 및 클래스 정적 상수 |
| **열거형(enum) 타입** | `PascalCase` | `WireColor`, `ComponentType` | `enum class` 우선 사용 |
| **열거형 항목 (Enumerator)** | `kPascalCase` | `kInputPort`, `kNormallyOpen` | `k` 접두사 사용 |
| **매크로** | `ALL_CAPS_WITH_UNDERSCORE` | `PLC_ASSERT`, `DISABLE_COPY` | 가급적 `constexpr`/`inline`으로 대체 |

---

## 2. 헤더 파일 및 Include 규칙

### 2.1 헤더 가드
- 모든 헤더 파일은 `#pragma once`를 기본으로 사용합니다.

### 2.2 Include 순서
포괄적인 헤더부터 일반적인 헤더 순으로 그룹을 나누고 빈 줄로 구분합니다 (`.clang-format`의 정렬 규칙 준수).
1. 매칭되는 해당 소스의 헤더 (예: `physics_engine.cpp` -> `plc_emulator/physics/physics_engine.h`)
2. C 시스템 헤더 (`<sys/types.h>`, `<unistd.h>` 등)
3. C++ 표준 라이브러리 헤더 (`<vector>`, `<memory>`, `<string>`, `<algorithm>` 등)
4. 서드파티 라이브러리 헤더 (`<imgui.h>`, `<GLFW/glfw3.h>`, `<box2d/box2d.h>` 등)
5. 프로젝트 내부 헤더 (`"plc_emulator/core/..."`, `"plc_emulator/components/..."` 등)

```cpp
#include "plc_emulator/physics/physics_engine.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <box2d/box2d.h>
#include <imgui.h>

#include "plc_emulator/components/component_registry.h"
#include "plc_emulator/core/application.h"
```

### 2.3 헤더 의존성 최소화
- 헤더 파일 내 불필요한 `#include`는 지양하고, 포인터나 참조로만 사용되는 클래스는 전방 선언(Forward Declaration)을 활용합니다.
- 순환 참조(Circular Dependency)를 철저히 방지합니다.

---

## 3. 네임스페이스 (Namespaces)

- 모든 프로젝트 코드는 `namespace plc_emulator` 내에 정의합니다.
- 서브시스템 단위로 하위 네임스페이스를 구분합니다 (예: `plc_emulator::physics`, `plc_emulator::programming`, `plc_emulator::rtl`).
- **헤더 파일에서 `using namespace ...;` 사용은 전면 금지**됩니다.
- `.cpp` 파일 내부 전역 함수나 헬퍼 상수는 **익명 네임스페이스(`namespace { ... }`)** 내에 선언하여 외부 심볼 충돌을 방지합니다.

```cpp
namespace plc_emulator {
namespace {

constexpr float kInternalTolerance = 0.001f;

void HelperFunction() {
  // 내부에서만 쓰이는 헬퍼 로직
}

}  // namespace

// 공개 구현 코드...

}  // namespace plc_emulator
```

---

## 4. 클래스 및 객체 지향 설계

### 4.1 접근 지정자 순서
- 클래스 선언 시 `public:` -> `protected:` -> `private:` 순서로 작성합니다.
- 들여쓰기는 본문 대비 1칸 내어쓰기 (`AccessModifierOffset: -1`).

### 4.2 생성자 및 소멸자
- 단일 인자를 받는 생성자는 암시적 형변환을 방지하기 위해 **반드시 `explicit`**으로 선언합니다.
- 상속 대상 클래스 또는 인터페이스는 반드시 가상 소멸자(`virtual ~BaseClass() = default;`)를 정의합니다.
- 불필요한 복사/대입을 막아야 하는 싱글턴, 매니저, 엔진 객체는 `= delete;`를 명시합니다.

### 4.3 `override` 명시
- 부모 클래스의 가상 함수를 오버라이드할 때는 항상 `override` 키워드를 명시하고, 중복되는 `virtual`은 생략합니다.

---

## 5. 모던 C++ 및 메모리 관리 (C++20)

### 5.1 스마트 포인터 & 소유권 원칙
- Raw `new` 및 `delete` 호출을 금지합니다.
- **소유권 독점**: `std::unique_ptr` 및 `std::make_unique` 사용.
- **소유권 공유**: `std::shared_ptr` 및 `std::make_shared` 사용.
- **비소유 참조(Non-owning View)**: 생명주기가 상위에서 보장되는 경우 Raw Pointer(`T*`) 또는 `const T&`로 전달하여 불필요한 레퍼런스 카운팅 오버헤드를 방지합니다.

### 5.2 `const` 및 불변성(Immutability)
- 변경되지 않는 모든 변수, 매개변수, 멤버 함수는 `const`로 선언합니다.
- 컴파일 타임 상수는 `constexpr` 또는 `consteval`을 적극 활용합니다.
- 함수 인자로 복합 객체를 전달할 때는 복사 비용을 방지하기 위해 `const T&`를 기본으로 사용합니다.

### 5.3 `auto` 키워드 사용
- 우변에 생성식이나 명시적 캐스팅이 있어 타입이 한눈에 드러나는 경우(예: `auto ptr = std::make_unique<Component>();`), 복잡한 반복자 타입 등에 사용합니다.
- 기본 수치형(`int`, `float`, `double`)이나 반환 타입을 직관적으로 알기 어려운 일반 함수 호출에는 명시적 타입을 작성합니다.

### 5.4 Null 포인터
- `NULL`이나 `0` 대신 항상 `nullptr`를 사용합니다.

---

## 6. 함수 및 파라미터 규칙

### 6.1 입출력 파라미터 규칙
- **입력 전용**: 기본형은 값 복사(`T`), 객체는 `const T&`.
- **출력/수정(In-Out)**: 값을 수정하거나 반환해야 하는 파라미터는 포인터(`T*`)를 사용합니다.
  - 호출부에서 `UpdateState(&state)` 형태로 주소 연산자(`&`)가 노출되므로, 인자가 수정됨을 직관적으로 확인할 수 있습니다.
- 값이 없을 수 있는 반환값은 `std::optional<T>`를 우선 활용합니다.

### 6.2 단일 책임 및 함수 크기
- 각 함수는 한 가지 책임에 집중하며, 50줄 이내의 간결한 작성을 권장합니다.

---

## 7. 코드 서식 (Formatting)

- **들여쓰기**: 스페이스 **2칸** (탭 사용 금지).
- **최대 줄 길이**: 80자 기준 (최대 가독성 확보).
- **중괄호**: 구문과 같은 줄에 시작 (`Attach` / K&R 스타일).
- **포인터/참조 기호 정렬**: 타입 쪽에 부착 (`T* ptr`, `const std::string& ref`).

```cpp
if (condition) {
  DoSomething();
} else {
  DoOtherThing();
}
```

---

## 8. 실시간 시뮬레이션 성능 및 안전 규칙

- 크래시를 방지하기 위해 포인터 역참조 전 반드시 `nullptr` 검사를 수행합니다.
- 실시간 루프(`ProcessInput`, `StepSimulation`, `RenderWiringCanvas`) 내부에서 빈번한 동적 힙 메모리 할당(`std::vector::push_back` 재할당, `new` 등)을 피하고, 사전 할당된 버퍼나 풀을 활용합니다.
- UI 이벤트와 컴포넌트 내부 물리/전기 상태(`internalStates`)를 직접 결합하지 않고, 커맨드/리졸버 패턴을 통해 안전하게 갱신합니다.

