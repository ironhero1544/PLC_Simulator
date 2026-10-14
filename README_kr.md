# PLC Simulator

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Windows](https://img.shields.io/badge/Platform-Windows-lightgrey)

![PLC Simulator](docs_img/plcsimulator_lockup_whitebg.svg)

통합 물리 시뮬레이션, OpenPLC 호환 레더 실행 경로, RTL 하드웨어 설계 워크플로우를 포함한 실시간 PLC 배선/프로그래밍 시뮬레이터입니다.

[English](README.md) · [日本語](README_ja.md)

[프로젝트 개발 보고서](docs/Project_History.md)

## 이 프로젝트를 만든 이유

PLC 실습은 장비 접근성, 시간, 비용 제약이 큰 편입니다.
이 프로젝트의 목표는 다음과 같습니다.

- 배선 + 프로그래밍 + 시뮬레이션을 하나의 환경에서 제공
- 실제 PLC 학습 흐름에 가까운 작업 경험 제공
- 데스크톱에서 빠른 반복 실습 지원
- PLC 로직과 RTL 하드웨어 설계를 하나의 도구에서 연결

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
- 명령어 지원: `Gxworks2` 에서 `FXCPU`로 지원하는 모든 명령어 호환
- 블록 선택, 복사/붙여넣기, 드래그 기반 렁 순서 변경
- `F9` 가로선 / `Shift+F9` 세로 연결로 직렬·병렬 회로 편집
- OpenPLC 호환 LD 변환 및 컴파일 파이프라인
- GX2 CSV 레더 프로그램 저장/불러오기

### 모니터 모드
![모니터모드.gif](docs_img/%EB%AA%A8%EB%8B%88%ED%84%B0%EB%AA%A8%EB%93%9C.gif)
- 실시간 I/O, 타이머, 카운터 상태 확인
- 활성 컴파일된 레더에 동기화된 현재값 및 프리셋 표시

### RTL 모드
![RTL_code.png](docs_img/RTL_code.png)
![RTL_컴포넌트.gif](docs_img/RTL_%EC%BB%B4%ED%8F%AC%EB%84%8C%ED%8A%B8.gif)
- 구문 강조를 포함한 통합 Verilog RTL 에디터
- RTL 모듈 관리: RTL 라이브러리에서 모듈 생성, 이름 변경, 삭제, 정리
- 분석, 빌드, 테스트벤치 워크플로우 및 전용 로그 패널
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
- NSIS 설치 프로그램: 제거 시 공유 WSL 배포판과 도구는 유지

### 다국어 및 도움말

- 다국어 UI: 한국어, 영어, 일본어 (`resources/lang`)
- 배선, 프로그래밍, RTL, 단축키, 카메라 조작을 포함한 내장 도움말

## 2.0 변경 사항

### 자동화 설비 부품 추가

- 센서 내장 복동·흡착 실린더, 스토퍼, 서보 리프트와 실린더 일체형 부품
- DC 모터 드라이버, 서보 모터·앰프, 광화이버 센서와 송수신 헤드
- 6칸 저장창고와 구동 창고, 공작물 감지·중력 공급·박스 내 정렬
- 부품별 평면·정면 표현과 자동화설비산업기사 부품 필터

![자동화 설비 추가 부품 모음](docs/design_assets/native_components_f_preview.png)

### 프로그래밍 모드 개선

- 명령 범위에 맞춘 셀 선택, 행·셀 복사와 붙여넣기, 분기 편집 개선
- 펄스·비교 접점과 모니터의 접점·코일 상태 표시 개선
- D 주소로 지정한 타이머·카운터 설정값과 현재값 표시 개선
- 편집·모니터 전환 및 재컴파일 시 남아 있던 출력 상태 초기화

![개선된 래더 모니터와 선택 표시](docs/images/program-guide/07-monitor-selection.png)

![타이머 현재값과 설정값 표시](docs/images/program-guide/monitor-timer-values.png)

### HMI 추가

- PLC에 별도 통신 배선 없이 연결되는 HMI
- 버튼·표시등·숫자 표시·설정값 입력·메시지를 구성하는 화면 편집기
- 여러 화면과 화면 전환 버튼, 크기를 조절하는 4:3 조작창
- 화면 편집 시 일시정지하고 닫으면 이전 운전 상태 복원

![HMI 화면 편집기](docs/images/exam03-guide/09-hmi-editor.png)

### Z·L 레이어 추가

- **Z**: 물리 높이·이동 범위를 지정해 서로 다른 높이의 상호작용을 구분
- **L**: 표시 순서·그룹·투명도·숨김을 다중 선택과 드래그로 조절
- 실제 동작 높이와 화면에 그리는 순서를 독립적으로 저장

![물리 Z 레이어 설정](docs/images/program-guide/03-z-layer.png)

![표시 순서·그룹을 관리하는 L 레이어](docs/images/program-guide/04-render-layer.png)

### 실배선 도구 개선

- 전기·공압·광 연결을 핀 종류로 결정하는 통합 배선 도구
- 부품 이름 편집과 캔버스에 이름 표시
- 센서 내장 부품, 모터 드라이버, 저장창고의 물리 동작과 배선 연동

![통합 배선 도구와 경유점 연결](docs/images/program-guide/wiring-route.gif)

### 사용자 가이드 추가

- 부품 배치·배선·래더·모니터·HMI·레이어·RTL 실습 가이드
- Ctrl+? 또는 Ctrl+, 도움말에서 여는 내장 Markdown 뷰어
- 인터넷 없이 이미지·GIF·표·코드·수식·논리도 표시
- 목차·검색·문서 이동·이미지 확대·창 크기 조절

![내장 사용자 가이드](docs/images/program-guide/user-guide-viewer.png)

### RTL 툴체인 개선

- 분석·빌드·테스트벤치를 WSL2 기반으로 전환
- MinGW-w64로 빌드한 부품은 Windows 네이티브 EXE로 실행
- Tool Doctor에서 배포판 선택·진단·도구 설치·시작/중지·WSL2 전환·업데이트
- 이미 빌드된 RTL 부품은 WSL 설치 없이 사용

![WSL2 RTL Tool Doctor](docs/images/program-guide/06-rtl-doctor.png)


## 핵심 방향

- **실습 흐름 연결**: 부품 배치와 배선부터 래더 작성, 운전, 상태 확인까지 한 프로그램에서 진행합니다.
- **자동화 설비 연습**: 센서·실린더·모터·공작물의 상호작용을 보며 제어 순서를 익힙니다.
- **수정과 확인**: 배선이나 프로그램을 바꾼 뒤 동작을 반복 확인해 원인을 찾을 수 있도록 합니다.
- **부품 확장**: 기본 부품 외에도 직접 만든 RTL 회로를 부품으로 등록해 사용합니다.

## 시작하기

1. [릴리스와 배포 파일](https://github.com/ironhero1544/PLC_Simulator/releases)에서 원하는 실행 형식을 선택합니다.
2. `PLCSimulator.exe`를 실행하고 **실배선**에서 부품을 배치합니다.
3. **배선** 도구로 시작 핀과 도착 핀을 차례로 연결합니다.
4. **프로그래밍 → 작성(F3)**에서 래더를 입력하고 **컴파일(F4)**합니다.
5. **시작**으로 운전하고 **모니터(F2)**에서 입력·출력과 회로 상태를 확인합니다.

처음 사용한다면 [버튼으로 램프 켜기](docs/guides/program/05-first-practice.md)부터 진행하세요.

| 배포 형식 | 사용 방법 |
| --- | --- |
| 설치 EXE | 설치 마법사로 설치 |
| MSI | Windows Installer로 설치 |
| Portable ZIP | 압축을 풀고 폴더 안의 EXE 실행 |
| 단일 Portable EXE | 실행 시 사용자 로컬 캐시에 필요한 파일을 풀고 실행 |

## 도움말과 사용법 가이드

**Ctrl+?** (`Ctrl+Shift+/`), **Ctrl+,** 또는 **Ctrl+/**로 도움말을 열고 **사용법 가이드 열기**를 선택합니다.
내장 가이드는 인터넷 없이 이미지·GIF·표·코드·수식·논리도를 표시합니다.
목차·검색·문서 이동·이미지 확대와 창 크기 조절을 지원하며 PLC Simulator 창 위에 표시됩니다.
모든 사용법·실습 가이드는 한국어·영어·일본어로 제공하며, 내장 뷰어는 프로그램에서 선택한 언어에 맞춰 표시합니다.

- [프로그램 사용법 가이드](docs/guides/program/README.md)
- [자동화설비산업기사 공개문제 3번 실습](docs/guides/exam03/GUIDE.md)
- [시작·정지·타이머·카운터 기초 회로](docs/guides/program/reference-basic-circuits.md)
- [Full Adder RTL 부품 만들기·배선·운전](docs/guides/program/rtl-full-adder.md)

## 실행 환경

- Windows 64비트, OpenGL 3.3을 지원하는 그래픽 드라이버
- 내장 가이드: Microsoft Edge WebView2 Runtime
- RTL 부품 빌드·테스트: WSL2, Linux 배포판, Verilator 5.x 이상, g++, make, MinGW-w64 POSIX
- Tool Doctor의 자동 도구 설치는 Ubuntu/Debian 지원

## 서드파티 라이브러리

- GLFW
- GLAD
- Dear ImGui
- nlohmann/json
- miniz
- Box2D
- NanoSVG
- webview / Microsoft Edge WebView2
- marked / DOMPurify / highlight.js / KaTeX / Mermaid

## 기술 스택

| 영역 | 스택 |
|---|---|
| 언어 | C++20 |
| 빌드 | CMake, CPack (NSIS + ZIP + WiX MSI) |
| 렌더링/UI | OpenGL, GLFW, GLAD, Dear ImGui |
| 물리 | 자체(in-house) 멀티 도메인 물리 엔진(전기/공압/기계) + Box2D |
| PLC/레더 | OpenPLC 호환 LD 변환/실행 파이프라인 |
| RTL | verilator (외부 툴체인, 앱에서 관리) |
| 데이터/입출력 | nlohmann/json, miniz, XML serializer |

이 프로젝트는 범용 외부 게임 엔진이 아니라, 자체 시뮬레이션 엔진을 코어로 사용합니다.

## 소스 빌드

CMake 3.20 이상, C++20 컴파일러(MSVC 또는 MinGW), Git, Python 3이 필요합니다.
MSVC는 Developer PowerShell에서, MinGW는 컴파일러가 PATH에 등록된 터미널에서 실행합니다.

```powershell
git clone https://github.com/ironhero1544/PLC_Simulator.git
cd PLC_Simulator
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

실행 파일은 단일 구성 빌드에서 `build/bin/PLCSimulator.exe`, Visual Studio 다중 구성 빌드에서 `build/bin/Release/PLCSimulator.exe`에 생성됩니다.
가이드 문서와 이미지는 빌드 시 실행 파일의 `resources/help`에 포함됩니다.

### 테스트

```powershell
cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-test --config Release
ctest --test-dir build-test -C Release --output-on-failure
```

### 배포 파일 생성

CPack으로 NSIS 설치 EXE와 ZIP을 생성합니다. MSI는 WiX 3.x가 추가로 필요합니다.
단일 Portable EXE는 CPack ZIP을 입력으로 `scripts/BuildStandalone.ps1`에서 만듭니다.

## 프로젝트 구조

```text
include/plc_emulator/   # 공개 헤더
src/                    # 현재 사용 중인 구현 코드
  application/          # 앱 라이프사이클, 입력, 렌더링, RTL UI
  programming/          # 레더 에디터, 컴파일러, PLC 실행기
  wiring/               # 배선 캔버스, 부품, 물리 동기화
  rtl/                  # RTL 프로젝트/라이브러리/런타임 관리
  project/              # 프로젝트 파일 저장/불러오기, GX2 CSV
docs/guides/            # Markdown guides
scripts/                # Guide packaging and release scripts
resources/              # 폰트, i18n 언어 파일, 에셋
tools/                  # 기존 네이티브 RTL 스크립트 (WSL2 Tool Doctor에서는 사용하지 않음)
tests/                  # 단위/통합 테스트
legacy/                 # 구 MVP 코드 (참조 전용, 빌드 제외)
```

## 단축키

| 단축키 | 동작 |
| --- | --- |
| `Ctrl+? / Ctrl+, / Ctrl+/` | 도움말·사용법 가이드 |
| `Tab` | 실배선 / 프로그래밍 전환 |
| `Ctrl+S` | 프로젝트 저장 |
| `F2 / F3` | 모니터 / 작성 |
| `F4` | 컴파일 (작성 모드) |
| `F5 / F6 / F7` | A접점 / B접점 / 코일 |
| `F8` | 응용 명령 |
| `F9` | 가로 연결선 추가 |
| `Shift+F9` | 세로 연결 추가 |
| `Ctrl+Z / Ctrl+Y` | 실행 취소 / 다시 실행 |
| `Ctrl+C / Ctrl+V` | 복사 / 붙여넣기 |
| `Delete` | 선택 삭제 |

## 라이선스와 기여

[GPL-3.0](LICENSE). 부품 추가와 오류 수정 Pull Request를 환영합니다.
UI·물리·래더 실행은 C++20, GLFW·Dear ImGui·OpenGL·Box2D 등을 사용합니다.
내장 가이드의 의존성과 라이선스는 [뷰어 의존성 안내](third_party/webview/README.md)에서 확인할 수 있습니다.

## 제작자 안내

이 프로젝트는 REA(Reverse Engineer Anything)와 전혀 무관하며 실제 실습을 연습할 수 있도록 GXworks2 호환가능한 CSV구조와 명령어 UI UX만을 참고하였고 실제 제품의 작동과는 전혀 다른 방식으로 작동합니다. 제작자는 본 프로그램의 영리 목적 사용을 지양하며, 교육 및 비영리 목적의 사용을 지향합니다.
