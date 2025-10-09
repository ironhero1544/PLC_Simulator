cmake_minimum_required(VERSION 3.20)
project(PLCSimulator)

# CMake 호환성 정책 설정
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        src/main.cpp
        src/Application.cpp
        src/Application_Components.cpp
        src/Application_Wiring.cpp
        src/Application_Ports.cpp
        src/Application_Snap.cpp
        src/Application_Physics.cpp
        src/ProgrammingMode.cpp
        src/DataTypes.cpp
        src/ProgrammingMode_Sim.cpp
        src/ProgrammingMode_Edit.cpp
        src/ProgrammingMode_UI.cpp
        src/PLCSimulatorCore.cpp
        src/DataRepository.cpp
        src/EventSystem.cpp
        src/IOMapper.cpp
        # === Phase 3: 새로운 물리엔진 소스파일들 ===
        src/PhysicsSolvers.cpp
        src/PhysicsEngine.cpp
        # === Phase 5: OpenPLC 통합 소스파일들 ===
        src/LadderToLDConverter.cpp
        src/LDToLadderConverter.cpp
        src/OpenPLCCompilerIntegration.cpp
        src/CompiledPLCExecutor.cpp
        # === Phase 6: XML 직렬화 엔진 ===
        src/XMLSerializer.cpp
        # === Phase 7: 프로젝트 파일 관리자 ===
        src/ProjectFileManager.cpp
        # === Phase 8: LadderIR 중간 표현 엔진 ===
        src/LadderIR.cpp
        src/main.cpp
)

set(FONT_FILE ${CMAKE_SOURCE_DIR}/unifont.ttf)
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

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()