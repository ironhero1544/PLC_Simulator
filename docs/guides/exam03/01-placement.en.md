[Contents](GUIDE.en.md) · [Power wiring →](02-power.en.md)

<a id="1-부품-배치"></a>

# 1. Component placement

<a id="시작과-배치-파일"></a>

## Starting and loading the placement file

Launch PLC Simulator and select **Wiring**.
Leave the state at STOP. Use **Load** on the right of the Wiring toolbar
to select `exam03-placement-only.json`. The top `Project → Load` menu
and Wiring JSON loading are different commands.

![Empty Wiring screen](../../images/exam03-guide/00-empty.png)

![Placement JSON loaded in the app](../../images/exam03-guide/01-placement.png)

Placement is loaded when the controls, valve row, and mechanisms appear as shown, with no wires.
HMI screens should also be empty. If the overview is too small, zoom into the controls first.

The file contains 39 components with positions, rotation, size, Z ranges, render layers,
and servo mechanics. It has zero wires, zero HMI widgets, and no PLC program.
The power/input/output setup uses Sink input / Source output.
The 〔number〕 after a name is its placement-file component ID. Use it to distinguish
similarly named valves/cylinders. Connect by terminal names, not screen coordinates.

Zoom with the mouse wheel and pan by dragging the middle button.
Move components by dragging their bodies with the **Select**.
`R` rotates; `Shift+R` rotates in the opposite direction.

<a id="배치-관계"></a>

## Placement relationships

Arrange the top controls as DC power, PLC, servo amplifier, optical amplifier, then HMI.
Below them are air supply/distribution and actuator valves;
the lower equipment follows supply/processing/transfer → conveyor → stopper/pickup/rack → storage box.
Leave wiring space and keep sensor bodies clear of workpiece travel paths.
Four supply workpieces are stacked over the detection sensor.
Gravity feeds the next workpiece after the first is supplied and the cylinder retracts.

After loading the placement file, you do not need to reenter coordinates.

[Component coordinate/Z table](reference-placement.en.md)

<a id="z와-표시-레이어"></a>

## Z and render layers

**Z** on the right is physical height; **L** is drawing order.
Different Z heights prevent physical interaction even at the same screen position.
Sending a body backward in L does not change its Z or operating conditions.
The placement JSON preserves groups/Z settings; use them unchanged initially.
If optical heads obscure transport mechanisms, check their group order in L.

Lift〔14〕 mechanics: **60 pulse/rev, 1 mm lead, 100 mm stroke,
maximum speed 100 mm/s, acceleration 200 mm/s²**.
Servo pulse position is not the same value as screen Z height.
With these settings, JOG 500/1000/1500 mm/min corresponds to 500/1000/1500 pulse/s.

---

[Contents](GUIDE.en.md) · [Power wiring →](02-power.en.md)
