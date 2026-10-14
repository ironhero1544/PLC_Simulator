<a id="full-adder-rtl-부품-배치배선사용-실습"></a>

# Full Adder: placing, wiring, and using an RTL component

[User guide contents](README.en.md) · [Basic RTL creation](12-rtl.en.md)

**Place your Full Adder on the Wiring canvas, change inputs with buttons, and read the results on two lamps.**
A/B/CIN are one-bit inputs. SUM is the low sum bit; COUT is carry-out.
For example, `1 + 1 + 0 = 2` gives SUM=0 and COUT=1.

<a id="1-full-adder-만들기"></a>

## 1. Creating a Full Adder

1. Prepare the build environment with [Tool Doctor](rtl-tool-doctor.en.md).
2. Select **RTL → RTL Library → New RTL Module**.
3. Set **Display name** to `FULL ADDER` and **Top module** to `full_adder`.
4. Select the generated [TOP] file and enter the code below in the editor on the right.

```verilog
module full_adder(
  input wire VCC,
  input wire GND,
  input wire A,
  input wire B,
  input wire CIN,
  output wire SUM,
  output wire COUT
);
  assign SUM = A ^ B ^ CIN;
  assign COUT = (A & B) | (A & CIN) | (B & CIN);
endmodule
```

![Full Adder source and top-module settings](../../images/program-guide/full-adder-01-source.jpg)

*Full Adder entered in the app. Distinguish the filename from the top-module name.*

VCC/GND are power pins. This exercise uses **industrial 24V logic**.
Declare each port on its own line, as in the code above.
The filename can be generated automatically. **Top module** must match `module full_adder`.

5. Select **Analyze**. Check inputs VCC/GND/A/B/CIN and outputs SUM/COUT.
6. Select **Build** and wait for ‘Build status: Success’.
7. Enable **Enable As Component** in the results below, then close the RTL window.

![Successful Full Adder build and activation](../../images/program-guide/full-adder-02-enabled.jpg)

*‘Enable As Component’ checked after a successful build.*

<a id="2-캔버스에-배치하기"></a>

## 2. Placing components

| Component | Quantity | Purpose |
| --- | --- | --- |
| DC power supply | 1 | Supply 24V/0V |
| FULL ADDER | 1 | The RTL component just created |
| Lamp/button unit | 3 | A/B/CIN inputs; use each unit's third green button |
| Lamp/button unit | 1 | First lamp: SUM; second lamp: COUT |

1. Switch to **Wiring**. Set filters to **All / All functions**.
2. Scroll down to the `FULL ADDER` card. You can narrow the filters to **All / Control**.
3. **Drag the card into the center of the canvas.** Selecting its name in the library does not place it.
4. Put power at the upper left, three input units vertically on the left, and the output unit on the right.
5. Rename input units A/B/CIN and output unit SUM / COUT, then enable **Show name**.
6. Double-click FULL ADDER and check **RTL Runtime → Logic family → Industrial 24V**.
   Power is VCC and ground is GND. Do not assign clock/reset to this combinational circuit.

![FULL ADDER card in the Control filter](../../images/program-guide/full-adder-03-list.jpg)

*Certification is ‘All’; function is ‘Control’. The card is named FULL ADDER.*

![Power, input, Full Adder, and output placement](../../images/program-guide/full-adder-04-placement.jpg)

*A/B/CIN on the left, FULL ADDER in the center, SUM / COUT on the right.*

If missing, check **Analyze success → Build success → Enable As Component** in the library.
Also check that the component filters do not hide RTL components.

<a id="3-전원-배선"></a>

## 3. Power wiring

Select **Wire**. Connections between electrical pins are electrical wires.
Click the starting pin → required waypoints → destination pin.

| Starting pin | Destination pin |
| --- | --- |
| DC 24V | FULL ADDER VCC |
| DC 0V | FULL ADDER GND |
| DC 24V | **Third-row NO** on each A/B/CIN input unit |
| DC 0V | **Third-row NC** on each A/B/CIN input unit |
| DC 0V | **First-row LAMP_0V** on the output unit |
| DC 0V | **Second-row LAMP_0V** on the output unit |

NO/NC are button contacts; LAMP_24V/LAMP_0V are lamp pins.
Do not wire input-unit lamps or output-unit button contacts in this exercise.

![Full Adder power wiring](../../images/program-guide/full-adder-05-power.jpg)

*Power wiring completed first. Signal wires are connected next.*

<a id="4-신호-배선"></a>

## 4. Signal wiring

| Starting pin | Destination pin |
| --- | --- |
| A input unit, third-row **COM** | FULL ADDER **A** |
| B input unit, third-row **COM** | FULL ADDER **B** |
| CIN input unit, third-row **COM** | FULL ADDER **CIN** |
| FULL ADDER **SUM** | Output unit, first-row **LAMP_24V** |
| FULL ADDER **COUT** | Output unit, second-row **LAMP_24V** |

With the button ON, COM connects to NO at 24V, making input 1;
with it OFF, COM connects to NC at 0V, making input 0.
**COM goes to the input; NO/NC select between two voltages.**
Do not directly connect 24V and 0V to the same pin.

![Full Adder with power and signal wiring completed](../../images/program-guide/full-adder-06-wired.jpg)

*Input COMs connect to the left-side input pins; SUM/COUT connect to output-unit lamps.*

<a id="5-운전하고-결과-읽기"></a>

## 5. Running and reading results

1. In **Programming**, **compile** an exercise program containing only END.
   No X/Y-control ladder circuit is needed here.
2. Select **RUN** at the top right and return to **Wiring → Select tool**.
3. **Double-click each input unit's third green button** to toggle it.
   The third button latches. Do not use the red/yellow momentary buttons above.
4. Read **first red lamp = SUM**, **second yellow lamp = COUT** on the output unit.

| A | B | CIN | SUM: first lamp | COUT: second lamp |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | OFF | OFF |
| 0 | 0 | 1 | ON | OFF |
| 0 | 1 | 0 | ON | OFF |
| 0 | 1 | 1 | OFF | ON |
| 1 | 0 | 0 | ON | OFF |
| 1 | 0 | 1 | OFF | ON |
| 1 | 1 | 0 | OFF | ON |
| 1 | 1 | 1 | ON | ON |

Read input states from each button's pressed appearance.
Input-unit green **lamps** are not wired, so their brightness does not indicate input state.

![Eight Full Adder input combinations and SUM/COUT lamp states](../../images/program-guide/full-adder-operation.gif)

*Read the input values above together with the SUM/COUT lamps on the right.*

<a id="안-될-때"></a>

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Cannot start | Successful ladder compilation |
| Both lamps always OFF | RUN, VCC/GND, industrial 24V setting, build success |
| One input does not change | Third-row COM/NO/NC and green-button double-click |
| Reversed result | NO=24V, NC=0V, correct SUM/COUT lamp rows |
| Old behavior after source edit | Stop, reanalyze/rebuild, then start again |

Finally, save `.plcproj` with **Project → Save project…**.
This saves wiring, program, and RTL components together.

[User guide contents](README.en.md) · [Basic RTL creation](12-rtl.en.md)
