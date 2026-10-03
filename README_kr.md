# PLC_Simulator
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)]()
![plcsimulator_lockup_whitebg.svg](docs_img/plcsimulator_lockup_whitebg.svg)

통합 물리 시뮬레이션, OpenPLC 호환 레더 실행 경로, RTL 하드웨어 설계 워크플로우를 포함한 실시간 PLC 배선/프로그래밍 시뮬레이터입니다.

[English](README.md), [日本語](README_ja.md)로 보기

[프로젝트 개발 보고서](docs/Project_History.md) · [1.1.0 패치 노트](PATCH_NOTES_1.1.0.md)

## 이 프로젝트를 만든 이유

PLC 실습은 장비 접근성, 시간, 비용 제약이 큰 편입니다.  
이 프로젝트의 목표는 다음과 같습니다.

- 배선 + 프로그래밍 + 시뮬레이션을 하나의 환경에서 제공
- 실제 PLC 학습 흐름에 가까운 작업 경험 제공
- 데스크톱에서 빠른 반복 실습 지원
- PLC 로직과 RTL 하드웨어 설계를 하나의 도구에서 연결

## 핵심 방향

- 실용 워크플로우: 배선 구성, 레더 작성, 동작 검증, RTL 모듈 설계를 한 곳에서 처리
- 확장성: component/physics/programming/RTL 모듈 분리 구조
- 성능: LOD, 뷰포트 컬링, 공간 분할, 리소스 캐시 최적화
- 현실성: OpenPLC 기반 변환/실행 경로 + Box2D 연동

## 주요 기능

### 배선 모드
![실배선모드.png](docs_img/%EC%8B%A4%EB%B0%B0%EC%84%A0%EB%AA%A8%EB%93%9C.png)
- 드래그 앤 드롭 부품 배치가 가능한 인터랙티브 캔버스
- 자동 배선 라우팅 및 연결 관리
- 겹침 회피 및 다중 화살표 병합을 지원하는 스마트 태그 라벨
- PLC I/O, 공압 (FRL, 실린더, 솔레노이드 밸브, 미터 밸브), 스위치, 센서, 릴레이 등 부품 라이브러리
- 싱크/소스 배선 모드 변환
- 제스처 및 터치 지원: 핀치 줌, 두 손가락 팬, FRL/미터 밸브 회전 조절

### 레더 프로그래밍 모드
![프로그래밍모드.png](docs_img/%ED%94%84%EB%A1%9C%EA%B7%B8%EB%9E%98%EB%B0%8D%EB%AA%A8%EB%93%9C.png)
- GX Developer 스타일 레더 에디터
- 명령어 지원: `XIC`, `XIO`, `OTE`, `SET`, `RST`, `TON`, `CTU`, `RST_TMR_CTR`, `BKRST`
- 블록 선택, 복사/붙여넣기, 드래그 기반 렁 순서 변경
- `F9` / `Shift+F9`로 세로 분기(OR) 편집
- OpenPLC 호환 LD 변환 및 컴파일 파이프라인
- GX2 CSV 레더 프로그램 저장/불러오기

### 모니터 모드
![모니터모드.gif](docs_img/%EB%AA%A8%EB%8B%88%ED%84%B0%EB%AA%A8%EB%93%9C.gif)
- 실시간 I/O, 타이머, 카운터 상태 확인
- 활성 컴파일된 레더에 동기화된 현재값 및 프리셋 표시

### RTL 모드 *(1.1.0 신규)*
![RTL_code.png](docs_img/RTL_code.png)
![RTL_컴포넌트.gif](docs_img/RTL_%EC%BB%B4%ED%8F%AC%EB%84%8C%ED%8A%B8.gif)
- 구문 강조를 포함한 통합 Verilog RTL 에디터
- RTL 모듈 관리: RTL 라이브러리에서 모듈 생성, 이름 변경, 삭제, 정리
- 분석, 빌드, 테스트벤치 워크플로우 및 전용 로그 패널
- RTL 툴체인 관리: verilator 원클릭 설치, 검증, 삭제
- `.plccomp` 컴포넌트 패키지: Verilog 소스 포함 또는 런타임 전용 번들로 재사용 가능한 RTL 모듈 내보내기/가져오기
- RTL 런타임 아티팩트가 `.plcproj` 프로젝트 패키지에 번들되어 크로스 머신 이식 가능

### 시뮬레이션 엔진

