# AGENTS.md
# Project-specific instructions for PLC_Emulator

## Refactoring rules
- Follow Google C++ Style Guide.
- Do not use dynamic memory allocation (no new/delete, no std::unique_ptr/shared_ptr for new code in this refactor).
- Do not use exceptions (no throw/catch).

## Meter Valve + Pneumatics Overhaul Plan (2026-01-27)
- Goal: Fix meter-in/meter-out behavior, make pneumatic system require both inlet and exhaust paths, and ensure cylinders stop when pressure is insufficient.
- Step 1: Meter valve popup UX stabilization.
  - Use a single source of truth for menu open/close (either behavior handler or render path, not both).
  - When a meter popup opens, close all other meter popups in the scene.
  - If popup cannot open or loses focus, reset `meter_menu_open` to 0.0f so other UI stays interactive.
- Step 2: Pneumatic graph reconstruction with direction-aware attenuation.
  - Build adjacency from pneumatic wires plus component-internal links (valves, manifold, meter valve).
  - Keep current port IDs (no extra R ports); treat the inactive A/B side of a valve as vent-to-atmosphere.
  - Represent meter valve as a directional restriction with per-edge attenuation based on `flow_setting` and `meter_mode`.
- Step 3: Supply propagation.
  - BFS/queue from FRL output ports, propagate pressure with attenuation per edge.
  - Record `port_pressures_` and a `supply_reachable` flag per port.
- Step 4: Exhaust propagation.
  - BFS/queue from vented ports (valve inactive side) as exhaust sources.
  - Compute `exhaust_quality` per port (0..1) based on attenuation to atmosphere.
- Step 5: Cylinder and processing cylinder gating.
  - Require both A and B ports to be connected; if either is missing, hard-stop cylinder (velocity 0, position fixed).
  - Compute effective pressure using supply pressure minus exhaust backpressure; if below threshold, stop in place.
  - Apply meter-in/out attenuation as pressure loss so cylinder speed drops as expected.
- Step 6: Validation scenarios.
  - Supply only (no A/B link): cylinder does not move.
  - A/B both linked with vent path: cylinder moves.
  - Meter-in/out extremes: observable speed difference.
