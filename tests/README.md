# PLC Emulator Tests
# PLC 에뮬레이터 테스트

이 디렉토리는 Google Test를 사용한 단위 테스트 및 통합 테스트를 포함합니다.

## 디렉토리 구조 (Directory Structure)

```
tests/
├── unit/                 # 단위 테스트 (Unit tests)
│   ├── test_data_types.cc
│   └── test_component_manager.cc
├── integration/          # 통합 테스트 (Integration tests)
│   └── test_wiring_integration.cc
├── fixtures/            # 테스트 픽스처 (Test fixtures)
├── test_main.cc         # Google Test 메인 진입점
├── CMakeLists.txt       # 테스트 빌드 설정
└── README.md            # 이 파일
```

## 빌드 및 실행 (Build & Run)

### 1. CMake 구성 (CMake Configuration)
```bash
cd cmake-build-debug
cmake -DBUILD_TESTING=ON ..
```

### 2. 빌드 (Build)
```bash
cmake --build . --target plc_tests
```

### 3. 테스트 실행 (Run Tests)
```bash
# 직접 실행
./plc_tests

# 또는 CTest 사용
ctest --output-on-failure

# 또는 커스텀 타겟
cmake --build . --target run_tests
```

## 테스트 작성 가이드 (Test Writing Guide)

### 단위 테스트 예제 (Unit Test Example)

```cpp
#include <gtest/gtest.h>
#include "your_header.h"

namespace plc {
namespace {

TEST(YourClassTest, MethodName) {
  // Arrange (준비)
  YourClass obj;
  
  // Act (실행)
  int result = obj.Method();
  
  // Assert (검증)
  EXPECT_EQ(result, expected_value);
}

}  // namespace
}  // namespace plc
```

### 테스트 픽스처 사용 (Using Test Fixtures)

```cpp
class YourClassTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 각 테스트 전 실행
    obj_ = std::make_unique<YourClass>();
  }

  void TearDown() override {
    // 각 테스트 후 실행
    obj_.reset();
  }

  std::unique_ptr<YourClass> obj_;
};

TEST_F(YourClassTest, TestMethod) {
  EXPECT_TRUE(obj_->IsValid());
}
```

## Google Test 단언문 (Assertions)

### 기본 단언 (Basic Assertions)
- `EXPECT_TRUE(condition)` - 조건이 참인지 확인
- `EXPECT_FALSE(condition)` - 조건이 거짓인지 확인
- `EXPECT_EQ(val1, val2)` - 두 값이 같은지 확인
- `EXPECT_NE(val1, val2)` - 두 값이 다른지 확인
- `EXPECT_LT(val1, val2)` - val1 < val2
- `EXPECT_LE(val1, val2)` - val1 <= val2
- `EXPECT_GT(val1, val2)` - val1 > val2
- `EXPECT_GE(val1, val2)` - val1 >= val2

### 부동소수점 비교 (Floating Point)
- `EXPECT_FLOAT_EQ(val1, val2)` - float 비교
- `EXPECT_DOUBLE_EQ(val1, val2)` - double 비교
- `EXPECT_NEAR(val1, val2, abs_error)` - 오차 범위 내 비교

### 문자열 비교 (String Comparison)
- `EXPECT_STREQ(str1, str2)` - C 문자열 비교
- `EXPECT_STRNE(str1, str2)` - C 문자열 다름
- `EXPECT_EQ(str1, str2)` - std::string 비교

### 포인터 확인 (Pointer Checks)
- `EXPECT_EQ(ptr, nullptr)` - nullptr 확인
- `EXPECT_NE(ptr, nullptr)` - nullptr 아님 확인

## 테스트 커버리지 목표 (Coverage Goals)

- [x] DataTypes - 기본 데이터 구조 테스트
- [x] ComponentManager - 컴포넌트 관리 테스트
- [ ] WindowManager - 윈도우 관리 테스트
- [ ] RenderManager - 렌더링 테스트
- [ ] InputHandler - 입력 처리 테스트
- [ ] PhysicsEngine - 물리 엔진 테스트
- [ ] IOMapper - I/O 매핑 테스트
- [ ] ProgrammingMode - 프로그래밍 모드 테스트

## CI/CD 통합 (CI/CD Integration)

이 테스트는 GitHub Actions를 통해 자동으로 실행됩니다:
- Pull Request 시 자동 실행
- main 브랜치 push 시 실행
- 테스트 실패 시 병합 차단

## 참고 자료 (References)

- [Google Test Documentation](https://google.github.io/googletest/)
- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [C++ Testing Best Practices](https://github.com/cpp-best-practices/cppbestpractices)
