# PLC Emulator 2.0 프로그래밍 모드 전면 개편 계획서
## Astra 실행용 구현 명세 — OpenPLC 엔진 유지 버전

> **문서 버전**: v0.5-Astra-OpenPLC  
> **작성일**: 2026-09-10  
> **핵심 원칙**: 기존 OpenPLC 기반 PLC 엔진은 유지한다. 교체 대상은 문자열 생성/정규식 역파싱과 그 주변의 불안정한 변환 계층이다.

---

# 0. 최우선 불변 조건

## 절대 변경 금지

- **기존 OpenPLC 기반 PLC 엔진을 폐기하거나 다른 런타임으로 교체하지 않는다.**
- **새 VM을 만들지 않는다.**
- **별도의 FX3U Native Runtime으로 갈아타지 않는다.**
- **OpenPLC를 단순 import/export backend로 축소하지 않는다.**
- **기존 PLC 실행 의미와 OpenPLC 기반 처리 구조를 유지한다.**
- 배선/물리 엔진은 그대로 유지한다.
- 1.x의 기존 프로젝트 파일 호환은 목표가 아니다.

## 이번 개편에서 실제로 제거할 것

- `Ladder -> LD text -> C++ source text -> regex reparse`와 같은 불필요한 문자열 왕복
- 생성 C++ 문자열을 `std::regex`로 다시 읽는 구조
- 런타임에서 반복되는 device address 문자열 파싱
- `address + preset` 식의 임시 다중 오퍼랜드 표현
- GX Works2 CSV의 `VCX=` 같은 비표준 의존
- DFS 기반 무차별 path enumeration을 실행 의미의 핵심으로 사용하는 방식

즉, **PLC 엔진을 바꾸는 것이 아니라 OpenPLC 엔진으로 들어가는 데이터 경로를 구조화한다.**

---

# 1. 목표

다음 결과를 실제 코드로 완성한다.

1. 기존 OpenPLC 기반 PLC 실행 엔진 유지
2. GX Works2 스타일 래더 편집 UX
3. FX3U 디바이스/명령 표현 확장
4. GX Works2 CSV 양방향 호환
5. 중간 C++ 소스 문자열 생성 및 regex 역파싱 제거
6. F4 변환 시 구조화된 IR/Instruction 데이터로 OpenPLC 실행 경로에 직접 전달
7. 기존 Wiring / Physics 연동 유지
8. 명령어 확장이 쉬운 `InstructionDef` 기반 구조

---

# 2. 기존 구조와 신규 구조

## 기존 구조

```text
Ladder Grid
   ↓
DFS Path Enumeration
   ↓
OpenPLC LD Text
   ↓
C++ Source Text Generation
   ↓
std::regex Reparse
   ↓
CompiledPLCExecutor
   ↓
OpenPLC 기반 PLC 실행
```

문제는 OpenPLC가 아니라 **OpenPLC 실행기 앞뒤에 끼어 있는 불필요한 텍스트 왕복 계층**이다.

## 신규 구조

```text
GX-style Ladder Editor
        │
        ▼
Canonical Ladder AST / IR
        │
        │ F4 validate / resolve
        ▼
Structured OpenPLC Execution IR
        │
        ▼
Existing OpenPLC-based PLC Engine
        │
        ▼
I/O Gateway
        │
        ▼
Existing Wiring / Physics
```

외부 포맷은 다음과 같이 AST/IR 경계에서 직렬화한다.

```text
GX Works2 CSV ─┐
               ├──> Canonical Ladder AST / IR ──> OpenPLC Engine
OpenPLC LD  ───┘
```

핵심:

- OpenPLC 엔진은 유지한다.
- OpenPLC와 PLC_Simulator 사이의 전달 방식을 구조화한다.
- 실행 전용 데이터는 문자열이 아니라 typed structure로 전달한다.
- 텍스트는 파일 입출력과 UI 입력에서만 사용한다.

---

# 3. Astra 실행 계약

## Role

