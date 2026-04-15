cmake_minimum_required(VERSION 3.20)
project(PLCSimulator VERSION 1.0.4)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Windows 네이티브 RTL 도구 설정 (Offline Mode - verilator.zst 사용)
# ============================================================================
if(WIN32 AND NOT MSVC)
    # Tools 디렉토리 경로
    get_filename_component(TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools" ABSOLUTE)
    set(SETUP_TOOLS_SCRIPT "${TOOLS_DIR}/setup_tools.ps1")
    
    # MinGW64와 Verilator 경로
    set(MINGW64_PATH "${TOOLS_DIR}/mingw64")
    set(VERILATOR_PATH "${TOOLS_DIR}/verilator")
    set(VERILATOR_ZST "${TOOLS_DIR}/verilator.zst")
    
    # 도구가 없으면 자동으로 설정 스크립트 실행
    if((NOT EXISTS "${MINGW64_PATH}/bin/g++.exe" OR NOT EXISTS "${VERILATOR_PATH}/bin/verilator.exe") AND EXISTS "${SETUP_TOOLS_SCRIPT}")
        message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        message(STATUS "RTL Tools Setup Required (Offline Mode)")
        message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        message(STATUS "Detected missing tools. Running setup_tools.ps1...")
        message(STATUS "This will extract verilator.zst and setup MinGW64.")
        message(STATUS "")
        
        # PowerShell로 설정 스크립트 실행
        execute_process(
            COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${SETUP_TOOLS_SCRIPT}"
            RESULT_VARIABLE SETUP_RESULT
            OUTPUT_VARIABLE SETUP_OUTPUT
            ERROR_VARIABLE SETUP_ERROR
        )
        
        if(NOT SETUP_RESULT EQUAL 0)
            message(WARNING "Setup script exited with code: ${SETUP_RESULT}")
            message(WARNING "Output: ${SETUP_OUTPUT}")
            message(WARNING "Error: ${SETUP_ERROR}")
        else()
            message(STATUS "✓ Setup completed successfully")
        endif()
        
        message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    endif()
    
    # 도구 경로 검증
    if(EXISTS "${MINGW64_PATH}/bin/g++.exe")
        message(STATUS "✓ MinGW64 found at: ${MINGW64_PATH}")
        if(NOT CMAKE_C_COMPILER)
            set(CMAKE_C_COMPILER "${MINGW64_PATH}/bin/gcc.exe" CACHE PATH "C Compiler")
        endif()
        if(NOT CMAKE_CXX_COMPILER)
            set(CMAKE_CXX_COMPILER "${MINGW64_PATH}/bin/g++.exe" CACHE PATH "C++ Compiler")
        endif()
    else()
        message(WARNING "MinGW64 not found at: ${MINGW64_PATH}")
        message(WARNING "Run: powershell -NoProfile -ExecutionPolicy Bypass -File \"${SETUP_TOOLS_SCRIPT}\"")
    endif()
    
    if(EXISTS "${VERILATOR_PATH}/bin/verilator.exe")
        message(STATUS "✓ Verilator found at: ${VERILATOR_PATH}")
        set(VERILATOR_ROOT "${VERILATOR_PATH}/share/verilator" CACHE PATH "Verilator installation root")
        message(STATUS "  VERILATOR_ROOT set to: ${VERILATOR_ROOT}")
    else()
        message(WARNING "Verilator not found at: ${VERILATOR_PATH}")
        if(EXISTS "${VERILATOR_ZST}")
            message(STATUS "Note: verilator.zst found - will be extracted by setup script")
        endif()
    endif()
endif()

option(BUILD_MAIN_APP "Build main PLC simulator app" ON)
option(BUILD_HAPTICS_BRIDGE "Build WinRT haptics bridge DLL" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks during build" OFF)
option(BUILD_TESTING "Build tests" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
    if(MSVC)
        add_compile_options(/utf-8)
        add_compile_definitions(_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
    else()
        add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endif()

include(FetchContent)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
if(CMAKE_VERSION VERSION_LESS 3.5)
    cmake_policy(VERSION 3.5)
endif()

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.89.2)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.2)
FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz.git GIT_TAG 3.0.2)
FetchContent_Declare(box2d GIT_REPOSITORY https://github.com/erincatto/box2d.git GIT_TAG v3.1.1)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if(BUILD_MAIN_APP)
    FetchContent_MakeAvailable(glfw imgui nlohmann_json miniz box2d)

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

    configure_file(
            ${glfw_SOURCE_DIR}/deps/glad_gl.c
            ${CMAKE_CURRENT_BINARY_DIR}/generated/glad_gl.c
            COPYONLY
    )

    add_library(glad ${CMAKE_CURRENT_BINARY_DIR}/generated/glad_gl.c)
    target_include_directories(glad PUBLIC ${glfw_SOURCE_DIR}/deps)

    set(IMGUI_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/imgui)
    file(MAKE_DIRECTORY ${IMGUI_GENERATED_DIR})
    foreach(IMGUI_FILE
            imgui.cpp
            imgui_demo.cpp
            imgui_draw.cpp
            imgui_tables.cpp
            imgui_widgets.cpp
            backends/imgui_impl_glfw.cpp
            backends/imgui_impl_opengl3.cpp)
        get_filename_component(IMGUI_SUBDIR ${IMGUI_FILE} DIRECTORY)
        if(IMGUI_SUBDIR)
            file(MAKE_DIRECTORY ${IMGUI_GENERATED_DIR}/${IMGUI_SUBDIR})
        endif()
        configure_file(
                ${imgui_SOURCE_DIR}/${IMGUI_FILE}
                ${IMGUI_GENERATED_DIR}/${IMGUI_FILE}
                COPYONLY
        )
    endforeach()

    add_library(imgui_lib
            ${IMGUI_GENERATED_DIR}/imgui.cpp
            ${IMGUI_GENERATED_DIR}/imgui_demo.cpp
            ${IMGUI_GENERATED_DIR}/imgui_draw.cpp
            ${IMGUI_GENERATED_DIR}/imgui_tables.cpp
            ${IMGUI_GENERATED_DIR}/imgui_widgets.cpp
            ${IMGUI_GENERATED_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_GENERATED_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(imgui_lib PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${glfw_SOURCE_DIR}/include
    )

    find_package(OpenGL REQUIRED)

    add_executable(PLCSimulator
            src/main.cpp

            src/core/application.cpp
            src/application/app_layout.cpp
            src/application/app_lod.cpp
            src/application/app_input.cpp
            src/application/app_debug.cpp
            src/application/app_render.cpp
            src/application/app_project.cpp
            src/application/app_rtl.cpp
            src/application/app_lifecycle.cpp
            src/core/win32_input.cpp
            src/core/win32_haptics_stub.cpp
            src/core/ui_settings.cpp
            src/core/data_types.cpp
            src/core/plc_simulator_core.cpp
            src/core/window_manager.cpp
            src/core/render_manager.cpp
            src/core/input_handler.cpp
            src/core/component_manager.cpp
            src/input/platform_input_collector.cpp
            src/input/ui_capture_registry.cpp

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
            src/components/rtl_module_def.cpp
            src/third_party/nanosvg_impl.cpp

            src/lang/lang_manager.cpp

            src/wiring/application_components.cpp
            src/wiring/application_wiring.cpp
            src/wiring/application_ports.cpp
            src/wiring/application_snap.cpp

            src/programming/programming_mode.cpp
            src/programming/programming_mode_sim.cpp
            src/programming/programming_mode_edit.cpp
            src/programming/programming_mode_ui.cpp
            src/programming/compiled_plc_executor.cpp

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

            src/io/io_mapper.cpp

            src/rtl/rtl_project_manager.cpp
            src/rtl/rtl_runtime_manager.cpp

            src/data/data_repository.cpp
            src/data/event_system.cpp
            src/data/memory_manager.cpp

            src/project/project_file_manager.cpp
            src/project/xml_serializer.cpp
            src/project/ladder_to_ld_converter.cpp
            src/project/ld_to_ladder_converter.cpp
            src/project/ladder_ir.cpp
            src/project/openplc_compiler_integration.cpp
    )

    if(WIN32)
        target_sources(PLCSimulator PRIVATE resources/icon/app_icon.rc)
    endif()

    target_include_directories(PLCSimulator PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${glfw_SOURCE_DIR}/deps
            ${glfw_SOURCE_DIR}/include
            ${nlohmann_json_SOURCE_DIR}/include
            ${miniz_SOURCE_DIR}
    )

    target_include_directories(PLCSimulator SYSTEM PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
    )

    set(FONT_DIR ${CMAKE_SOURCE_DIR}/resources/fonts)
    if(EXISTS ${FONT_DIR})
        add_custom_command(
                TARGET PLCSimulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/fonts"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FONT_DIR}"
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/fonts"
        )
    endif()

    set(LANG_DIR ${CMAKE_SOURCE_DIR}/resources/lang)
    if(EXISTS ${LANG_DIR})
        add_custom_command(
                TARGET PLCSimulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/lang"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${LANG_DIR}"
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/lang"
        )
    endif()

    set(ICON_DIR ${CMAKE_SOURCE_DIR}/resources/icon)
    if(EXISTS ${ICON_DIR})
        add_custom_command(
                TARGET PLCSimulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/icon"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ICON_DIR}"
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/icon"
        )
    endif()

    set(SYMBOL_DIR ${CMAKE_SOURCE_DIR}/resources/symbol)
    if(EXISTS ${SYMBOL_DIR})
        add_custom_command(
                TARGET PLCSimulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/symbol"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SYMBOL_DIR}"
                "$<TARGET_FILE_DIR:PLCSimulator>/resources/symbol"
        )
    endif()

    target_link_libraries(PLCSimulator PRIVATE
            glfw
            imgui_lib
            glad
            miniz
            box2d
            OpenGL::GL
            runtimeobject
            comdlg32
            winmm
    )

    if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(PLCSimulator PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    if(MSVC)
        target_compile_options(PLCSimulator PRIVATE
                /W4 /WX /permissive- /Zc:__cplusplus /Zc:inline /utf-8
                $<$<CONFIG:Release>:/O2>
                $<$<CONFIG:Release>:/GL>
                $<$<CONFIG:Debug>:/Od>
                $<$<CONFIG:Debug>:/Zi>
                $<$<CONFIG:Debug>:/RTC1>
        )

        target_compile_definitions(PLCSimulator PRIVATE
                _CRT_SECURE_NO_WARNINGS
                NOMINMAX
                WIN32_LEAN_AND_MEAN
                _UNICODE
                UNICODE
                $<$<CONFIG:Debug>:_DEBUG>
                $<$<CONFIG:Release>:NDEBUG>
        )

        target_link_options(PLCSimulator PRIVATE
                $<$<CONFIG:Release>:/LTCG>
                $<$<CONFIG:Release>:/OPT:REF>
                $<$<CONFIG:Release>:/OPT:ICF>
        )
    else()
        target_compile_options(PLCSimulator PRIVATE
                -Wall -Wextra -Wpedantic -Werror
                -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
                -Wunused -Woverloaded-virtual -Wformat=2 -Wimplicit-fallthrough
                $<$<CONFIG:Release>:-O3>
                $<$<CONFIG:Release>:-DNDEBUG>
                $<$<CONFIG:Release>:-Wno-array-bounds>
                $<$<CONFIG:Release>:-Wno-stringop-overflow>
                $<$<CONFIG:Debug>:-O0>
                $<$<CONFIG:Debug>:-g3>
                $<$<CONFIG:Debug>:-D_DEBUG>
        )

        set_source_files_properties(src/core/win32_input.cpp PROPERTIES
                COMPILE_OPTIONS "-Wno-cast-function-type")
    endif()
endif()

if(WIN32 AND BUILD_HAPTICS_BRIDGE)
    add_subdirectory(haptics_bridge)
endif()

# =============================================================================
# Testing
# =============================================================================

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Install / Packaging
# =============================================================================

if(BUILD_MAIN_APP)
    install(TARGETS PLCSimulator
            RUNTIME DESTINATION .)

    # Bundle required MinGW runtime DLLs for portable install/installer output.
    if(MINGW)
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set(REQUIRED_MINGW_DLLS
                libgcc_s_seh-1.dll
                libstdc++-6.dll
                libwinpthread-1.dll
        )

        foreach(RUNTIME_DLL IN LISTS REQUIRED_MINGW_DLLS)
            set(RUNTIME_DLL_PATH "${MINGW_BIN_DIR}/${RUNTIME_DLL}")
            if(EXISTS "${RUNTIME_DLL_PATH}")
                install(FILES "${RUNTIME_DLL_PATH}"
                        DESTINATION ".")
            else()
                message(WARNING "Required MinGW runtime DLL not found: ${RUNTIME_DLL_PATH}")
            endif()
        endforeach()
    endif()

    if(TARGET haptics_bridge)
        install(TARGETS haptics_bridge
                RUNTIME DESTINATION .
                LIBRARY DESTINATION .
                ARCHIVE DESTINATION .)
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/"
                DESTINATION "resources")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
        install(FILES "${CMAKE_SOURCE_DIR}/README.md"
                DESTINATION ".")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/README_kr.md")
        install(FILES "${CMAKE_SOURCE_DIR}/README_kr.md"
                DESTINATION ".")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/README_ja.md")
        install(FILES "${CMAKE_SOURCE_DIR}/README_ja.md"
                DESTINATION ".")
    endif()

    set(PLC_LICENSE_FILE "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE-GPL-3.0.txt")
        set(PLC_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE-GPL-3.0.txt")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
        set(PLC_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE")
    endif()

    if(PLC_LICENSE_FILE)
        install(FILES "${PLC_LICENSE_FILE}"
                DESTINATION ".")
    endif()

    # =============================================================================
    # CPack
    # =============================================================================

    set(CPACK_PACKAGE_NAME "PLCSimulator")
    set(CPACK_PACKAGE_VENDOR "ironhero")
    set(CPACK_PACKAGE_CONTACT "ironhero1544@users.noreply.github.com")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
            "PLC wiring/programming simulator with custom physics engine")

    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PLCSimulator")
    set(CPACK_MONOLITHIC_INSTALL ON)

    if(PLC_LICENSE_FILE)
        set(CPACK_RESOURCE_FILE_LICENSE "${PLC_LICENSE_FILE}")
    endif()

    if(WIN32)
        set(CPACK_GENERATOR "NSIS;ZIP")
        set(CPACK_PACKAGE_EXECUTABLES "PLCSimulator" "PLCSimulator")
        set(CPACK_CREATE_DESKTOP_LINKS "PLCSimulator")

        set(CPACK_NSIS_DISPLAY_NAME "PLCSimulator")
        set(CPACK_NSIS_PACKAGE_NAME "PLCSimulator")
        set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/ironhero1544")
        set(CPACK_NSIS_HELP_LINK "https://github.com/ironhero1544")
        set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_MODIFY_PATH ON)

        if(EXISTS "${CMAKE_SOURCE_DIR}/resources/icon/icon.ico")
            set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icon/icon.ico")
            set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icon/icon.ico")
            set(CPACK_NSIS_INSTALLED_ICON_NAME "PLCSimulator.exe")
            set(CPACK_NSIS_MUI_FINISHPAGE_RUN "PLCSimulator.exe")
        endif()
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()

    include(CPack)
endif()

message(STATUS "=============================================================================")
message(STATUS "PLC Simulator Configuration Summary")
message(STATUS "=============================================================================")
message(STATUS "  Build main app:       ${BUILD_MAIN_APP}")
message(STATUS "  Build haptics bridge: ${BUILD_HAPTICS_BRIDGE}")
message(STATUS "  Compiler:             ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Compiler path:        ${CMAKE_CXX_COMPILER}")
message(STATUS "  Version:              ${PROJECT_VERSION}")
message(STATUS "=============================================================================")