cmake_minimum_required(VERSION 3.20)
project(PLCSimulator)

# CMake
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # -std=c++20  -std=gnu++20 

# Export compile_commands.json for clang-tidy, clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified: defaulting to Release")
endif()

#  
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

# CMake 
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(CMAKE_VERSION VERSION_LESS 3.5)
    cmake_policy(VERSION 3.5)
endif()

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.89.2)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.2)
FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git GIT_TAG 3.0.2)
FetchContent_Declare(box2d GIT_REPOSITORY https://github.com/erincatto/box2d.git GIT_TAG v3.1.1)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glad imgui nlohmann_json miniz box2d)
# Suppress warnings from Box2D.
if(TARGET box2d::box2d)
    get_target_property(BOX2D_TARGET box2d::box2d ALIASED_TARGET)
endif()
if(NOT BOX2D_TARGET AND TARGET box2d)
    set(BOX2D_TARGET box2d)
endif()
if(BOX2D_TARGET)
    if(MSVC)
        target_compile_options(${BOX2D_TARGET} PRIVATE /W0)
    else()
        target_compile_options(${BOX2D_TARGET} PRIVATE -w)
    endif()
    get_target_property(BOX2D_INCLUDES ${BOX2D_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    if(BOX2D_INCLUDES)
        target_include_directories(${BOX2D_TARGET} SYSTEM INTERFACE ${BOX2D_INCLUDES})
    endif()
endif()


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
        src/application/app_layout.cpp
        src/application/app_lod.cpp
        src/application/app_input.cpp
        src/application/app_debug.cpp
        src/application/app_render.cpp
        src/application/app_project.cpp
        src/application/app_lifecycle.cpp
        src/core/win32_input.cpp
        src/core/ui_settings.cpp
        src/core/data_types.cpp
        src/core/plc_simulator_core.cpp
        src/core/window_manager.cpp
        src/core/render_manager.cpp
        src/core/input_handler.cpp
        src/core/component_manager.cpp

        # Components
        src/components/component_registry.cpp
        src/components/component_behavior.cpp
        src/components/button_unit_def.cpp
        src/components/box_def.cpp
        src/components/conveyor_def.cpp
        src/components/cylinder_def.cpp
        src/components/emergency_stop_def.cpp
        src/components/frl_def.cpp
        src/components/inductive_sensor_def.cpp
        src/components/limit_switch_def.cpp
        src/components/manifold_def.cpp
        src/components/meter_valve_def.cpp
        src/components/plc_def.cpp
        src/components/processing_cylinder_def.cpp
        src/components/power_supply_def.cpp
        src/components/ring_sensor_def.cpp
        src/components/sensor_def.cpp
        src/components/tower_lamp_def.cpp
        src/components/valve_double_def.cpp
        src/components/valve_single_def.cpp
        src/components/workpiece_def.cpp
        src/third_party/nanosvg_impl.cpp

        # Language
        src/lang/lang_manager.cpp
        
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
        src/physics/component_physics_adapter.cpp
        src/physics/physics_solvers.cpp
        src/physics/application_physics.cpp
        src/application_physics/physics_task_runner.cpp
        src/application_physics/physics_update.cpp
        src/application_physics/physics_box2d.cpp
        src/application_physics/physics_electrical.cpp
        src/application_physics/physics_pneumatic.cpp
        src/application_physics/physics_actuators.cpp
        src/application_physics/physics_workpiece.cpp
        src/application_physics/physics_ports.cpp
        src/application_physics/physics_plc.cpp
        src/application_physics/physics_sync.cpp
        src/application_physics/physics_stats.cpp
        
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
    src/core/win32_input.cpp
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resources/icon/app_icon.rc)
endif()

set(FONT_FILE ${CMAKE_SOURCE_DIR}/resources/fonts/unifont.ttf)
if(EXISTS ${FONT_FILE})
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FONT_FILE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

set(JP_FONT_FILE ${CMAKE_SOURCE_DIR}/resources/fonts/JP.ttf)
if(EXISTS ${JP_FONT_FILE})
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${JP_FONT_FILE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

set(LANG_DIR ${CMAKE_SOURCE_DIR}/resources/lang)
if(EXISTS ${LANG_DIR})
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/lang"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${LANG_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/lang"
    )
endif()

set(ICON_DIR ${CMAKE_SOURCE_DIR}/resources/icon)
if(EXISTS ${ICON_DIR})
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/icon"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ICON_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/icon"
    )
endif()

set(SYMBOL_DIR ${CMAKE_SOURCE_DIR}/resources/symbol)
if(EXISTS ${SYMBOL_DIR})
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/symbol"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SYMBOL_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources/symbol"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${glad_SOURCE_DIR}/include
        ${glfw_SOURCE_DIR}/include
        ${nlohmann_json_SOURCE_DIR}/include
        ${miniz_SOURCE_DIR}
)

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# ImGui 
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

