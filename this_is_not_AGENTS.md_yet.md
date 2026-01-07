# AGENTS.md

# PLC Emulator: Component Designer + Part Maker Program Plan

Last updated: 2026-01-05
Owner: user
Scope: Wiring-mode component redesign + user component creation tool (basic + advanced) + export/import as zip with frontend/backend split + C++ code generation pipeline.

## 1) Goals and non-goals

### Goals
- Redesign wiring-mode parts (visual + ports + I/O devices) so they are freely drawable like a design tool.
- Provide a **basic** component maker for non-programmers: draw logic circuits, set I/O behavior, set pneumatic/electric, set input/output port counts, and define parameters.
- Provide an **advanced** component maker for power users: define behavior through a C++-based custom language that runs on the component behavior engine.
- Support freeform component shapes, ports, and embedded devices in both basic and advanced modes.
- Export/import completed parts as **.zip** with separated frontend (visual/design) and backend (behavior) files.
- Convert component definitions to real C++ code used by the emulator runtime.


## 2) Users and modes

### User types
- Beginner: wants to draw logic and assign I/O without coding.
- Advanced: wants custom behavior in a C++-like scripting language.
- Designer: wants complete control over ports, shape, labels, and UI.

### Modes
- **Basic mode**: visual logic editor + property sheets.
- **Advanced mode**: visual design + script editor (C++-based custom language).

### Mode goals and UX constraints
- UI must stay consistent with existing PLC Emulator design language; allow new tools but avoid visual drift.
- Basic mode must be 1-click accessible from wiring mode and hide advanced features by default.
- Advanced mode must expose scripting without blocking visual design work.

### User journeys
- Beginner flow: create component -> draw shape/ports -> add I/O devices -> draw logic -> save zip.
- Advanced flow: create component -> design shape/ports -> add I/O devices -> open script -> validate -> save zip.
- Designer flow: focus on layout, typography, port styling, and preview rendering without touching behavior.

### Mode transitions
- Basic -> Advanced: keep existing logic graph, generate scaffold script as reference comments, mark as "mixed".
- Advanced -> Basic: allow only if script uses supported subset; otherwise lock basic editor with warnings.

### Access and safety
- Basic mode has guardrails: port count/domain validation, missing I/O checks, and automatic defaults.
- Advanced mode provides warnings and a sandboxed runtime with controlled I/O API.

## 3) Component definition model

### Conceptual model
- Component = Visual Design (frontend) + Behavior (backend) + Metadata
- Must be versioned for migration.

### Required metadata
- id (uuid), name, category, version, author
- capabilities: electric/pneumatic, input/output port counts
- parameter schema (typed, defaults, ranges)
- I/O semantics and labels

### Frontend (design) data
- shapes: lines, rectangles, rounded boxes, arcs, polygons
- ports: position, type (in/out), domain (electric/pneumatic), label, color
- devices: lamps, buttons, relays, coils, sensors, etc.
- layout: grid snap, alignment guides
- style: stroke width, fill, font, palette

### Backend (behavior) data
- Basic: logic graph (gates, timers, coils) + I/O mapping
- Advanced: script module + compiled artifacts
- Event model: tick, input change, output update

## 4) File formats and zip layout

### Zip structure (proposed)
- component.json            (metadata, version, flags)
- frontend/
  - design.json             (shape + ports + devices)
  - preview.png             (optional)
- backend/
  - basic_logic.json        (only for basic mode)
  - advanced_script.pcl     (custom C++-like source)
  - generated/
    - component.cpp         (generated runtime code)
    - component.h
- assets/
  - icons/
  - textures/
- manifest.json             (index of files and hashes)

### Versioning
- Schema version in component.json
- Migration scripts or adapters

## 5) Architecture changes (high level)

### Frontend
- Wiring mode redesign
- Component designer UI (canvas + property panes)
- Port/shape/device editor
- Basic logic editor
- Advanced script editor
- Validation UI (warnings, missing ports, invalid I/O)

### Backend
- Component definition loader
- Zip pack/unpack
- Code generator (C++ output)
- Behavior engine (runtime evaluation)
- Custom language compiler (basic parser, AST, bytecode or direct C++ emit)

