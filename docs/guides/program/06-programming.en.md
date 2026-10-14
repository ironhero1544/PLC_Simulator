[User guide contents](README.en.md) · [← 5. First exercise: lighting a lamp with a button](05-first-practice.en.md) · [7. Observing ladder operation in Monitor →](07-monitoring.en.md)

<a id="6-래더-작성과-편집"></a>

# 6. Writing and editing ladder logic

Read ladder logic as **when the conditions on the left are true, perform the output on the right**.
Series means ‘AND’, parallel means ‘OR’, and a slashed contact passes when its device is OFF.

<a id="주소-읽기"></a>

## Reading addresses

| Symbol | Purpose | Example |
| --- | --- | --- |
| X | Physical input | `X0`: button or sensor |
| Y | Physical output | `Y0`: lamp, valve, or driver |
| M | Internal state | `M100`: HMI request, `M200`: run state |
| D | Numeric data | `D100`: speed setpoint |
| T / C | Timer / counter | Use completion contacts and current values |
| K | Decimal constant | `K10`: the number 10 |

X/Y addresses are octal. `X7` is followed by `X10`, not `X8`.
M and D addresses are decimal. Distinguish available physical pins from internal memory address ranges.

<a id="접점-입력"></a>

## Entering contacts

1. Click a cell in **Edit (F3)**.
2. Press `F5` for a normally open contact or `F6` for a normally closed contact.
3. Enter the address and confirm with Enter.
4. Adding a condition in the next cell of the same row creates a series condition.
5. Select an existing instruction and press Enter to edit it.

![Contact input dialog with X0 entered](../../images/program-guide/basic-contact-input.png)

*Press F5, enter X0, and press Enter.*

<a id="코일응용-명령-입력"></a>

## Entering coils and application instructions

Enter a coil in an empty cell with `F7` → `Y0` → Enter.
For an application instruction, press `F8`, enter the instruction and operands separated by spaces, then Enter.

For example, `OUT T0 K10` is a timer that waits one second.
Put spaces between `OUT`, `T0`, and `K10`. See [Light a lamp after one second](reference-basic-circuits.en.md#2-1초-뒤-램프-켜기).

![Instruction dialog with OUT T0 K10 entered](../../images/program-guide/basic-timer-command-input.png)

*Press F8, enter OUT T0 K10, and press Enter.*

![Entering a basic timer instruction](../../images/program-guide/basic-timer-entry.gif)

*Enter a timer after the X0 contact, then put a T0 contact and Y0 coil on the next row.*

<a id="상태를-기억할-때"></a>

## Remembering a state

| Instruction | Operation | Typical use |
| --- | --- | --- |
| OUT coil | ON/OFF follows the condition each scan | Output while a button is held |
| `SET M200` | Latch ON when the condition is true | Start automatic operation |
| `RST M200` | Clear the latched ON state | Stop/reset |
| `MOV K100 D100` | Copy a number | Set speed or state values |

Design a reset condition for every SET. Repeated OUT coils for the same Y output can let
later circuits overwrite earlier results, so combine output conditions.

<a id="행셀분기-편집"></a>

## Editing rows, cells, and branches

1. Use `Insert` to add a circuit before END.
2. Click a cell or move with arrow keys. Check the selection in **Cursor information** on the right.
3. Drag a rectangle to select multiple cells. Use circuit numbers to select rows.
4. Copy/paste the selection with `Ctrl+C / Ctrl+V`.
5. Clear the selection with `Delete`. Keep END as the final termination circuit.
6. Drag row numbers to reorder a selected group of rows.
7. Undo/redo with `Ctrl+Z / Ctrl+Y`.

**F9 adds horizontal wire; Shift+F9 adds vertical connections.**
For horizontal wire, enter the length. For a vertical connection, enter how many circuits to connect downward in the selected column.
Do not add vertical connections in the first column or on END. Create the lower circuits first.

Check both ends of a parallel branch. Conditions on separate rows without vertical connections
are not one OR branch. See the [industrial engineer ladder screenshots](../exam03/logic-02-manual.en.md).

<a id="회로-메모"></a>

## Circuit notes

Select a circuit and press the backtick key `` ` `` to open its note dialog.
Write a condition and purpose, such as `Supply extended → start processing`, to make large programs easier to read later.
Notes are saved with the project.

<a id="컴파일"></a>

## Compiling

1. Select **Compile** after editing. In Edit mode, `F4` also compiles.
2. If compilation fails, check the first error's instruction, address, and operands.
3. Correct it and compile again.
4. Confirm success and select **RUN**.

![Basic timer circuit](../../images/program-guide/basic-timer-complete.png)

*The first row waits one second; the second turns Y0 on after the wait.*

**Next exercise:** enter [start/stop latching, timers, counters, and HMI setpoints](reference-basic-circuits.en.md)
one circuit at a time and test each one.

---

[User guide contents](README.en.md) · [← 5. First exercise: lighting a lamp with a button](05-first-practice.en.md) · [7. Observing ladder operation in Monitor →](07-monitoring.en.md)
