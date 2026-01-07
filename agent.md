# PLC Emulator Work Plan

## Scope
- GXWorks2-style Ctrl+Arrow line toggle (no output column edits)
- Fix timer/counter behavior (SET/RST/TON/CTU) in compiled executor
- Sync timer/counter UI state from executor memory
- Compile-failure warning with rung highlighting until fixed

## Plan
### 1) Ctrl+Arrow path toggle (GXWorks2)
- Toggle HLINE along the entire move path (left/right/up/down)
- Allow toggling even when a cell already has an instruction
- Block edits in output column (default column 11) for Ctrl/F9
- Keep vertical moves connected by toggling same column across rungs
- Update keyboard help text

### 2) Timer/Counter UI synchronization
- After each scan, pull T/C values from executor memory
- Update timer_states_ and counter_states_ (value, done, enabled, lastPower)
- Keep UI panels accurate during monitor mode

### 3) Execute SET/RST/TON/CTU reliably in executor
- Keep the execution model simple and stable (no heavy parsing)
- Ensure RST_TMR_CTR resets T/C value, done, enabled, lastPower
- CTU counts on rising edge only; done at preset
- TON accumulates time; done when >= preset; define reset on input OFF

### 4) Compile-failure warnings and rung marking
- On compile failure, show warning message in UI
- Map error lines to rung numbers (via LD rung comments) when possible
- Highlight offending rung(s) until they are modified and recompiled
- Clear warnings on successful compile

### 5) Validation
- Verify Ctrl+Arrow path toggle (all directions) and output column block
- Verify SET/RST/TON/CTU/RST_TMR_CTR runtime behavior
- Verify timer/counter UI matches executor memory
- Verify compile failure warnings and rung highlights persist until fixed
