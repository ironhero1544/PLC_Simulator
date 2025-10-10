# 디렉토리 구조 재구성 계획
# Google C++ 프로젝트 표준 및 모범 사례 준수

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
❌ 기능별 디렉토리 분리 없음 (모든 헤더/소스가 평탄한 구조)
```

---

## 목표 디렉토리 구조

### 새로운 표준 구조
```
PLC_Emulator/
├── .clang-format              # 포매팅 설정
├── .clang-tidy               # 정적 분석 설정 (새로 추가)
├── .gitignore                # Git 무시 파일
├── CMakeLists.txt            # 루트 CMake 설정
├── README.md                 # 프로젝트 소개
├── LICENSE                   # 라이선스 파일
│
├── cmake/                    # CMake 모듈 및 스크립트
│   ├── CompilerWarnings.cmake
│   ├── StaticAnalyzers.cmake
│   └── CodeCoverage.cmake
│
├── docs/                     # 문서
│   ├── README.md
│   ├── architecture.md       # 아키텍처 설계
│   ├── user_guide.md        # 사용자 가이드
│   ├── api_reference.md     # API 문서
│   └── refactoring/         # 리팩토링 문서
│       ├── plan.md          # 현재 REFACTORING_PLAN.md
│       ├── cmake_guide.md   # 현재 CMAKE_GUIDE.md
│       └── directory_structure.md  # 이 문서
│
├── include/                  # 공개 헤더 파일
│   └── plc_emulator/        # 네임스페이스별 디렉토리
│       ├── core/            # 핵심 기능
│       │   ├── application.h
│       │   ├── data_types.h
│       │   └── plc_simulator_core.h
│       ├── wiring/          # 실배선 모드
│       │   ├── component_manager.h
│       │   ├── wiring_manager.h
│       │   └── snap_system.h
│       ├── programming/     # 프로그래밍 모드
│       │   ├── programming_mode.h
│       │   ├── ladder_editor.h
│       │   └── compiled_plc_executor.h
│       ├── physics/         # 물리 엔진
│       │   ├── physics_engine.h
│       │   ├── physics_network.h
│       │   └── physics_states.h
│       ├── io/             # I/O 매핑
│       │   ├── io_mapper.h
│       │   └── io_mapping.h
│       ├── data/           # 데이터 관리
│       │   ├── data_repository.h
│       │   └── event_system.h
│       └── project/        # 프로젝트 관리
│           ├── project_file_manager.h
│           └── xml_serializer.h
│
├── src/                     # 구현 파일
│   ├── core/               # 핵심 기능 구현
│   │   ├── application.cpp
│   │   ├── application_rendering.cpp
│   │   ├── application_input.cpp
│   │   ├── data_types.cpp
│   │   └── plc_simulator_core.cpp
│   ├── wiring/             # 실배선 모드 구현
│   │   ├── component_manager.cpp
│   │   ├── wiring_manager.cpp
│   │   ├── application_components.cpp
│   │   ├── application_wiring.cpp
│   │   ├── application_ports.cpp
│   │   └── application_snap.cpp
│   ├── programming/        # 프로그래밍 모드 구현
│   │   ├── programming_mode.cpp
│   │   ├── programming_mode_sim.cpp
│   │   ├── programming_mode_edit.cpp
│   │   ├── programming_mode_ui.cpp
│   │   └── compiled_plc_executor.cpp
│   ├── physics/            # 물리 엔진 구현
│   │   ├── physics_engine.cpp
│   │   ├── physics_solvers.cpp
│   │   └── application_physics.cpp
│   ├── io/                # I/O 매핑 구현
│   │   └── io_mapper.cpp
│   ├── data/              # 데이터 관리 구현
│   │   ├── data_repository.cpp
│   │   └── event_system.cpp
│   ├── project/           # 프로젝트 관리 구현
│   │   ├── project_file_manager.cpp
│   │   ├── xml_serializer.cpp
│   │   ├── ladder_to_ld_converter.cpp
│   │   ├── ld_to_ladder_converter.cpp
│   │   ├── ladder_ir.cpp
│   │   └── openplc_compiler_integration.cpp
│   └── main.cpp           # 진입점
│
├── tests/                  # 단위 테스트
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   ├── core/
│   │   ├── test_data_types.cpp
│   │   └── test_application.cpp
│   ├── wiring/
│   │   └── test_component_manager.cpp
│   ├── programming/
│   │   └── test_ladder_editor.cpp
│   ├── physics/
│   │   └── test_physics_engine.cpp
│   └── mocks/             # 테스트용 Mock 객체
│       └── mock_window.h
│
├── examples/              # 예제 프로젝트
│   ├── basic_circuit.plcproj
│   ├── pneumatic_system.plcproj
│   └── ladder_examples/
│       ├── timer_example.ld
│       └── counter_example.ld
│
├── resources/             # 리소스 파일
│   ├── fonts/
│   │   └── unifont.ttf
│   ├── icons/
│   └── configs/
│       └── default_settings.json
│
├── scripts/               # 빌드/유틸리티 스크립트
│   ├── format_all.sh     # 전체 코드 포매팅
│   ├── format_all.bat    # Windows용
│   ├── run_tests.sh      # 테스트 실행
│   ├── run_tests.bat     # Windows용
│   └── install_deps.sh   # 의존성 설치
│
├── third_party/           # 서드파티 라이브러리
│   └── README.md         # FetchContent로 관리되므로 빈 디렉토리
│
└── build/                 # 빌드 출력 (.gitignore)
    ├── bin/              # 실행 파일
    ├── lib/              # 라이브러리
    └── tests/            # 테스트 바이너리
