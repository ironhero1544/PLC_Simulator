# Legacy Code | 레거시 코드

This directory contains the original code structure before Phase 0 refactoring.
이 디렉토리는 Phase 0 리팩토링 이전의 원본 코드 구조를 포함합니다.

## Purpose | 목적

These files are kept for reference during the refactoring process.
이 파일들은 리팩토링 과정 중 참조용으로 보관됩니다.

- **DO NOT BUILD** from this directory | 이 디렉토리에서 빌드하지 마세요
- Use as **REFERENCE ONLY** | **참조 전용**으로만 사용하세요
- Compare with new structure when needed | 필요시 새 구조와 비교하세요

## Original Structure | 원본 구조

```
legacy/
├── include/           # Original header files (18 files)
│                      # 원본 헤더 파일 (18개)
├── src/              # Original source files (24 files)
│                      # 원본 소스 파일 (24개)
├── resources/        # Original resources
│   └── unifont.ttf   # 원본 리소스
└── docs/
    └── .copilot/     # Original planning docs
                       # 원본 계획 문서
```

## New Structure | 새 구조

The new structure is in the project root:
새 구조는 프로젝트 루트에 있습니다:

```
include/plc_emulator/
├── core/             # Core functionality
├── wiring/           # Wiring mode
├── programming/      # Programming mode
├── physics/          # Physics engine
├── io/              # I/O mapping
├── data/            # Data management
└── project/         # Project management

src/
├── core/            # Core implementation
├── wiring/          # Wiring implementation
├── programming/     # Programming implementation
├── physics/         # Physics implementation
├── io/             # I/O implementation
├── data/           # Data implementation
└── project/        # Project implementation
```

## Migration Date | 마이그레이션 날짜

Phase 0 completed: 2024
Phase 0 완료: 2024

## Can I Delete This? | 삭제해도 되나요?

**After refactoring is complete and stable**, you can safely delete this directory.
**리팩토링이 완료되고 안정화된 후**, 이 디렉토리를 안전하게 삭제할 수 있습니다.

Recommended to keep until:
다음까지는 보관 권장:
- All refactoring phases complete (Phase 0-10) | 모든 리팩토링 단계 완료 (Phase 0-10)
- Code is in production | 코드가 프로덕션에 배포됨
- No major bugs found | 주요 버그 없음

---

*This is reference code only. Do not modify or build from here.*
*이것은 참조 코드입니다. 여기서 수정하거나 빌드하지 마세요.*
