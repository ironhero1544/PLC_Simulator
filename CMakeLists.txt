cmake_minimum_required(VERSION 3.20)
project(PLCSimulator)

# CMake 호환성 정책 설정
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # -std=c++20 사용, -std=gnu++20 방지

# Export compile_commands.json for clang-tidy, clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 빌드 타입 기본값 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified: defaulting to Release")
endif()

# 출력 디렉토리 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
    if(MSVC)
        add_compile_options(/utf-8 /execution-charset:utf-16le)
        add_compile_definitions(_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
    else()
        add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endif()

include(FetchContent)

# CMake 최소 버전 정책을 강제로 설정
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(CMAKE_VERSION VERSION_LESS 3.5)
    cmake_policy(VERSION 3.5)
endif()

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.89.2)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.2)
FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git GIT_TAG 3.0.2)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glad imgui nlohmann_json miniz)

add_library(imgui_lib
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${glfw_SOURCE_DIR}/include
)

find_package(OpenGL REQUIRED)

add_executable(${PROJECT_NAME}
        # Main entry point
        src/main.cpp
        
        # Core
        src/core/application.cpp
        src/core/data_types.cpp
        src/core/plc_simulator_core.cpp
        src/core/window_manager.cpp
        src/core/render_manager.cpp
        src/core/input_handler.cpp
        src/core/component_manager.cpp
        
        # Wiring
        src/wiring/application_components.cpp
        src/wiring/application_wiring.cpp
        src/wiring/application_ports.cpp
        src/wiring/application_snap.cpp
        
        # Programming
        src/programming/programming_mode.cpp
        src/programming/programming_mode_sim.cpp
        src/programming/programming_mode_edit.cpp
        src/programming/programming_mode_ui.cpp
        src/programming/compiled_plc_executor.cpp
        
        # Physics
        src/physics/physics_engine.cpp
        src/physics/physics_solvers.cpp
        src/physics/application_physics.cpp
        
        # I/O
        src/io/io_mapper.cpp
        
        # Data
        src/data/data_repository.cpp
        src/data/event_system.cpp
        src/data/memory_manager.cpp
        
        # Project
        src/project/project_file_manager.cpp
        src/project/xml_serializer.cpp
        src/project/ladder_to_ld_converter.cpp
        src/project/ld_to_ladder_converter.cpp
        src/project/ladder_ir.cpp
        src/project/openplc_compiler_integration.cpp
)

set(FONT_FILE ${CMAKE_SOURCE_DIR}/resources/fonts/unifont.ttf)
if(EXISTS ${FONT_FILE})
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FONT_FILE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${glad_SOURCE_DIR}/include
        ${glfw_SOURCE_DIR}/include
        ${nlohmann_json_SOURCE_DIR}/include
        ${miniz_SOURCE_DIR}
)

# ImGui를 시스템 헤더로 처리하여 경고 비활성화
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

set(LINK_LIBS
        glfw
        imgui_lib
        glad
        miniz
        OpenGL::GL
)

if(WIN32)
    # Windows API 라이브러리 (PowerShell .NET 방식 사용)
    list(APPEND LINK_LIBS 
        comdlg32     # 파일 다이얼로그
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBS})

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# =============================================================================
# 컴파일러 경고 및 최적화 설정 (Google C++ Style Guide 준수)
# =============================================================================

if(MSVC)
    # MSVC (Visual Studio) 컴파일러 설정
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4                 # 경고 레벨 4 (높음)
        /WX                 # 경고를 에러로 처리 (옵션)
        /permissive-        # 표준 준수 모드
        /Zc:__cplusplus     # __cplusplus 매크로 올바른 값 설정
        /Zc:inline          # 인라인 함수 처리 개선
        /utf-8              # 소스 파일 UTF-8 인코딩
        
        # 성능 최적화 (Release)
        $<$<CONFIG:Release>:/O2>        # 속도 최적화
        $<$<CONFIG:Release>:/GL>        # 전체 프로그램 최적화
        
        # 디버그 정보 (Debug)
        $<$<CONFIG:Debug>:/Od>          # 최적화 비활성화
        $<$<CONFIG:Debug>:/Zi>          # 디버그 정보 생성
        $<$<CONFIG:Debug>:/RTC1>        # 런타임 체크
    )
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS         # CRT 보안 경고 억제
        NOMINMAX                        # Windows.h min/max 매크로 비활성화
        WIN32_LEAN_AND_MEAN            # Windows 헤더 최소화
        _UNICODE                        # 유니코드 빌드
        UNICODE
        
        # 빌드 타입별 정의
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
    
    # 링커 최적화 (Release)
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG>      # 링크 타임 코드 생성
        $<$<CONFIG:Release>:/OPT:REF>   # 참조되지 않는 함수 제거
        $<$<CONFIG:Release>:/OPT:ICF>   # 동일 함수 병합
    )
    
