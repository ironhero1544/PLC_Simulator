[User guide contents](README.en.md) · [← 6. Writing and editing ladder logic](06-programming.en.md) · [8. HMI operation and screen editing →](08-hmi.en.md)

<a id="7-모니터로-래더-동작-보기"></a>

# 7. Observing ladder operation in Monitor

**Monitor shows which inputs and outputs are ON in your ladder program.**
Start with a circuit where X0 turns Y0 on. ON means on, OFF means off.

<a id="1-배선-없이-한-줄-회로-시험하기"></a>

## 1. Testing a one-row circuit without wiring

Save any current project first and use a separate exercise.
This step tests **only the ladder program**, with no components or wires.

1. Select **Programming → Edit (F3)**.
2. Select the first cell on the left and enter a contact with **F5 → X0 → Enter**.
   Even with only END, pressing F5 in its leftmost cell creates a new circuit.
3. Enter a coil in the next empty cell with **F7 → Y0 → Enter**.
4. Select **Compile**. **CONVERTED** at the bottom means it is ready.
5. Leave the top-right state at **STOP** and select **Monitor (F2)**.

![Basic Monitor screen with only X0 and Y0](../../images/program-guide/monitor-basic-overview.png)

*X0 on the left is the input condition; Y0 on the right is the output. Click the X to test in ‘Input control’ on the right.*

6. Click **X0** once in **Input control** on the right.
7. The X0 contact and Y0 coil turn pale blue, and **Y0** on the right turns red.
8. Click **X0** again. X0 and Y0 turn off.

**Click a right-side X button once to turn it on, and again to turn it off.**
You do not need to hold it down. Y buttons display the result.
Clicking Y0 does not turn the output on.

![Y0 changes as X0 is turned on and off](../../images/program-guide/monitor-basic-toggle.gif)

*Enlarged views of the contacts, coils, and input/output indicators on the same screen.*

<a id="2-화면에서-무엇을-읽어야-하나요"></a>

## 2. What to read on screen

| Area | Meaning |
| --- | --- |
| Pale blue contact/coil background | The condition or output is active |
| Green input X on the right | Input ON; gray means OFF |
| Red output Y on the right | Output ON; gray means OFF |
| Dark blue outlined rectangle | Selected cell; move by clicking or using arrow keys |

**The selection outline and ON indicator are different.**
The selected cell's outline remains when X0 is OFF. Do not interpret the outline alone as ON.
Hovering over a button can change its color. Move the pointer aside before reading its state.

![Selection rectangle remains with X0 OFF](../../images/program-guide/monitor-selection-off.png)

*The selection outline remains, but the inner background is white. X0 is OFF.*

If the list is long, **scroll inside the right pane**.
To view lower circuits, **scroll inside the center ladder area**.

<a id="3-타이머-값-읽기"></a>

## 3. Reading timer values

Enter the two-row circuit in [Light a lamp after one second](reference-basic-circuits.en.md#2-1초-뒤-램프-켜기).
Select **Compile → Monitor (F2)**, then turn on X0 on the right.

Find T0 under **Timers / Counters** at the top right.
`T0 K0/10` means **current value 0, target 10**.
T0 in this example counts in 0.1-second increments, so reaching 10 means one second has elapsed.

![Timer values before starting and after completion](../../images/program-guide/monitor-timer-values.png)

*Before starting: 0/10 and 0%. After completion: 10/10 and 100%. The bar shows progress.*

1. Keep X0 on; the number and progress bar increase.
2. At target 10, the **T0 contact** and **Y0** on the next row turn on.
3. Turn X0 off; T0 returns to 0 and Y0 turns off.

`T0=10 K10=10` below the coil also shows **current value 10, preset 10**.
Clicking the number does not change the time. Edit the instruction in Edit mode.

<a id="4-카운터-값-읽기"></a>

## 4. Reading counter values

Enter the three-row circuit in [Light a lamp after three inputs](reference-basic-circuits.en.md#3-세-번-입력하면-램프-켜기).
Select **Compile → Monitor (F2)**.

1. Turn X0 on, then off. C0 on the right becomes **1/3**.
2. Turn it on and off again. The value becomes **2/3**.
3. On the third ON, it reaches **3/3** and **Y0** turns on.
4. Turn X0 off. The counter remembers the count, so Y0 stays on.
5. Turn X1 on to reset to **0/3** and turn Y0 off. Turn X1 off again.

**Holding X0 on counts only once.** Turn it off and on again for the next count.

![Counter counts 0, 1, 2, 3 and reset](../../images/program-guide/monitor-counter-count.gif)

*Watch the count and Y0 together. Y0 turns on at three counts and off when X1 resets the counter.*

<a id="5-배선한-버튼센서의-상태-보기"></a>

## 5. Observing wired buttons and sensors

Above, you tested only ladder logic with the right-side X buttons.
After building the [button/lamp wiring exercise](05-first-practice.en.md), test the actual components as follows.

1. Compile the ladder program and select **RUN** at the top right. **RUN** appears.
2. Press the wired button in **Wiring → Select tool**.
3. Read X0 and Y0 in **Programming → Monitor (F2)**.
4. The right-side X buttons are disabled in this state. Change inputs using wired buttons/sensors.

| Display | Meaning in this exercise |
| --- | --- |
| STOP + Monitor (F2) | Test ladder logic with the right-side X buttons |
| RUN + Monitor (F2) | Observe inputs/outputs of wired components |
| Edit (F3) | Enter/edit ladder logic |

A ladder-only test does not test the physical lamp on the canvas.
Check the physical lamp in RUN after finishing the wiring.

<a id="6-수정하고-다시-시험하기"></a>

## 6. Editing and retesting

1. If running, select **STOP** at the top right.
2. Select **Edit (F3)**. This also ends a ladder-only test.
3. Select the contact/instruction to edit and press **Enter**.
4. Change the value, confirm with Enter, and **compile again**.
5. Use Monitor (F2) for ladder-only tests or Start for wiring tests.

<a id="7-반응이-없으면-이-순서로-보기"></a>

## 7. Troubleshooting in order

| Symptom | First action |
| --- | --- |
| Input control is missing on the right | Select Monitor (F2), then scroll the right pane |
| X0 button does not change | In RUN, use the wired button; stop for a ladder-only test |
| X0 is ON but Y0 stays OFF | Check X0/Y0 addresses and compilation of the latest circuit |
| T0 stays at 0 | Keep X0 ON; check OUT T0 K10 |
| C0 increases only once | Turn X0 off, then on again |
| Counter does not count after reset | Turn X1 off before testing X0 |
| Y0 is ON but physical lamp is off | Check RUN and Y0→lamp wiring |

If paused in HMI editing or the Z window, close the editor or resume the simulation.
If the app was already STOP before HMI editing, closing the editor does not automatically enter RUN.

Circuits using M/D also show those devices on the right.
M is a remembered ON/OFF state; `D100 = 100` shows the number stored in D100.
Start by learning the X0, Y0, T0, and C0 exercises above.

---

[User guide contents](README.en.md) · [← 6. Writing and editing ladder logic](06-programming.en.md) · [8. HMI operation and screen editing →](08-hmi.en.md)