## 6) Milestones and phases

### Phase 1: Foundations
- Define schema + zip layout
- Define metadata + port/device spec
- Implement load/save for new component format
- Basic UI scaffolding for component designer

### Phase 2: Visual redesign
- Redesign wiring-mode components
- Add new port/shape/device rendering pipeline
- Create component preview rendering

### Phase 3: Basic maker
- Logic graph editor
- Property-based I/O behavior setup
- Validate I/O port counts and domain constraints

### Phase 4: Advanced maker
- C++-like custom language design
- Parser/AST + validation
- Script editor integration

### Phase 5: Code generation
- Generate component C++ code
- Integrate with emulator runtime
- Cache build outputs

### Phase 6: QA and packaging
- Golden test components
- Import/export validations
- Version migrations

## 7) Detailed work breakdown

### 7.1 Wiring-mode component redesign
- Define new component visual primitives
- Update drawing/selection handles for shapes and ports
- Implement resize/rotate and snap
- Update hit-testing with new shapes
- Add style presets for components

### 7.2 Component designer (UI)
- Canvas with layers (shape/ports/devices)
- Properties panel (common + advanced)
- Component library browser
- Undo/redo and history snapshots
- Docking UI: all panels/tabs can be detached and docked freely by the user
- Save/load to zip

### 7.3 Basic mode behavior editor
- Logic nodes (full set)
  - Base: AND, OR, NOT, XOR, NAND, NOR
  - I/O: IN (digital), OUT (digital), PULSE/EDGE (rising/falling)
  - Memory/Latch: SR, RS, TOGGLE (flip-flop)
  - Timers: TON, TOF, TP
  - Counters: CTU, CTD, CTUD, RESET
  - Compare: EQ, NEQ, GT, GTE, LT, LTE
  - Utility: DEBOUNCE, LIMIT, MUX, DEMUX
- Connections and port mapping
- I/O semantics (rise/fall, latching)
- Simulate in-editor

### 7.4 Advanced mode scripting
- Define custom language syntax (C++-based)
- Safe sandboxed runtime
- Access to component I/O API
- Debug hooks and logging
- Runtime policy (agreed)
  - Tick: 10ms fixed step; event + tick mixed execution
  - Memory: per-component cap 1GB (track allocations)
  - Numeric: float only; vectors/matrices use C++ types
  - Work budget: event-queue-aware scheduling; allow >=50 state updates per tick
  - Loops: default max 10 iterations unless explicitly extended
  - Recursion: must be statically/proven bounded (required termination condition)
  - Errors: script failure -> warning on component + removed from physics engine

### 7.5 Code generation pipeline
- AST -> C++ emitter
- Component API wrappers
- Build cache and incremental updates
- Error mapping back to editor

### 7.6 Zip + persistence
- Serialize/deserialize component.json
- Package all assets in zip
- Manifest with SHA256
- Backward compatibility layer
- Validation gate: export/save is blocked on errors; warnings are allowed

## 8) Data schemas (draft)

### component.json (core)
- id, name, version, author
- domain: electric/pneumatic/both
- ports: counts + details
- parameters: schema
- frontend_path, backend_path

### frontend/design.json
- shapes: list
- ports: list
- devices: list
- styles

### backend/basic_logic.json
- nodes: list
- edges: list
- io_map

### backend/advanced_script.pcl
- source code

## 9) Risks and mitigation
- Custom language complexity -> start with limited subset
- Performance of rendering -> cache geometry
- Schema churn -> include migrations
- User data loss -> auto-save + versioned backups

## 10) Open questions
- Which UI framework for advanced editor (ImGui vs embedded)
- Target build flow for generated C++ (runtime compile vs precompile)
- Component packaging location (project vs global library)

## 11) Acceptance criteria
- User can design a new component visually.
- User can assign ports and I/O behavior.
- Component exports as zip with frontend/backend split.
- Component imports and renders in wiring mode.
- Generated C++ integrates into runtime and runs.

## 12) First actions
- Approve schema + zip layout
- Decide custom language scope (subset)
- Prototype design canvas
