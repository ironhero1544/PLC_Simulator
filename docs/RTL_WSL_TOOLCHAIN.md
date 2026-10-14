# RTL WSL2 툴체인 및 Tool Doctor

처음 환경을 준비하거나 진단 오류를 해결하려면 [Tool Doctor 사용법 가이드](guides/program/rtl-tool-doctor.md)를 읽으세요.

## 사용 방법

1. `RTL → 툴체인`에서 **RTL Tool Doctor (WSL2)**를 연다.
2. 설치된 배포판 또는 기본 배포판을 선택하고 빌드 CPU 수를 지정한다.
   CPU 수는 1~64이며 기본은 2이다.
3. **저장 후 진단**으로 RTL 분석·빌드·테스트벤치에 사용할 설정을 저장한다.
4. WSL 호스트, WSL2 배포판, Linux 커널, Verilator, Linux g++, MinGW Windows 컴파일러와 make를 확인한다.
   각 진단 항목을 펼치면 원래 출력과 버전을 볼 수 있다.
5. 환경이 준비되면 기존 RTL 편집기의 분석, 빌드, 테스트벤치를 사용한다.

프로필은 `%LOCALAPPDATA%\PLCSimulator\rtl-wsl.json`에 저장한다.
임시 파일 작성 후 교체하여 저장 중 실패로 기존 설정이 잘리지 않게 했다.
**저장된 프로필 진단**은 저장된 설정을 검사하며, 드롭다운 변경을 적용하려면
먼저 **저장 후 진단**을 사용한다. 관리 작업은 화면에서 선택한 배포판에 적용한다.

| 관리 작업 | 실행 내용 |
|---|---|
| WSL2 / Ubuntu 설치 | 관리자 권한으로 `wsl --install --distribution Ubuntu --no-launch` |
| RTL 도구 설치 / 복구 | 선택한 WSL2의 root로 `apt-get update`, `apt-get install -y verilator g++ make g++-mingw-w64-x86-64-posix` |
| 선택 배포판 시작 | 선택 배포판에서 `/usr/bin/true` 실행 |
| 선택 배포판 중지 | `wsl --terminate <배포판>` |
| WSL2로 전환 | `wsl --set-version <배포판> 2` |
| WSL 업데이트 | `wsl --update` |

도구 설치는 apt가 있는 Ubuntu/Debian을 대상으로 한다. Verilator는 timing
지원이 있는 **5.x 이상**이 필요하다. 오래된 배포판의 기본 패키지가 4.x이면
진단에서 호환 실패를 표시하므로, 5.x 패키지를 제공하는 배포판을 사용한다.
WSL 신규 설치 시 Windows 관리자 승인과 재시작이 필요할 수 있다.
관리 작업은 백그라운드에서 실행하고 종료 코드와 로그로 성공 여부를 판단한다.

Windows 네이티브 RTL 워커는 WSL 진단이나 배포판 시작·중지와 독립적으로 실행한다.
배포판 중지 후 자동 진단으로 즉시 다시 켜지 않게 하며, Doctor 창을 닫아도
진행 중 작업의 완료를 처리한다. WSL 관리 작업 중에는 새로운 분석/빌드 요청을 막는다.
배포판 중지는 해당 배포판에서 실행 중인 다른 프로그램에도 적용된다.

## 실행 및 캐시

- `wsl.exe`는 Windows System32에서 실행하고, 배포판·작업 폴더·프로그램·인자는
  구조화된 값으로 전달한다. 일반 도구 실행에 셸 문자열을 조합하지 않는다.
- 호스트 출력의 UTF-16LE를 UTF-8로 변환한다. 공백과 한글 파일 경로를 지원한다.
- HDL은 캐시의 `src` 아래에 폴더 구조를 유지하여 모두 저장한다.
  절대/루트 경로와 `..` 경로는 거부한다.
- WSL2의 `verilator --cc --exe --compiler gcc --timing`으로 C++ 모델을 생성한다.
  생성한 Makefile을 `x86_64-w64-mingw32-g++-posix` / MinGW `ar`로 빌드한다.
  CPU 수는 make의 `-j`에 적용한다.
