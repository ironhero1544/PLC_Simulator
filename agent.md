# AGENTS.md
# Project-specific instructions for PLC_Emulator

## Refactoring rules
- Follow Google C++ Style Guide.
- Do not use dynamic memory allocation (no new/delete, no std::unique_ptr/shared_ptr for new code in this refactor).
- Do not use exceptions (no throw/catch).

## Comment cleanup plan
- Core/physics headers: remove garbled or redundant comments, then rewrite remaining comments as bilingual block comments (`/* ... */`) per the style guide.
- Source files: convert comments to concise English `//` style only, remove non-ASCII and overlong explanations.
- Keep comment density low: document public interfaces, non-obvious logic, and units/assumptions; drop obvious restatements.
- Normalize trailing namespace/include guard comments to block style in headers.
- Re-scan to ensure no header `//` comments and no non-ASCII comments in `.cpp`.

