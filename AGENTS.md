# Repository Guidelines

## Project Structure & Modules
- `src/`: Core simulator sources (e.g., `PLCSimulatorCore.cpp`, physics, programming modes, compiler integration).
- `include/`: Public headers mirroring `src/` names (e.g., `PhysicsEngine.h`).
- `CMakeLists.txt`: Top-level CMake build configuration.
- Assets: `unifont.ttf` for UI/visuals.
- Artifacts: Built binaries land in your build directory; a local `main.exe` may appear during development.

## Build, Run, and Test
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- Build: `cmake --build build --config Release --parallel`
- Run: `./build/PLC_Emulator` (platform target name) or `./main.exe` if present.
- Tests: If enabled, run `ctest --test-dir build --output-on-failure`. The repository currently has no formal tests; add CTest targets when introducing them.

## Coding Style & Naming
- Language: Modern C++ (C++17+). Prefer standard library over custom utils when possible.
- Indentation: 4 spaces, no tabs. Match surrounding style for braces and spacing.
- Files/Classes: PascalCase (e.g., `DataRepository.cpp`, `PhysicsEngine`). Headers `.h`, sources `.cpp` with mirrored names.
- Functions/Methods: lowerCamelCase. Constants/macros: UPPER_SNAKE_CASE.
- Formatting: If available, use `clang-format` before committing (e.g., `clang-format -i src/*.cpp include/*.h`).

## Testing Guidelines
- Location: Place tests under `tests/` and register with CTest in `CMakeLists.txt`.
- Framework: GoogleTest or Catch2 are acceptable; keep tests fast and deterministic.
- Naming: Mirror module under test (e.g., `tests/PhysicsEngineTests.cpp`). Aim for meaningful coverage on new/changed code.

## Commit & PR Guidelines
- Commits: Small, focused, imperative mood (e.g., "Add PhysicsEngine step integration"). Reference issues (`#123`) when applicable.
- PRs: Include summary, rationale, notable design choices, and screenshots for UI-affecting changes. Link issues, update docs, and note any migration steps.
- Quality: Build cleanly on all supported configs (`Debug/Release`). Add/adjust tests with behavior changes.

## Security & Configuration Tips
- Avoid committing generated binaries or IDE files; honor `.gitignore` and keep secrets/config outside source control.
- For file IO and project state, prefer existing utilities (e.g., `ProjectFileManager`) to maintain consistency and safety.

