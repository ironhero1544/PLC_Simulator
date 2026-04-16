# PLC Simulator 1.1.0 Patch Notes

> Feature-level update notes comparing baseline commit `2a26d5e17d71ebc4448353b5e1611a25d1f80d27` with the current local 1.1.0 codebase.

## Summary

`1.1.0` is a substantial feature update rather than a small patch release.

The biggest changes are:

- large-scale **RTL feature integration**
- major **programming mode expansion and stabilization**
- broad **wiring canvas input / camera / touch / tag UX improvements**
- expanded **tooling, help text, and multi-language coverage**

## Major Additions and Changes

### 1. RTL mode and RTL workflow added

- dedicated RTL UI and workflow surfaces were added
- RTL project management and runtime management were introduced
- RTL module/component definitions were added to the simulator
- RTL-specific theme/resources were added
- RTL-related state and project data handling were expanded

### 2. RTL toolchain operations expanded

- native RTL toolchain setup flow was added
- toolchain verification flow was added
- toolchain deletion flow was added
- installer / uninstaller packaging now includes tool scripts needed for RTL maintenance
- RTL toolchain state can be refreshed and managed from the application UI

## 3. Programming mode significantly expanded

- `RST`, `RST_TMR_CTR`, and `BKRST` handling was expanded across edit/UI/runtime paths
- instruction parsing, display, and save/load handling for reset-related instructions were strengthened
- compiled PLC execution path was updated for newer reset behavior
- ladder edit logic, UI behavior, and project serialization paths were heavily revised
- programming mode usability and stability were broadly improved

### 4. Wiring canvas input pipeline was reworked

- Win32 raw input handling was expanded
- platform input collector and UI capture registry were added
- canvas navigation and component interaction routing were reorganized
- touch, touchpad, wheel, pen, and side-button input handling were upgraded
- camera interaction blocking now respects overlay capture and component-specific rules more accurately

### 5. Wiring canvas UX improved

- tag label placement was heavily improved to reduce overlap and clutter
- PLC `X` / `Y` tag ordering and grouping behavior was refined
- same-name labels can be merged into a single label with multiple arrows
- label placement now avoids other components more aggressively
- canvas camera help text and shortcut guidance were updated to match current behavior

### 6. Touch and gesture support improved

- touch-screen panel scrolling support was added/improved for non-canvas UI areas
- touch pinch zoom behavior was refined
- trackpad and touch navigation behavior was updated
- FRL / METER_VALVE gesture interaction was added and tuned

### 7. Component interaction/state handling improved

- component input resolver layer was introduced
- several interactive components moved toward command/result-based interaction handling
- component state resolution and physics/electrical synchronization paths were improved
- FRL, meter valve, button unit, emergency stop, and limit switch interaction paths were updated

### 8. Project/package workflow improved

- project save/load and package save flows were extended
- `Ctrl+S` project package save behavior was added for key modes
- CPack packaging now targets the `1.1.0` release line
- release packaging includes more resources and scripts than before

### 9. Help text and localization expanded

- `ko`, `en`, and `ja` language resources were significantly updated
- new RTL/toolchain/help strings were added
- wiring camera help and shortcut help were updated to reflect real current controls

## User-visible Highlights

- new **RTL mode** and related toolchain workflow
- stronger **programming mode** support for reset-family instructions
- improved **touch / trackpad / pen** handling
- better **wiring label readability**
- improved **camera/navigation guidance**
- expanded **tooling and packaging support**

## Compatibility Notes

- existing `1.0.0` JSON / CSV project data appears to remain usable in the current codebase
- on older Windows environments, RTL PowerShell scripts may require a newer PowerShell runtime (`pwsh` / PowerShell 7+)

## Release Positioning

This update fits a **minor version bump**:

- not just a bugfix patch
- significant new features were added
- existing project compatibility appears to be retained

That makes `1.1.0` the appropriate release label for this update.
