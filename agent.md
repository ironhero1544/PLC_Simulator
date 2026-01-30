# AGENTS.md
# Project-specific instructions for PLC_Emulator

## Refactoring rules
- Follow Google C++ Style Guide.
- Do not use dynamic memory allocation (no new/delete, no std::unique_ptr/shared_ptr for new code in this refactor).
- Do not use exceptions (no throw/catch).

## Project analysis (current code)
- Core orchestration lives in `include/plc_emulator/core/application.h` and `src/core/application.cpp`. Wiring mode renders via `RenderWiringCanvas()` in `src/wiring/application_wiring.cpp`.
- `placed_components_` and `wires_` live in `Application`. Rendering order is the vector order; selection uses reverse iteration (topmost by last index).
- Ports are defined per component in `src/components/*_def.cpp` and consumed by `UpdatePortPositions()` in `src/core/application.cpp` plus `FindPortAtPosition()` in `src/wiring/application_ports.cpp`.
- Hit-test uses axis-aligned bounding boxes with `comp.position` + `comp.size` in many places (`src/wiring/application_wiring.cpp` and `src/physics/application_physics.cpp`).
- `PlacedComponent::rotation` exists but is unused; there is no rotation/flip/z-order support today.
- Double-click is already used by component behaviors (limit switch, button unit, emergency stop) in `src/components/component_behavior.cpp`.
- Project save/load currently handles ladder CSV only (`ProjectFileManager`); layout/xml is referenced in headers but not implemented (`include/plc_emulator/project/project_file_manager.h`).

## Implementation design report: Layering + Rotation (UX-minimal)
### Goals
- Solve interaction visibility when components overlap.
- Allow flexible placement via 90deg rotation and optional flips.
- Preserve current UX patterns; avoid heavy UI changes.

### 1) Data model changes
- Extend `PlacedComponent` in `include/plc_emulator/core/data_types.h`:
  - `int z_order` (default = creation order)
  - `int rotation_quadrants` (0..3)
  - `bool flip_x`, `bool flip_y`
- Keep `rotation` for now (unused) or clearly deprecate it in comments.
- Add helper utilities (new header/source, e.g., `include/plc_emulator/core/component_transform.h` + `src/core/component_transform.cpp`):
  - `Size GetDisplaySize(const PlacedComponent& comp)` (swap width/height when rotation is odd).
  - `ImVec2 LocalToWorld(const PlacedComponent& comp, ImVec2 local)` with rotation/flip.
  - `ImVec2 WorldToLocal(const PlacedComponent& comp, ImVec2 world)` for hit-test.
  - `bool IsPointInsideComponent(const PlacedComponent& comp, ImVec2 world)` using inverse transform against base size.
- Rotation anchor: rotate around component center, and keep `comp.position` as top-left of the *display* AABB. This keeps visual rotation stable in place (center stays fixed).

### 2) Z-order control (Bring to front/back)
- Add per-component `z_order` integer (default = creation order).
- Use a shared context menu for Z-order + rotation + flip (see section 5).
- Optional shortcuts (confirm with user): `Ctrl+Up`, `Ctrl+Down`, `Shift+Ctrl+Up`, `Shift+Ctrl+Down`.
- Rendering: do NOT reorder `placed_components_` (avoid physics side effects). Instead, build a sorted index list for rendering and hit-test.
- Selection hit-test: use the topmost component by `z_order` under cursor.
- Exception: processing cylinder drill head must render above workpieces; see section 4 for overlay strategy.

### 3) X-ray / Ghost mode (temporary visibility)
- Goal: non-selected components rendered at reduced alpha while selected stays full opacity.
- Conflict: `Alt` is already used for tagged wire reveal and right-click tag removal in `src/wiring/application_wiring.cpp`.
- Recommendation: use a different modifier (`Shift`, `G`, or `Ctrl+Alt`) to avoid key conflict.
- Rendering approach without touching all component renderers:
  - Capture `ImDrawList` vertex range before and after each component render.
  - If ghosted, multiply vertex alpha in that range.
  - This avoids changing each `*_def.cpp` renderer.

### 4) Rendering + rotation/flip (UI)
- Rotation is limited to 90deg steps (`rotation_quadrants`).
- Use post-render vertex transforms to avoid rewriting every component renderer:
  - Capture `vtx_start` before `def->Render(...)`.
  - Render component as-is (unrotated).
  - Apply rotation/flip to vertices [vtx_start, vtx_end) around screen-space center.
  - This rotates both geometry and text labels consistently.
- Selection box uses `GetDisplaySize(comp)` so the AABB matches rotated dimensions.
- Processing cylinder overlay:
  - Split `RenderProcessingCylinder` into `RenderProcessingCylinderBody` + `RenderProcessingCylinderDrillHead`,
    or add a second render callback in `ComponentDefinition`.
  - Draw drill head in a final pass after workpieces regardless of z-order.

### 5) Input + context menu (UX)
- Context menu actions:
  - Bring to Front / Send to Back / Bring Forward / Send Backward
  - Rotate +90 / -90
  - Flip Horizontal / Flip Vertical
  - Optional: Delete
- Trigger policy to avoid conflicts:
  - Keep existing double-click behaviors (limit switch, button unit, emergency stop).
  - Open context menu on double-click only if component behavior did NOT handle the event.
  - Provide fallback trigger (e.g., `Ctrl+RightClick` on component) since right-click is currently delete.
- Shortcuts: `R` and `Shift+R` for rotation.

### 6) Ports, wiring, and hit-test updates
- `UpdatePortPositions()` should use `LocalToWorld()` so wires follow rotation/flip.
- `FindPortAtPosition()` should use `port_positions_` map (already computed) to ensure rotated ports are clickable.
- Replace direct AABB checks with `IsPointInsideComponent()` helper to respect rotation.

### 7) Physics impact (phase separation)
- Physics currently assumes axis-aligned `comp.size` for AABBs and interactions.
- Minimum change: use `GetDisplaySize(comp)` for AABB in physics when rotation is odd.
- Rotation-aware physics (sensor orientation, processing cylinder geometry) is a follow-up phase.

### 8) Data + file format
- Extend layout serialization with `z_order`, `rotation_quadrants`, `flip_x`, `flip_y`.
- Current `ProjectFileManager` saves ladder CSV only; add a `layout.json` or `layout.xml` file to the project package.
- Backward compatibility: when fields missing, default to `z_order` = creation order and rotation/flip = identity.

### 9) Touch points (likely files)
- Rendering: `src/wiring/application_components.cpp` (render order, vertex post-process).
- Hit-test / selection: `src/wiring/application_wiring.cpp` (all AABB checks).
- Port positions: `UpdatePortPositions()` in `src/core/application.cpp`, `FindPortAtPosition()` in `src/wiring/application_ports.cpp`.
- Component behavior conflicts: `src/components/component_behavior.cpp`.
- Physics AABB: `src/physics/application_physics.cpp`.
- Project I/O: `src/project/project_file_manager.cpp`, `include/plc_emulator/project/project_file_manager.h`.

### 10) Validation checklist
- Overlapped components: correct selection by z-order; visibility aided by ghost mode.
- Rotation keeps ports aligned; wires remain connected after rotate/flip.
- Double-click behavior still toggles limit switch/button unit/emergency stop.
- Key conflicts resolved (ghost vs tag reveal).
- No exceptions, no new/delete; ASCII only in new edits unless required.
