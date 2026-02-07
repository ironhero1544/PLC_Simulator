# PLC_Simulator
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)]()
![plcsimulator_lockup_whitebg.svg](docs_img/plcsimulator_lockup_whitebg.svg)
Real-time PLC wiring/programming simulator with integrated physics and OpenPLC-compatible ladder execution.

Read in [한국어](README_kr.md), [日本語](README_ja.md)

[Project History on here](docs/Project_History_en.md)

## Why this project?

PLC training is often constrained by hardware accessibility, time, and cost.  
This project aims to:

- provide wiring + programming + simulation in one environment
- offer a workflow close to real PLC learning
- enable fast iterative practice on a desktop simulator

## It focuses on

- practical workflow: build wiring, write ladder logic, and verify behavior in one place
- extensibility: modular architecture across components/physics/programming
- performance: LOD, viewport culling, and spatial partitioning optimizations
- realism: OpenPLC-based conversion/execution path with Box2D integration

## Features

- Wiring mode
- Ladder programming mode
- Monitor mode (I/O, timer, and counter inspection)
- PLC design based on Mitsubishi FX3U-32M
- Instruction support: `XIC`, `XIO`, `OTE`, `SET`, `RST`, `TON`, `CTU`, `RST_TMR_CTR`, `BKRST`
- OpenPLC-compatible LD conversion/compilation pipeline
- Physics simulation (electrical / pneumatic / mechanical + workpiece interaction)
- Box2D integration for collisions and interactions
- Multi-language resources (`resources/lang`: ko/en/ja)
- Project save/load

## Prerequisites

- CPU: 4 threads or more
- RAM: 2 GB minimum
- CMake >= 3.20
- C++20 compiler
- OpenGL runtime
- Git (for FetchContent dependency download)

## Third-party libraries

- GLFW
- GLAD
- Dear ImGui
- nlohmann/json
- miniz
- Box2D
- NanoSVG

## Tech Stack

| Area | Stack |
|---|---|
| Language | C++20 |
| Build | CMake |
| Rendering/UI | OpenGL, GLFW, GLAD, Dear ImGui |
| Physics | Custom in-house multi-domain physics engine (electrical/pneumatic/mechanical) + Box2D |
| PLC/Ladder | OpenPLC-compatible LD conversion and execution pipeline |
| Data/IO | nlohmann/json, miniz, XML serializer |

This project uses a custom in-house simulation engine as its core, not a general-purpose external game engine.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Run

```bash
# single-config generator
./build/bin/PLCSimulator

# multi-config (Visual Studio)
./build/bin/Release/PLCSimulator.exe
```

## Tests

```bash
cmake -S . -B build-test -DBUILD_TESTING=ON
cmake --build build-test --target plc_tests
ctest --test-dir build-test --output-on-failure
```

## Project structure

```text
include/plc_emulator/   # public headers
src/                    # active implementation
resources/              # fonts, i18n, assets
tests/                  # unit/integration tests
legacy/                 # old MVP code (reference only, not built)
```

## Usage notes

- `F2`: monitor mode
- `F5/F6/F7`: XIC/XIO/Coil
- `F9`, `Shift+F9`: add/remove vertical line
- `Ctrl+Z`, `Ctrl+Y`: undo/redo

(Shortcut strings are defined in `resources/lang/*.lang`.)

## Status

We're accepting pull requests for additional components and other bug fixes. Development is progressing incrementally.
## License

GPL-3.0
