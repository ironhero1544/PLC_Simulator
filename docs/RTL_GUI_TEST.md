# RTL WSL2 / Windows Native 실제 GUI 테스트

검증일: 2026-10-08. Computer Use의 Windows 앱 조작으로 최신 빌드
`cmake-build-p1-msvc/bin/PLCSimulator.exe`를 실행했다.
설치된 이전 버전과 실행 경로를 구분하여 확인했다.

## 확인 결과

| 실제 조작 | 결과 |
| --- | --- |
| RTL → Toolchain 진단 | WSL 호스트, WSL2 배포판, 커널, Verilator, Linux C++, MinGW, make 모두 정상 |
| 라이브러리에 `cu_rtl_native` 생성 및 소스 입력 | `clk`, `rst`, `out` 포트와 `assign out = 1'b1` 저장 |
| 분석 버튼 | 성공 표시 확인 |
| 빌드 버튼 | WSL의 Verilator 및 MinGW로 Windows EXE 생성 |
| 재실행 후 같은 모듈 선택 | 수정 후 종료 없이 분석·빌드 성공 표시 |
| 부품 활성화 및 캔버스 드래그 배치 | 부품 목록 및 실배선 캔버스에 표시 |
| 테스트 래더 `LD M8000 / OUT Y0` 입력, F4 변환, RUN | 변환 성공 및 RUN 표시 |
| Windows 워커 실행 | 생성 캐시의 `rtl_worker.exe` 프로세스 확인 |
| `out` → 타워램프 빨강, DC 0V → COM 배선 | 빨간 램프 점등 확인 |
| STOP 버튼 | STOP 상태 복귀 |

생성 EXE를 dumpbin으로 확인했으며, DLL 의존성은 KERNEL32.dll 및
msvcrt.dll이었다. MinGW 런타임 DLL 배포 없이 사용하는 정적 빌드다.

## 발견 및 수정

1. 외부 작업 폴더에서 앱을 실행하면 상대 경로의 폰트를 찾지 못했다.
   EXE 옆 `resources/fonts`를 우선 찾도록 수정했다. 수정본을 같은
   방식으로 실행하여 한글 및 RTL 편집기 표시를 확인했다.
2. 성공한 빌드의 긴 명령행을 진단 정규식으로 처리하면
   `regex_error(error_complexity)`로 앱이 종료됐다. 실제 빌드 로그를
   사용한 MSVC 재현 프로그램에서 예외를 확인했다. 중복된 반복
   패턴을 제거하고 HDL 위치가 없는 명령행은 건너뛰도록 수정했다.
   파일·줄·열 진단 유지와 실제 모듈 재선택, 배치 및 점등까지 확인했다.

재현 파일: `tmp/rtl-diagnostic-regex-check.cpp`.
빌드 로그: `tmp/rtl-ui-build.log`.
회귀 테스트 로그: `tmp/rtl-ui-regression.log`.

## 범위와 남겨둔 테스트 자료

기존 사용자 모듈은 변경하지 않았다. 사용자 RTL 라이브러리에 테스트용
`새 RTL 모듈`(top `cu_rtl_native`, ID `_rtl__2`)을 추가해 활성화했다.
현재 테스트 앱은 STOP 상태이며, 배선은 저장하지 않은 테스트 장면이다.

이번 GUI 검증은 진단·분석·빌드·실배선 런타임 경로에 대한 것이다.
Doctor 설치/UAC/배포판 전환·중지 버튼은 실행하지 않았다.
테스트벤치 및 패키지 복원은 이전 자동 통합 테스트 결과를 참조한다.
관련 구현과 검증 범위: [RTL_WSL_TOOLCHAIN.md](RTL_WSL_TOOLCHAIN.md).