set(LINK_LIBS
        glfw
        imgui_lib
        glad
        miniz
        box2d
        OpenGL::GL
)

if(WIN32)
    # Windows API (PowerShell .NET 
    list(APPEND LINK_LIBS 
        comdlg32     #      
        winmm
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBS})

# Windows 
# Release , Debug 
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE  # GUI   ()
    )
endif()

# =============================================================================
#   (Google C++ Style Guide 
# =============================================================================

if(MSVC)
    # MSVC (Visual Studio)  
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4                 #  4 ()
        /WX                 # (
        /permissive-        #  
        /Zc:__cplusplus     # __cplusplus 
        /Zc:inline          #  
        /utf-8              #   UTF-8         
        #  (Release)
        $<$<CONFIG:Release>:/O2>        #          $<$<CONFIG:Release>:/GL>        #          
        #  (Debug)
        $<$<CONFIG:Debug>:/Od>          # 
        $<$<CONFIG:Debug>:/Zi>          #  
        $<$<CONFIG:Debug>:/RTC1>        # 
    )
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS         # CRT 
        NOMINMAX                        # Windows.h min/max 
        WIN32_LEAN_AND_MEAN            # Windows          _UNICODE                        # 
        UNICODE
        
        # 
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
    
    # (Release)
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG>      # 
        $<$<CONFIG:Release>:/OPT:REF>   #    
        $<$<CONFIG:Release>:/OPT:ICF>   #   
    )
    
else()
    # GCC/Clang  
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall               #         -Wextra             #  
        -Wpedantic          #          -Werror             # (
        
        # Google C++ Style 
        -Wshadow                    #  
        -Wnon-virtual-dtor          #  
        -Wold-style-cast            # C 
        -Wcast-align                #  
        -Wunused                    #   
        -Woverloaded-virtual        #  
        -Wformat=2                  # printf         -Wimplicit-fallthrough      # switch fallthrough 
        
        #  (Release)
        $<$<CONFIG:Release>:-O3>                      #          $<$<CONFIG:Release>:-DNDEBUG>                 # assert
        $<$<CONFIG:Release>:-Wno-array-bounds>        #   false positive 
        $<$<CONFIG:Release>:-Wno-stringop-overflow>   #   false positive 
        
        #  (Debug)
        $<$<CONFIG:Debug>:-O0>          # 
        $<$<CONFIG:Debug>:-g3>          #  
        $<$<CONFIG:Debug>:-D_DEBUG>     #     
    )
    
    #  ()
    # target_link_options(${PROJECT_NAME} PRIVATE
    #     # Release: 
    #     $<$<CONFIG:Release>:-s>         #  ( 
    #     $<$<CONFIG:Release>:-mwindows>  # GUI ()
    # )
endif()

# =============================================================================
# clang-format  (
# =============================================================================

#   
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.cc
    ${CMAKE_SOURCE_DIR}/include/plc_emulator/*.h
    ${CMAKE_SOURCE_DIR}/include/plc_emulator/*.hpp
)

# clang-format 
find_program(CLANG_FORMAT NAMES clang-format)

if(CLANG_FORMAT)
    message(STATUS "clang-format found: ${CLANG_FORMAT}")
    
    # format    
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on all source files"
        VERBATIM
    )
    
    # format-check  (CI/CD
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
# clang-tidy  ( 
# =============================================================================

find_program(CLANG_TIDY NAMES clang-tidy)

if(CLANG_TIDY)
    message(STATUS "clang-tidy found: ${CLANG_TIDY}")
    
    # clang-tidy   (
    option(ENABLE_CLANG_TIDY "Enable clang-tidy checks during build" OFF)
    
    if(ENABLE_CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY 
            ${CLANG_TIDY};
            -header-filter=${CMAKE_SOURCE_DIR}/include/plc_emulator/*;
            -checks=*,-fuchsia-*,-google-readability-todo,-modernize-use-trailing-return-type;
        )
        message(STATUS "clang-tidy enabled for build")
    endif()
    
    # tidy   clang-tidy 
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
# cppcheck  ( 
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
#  
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
# 
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
message(STATUS "For strict warning-free builds:")
message(STATUS "  Warnings as errors enabled with /WX (MSVC) or -Werror (GCC/Clang)")
message(STATUS "")
message(STATUS "=============================================================================")
message(STATUS "")

# =============================================================================
#  (Testing Configuration)
# =============================================================================

option(BUILD_TESTING "Build the testing tree." OFF)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Testing enabled - tests will be built")
endif()

# =============================================================================