else()
    # GCC/Clang 컴파일러 설정
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall               # 기본 경고 활성화
        -Wextra             # 추가 경고
        -Wpedantic          # 표준 준수 검사
        -Werror             # 경고를 에러로 처리 (옵션)
        
        # Google C++ Style 권장 경고
        -Wshadow            # 변수 섀도잉 경고
        -Wnon-virtual-dtor  # 가상 소멸자 누락 경고
        -Wold-style-cast    # C 스타일 캐스트 경고
        -Wcast-align        # 정렬 문제 경고
        -Wunused            # 사용하지 않는 변수 경고
        -Woverloaded-virtual # 가상 함수 오버로딩 경고
        # TODO: Fix conversion warnings after refactoring
        # -Wconversion        # 암시적 형변환 경고
        # -Wsign-conversion   # 부호 변환 경고
        -Wformat=2          # printf 포맷 검사
        -Wimplicit-fallthrough # switch fallthrough 경고
        
        # 성능 최적화 (Release)
        $<$<CONFIG:Release>:-O3>        # 최대 최적화
        $<$<CONFIG:Release>:-DNDEBUG>   # assert 비활성화
        
        # 디버그 정보 (Debug)
        $<$<CONFIG:Debug>:-O0>          # 최적화 비활성화
        $<$<CONFIG:Debug>:-g3>          # 최대 디버그 정보
        $<$<CONFIG:Debug>:-D_DEBUG>     # 디버그 매크로
        
        # AddressSanitizer (Debug 빌드 옵션)
        # $<$<CONFIG:Debug>:-fsanitize=address>
        # $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    
    # 링커 최적화 (Release)
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:-s>         # 심볼 제거
        # AddressSanitizer (Debug 빌드 옵션)
        # $<$<CONFIG:Debug>:-fsanitize=address>
    )
endif()

# =============================================================================
# clang-format 통합 (코드 포매팅)
# =============================================================================

# 모든 소스 파일 수집
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.cc
    ${CMAKE_SOURCE_DIR}/include/plc_emulator/*.h
    ${CMAKE_SOURCE_DIR}/include/plc_emulator/*.hpp
)

# clang-format 찾기
find_program(CLANG_FORMAT NAMES clang-format)

if(CLANG_FORMAT)
    message(STATUS "clang-format found: ${CLANG_FORMAT}")
    
    # format 타겟: 모든 소스 파일 포매팅
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on all source files"
        VERBATIM
    )
    
    # format-check 타겟: 포매팅 검사만 (CI/CD용)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror -style=file ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking code formatting (no changes)"
        VERBATIM
    )
else()
    message(WARNING "clang-format not found. Install it for code formatting support.")
    message(WARNING "  Windows: Download from https://releases.llvm.org/")
    message(WARNING "  Linux: sudo apt install clang-format")
    message(WARNING "  macOS: brew install clang-format")
endif()

# =============================================================================
# clang-tidy 통합 (정적 분석)
# =============================================================================

find_program(CLANG_TIDY NAMES clang-tidy)

if(CLANG_TIDY)
    message(STATUS "clang-tidy found: ${CLANG_TIDY}")
    
    # clang-tidy 자동 실행 (빌드 시)
    option(ENABLE_CLANG_TIDY "Enable clang-tidy checks during build" OFF)
    
    if(ENABLE_CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY 
            ${CLANG_TIDY};
            -header-filter=${CMAKE_SOURCE_DIR}/include/plc_emulator/*;
            -checks=*,-fuchsia-*,-google-readability-todo,-modernize-use-trailing-return-type;
        )
        message(STATUS "clang-tidy enabled for build")
    endif()
    
    # tidy 타겟: 수동으로 clang-tidy 실행
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            ${ALL_SOURCE_FILES}
            -header-filter=${CMAKE_SOURCE_DIR}/include/plc_emulator/*
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
        VERBATIM
    )
else()
    message(WARNING "clang-tidy not found. Install it for static analysis support.")
endif()

# =============================================================================
# cppcheck 통합 (정적 분석)
# =============================================================================

find_program(CPPCHECK NAMES cppcheck)

if(CPPCHECK)
    message(STATUS "cppcheck found: ${CPPCHECK}")
    
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK}
            --enable=all
            --std=c++20
            --language=c++
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --suppress=unmatchedSuppression
            --inline-suppr
            --quiet
            -I ${CMAKE_SOURCE_DIR}/include/plc_emulator
            ${CMAKE_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
        VERBATIM
    )
else()
    message(WARNING "cppcheck not found. Install it for additional static analysis.")
endif()

# =============================================================================
# 빌드 정보 출력
# =============================================================================

message(STATUS "=============================================================================")
message(STATUS "PLC Simulator Configuration Summary")
message(STATUS "=============================================================================")
message(STATUS "  CMake version:        ${CMAKE_VERSION}")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:         C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:             ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Compiler path:        ${CMAKE_CXX_COMPILER}")
message(STATUS "  Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output directory:     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "-----------------------------------------------------------------------------")
message(STATUS "  clang-format:         ${CLANG_FORMAT}")
message(STATUS "  clang-tidy:           ${CLANG_TIDY}")
message(STATUS "  cppcheck:             ${CPPCHECK}")
message(STATUS "=============================================================================")

# =============================================================================
# 사용자 가이드
# =============================================================================

message(STATUS "")
message(STATUS "Available CMake targets:")
message(STATUS "  make / ninja           - Build the project")
message(STATUS "  make format            - Format all source files with clang-format")
message(STATUS "  make format-check      - Check formatting without changes (CI/CD)")
message(STATUS "  make tidy              - Run clang-tidy static analysis")
message(STATUS "  make cppcheck          - Run cppcheck static analysis")
message(STATUS "")
message(STATUS "To enable clang-tidy during build:")
message(STATUS "  cmake -DENABLE_CLANG_TIDY=ON ..")
message(STATUS "")
message(STATUS "For strict warning-free builds (warnings as errors):")
message(STATUS "  Already enabled with /WX (MSVC) or -Werror (GCC/Clang)")
message(STATUS "")
message(STATUS "To build with AddressSanitizer (Debug mode):")
message(STATUS "  Uncomment ASan options in CMakeLists.txt")
message(STATUS "=============================================================================")
message(STATUS "")