```

---

## 디렉토리 설계 원칙

### 1. 명확한 계층 구조
- **기능별 분리**: core, wiring, programming, physics, io, data, project
- **대칭성 유지**: include/와 src/의 디렉토리 구조 일치
- **네임스페이스 반영**: include/plc_emulator/기능명/ → namespace plc::기능명

### 2. 스케일러빌리티 (확장성)
- 새 기능 추가 시 독립적인 디렉토리 추가 가능
- 모듈 간 의존성 최소화
- 각 모듈은 자체 CMakeLists.txt 가능 (향후 확장)

### 3. 표준 준수
- **Google C++ 프로젝트 구조** 참고
- **CMake 모범 사례** 적용
- **테스트 코드 분리**: tests/ 디렉토리
- **문서화**: docs/ 디렉토리

### 4. 빌드 산출물 분리
- 소스 코드와 빌드 출력 완전 분리
- .gitignore로 빌드 디렉토리 제외
- 리소스 파일은 resources/에 집중

### 5. 도구 및 스크립트 관리
- scripts/: 빌드/테스트 자동화 스크립트
- cmake/: CMake 모듈 및 함수
- 플랫폼별 스크립트 제공 (.sh, .bat)

---

## 파일 매핑 (현재 → 새 위치)

### 헤더 파일 (include/)

```
현재 위치 → 새 위치

# Core
include/Application.h → include/plc_emulator/core/application.h
include/DataTypes.h → include/plc_emulator/core/data_types.h
include/PLCSimulatorCore.h → include/plc_emulator/core/plc_simulator_core.h

# Wiring
(새로 추가될 클래스들 - Phase 5에서 Application 분할 시)
→ include/plc_emulator/wiring/component_manager.h
→ include/plc_emulator/wiring/wiring_manager.h
→ include/plc_emulator/wiring/snap_system.h

# Programming
include/ProgrammingMode.h → include/plc_emulator/programming/programming_mode.h
include/CompiledPLCExecutor.h → include/plc_emulator/programming/compiled_plc_executor.h
include/LadderToLDConverter.h → include/plc_emulator/project/ladder_to_ld_converter.h
include/LDToLadderConverter.h → include/plc_emulator/project/ld_to_ladder_converter.h
include/OpenPLCCompilerIntegration.h → include/plc_emulator/project/openplc_compiler_integration.h
include/LadderIR.h → include/plc_emulator/project/ladder_ir.h

# Physics
include/PhysicsEngine.h → include/plc_emulator/physics/physics_engine.h
include/PhysicsNetwork.h → include/plc_emulator/physics/physics_network.h
include/PhysicsStates.h → include/plc_emulator/physics/physics_states.h

# I/O
include/IOMapper.h → include/plc_emulator/io/io_mapper.h
include/IOMapping.h → include/plc_emulator/io/io_mapping.h

# Data
include/DataRepository.h → include/plc_emulator/data/data_repository.h
include/EventSystem.h → include/plc_emulator/data/event_system.h

# Project
include/ProjectFileManager.h → include/plc_emulator/project/project_file_manager.h
include/XMLSerializer.h → include/plc_emulator/project/xml_serializer.h
```

### 소스 파일 (src/)

```
현재 위치 → 새 위치

# Core
src/main.cpp → src/main.cpp (그대로)
src/Application.cpp → src/core/application.cpp
src/DataTypes.cpp → src/core/data_types.cpp
src/PLCSimulatorCore.cpp → src/core/plc_simulator_core.cpp

