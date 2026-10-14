[Contents](GUIDE.en.md) · [← Trial run](06-checks.en.md)

<a id="단계서보복귀-조건-참고"></a>

# Stage, servo, and recovery reference

Supply extends X0 → retracts X1 → motor start stage 33 → processing down X4 → 3s T0 →
processing up X5 → transfer extends X2 → retracts X3 → conveyor → material X14/X15.
Keep the processing motor ON until up-complete, then stop it when entering transfer.

For metal, stop the conveyor only after both stopper-down X6 and position optical X16.
Lift-down completion requires BUSY OFF and INP ON.
After vacuum ON, wait for X20 before selecting rack position.
Confirm rack retracted X13 or extended X12 before moving to storage height.
Cylinder extends X10 → vacuum OFF/X20 OFF → retracts X11 → lift home →
stopper up X21 completes the sequence.

Nonmetal transport finishes after box-passage X22 turns ON and then OFF.
Do not use a fixed 16-second timer as storage completion.
The four workpieces are ordered nonmetal → metal → nonmetal → metal.
Feeding two metals first can leave nonmetals unprocessed because filling both storage targets ends operation.

<a id="서보와-원점"></a>

## Servo and home

`DDRVA position speed Y0 Y4` commands an absolute position.
`DPLSV D200 Y0 Y4` is upward JOG; `DPLSV D202 Y0 Y4` is negative-speed downward JOG.
For automatic/homing/initialization moves, confirm pickup retracted X11 before moving the lift.
Home completion requires `D8340=0`, `X17=ON`, and `M8340=OFF` together.
INP alone may mistake completion at the old position for completion of the new move.
Even after manual JOG following homing, return to absolute 0 at the start stage before supply.

Rack slots from the front: lower 1/2, middle 3/4, upper 5/6.
First metal goes to slot 2 at the retracted rack position; second to slot 5 at the extended position.
Use the placement file and circuit tables as one set for pulse positions and rack Z ranges.
Revalidate storage pulses if mechanics or Z ranges change.

<a id="일시정지와-재시작"></a>

## Pause and restart

M113→M204 pauses; T3 K50 (5s) then enters M205 initialization.
Turning Y17 off during pause cuts run air and SON.
Maintain vacuum during pickup. After 5s, retract supply/transfer/pickup/rack,
raise processing/stopper, and home the servo.
Clear initialization only after return/up sensors, servo position 0, INP/BUSY, and T13 are checked.
Turning all output coils OFF alone does not stop a double-solenoid valve correctly.

---

[Contents](GUIDE.en.md) · [← Trial run](06-checks.en.md)