당신은 PLC_Simulator 2.0 프로그래밍 모드 개편을 담당하는 수석 C++ 엔지니어다.

기존 OpenPLC 기반 PLC 엔진은 제품의 핵심 자산이다.  
이번 작업의 목적은 OpenPLC를 제거하거나 대체하는 것이 아니라, **현재 OpenPLC 통합 경로의 문자열/정규식 기반 기술부채를 제거하고 GX Works2 호환성과 명령어 확장성을 높이는 것**이다.

## Success criteria

- 프로젝트가 정상 빌드된다.
- OpenPLC 기반 PLC 엔진이 그대로 유지된다.
- 기존 `GenerateExecutionCode()` → C++ source string → `ParseCompiledCode()` regex 경로가 제거되거나 실행 경로에서 완전히 우회된다.
- F4 변환 결과가 구조화된 명령/IR 형태로 OpenPLC 실행기에 전달된다.
- PLC scan loop에서 device address 문자열 재파싱을 하지 않는다.
- X/Y/M/S/T/C/D 및 특수 디바이스 확장이 가능하다.
- 다중 오퍼랜드 명령을 구조적으로 표현할 수 있다.
- GX Works2 CSV의 다중 오퍼랜드 및 분기 구조를 읽고 내보낼 수 있다.
- Wiring / Physics 인터페이스는 가능한 한 유지된다.
- 새 VM이나 별도 PLC 런타임이 추가되지 않는다.

## 금지 사항

- OpenPLC 엔진을 삭제하지 않는다.
- OpenPLC를 단순 export backend로 낮추지 않는다.
- 별도 FX3U runtime을 새로 만들지 않는다.
- bytecode VM/JIT/가상 스택 머신을 만들지 않는다.
- 기존 엔진을 유지하기 어렵다는 이유만으로 아키텍처를 새 실행기로 교체하지 않는다.
- `std::string` 자체를 프로젝트에서 금지하지 않는다.
- 성능을 `0ms`, 오류율을 `0%`라고 단정하지 않는다.

---

# 4. 공통 Device / Operand 모델

## 4.1 DeviceAddress

모든 주소 해석을 중앙화한다.

```cpp
enum class DeviceKind {
  X,
  Y,
  M,
  S,
  T,
  C,
  D,
  SpecialM
};

struct DeviceAddress {
  DeviceKind kind;
  uint32_t index;
};
```

요구사항:

- X/Y 주소 체계를 한 곳에서 해석한다.
- UI, CSV, OpenPLC 변환기, 실행기가 각각 `stoi()`를 직접 하지 않는다.
- 외부 문자열 표기와 내부 canonical index를 분리한다.
- Special M은 일반 M 배열 범위와 의미를 분리할 수 있어야 한다.

## 4.2 Operand

```cpp
enum class OperandKind {
  BitDevice,
  WordDevice,
  ImmediateDecimal,
  ImmediateHex
};

struct Operand {
  OperandKind kind;
  DeviceAddress device;
  int32_t immediate;
};
```

다중 오퍼랜드 명령을 `address`, `preset` 두 필드로 억지 표현하지 않는다.

---

# 5. InstructionDef

```cpp
struct InstructionDef {
  Opcode opcode;
  const char* mnemonic;
  uint16_t fnc_no;
  uint8_t min_operands;
  uint8_t max_operands;
  InstructionFlags flags;
  OperandRule operand_rules[MAX_OPERANDS];
};
```

역할:

- mnemonic resolve
- operand count/type validation
- 16/32bit variant
- pulse variant
- FNC metadata
- UI box rendering metadata
- GX Works2 CSV serialization metadata
- OpenPLC 변환/실행 binding metadata

중요:

`InstructionDef`는 **OpenPLC 엔진을 대체하기 위한 dispatcher가 아니라 OpenPLC 엔진에 안정적으로 연결하기 위한 공통 명령 메타데이터**다.

---

# 6. F4 변환 파이프라인

F4는 다음을 수행한다.