- 워커는 Windows PE x64인 `rtl_worker.exe`이다. MinGW 및 pthread 런타임을
  정적으로 링크하여 새 워커의 배포에 별도 MinGW DLL이 필요하지 않게 한다.
  기존 PLC의 `EVAL`/`OUT` 입출력 프로토콜을 유지한다.
- 워커 실행 시 WSL 진단/호출이나 컴파일러 검사를 하지 않는다. 따라서
  부품 EXE가 포함된 프로젝트는 WSL이 없는 Windows에서도 실행할 수 있다.
- 빌드 캐시는 backend 버전, 배포판, top, 소스 경로 및 **소스 내용**을 반영한다.
  같은 글자 수로 HDL을 수정해도 캐시를 새로 만든다.
- 테스트벤치는 Linux 작업 폴더에 `dump.vcd`를 쓰고, 앱은 대응하는 Windows
  경로로 파형을 읽는다. 실행 시간과 최대 스텝 수를 제한한다.
- `.plccomp`/`.plcproj`에는 Windows 워커 EXE를 포함한다. 기존 네이티브 번들에
  MinGW DLL이 있으면 허용된 런타임 DLL도 함께 복원한다. PE 헤더를 확인하여
  잘못된 EXE나 Linux ELF를 거부한다. 이전 단계에서 만든 Linux 워커는 소스로
  재빌드해야 한다. Windows 대상 backend 이름을 변경하여 기존 ELF 캐시를 재사용하지 않는다.

기존 `tools/*.ps1`은 과거 네이티브 툴체인의 기록으로 남겨 두었다.
앱, CMake 설정 및 CPack 설치·제거 과정에서는 호출하거나 배포하지 않는다.
앱 제거 시 공유 WSL 배포판이나 Linux 도구를 삭제하지 않는다.

## 검증

Windows MSVC Release 앱 빌드 성공, CTest 회귀 테스트 **17/17 통과**를 확인했다.
추가 `rtl_wsl_tests`는 경로 변환, 명령 인자 quoting, 배포판 목록 파싱,
프로필 저장/읽기, 잘못된 EXE 거부 및 내용 기반 캐시를 검증한다.
앱 테스트는 Doctor 창을 닫은 상태에서도 중지 작업이 완료되고,
자동 진단으로 WSL을 다시 실행하지 않는 것을 검증한다.
실제 ImGui 렌더링으로 한국어 Doctor 화면의 레이아웃과 번역도 확인했다.

실제 WSL 통합 테스트는 환경 의존성이 있으므로 일반 CTest와 분리했다.

```powershell
.\cmake-build-p1-msvc\bin\rtl_wsl_tests.exe --integration
```

Ubuntu WSL2, Verilator 5.020, Linux g++ 13.3, MinGW-w64 GCC 13.2, make 4.3에서 다음을 확인했다.

- WSL2 진단 및 공백·한글·셸 특수문자 인자의 그대로 전달
- Linux 실행 시간 제한
- 중첩 폴더와 한글 파일 이름의 다중 HDL 분석·빌드
- Windows PE EXE 생성 및 사용할 수 없는 WSL 프로필에서도 Windows에서 AND 회로 4개 입력 조합 입출력
- 생성 EXE의 DLL 의존성이 KERNEL32.dll / msvcrt.dll뿐임을 dumpbin으로 확인
- 같은 길이의 AND→OR 수정 후 빌드 캐시 변경
- 지연과 `$fatal` 검사가 있는 테스트벤치 실행, 성공 메시지 및 VCD 생성
- Windows EXE 번들 내보내기와 복원, WSL 없이 사용할 수 있는 직접 실행 명령
- 루트 및 상위 폴더 탈출 경로 거부

통합 테스트는 프로젝트 안의 격리된 LOCALAPPDATA 프로필/캐시를 사용한다.
통합 테스트 자체는 배포판 설치·중지·업데이트·전환이나 패키지 설치를 수행하지 않는다.
Windows 워커 빌드 검증을 위해 Ubuntu에 MinGW-w64 POSIX C++ 패키지를 별도로 설치했다.
Doctor 관리 작업은 명령 구성과 완료 처리를 검증했다. MinGW 패키지 설치는
직접 수행했으나, Doctor 버튼을 통한 전체 도구 재설치/UAC/배포판 전환 절차는
실행하여 검증하지 않았다.
