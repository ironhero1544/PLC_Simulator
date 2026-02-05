# AGENTS.md
# Project-specific instructions for PLC_Emulator

## Refactoring rules
- Follow Google C++ Style Guide.
- Do not use dynamic memory allocation (no new/delete, no std::unique_ptr/shared_ptr for new code in this refactor).
- Do not use exceptions (no throw/catch).

## Physics LOD and Optimization Plan
### Goal
- Split large application / application_physics sources into organized directories.
- LOD decision based on viewport distance and zoom.
- Fix workpiece substeps to 4 (time based) and apply latest collision in time order.
- Optimize advanced physics and mechanical physics (required).

### Planned directory structure (split, not move)
- src/application/ (new)
  - app_lifecycle.cpp (ctor/dtor, Initialize, Run, Shutdown, Cleanup)
  - app_input.cpp (ProcessInput, Win32 hooks, touch helpers)
  - app_render.cpp (Render + UI rendering)
  - app_layout.cpp (GetLayoutScale, UpdatePortPositions, WorldToScreen, ScreenToWorld)
  - app_project.cpp (Save/Load project + ladder compile/load)
  - app_debug.cpp (debug toggles/logging)
  - app_lod.cpp (viewport/zoom based LOD calculation)
- src/application_physics/ (new)
  - physics_update.cpp (UpdatePhysics, UpdatePhysicsPerformanceStats, LOD gates)
  - physics_plc.cpp (SimulateLoadedLadder, Get/SetPlcDeviceState)
  - physics_electrical.cpp (SimulateElectrical)
  - physics_pneumatic.cpp (SimulatePneumatic)
  - physics_actuators.cpp (UpdateActuators, UpdateBasicPhysics, UpdateComponentLogic)
  - physics_workpiece.cpp (UpdateWorkpieceInteractions, substep=4, latest collision)
  - physics_sync.cpp (SyncPLCOutputsToPhysicsEngine, SyncPhysicsEngineToApplication)
  - physics_ports.cpp (GetPortsForComponent)
- Keep src/core/application.cpp and src/physics/application_physics.cpp as thin delegators until fully migrated.
- Update CMakeLists.txt to compile new files.

### Plan A
1) Create new directories and files, move function bodies into the new files, leave minimal wrappers in old files.
2) Implement viewport/zoom LOD calculation in app_lod.cpp and pass LOD to physics update.
3) Fix UpdateWorkpieceInteractions substeps to 4, step_dt = delta_time / 4.0f.
4) Collision priority: within each substep, apply the last collision resolution in time order.
5) Box2D integration (mechanical + continuous collision + sensor queries only):
   - Replace AABB overlap checks and swept-rod contact in physics_workpiece.cpp.
   - Replace IsDirectionalHit sensor logic in physics_actuators.cpp and
     UpdateBasicPhysicsImpl (raycast/sensor fixtures).
   - Keep PLC scan, electrical, and pneumatic logic unchanged.
   - Note: must stay within "no dynamic allocation" rule or provide a
     compliant allocator plan before implementation.
6) Add grid spatial hashing for sensors, conveyors, cylinders, boxes, and workpieces to reduce overlap checks (if Box2D is not fully covering all queries).
7) Add caching for AABB and transform-derived data; refresh only on move/rotate.
8) Advanced physics optimization: extend topology hash, dirty flags, and skip solves if inputs unchanged.
9) Mechanical optimization: cache matrices, skip updates when stable, sleep/wake based on forces and viewport.
10) Extend UpdatePhysicsPerformanceStats for per-stage timings to validate LOD behavior.

### Notes
- All new code must avoid dynamic allocation and exceptions.
- LOD is based on viewport distance and zoom only.