# Wiring
src/Application_Components.cpp → src/wiring/application_components.cpp
src/Application_Wiring.cpp → src/wiring/application_wiring.cpp
src/Application_Ports.cpp → src/wiring/application_ports.cpp
src/Application_Snap.cpp → src/wiring/application_snap.cpp

# Programming
src/ProgrammingMode.cpp → src/programming/programming_mode.cpp
src/ProgrammingMode_Sim.cpp → src/programming/programming_mode_sim.cpp
src/ProgrammingMode_Edit.cpp → src/programming/programming_mode_edit.cpp
src/ProgrammingMode_UI.cpp → src/programming/programming_mode_ui.cpp
src/CompiledPLCExecutor.cpp → src/programming/compiled_plc_executor.cpp

# Physics
src/Application_Physics.cpp → src/physics/application_physics.cpp
src/PhysicsEngine.cpp → src/physics/physics_engine.cpp
src/PhysicsSolvers.cpp → src/physics/physics_solvers.cpp

# I/O
src/IOMapper.cpp → src/io/io_mapper.cpp

# Data
src/DataRepository.cpp → src/data/data_repository.cpp
src/EventSystem.cpp → src/data/event_system.cpp

# Project
src/ProjectFileManager.cpp → src/project/project_file_manager.cpp
src/XMLSerializer.cpp → src/project/xml_serializer.cpp
src/LadderToLDConverter.cpp → src/project/ladder_to_ld_converter.cpp
src/LDToLadderConverter.cpp → src/project/ld_to_ladder_converter.cpp
src/LadderIR.cpp → src/project/ladder_ir.cpp
src/OpenPLCCompilerIntegration.cpp → src/project/openplc_compiler_integration.cpp
```

### 기타 파일

```
# 리소스
unifont.ttf → resources/fonts/unifont.ttf

# 문서
.copilot/REFACTORING_PLAN.md → docs/refactoring/plan.md
.copilot/CMAKE_GUIDE.md → docs/refactoring/cmake_guide.md
(새로 추가) → docs/architecture.md
(새로 추가) → docs/user_guide.md
(새로 추가) → README.md (루트)

# 빌드 산출물
main.exe → build/bin/PLCSimulator.exe (빌드 시)
```

---

## 마이그레이션 체크리스트

### 준비 단계
- [ ] 현재 상태 백업 (Git commit 또는 branch 생성)
- [ ] 빌드 성공 확인 (리팩토링 전 기준선)
- [ ] .gitignore 업데이트

### 1단계: 디렉토리 생성
```bash
# Linux/macOS/Git Bash
mkdir -p include/plc_emulator/{core,wiring,programming,physics,io,data,project}
mkdir -p src/{core,wiring,programming,physics,io,data,project}
mkdir -p tests/{core,wiring,programming,physics,mocks}
mkdir -p {docs/refactoring,cmake,examples,resources,scripts}
mkdir -p resources/{fonts,icons,configs}

