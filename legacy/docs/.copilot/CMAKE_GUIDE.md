# CMake 설정 가이드 - PLC Emulator

## 추가된 CMake 기능

### 1. 컴파일러 경고 설정 (강화됨)

#### MSVC (Visual Studio)
- `/W4` - 경고 레벨 4 (최고)
- `/WX` - 경고를 에러로 처리 (경고 0개 빌드 강제)
- `/permissive-` - 표준 준수 모드
- `/Zc:__cplusplus` - C++ 표준 버전 매크로 올바른 설정

#### GCC/Clang
- `-Wall -Wextra -Wpedantic` - 기본 경고
- `-Werror` - 경고를 에러로 처리
- `-Wshadow` - 변수 섀도잉 감지
- `-Wold-style-cast` - C 스타일 캐스트 금지
- `-Wconversion` - 암시적 형변환 경고
- 기타 Google C++ Style Guide 권장 경고들

### 2. 최적화 설정

#### Release 빌드
- MSVC: `/O2 /GL` + `/LTCG /OPT:REF /OPT:ICF` (링커)
- GCC/Clang: `-O3` + `-s` (심볼 제거)

#### Debug 빌드
- 최적화 비활성화 (`/Od` 또는 `-O0`)
- 디버그 심볼 생성 (`/Zi` 또는 `-g3`)
- 런타임 체크 활성화 (MSVC: `/RTC1`)

### 3. 코드 포매팅 (clang-format)

#### 타겟 추가됨
```bash
# 모든 소스 파일 자동 포매팅
cmake --build . --target format

# 또는 (Unix Makefiles)
make format

# 또는 (Ninja)
ninja format
```

#### 포매팅 검사만 (변경하지 않음, CI/CD용)
```bash
make format-check
```

### 4. 정적 분석 (clang-tidy)

#### 수동 실행
```bash
make tidy
```

#### 빌드 시 자동 실행
```bash
cmake -DENABLE_CLANG_TIDY=ON ..
make
```

### 5. 정적 분석 (cppcheck)

```bash
make cppcheck
```

### 6. 컴파일 명령어 DB 생성

`compile_commands.json` 자동 생성 (clangd, clang-tidy용)
- 위치: `build/compile_commands.json`
- IDE가 자동으로 인식

---

## 사용 방법

### 기본 빌드
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 포매팅 적용
```bash
cd build
cmake --build . --target format
```

### 정적 분석 실행
```bash
cd build
cmake --build . --target tidy
cmake --build . --target cppcheck
```

### 엄격 모드 (경고 = 에러)
기본적으로 활성화되어 있습니다. 비활성화하려면:

CMakeLists.txt에서 다음 줄 제거:
- MSVC: `/WX`
- GCC/Clang: `-Werror`

### AddressSanitizer (메모리 오류 검사)

CMakeLists.txt에서 주석 해제:
```cmake
# AddressSanitizer (Debug 빌드 옵션)
$<$<CONFIG:Debug>:-fsanitize=address>
$<$<CONFIG:Debug>:-fno-omit-frame-pointer>
```

그 후:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./bin/PLCSimulator
```

---

## 도구 설치

### Windows
- **clang-format/clang-tidy**: [LLVM releases](https://releases.llvm.org/) 다운로드
- **cppcheck**: [cppcheck.net](http://cppcheck.net/) 다운로드
- PATH에 추가 필요

### Linux (Ubuntu/Debian)
```bash
sudo apt install clang-format clang-tidy cppcheck
```

### macOS
```bash
brew install clang-format llvm cppcheck
```

---

## 빌드 타입

### Release (기본)
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```
- 최적화 활성화
- 디버그 심볼 없음
- 빠른 실행 속도

### Debug
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```
- 최적화 비활성화
- 디버그 심볼 포함
- 디버거 사용 가능

### RelWithDebInfo
```bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```
- 최적화 + 디버그 심볼
- 프로파일링용

---

## CI/CD 통합 예시

```yaml
# .github/workflows/build.yml
- name: Format Check
  run: |
    cmake --build build --target format-check
    
- name: Build with Warnings as Errors
  run: |
    cmake --build build

- name: Run Static Analysis
  run: |
    cmake --build build --target tidy
    cmake --build build --target cppcheck
```

---

## 참고 사항

1. **경고 0개 목표**: `/WX`, `-Werror`로 경고가 있으면 빌드 실패
2. **자동 포매팅**: 커밋 전 `make format` 실행 권장
3. **정적 분석**: 주기적으로 `make tidy` 실행
4. **표준 준수**: C++20 표준 엄격 준수 (`/permissive-`, `-Wpedantic`)

---

## 문제 해결

### clang-format을 찾을 수 없음
```
CMake Warning: clang-format not found
```
→ clang-format 설치 및 PATH 추가

### 경고가 너무 많아 빌드 실패
→ 임시로 `/WX` 또는 `-Werror` 제거 후 점진적 수정

### ASan 사용 시 실행 느림
→ Debug 빌드에서만 사용, Release는 비활성화

---

*CMakeLists.txt 수정 사항은 리팩토링 계획의 일부입니다.*
