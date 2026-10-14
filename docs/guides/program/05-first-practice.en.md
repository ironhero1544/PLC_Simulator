[User guide contents](README.en.md) · [← 4. Electrical, pneumatic, and optical wiring](04-wiring.en.md) · [6. Writing and editing ladder logic →](06-programming.en.md)

<a id="5-첫-실습-버튼으로-램프-켜기"></a>

# 5. First exercise: lighting a lamp with a button

Make the lamp on a button/lamp unit light while you hold its button.
Build **three components, four wires, and one ladder circuit** yourself rather than loading a finished file.

<a id="준비"></a>

## Preparation

Save your current work as `.plcproj` and use a separate exercise. To start with an empty screen,
save and relaunch the app. Avoid output addresses already used by existing equipment.

| Component | Quantity | Role |
| --- | --- | --- |
| PLC | 1 | Read X0 and control Y0 |
| DC power supply | 1 | Supply 24V to the button/lamp circuit |
| Lamp/button unit | 1 | Use the first button and first lamp |

<a id="1단계-배치와-모드"></a>

## Step 1: placement and modes

1. Drag these three components onto the Wiring canvas.
2. Place the power supply on the left, PLC in the middle, and button unit on the right, leaving wiring space.
3. Double-click the PLC body and set **Sink input / Source output**.
4. Read the pin names in the button unit's **first row**. Each row has COM/NO/NC and lamp 24V/0V.

<a id="2단계-네-가닥-배선"></a>

## Step 2: four wires

| Order | Starting pin | Destination pin |
| --- | --- | --- |
| 1 | DC supply 24V | First button COM |
| 2 | First button NO | PLC X0 |
| 3 | PLC Y0 | First lamp 24V |
| 4 | DC supply 0V | First lamp 0V |

**NC is not used in this exercise.** Distinguish button contact power from lamp power pins.
The PLC drawing has no separate supply pins. Do not look for imaginary supply pins or wire X/Y as supply pins.

![First wire: 24V → button COM through waypoints](../../images/program-guide/wiring-route.gif)

*First wire: 24V → button COM through waypoints*

<a id="3단계-래더-입력"></a>

## Step 3: entering ladder logic

1. Select **Programming → Edit (F3)**.
2. Click the left cell of the circuit above END. If there is only END, add a circuit with `Insert`.
3. Press `F5`.
4. Enter `X0` as the address and confirm with Enter.
5. Select the next empty cell in the same circuit and press `F7`.
6. Enter `Y0` and confirm with Enter.
7. Check that a line connects the X0 contact to the Y0 coil and END is last.

```text
X0가 ON이면 → Y0 ON
X0가 OFF이면 → Y0 OFF
```

This text explains the operation. Enter the X0 contact and Y0 coil separately in the ladder editor.
See [Writing ladder logic](06-programming.en.md) for the contact/instruction input dialogs.

<a id="4단계-컴파일과-운전"></a>

## Step 4: compile and run

1. Select **Compile**. If it fails, read the error and correct the relevant input.
2. After success, select **RUN** at the top right.
3. Return to Wiring and select the **Select**.
4. Hold the first button and observe the lamp.
5. Release the button and check that the lamp goes off.
6. In Programming's **Monitor (F2)**, check that the X0 contact and Y0 state change together.
7. Save as `button_lamp_01.plcproj` with **Project → Save project…**.

<a id="안-켜질-때의-순서"></a>

## If the lamp does not light

| Observation | Check first |
| --- | --- |
| X0 stays off | 24V→COM, NO→X0, the button row pressed, and input mode |
| X0 is on but Y0 is OFF | Ladder addresses, successful compilation, RUN state |
| Y0 is on but the lamp is off | Y0→lamp 24V, lamp 0V, Source output mode |
| Lamp stays on after releasing the button | NC used instead of NO, or Y0 controlled by another circuit |

---

[User guide contents](README.en.md) · [← 4. Electrical, pneumatic, and optical wiring](04-wiring.en.md) · [6. Writing and editing ladder logic →](06-programming.en.md)