- Mitsubishi FX3U-32M 기반 PLC 디자인
- 자체(in-house) 멀티 도메인 물리 엔진 (전기 / 공압 / 기계)
- 충돌 및 워크피스 상호작용을 위한 Box2D 통합
- 리밋 스위치, 버튼 유닛, 비상 정지 상태 병합을 위한 컴포넌트 입력 리졸버

### 프로젝트 워크플로우

- 배선, 레더, RTL 데이터를 담는 통합 프로젝트 패키지 형식 (`.plcproj`)
- 배선 및 프로그래밍 모드에서 `Ctrl+S` 빠른 저장
- NSIS 설치 프로그램에 언인스톨 시 RTL 툴체인 정리 옵션 포함
- 1.0.x 프로젝트 파일과 하위 호환

### 다국어 및 도움말

- 다국어 UI: 한국어, 영어, 일본어 (`resources/lang`)
- 배선, 프로그래밍, RTL, 단축키, 카메라 조작을 포함한 내장 도움말

## 요구 사항

- CPU: 4스레드 이상
- RAM: 최소 2GB
- CMake >= 3.20
- C++20 컴파일러
- OpenGL 런타임
- Git (FetchContent 의존성 다운로드용)
- *(선택)* 이전 Windows 버전에서 RTL 툴체인 스크립트 실행 시 PowerShell 7+

## 서드파티 라이브러리

- GLFW
- GLAD
- Dear ImGui
- nlohmann/json
- miniz
- Box2D
- NanoSVG

## 기술 스택

| 영역 | 스택 |
|---|---|
| 언어 | C++20 |
| 빌드 | CMake, CPack (NSIS + ZIP) |
| 렌더링/UI | OpenGL, GLFW, GLAD, Dear ImGui |
| 물리 | 자체(in-house) 멀티 도메인 물리 엔진(전기/공압/기계) + Box2D |
| PLC/레더 | OpenPLC 호환 LD 변환/실행 파이프라인 |
| RTL | verilator (외부 툴체인, 앱에서 관리) |
| 데이터/입출력 | nlohmann/json, miniz, XML serializer |

이 프로젝트는 범용 외부 게임 엔진이 아니라, 자체 시뮬레이션 엔진을 코어로 사용합니다.

## 빌드

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 실행

```bash
# single-config generator
./build/bin/PLCSimulator

# multi-config (Visual Studio)
./build/bin/Release/PLCSimulator.exe
```

## 테스트

```bash
cmake -S . -B build-test -DBUILD_TESTING=ON
cmake --build build-test --target plc_tests
ctest --test-dir build-test --output-on-failure
```

## 프로젝트 구조

```text
include/plc_emulator/   # 공개 헤더
src/                    # 현재 사용 중인 구현 코드
  application/          # 앱 라이프사이클, 입력, 렌더링, RTL UI
  programming/          # 레더 에디터, 컴파일러, PLC 실행기
  wiring/               # 배선 캔버스, 부품, 물리 동기화
  rtl/                  # RTL 프로젝트/라이브러리/런타임 관리
  project/              # 프로젝트 파일 저장/불러오기, GX2 CSV
resources/              # 폰트, i18n 언어 파일, 에셋
tools/                  # RTL 툴체인 설치/검증/삭제 스크립트
tests/                  # 단위/통합 테스트
legacy/                 # 구 MVP 코드 (참조 전용, 빌드 제외)
```

## 단축키

| 단축키 | 동작 |
|---|---|
| `F2` | 모니터 모드 전환 |
| `F5` / `F6` / `F7` | XIC / XIO / Coil |
| `F9` | 세로 라인 추가 |
| `Shift+F9` | 세로 라인 삭제 |
| `Ctrl+Z` / `Ctrl+Y` | 실행 취소 / 다시 실행 |
| `Ctrl+S` | 프로젝트 저장 |
| `Ctrl+C` / `Ctrl+V` | 선택 복사 / 붙여넣기 |
| `Delete` | 선택된 렁 블록 삭제 |

캔버스 네비게이션: 마우스 휠 줌, 미들 드래그 팬, `Alt`+우클릭 드래그 팬, 트랙패드 핀치/스크롤, 터치 핀치 줌 및 드래그 팬

(단축키 문자열은 `resources/lang/*.lang`에 정의되어 있습니다.)

## 상태

부품 추가와 기타 버그 수정에 대한 pull request를 받고 있습니다.
개발은 점진적으로 진행되고 있습니다.

## 라이선스

GPL-3.0
