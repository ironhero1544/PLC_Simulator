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

# 테스트 소스 파일 수집
file(GLOB_RECURSE TEST_SOURCES
  "unit/*.cc"
  "integration/*.cc"
)

# 테스트 실행 파일 생성
add_executable(plc_tests
  test_main.cc
  ${TEST_SOURCES}
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
