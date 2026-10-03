# 테스트 설정

# Google Test 가져오기
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

# Windows에서 gtest 강제로 shared CRT 사용
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 테스트 활성화
enable_testing()

add_executable(ladder_rewrite_tests
  ladder_rewrite_test.cpp
  physical_runtime_test.cpp
  ../src/physics/component_physics_adapter.cpp
  ../src/physics/pneumatic_simulation.cpp
  ../src/physics/box2d_simulation.cpp
  ../src/programming/execution_program.cpp
  ../src/programming/compiled_plc_executor.cpp
  ../src/programming/structured_execution.cpp
  ../src/programming/fx_cpu_execution.cpp
  ../src/programming/flow_execution.cpp
  ../src/programming/float_execution.cpp
  ../src/programming/string_execution.cpp
  ../src/programming/table_execution.cpp
  ../src/programming/utility_execution.cpp
  ../src/programming/module_execution.cpp
  ../src/programming/handy_execution.cpp
  ../src/programming/device_memory.cpp
    ../src/programming/storage_execution.cpp
    ../src/programming/high_speed_execution.cpp
    ../src/programming/panel_execution.cpp
    ../src/programming/serial_execution.cpp
    ../src/programming/ist_execution.cpp
    ../src/programming/sfc_execution.cpp
    ../src/programming/inverter_execution.cpp
    ../src/programming/modbus_execution.cpp
    ../src/programming/cf_execution.cpp
    ../src/programming/cf_storage.cpp
    ../src/programming/cf_transfer.cpp
    ../src/programming/pid_execution.cpp
    ../src/programming/position_table_execution.cpp
    ../src/programming/motion_execution.cpp
    ../src/programming/drum_execution.cpp
  ../src/project/structured_ladder_compiler.cpp
  ../src/project/instruction_codec.cpp
  ../src/project/gx_csv_codec.cpp
  ../src/project/openplc_compiler_integration.cpp
  ../src/project/ladder_ir.cpp
  ../src/project/ladder_to_ld_converter.cpp
  ../src/project/ld_to_ladder_converter.cpp
)
if(NOT TARGET box2d)
  FetchContent_MakeAvailable(box2d)
endif()
target_link_libraries(ladder_rewrite_tests PRIVATE box2d)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()
target_include_directories(ladder_rewrite_tests PRIVATE ../include ${imgui_SOURCE_DIR})
add_test(NAME ladder_rewrite COMMAND ladder_rewrite_tests)
add_test(NAME ladder_runtime COMMAND ladder_rewrite_tests --runtime-only)
add_test(NAME ladder_physical COMMAND ladder_rewrite_tests --physical-only)

if(TARGET PLCSimulator)
  get_target_property(APP_RUNTIME_SOURCES PLCSimulator SOURCES)
  list(FILTER APP_RUNTIME_SOURCES EXCLUDE REGEX "(^|/)src/main\\.cpp$")
  set(APP_RUNTIME_ABSOLUTE_SOURCES)
  foreach(APP_RUNTIME_SOURCE IN LISTS APP_RUNTIME_SOURCES)
    if(IS_ABSOLUTE "${APP_RUNTIME_SOURCE}")
      list(APPEND APP_RUNTIME_ABSOLUTE_SOURCES "${APP_RUNTIME_SOURCE}")
    else()
      list(APPEND APP_RUNTIME_ABSOLUTE_SOURCES
           "${CMAKE_SOURCE_DIR}/${APP_RUNTIME_SOURCE}")
    endif()
  endforeach()
  add_executable(application_runtime_tests
    application_runtime_test.cpp ${APP_RUNTIME_ABSOLUTE_SOURCES})
  foreach(PROPERTY INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS)
    get_target_property(APP_RUNTIME_${PROPERTY} PLCSimulator ${PROPERTY})
    if(APP_RUNTIME_${PROPERTY})
      set_property(TARGET application_runtime_tests PROPERTY ${PROPERTY}
                   ${APP_RUNTIME_${PROPERTY}})
    endif()
  endforeach()
  target_include_directories(application_runtime_tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
  get_target_property(APP_RUNTIME_LIBRARIES PLCSimulator LINK_LIBRARIES)
  target_link_libraries(application_runtime_tests PRIVATE ${APP_RUNTIME_LIBRARIES})
  add_test(NAME application_runtime COMMAND application_runtime_tests)
  set_tests_properties(application_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

# 테스트 소스 파일 수집
file(GLOB_RECURSE TEST_SOURCES
  "unit/*.cc"
  "integration/*.cc"
)

# 테스트 실행 파일 생성
add_executable(plc_tests
  test_main.cc
  ${TEST_SOURCES}
  ../src/core/component_manager.cpp
)

# 테스트 링크 설정
target_link_libraries(plc_tests
  PRIVATE
    gtest
    gtest_main
    # TODO: 실제 프로젝트 라이브러리 추가
)

# 테스트 include 디렉토리
target_include_directories(plc_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# C++20 표준 사용
target_compile_features(plc_tests PRIVATE cxx_std_20)

# 테스트 등록 (CTest 통합)
include(GoogleTest)
gtest_discover_tests(plc_tests)

# 테스트 실행 커스텀 타겟
add_custom_target(run_tests
  COMMAND plc_tests
  DEPENDS plc_tests
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running unit tests..."
)