1. 래더 토폴로지 검증
2. device address canonicalization
3. mnemonic → Opcode resolve
4. operand validation
5. FX variant resolve
6. Ladder AST → Structured OpenPLC Execution IR 변환
7. 기존 OpenPLC 기반 실행기에 구조화된 실행 데이터 전달
8. 성공 시 `CONVERTED`
9. 실패 시 기존 실행 프로그램 유지 + `ERROR`

상태:

```text
UNCONVERTED
  ├─ F4 success -> CONVERTED
  └─ F4 failure -> ERROR

CONVERTED
  └─ edit -> UNCONVERTED

ERROR
  └─ edit -> UNCONVERTED
```

---

# 7. OpenPLC 통합 개편

## 유지할 것

- 기존 OpenPLC 기반 PLC 실행 구조
- 기존 scan semantics
- OpenPLC 기반 타이머/카운터/PLC 상태 처리 자산
- 기존 Wiring / Physics 연동 경계

## 제거/교체할 것

- C++ source text generation
- generated C++ text regex parsing
- 문자열 형식에 의존하는 instruction reconstruction
- 중복 device address parsing
- 명령어별 임시 문자열 규칙

## 목표 인터페이스 예시

```cpp
struct OpenPLCInstruction {
  Opcode opcode;
  uint8_t flags;
  Operand operands[MAX_OPERANDS];
  uint8_t operand_count;
};

class OpenPLCExecutionAdapter {
 public:
  bool LoadProgram(std::vector<OpenPLCInstruction> program);
};
```

실제 타입명은 현재 코드베이스 구조에 맞춰 결정한다.

중요한 것은 **OpenPLC 실행 엔진은 그대로 두고, 앞단에서 C++ 텍스트를 만들었다 다시 regex로 읽는 과정을 제거하는 것**이다.

---

# 8. Ladder AST / topology

1.x DFS path enumeration을 실행 의미의 핵심으로 사용하지 않는다.

AST는 다음을 표현할 수 있어야 한다.

- series
- parallel
- nested parallel
- output
- comparison contact
- application instruction block
- MPS/MRD/MPP/ANB/ORB 의미
- 향후 MC/MCR/STL 확장

화면 cell 좌표와 논리 topology를 분리한다.

---

# 9. GX Works2 UI/UX

## 입력

- 중앙 모달 제거
- 현재 셀 위치 inline input
- `ld x0`
- `mov k10 d0`
- 자동완성
- operand validation
- Enter 후 cursor 이동

## 단축키

- F5/F6/F7/F8/F9 계열
- Shift 조합
- Ctrl + 방향키 결선
- F4 변환

## 렌더링

- 고정 12열 강제 제거
- variable-width instruction box
- contact/coil/box renderer 분리
- 도통 강조
- D/T/C monitor overlay
- convert state visualization

---

# 10. GX Works2 CSV

필수:

- multi-row operands
- timer/counter preset continuation row
- MOV/ADD/CMP 등 다중 operand
- MPS/MRD/MPP
- ANB/ORB
- nested branch topology
- `VCX=` 의존 제거

Round-trip 목표:

```text
GX Works2 Export
  -> PLC_Simulator Import
  -> Ladder AST
  -> PLC_Simulator Export
  -> GX Works2 Import
```

검증 대상:

- instruction
- device
- operands
- topology
- step numbering validity
- execution semantics

---

# 11. 지원 명령 로드맵

## Tier 1

- LD / LDI
- AND / ANI
- OR / ORI
- OUT
- SET / RST
- TON / CTU
- ANB / ORB
- MPS / MRD / MPP
- INV
- 기본 pulse/edge
- 기본 contact comparison

## Tier 2

- MOV
- CMP / ZCP
- ADD / SUB / MUL / DIV
- INC / DEC
- WAND / WOR / WXOR
- ZRST
- ALT
- BMOV / FMOV
- D/DP/P variant

## Tier 3

- rotate / shift
- DECO / ENCO / SUM / BON
- RTC
- CJ / CALL / FOR / NEXT

## Tier 4

- floating point
- string
- block data
- 기타 FX CPU 확장 명령

