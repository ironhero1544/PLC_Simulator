[User guide contents](README.en.md) · [Entering ladder logic](06-programming.en.md) · [Using Monitor](07-monitoring.en.md)

<a id="기본-회로를-한-가지씩-연습하기"></a>

# Practice basic circuits one at a time

In the tables, **left conditions** are contacts and **right instructions** are coils or application instructions.
Enter each table row as one circuit and keep END last.
Each example is a separate exercise. Do not combine all examples using Y0 into one program.

Use F5/F6 for contacts, F8 for application instructions, and Enter to confirm.
You can use F7 for basic OUT coils. Compile and test in RUN after entering the circuit.

For ladder-only practice without wiring, open **Monitor (F2) in STOP** and test with right-side X buttons.
See [basic monitoring](07-monitoring.en.md) for input buttons and current values.

<a id="1-시작정지를-기억하는-회로"></a>

## 1. Latching start and stop

Buttons generate input only while pressed. Store run state in M to keep running.
Assume X0 is Start and X1 is the Stop button's **NO signal**.
This is a basic simulator exercise, separate from emergency-stop wiring design for real equipment.

| Circuit | Left condition | Right instruction |
| --- | --- | --- |
| 1 | X0 normally open contact | `SET M200` |
| 2 | X1 normally open contact | `RST M200` |
| 3 | M200 normally open contact | `OUT Y0` |

1. Enter X0 with F5 in the first circuit, then `SET M200` with F8 in the next cell.
2. Enter X1 and `RST M200` in a new circuit.
3. Enter M200 and a Y0 coil in the final circuit.
4. Compile and start.
5. Check that M200/Y0 remain ON after pressing and releasing X0.
6. Check that X1 turns them off.

In this order, if both inputs are ON in one scan, the later RST clears the state.
Changing Stop to actual NC wiring changes X1's normal/pressed states, so rewrite the condition too.

<a id="2-1초-뒤-램프-켜기"></a>

## 2. Light a lamp after one second

| Circuit | Left condition | Right instruction |
| --- | --- | --- |
| 1 | X0 normally open contact | `OUT T0 K10` |
| 2 | T0 normally open contact | `OUT Y0` |

1. Enter X0, then F8 → `OUT T0 K10` → Enter in the next cell.
2. Enter a T0 contact and Y0 coil in the next circuit.
3. Compile/start, then press and hold X0.
4. At current value 10, check that the T0 completion contact and Y0 are ON.
5. Release X0 and check that the value, completion contact, and Y0 reset.

K10 for T0 means **one second**. Use only T0 in this exercise.

![Basic two-row timer circuit in the app](../../images/program-guide/basic-timer-complete.png)

First row: while X0 is held, T0 counts time.
Second row: after one second, the T0 contact turns on and lights Y0.
Releasing the button resets the timer and turns the lamp off.

<a id="3-세-번-입력하면-램프-켜기"></a>

## 3. Light a lamp after three inputs

| Circuit | Left condition | Right instruction |
| --- | --- | --- |
| 1 | X0 normally open contact | `OUT C0 K3` |
| 2 | C0 normally open contact | `OUT Y0` |
| 3 | X1 normally open contact | `RST C0` |

1. Enter X0 and `OUT C0 K3`.
2. Enter C0 and Y0.
3. Enter X1 and `RST C0`.
4. Compile/start, then press and release X0 three times.
5. Y0 lights on the third press. Holding the button counts only once.
6. Press X1 and check that C0 and Y0 reset.

![Basic three-row counter circuit in the app](../../images/program-guide/basic-counter-complete.png)

First row: count once each time X0 is pressed.
Second row: light Y0 after three counts.
Third row: pressing X1 returns the count to 0 and turns the lamp off.

<a id="4-hmi-버튼으로-설정값-바꾸기"></a>

## 4. Changing a setpoint with HMI buttons

HMI numbers are display-only, so use ladder logic to convert button requests into values.

| HMI widget | Address | Setting |
| --- | --- | --- |
| Low-speed button | M100 | Momentary |
| High-speed button | M101 | Momentary |
| Speed numeric display | D100 | Unit appropriate to the exercise |

| Circuit | Left condition | Right instruction |
| --- | --- | --- |
| 1 | M100 normally open contact | `MOV K100 D100` |
| 2 | M101 normally open contact | `MOV K500 D100` |

Apply HMI settings and compile both circuits. In RUN, press low/high speed and
check that the D100 display changes to 100/500 respectively.
This alone does not change motor speed. Configure the driver/PWM or motion instruction to use D100 separately.

<a id="5-조건을-직렬로-확인하기"></a>

## 5. Checking series conditions

‘Y1 when run request M200 is ON, processing sensor X2 is ON, and stop request M201 is OFF’ means
M200 normally open → X2 normally open → M201 normally closed → Y1 coil.

1. In one row, enter F5 M200, F5 X2, then F6 M201.
2. Finish with F7 Y1.
3. Read the conditions from left to right in Monitor.
4. If Y1 is OFF, find which contact interrupts the condition.

For parallel ‘OR’ conditions, see the branch explanation in [ladder editing](06-programming.en.md)
and the [actual parallel circuit screenshot](../exam03/logic-02-manual.en.md).

---

[User guide contents](README.en.md) · [Entering ladder logic](06-programming.en.md) · [Using Monitor](07-monitoring.en.md)