# Windows PowerShell
New-Item -ItemType Directory -Path include/plc_emulator/core,include/plc_emulator/wiring,include/plc_emulator/programming,include/plc_emulator/physics,include/plc_emulator/io,include/plc_emulator/data,include/plc_emulator/project -Force
New-Item -ItemType Directory -Path src/core,src/wiring,src/programming,src/physics,src/io,src/data,src/project -Force
New-Item -ItemType Directory -Path tests/core,tests/wiring,tests/programming,tests/physics,tests/mocks -Force
New-Item -ItemType Directory -Path docs/refactoring,cmake,examples,resources,scripts -Force
New-Item -ItemType Directory -Path resources/fonts,resources/icons,resources/configs -Force
```

### 2단계: 헤더 파일 이동
- [ ] Core 헤더 이동
  - Application.h → plc_emulator/core/application.h
  - DataTypes.h → plc_emulator/core/data_types.h
  - PLCSimulatorCore.h → plc_emulator/core/plc_simulator_core.h

- [ ] Programming 헤더 이동
  - ProgrammingMode.h → plc_emulator/programming/programming_mode.h
  - CompiledPLCExecutor.h → plc_emulator/programming/compiled_plc_executor.h

- [ ] Physics 헤더 이동
  - PhysicsEngine.h → plc_emulator/physics/physics_engine.h
  - PhysicsNetwork.h → plc_emulator/physics/physics_network.h
  - PhysicsStates.h → plc_emulator/physics/physics_states.h

- [ ] I/O 헤더 이동
  - IOMapper.h → plc_emulator/io/io_mapper.h
  - IOMapping.h → plc_emulator/io/io_mapping.h

- [ ] Data 헤더 이동
  - DataRepository.h → plc_emulator/data/data_repository.h
  - EventSystem.h → plc_emulator/data/event_system.h

- [ ] Project 헤더 이동
  - ProjectFileManager.h → plc_emulator/project/project_file_manager.h
  - XMLSerializer.h → plc_emulator/project/xml_serializer.h
  - LadderToLDConverter.h → plc_emulator/project/ladder_to_ld_converter.h
  - LDToLadderConverter.h → plc_emulator/project/ld_to_ladder_converter.h
  - LadderIR.h → plc_emulator/project/ladder_ir.h
  - OpenPLCCompilerIntegration.h → plc_emulator/project/openplc_compiler_integration.h

### 3단계: 소스 파일 이동
- [ ] Core 소스 이동
  - Application.cpp → core/application.cpp
  - DataTypes.cpp → core/data_types.cpp
  - PLCSimulatorCore.cpp → core/plc_simulator_core.cpp

- [ ] Wiring 소스 이동
  - Application_Components.cpp → wiring/application_components.cpp
  - Application_Wiring.cpp → wiring/application_wiring.cpp
  - Application_Ports.cpp → wiring/application_ports.cpp
  - Application_Snap.cpp → wiring/application_snap.cpp

- [ ] Programming 소스 이동
  - ProgrammingMode.cpp → programming/programming_mode.cpp
  - ProgrammingMode_Sim.cpp → programming/programming_mode_sim.cpp
  - ProgrammingMode_Edit.cpp → programming/programming_mode_edit.cpp
  - ProgrammingMode_UI.cpp → programming/programming_mode_ui.cpp
  - CompiledPLCExecutor.cpp → programming/compiled_plc_executor.cpp

- [ ] Physics 소스 이동
  - PhysicsEngine.cpp → physics/physics_engine.cpp
  - PhysicsSolvers.cpp → physics/physics_solvers.cpp
  - Application_Physics.cpp → physics/application_physics.cpp

- [ ] I/O 소스 이동
  - IOMapper.cpp → io/io_mapper.cpp

- [ ] Data 소스 이동
  - DataRepository.cpp → data/data_repository.cpp
  - EventSystem.cpp → data/event_system.cpp

- [ ] Project 소스 이동
  - ProjectFileManager.cpp → project/project_file_manager.cpp
  - XMLSerializer.cpp → project/xml_serializer.cpp
  - LadderToLDConverter.cpp → project/ladder_to_ld_converter.cpp
  - LDToLadderConverter.cpp → project/ld_to_ladder_converter.cpp
  - LadderIR.cpp → project/ladder_ir.cpp
  - OpenPLCCompilerIntegration.cpp → project/openplc_compiler_integration.cpp

### 4단계: 리소스 및 문서 이동
- [ ] 리소스 파일
  - unifont.ttf → resources/fonts/unifont.ttf

- [ ] 문서 파일
  - .copilot/REFACTORING_PLAN.md → docs/refactoring/plan.md
  - .copilot/CMAKE_GUIDE.md → docs/refactoring/cmake_guide.md
  - (이 파일) → docs/refactoring/directory_structure.md

- [ ] 새 문서 작성
  - README.md (루트)
  - docs/architecture.md
  - docs/user_guide.md

### 5단계: CMakeLists.txt 업데이트
- [ ] include 경로 업데이트
  ```cmake
  target_include_directories(${PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      # ...
  )
  ```

- [ ] 소스 파일 경로 업데이트
  ```cmake
  add_executable(${PROJECT_NAME}
      src/main.cpp
      # Core
      src/core/application.cpp
      src/core/data_types.cpp
      src/core/plc_simulator_core.cpp
      # Wiring
      src/wiring/application_components.cpp
      # ... (전체 목록)
  )
  ```

- [ ] 리소스 파일 경로 업데이트
  ```cmake
  set(FONT_FILE ${CMAKE_SOURCE_DIR}/resources/fonts/unifont.ttf)
  ```

### 6단계: #include 경로 수정
- [ ] 모든 소스 파일에서 include 경로 수정
  ```cpp
  // 변경 전
  #include "Application.h"
  #include "DataTypes.h"
  
  // 변경 후
  #include "plc_emulator/core/application.h"
  #include "plc_emulator/core/data_types.h"
  ```

- [ ] 검색 및 치환 사용 권장
  ```
  Application.h → plc_emulator/core/application.h
  DataTypes.h → plc_emulator/core/data_types.h
  ProgrammingMode.h → plc_emulator/programming/programming_mode.h
  # ... (18개 헤더 모두)
  ```

### 7단계: .gitignore 업데이트
```gitignore
# 빌드 출력
build/
cmake-build-*/
out/

# 실행 파일
*.exe
*.out
*.app
*.o
*.obj

# 라이브러리
*.lib
*.a
*.so
*.dylib
*.dll

# IDE 설정
.idea/
.vscode/
.vs/
*.suo
*.user
*.sln.docstates

# OS 파일
.DS_Store
Thumbs.db
desktop.ini

# 컴파일 데이터베이스 (생성되므로 제외)
compile_commands.json
```

### 8단계: 스크립트 작성
- [ ] scripts/format_all.sh
  ```bash
  #!/bin/bash
  find include src -name "*.h" -o -name "*.cpp" | xargs clang-format -i
  ```

- [ ] scripts/format_all.bat
  ```batch
  @echo off
  for /r include %%f in (*.h) do clang-format -i "%%f"
  for /r src %%f in (*.cpp) do clang-format -i "%%f"
  ```

- [ ] scripts/run_tests.sh
  ```bash
  #!/bin/bash
  cd build
  ctest --output-on-failure
  ```

### 9단계: 빌드 및 테스트
- [ ] 빌드 디렉토리 정리
  ```bash
  rm -rf build cmake-build-*
  mkdir build && cd build
  ```

- [ ] CMake 실행
  ```bash
  cmake ..
  ```

- [ ] 빌드
  ```bash
  cmake --build .
  ```

- [ ] 실행 테스트
  ```bash
  ./bin/PLCSimulator  # Linux/macOS
  .\bin\PLCSimulator.exe  # Windows
  ```

- [ ] 빌드 경고 확인 및 수정

### 10단계: Git 커밋
- [ ] 변경 사항 확인
  ```bash
  git status
  git diff
  ```

- [ ] 스테이징 및 커밋
  ```bash
  git add .
  git commit -m "refactor: Reorganize directory structure following Google C++ style"
  ```

---

## 예상 작업량 및 우선순위

### 핵심 작업 (필수)
1. **디렉토리 생성 및 파일 이동**
2. **CMakeLists.txt 업데이트**
3. **#include 경로 수정** (가장 많은 시간 소요)
4. **빌드 확인 및 수정**

### 부가 작업 (권장)
5. 문서 작성 (README, architecture.md)
6. .gitignore 업데이트
7. 스크립트 작성

### 선택 작업 (향후)
8. 테스트 코드 추가
9. 예제 프로젝트 추가
10. CI/CD 스크립트

---

## 주의사항

1. **점진적 마이그레이션**
   - 한 번에 모든 파일을 이동하지 말 것
   - 기능별로 단계적 이동 (예: Core → Wiring → Programming → ...)
   - 각 단계마다 빌드 확인

2. **백업 필수**
   - Git branch 생성 또는 전체 프로젝트 백업
   - 실수 시 되돌릴 수 있도록

3. **IDE 설정 업데이트**
   - CLion: CMake 다시 로드
   - Visual Studio: 솔루션 다시 생성
   - include 경로가 올바르게 인식되는지 확인

4. **헤더 가드 업데이트**
   - 파일 경로가 바뀌므로 헤더 가드도 업데이트 필요
   ```cpp
   // 변경 전
   #ifndef APPLICATION_H_
   #define APPLICATION_H_
   
   // 변경 후
   #ifndef PLC_EMULATOR_CORE_APPLICATION_H_
   #define PLC_EMULATOR_CORE_APPLICATION_H_
   ```

5. **빌드 시스템 캐시**
   - CMake 캐시 삭제 후 다시 생성
   - 빌드 디렉토리 완전 삭제 후 재생성 권장

---

## 참고 자료

1. **Google C++ Project Structure**
   - https://google.github.io/styleguide/cppguide.html

2. **CMake Best Practices**
   - https://cliutils.gitlab.io/modern-cmake/

3. **Pitchfork Layout (C++ 프로젝트 표준)**
   - https://github.com/vector-of-bool/pitchfork

4. **LLVM Project Structure** (참고)
   - https://llvm.org/docs/GettingStarted.html

---

*이 문서는 Phase 0: 디렉토리 구조 재구성의 상세 가이드입니다.*
c