---

# 12. 구현 Phase

## Phase 0 — Audit

- 관련 source/header 조사
- 현재 OpenPLC 엔진과 executor 경계 파악
- generated code / regex 경로 정확한 호출 관계 정리
- Wiring / Physics I/O 경계 확인

완료 조건:

- OpenPLC 엔진 중 유지 대상과 제거 대상 명확화
- 변경 대상 파일 목록 확보
- baseline build 확인

## Phase 1 — Device / Operand

- canonical address parser
- X/Y mapping
- D/S/Special M 기반
- Operand model
- InstructionDef

완료 조건:

- 주소 테스트 통과
- invalid address 명시적 실패
- build 성공

## Phase 2 — Ladder AST

- topology representation
- nested branch
- comparison/application nodes
- DFS 실행 의존 제거

완료 조건:

- 직렬/병렬/중첩 병렬 fixture 통과

## Phase 3 — OpenPLC structured execution bridge

- C++ source generation 제거
- regex reparse 제거
- structured instruction transfer
- 기존 OpenPLC engine에 direct binding

완료 조건:

- 기존 핵심 PLC 프로그램이 OpenPLC 엔진에서 동일하게 실행
- runtime string/regex 재파싱 없음

## Phase 4 — Core FX instructions

- sequence
- timer/counter
- branch/stack
- pulse
- compare

## Phase 5 — GX Works2 CSV

- multi-row operand
- topology
- round-trip fixtures

## Phase 6 — GX Works2 UI/UX

- inline editor
- shortcut
- F4 state
- variable box
- monitor overlay

## Phase 7 — Application instructions

- MOV/arithmetic/compare/block
- D/P/DP variants

---

# 13. 테스트 전략

## Device

- X0/X7/X10
- invalid X/Y
- D/S
- normal M
- special M
- K/H constant

## Ladder

- series
- parallel
- nested parallel
- inverted contact
- output
- timer/counter
- invalid topology

## OpenPLC integration

- F4 structured compile
- load into existing OpenPLC engine
- scan result comparison
- SET/RST
- timer/counter
- branch semantics
- no generated C++ source dependency
- no regex instruction reconstruction

## CSV

- timer preset continuation
- MOV multi-row operand
- branch stack commands
- nested branch
- semantic round-trip

## Integration

- editor -> F4 -> OpenPLC engine -> I/O -> Physics -> monitor

---

# 14. Astra 진행 규칙

- 계획만 설명하고 멈추지 않는다.
- OpenPLC 엔진 유지 조건을 최우선으로 지킨다.
- 구현 중 별도 PLC runtime이 필요하다고 판단하더라도 임의로 추가하지 않는다.
- 독립 작업은 병렬화 가능하나 공통 Device/Operand/AST 정의를 먼저 고정한다.
- build/test 실패는 현재 변경 범위 안이면 스스로 수정한다.
- 공식 FX 동작 의미가 불명확하여 결과가 달라질 수 있을 때만 질문한다.
- milestone에서만 짧게 진행 상황을 보고한다.

중간 보고 형식:

```text
현재 Phase:
완료:
검증:
다음:
막힌 점:
```

---

# 15. 최종 완료 정의

다음 상태가 되면 2.0 프로그래밍 코어 1차 개편 완료로 본다.

- OpenPLC 기반 PLC 엔진 유지
- generated C++ source / regex reparse 제거
- structured execution bridge 적용
- canonical Device/Operand 모델 적용
- scan runtime에서 문자열 주소 재파싱 제거
- GX Works2 스타일 F4 compile workflow
- 핵심 FX 명령 실행
- GX Works2 CSV semantic round-trip
- Wiring / Physics 연동 유지
- 의미 있는 자동 테스트와 build 통과
- 새 VM / 별도 FX runtime 없음

**이번 개편은 PLC 엔진 교체가 아니다.  
기존 OpenPLC 기반 엔진을 유지하면서, 그 앞단과 내부 전달 경로를 구조화하고 확장 가능하게 만드는 작업이다.**
