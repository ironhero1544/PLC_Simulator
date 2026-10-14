[User guide contents](README.en.md) · [← 3. Finding, placing, and labeling components](03-components.en.md) · [5. First exercise: lighting a lamp with a button →](05-first-practice.en.md)

<a id="4-전기공압광-배선"></a>

# 4. Electrical, pneumatic, and optical wiring

There is one **Wire** tool. **The starting pin automatically determines whether the connection is an electrical wire, pneumatic hose, or optical link.**
Check pin names and connection types before wire colors.

<a id="선-한-가닥-연결"></a>

## Connecting one wire

1. Select **Wiring → Wire**.
2. Click the starting pin. Zoom in first if the pins are close together.
3. Click empty space where the wire should bend to add a waypoint.
4. Add further waypoints in the same way.
5. Click the destination pin to complete the connection.
6. If you started from the wrong pin, right-click during wiring to cancel, then start again.
7. Check the pin names at both ends and the route.

![24V → button COM: starting pin, two waypoints, and destination pin](../../images/program-guide/wiring-route.gif)

*24V → button COM: starting pin, two waypoints, and destination pin*

Waypoints define the route. Crossing another wire on screen does not create a junction.
Check actual connections at the component pins.

<a id="전원과-제어선을-분리해서-작업"></a>

## Wiring power and control separately

| Stage | Example connection | Check |
| --- | --- | --- |
| Power | DC 24V → sensor 24V, DC 0V → sensor 0V | Connect both wires |
| Input | Sensor SIG/FWD/REV → PLC X | Match the ladder input address |
| Output | PLC Y → valve coil/driver control pin | Also provide actuator power and a return wire |
| Pneumatic | FRL OUT → valve P → cylinder A/B | Distinguish supply, working, and exhaust ports |
| Optical | Amplifier TX/RX → transmitter/receiver head | Distinguish electrical SIG from optical ports |

![Power wiring with separate 24V and 0V connections](../../images/exam03-guide/05-power-pair.png)

*Power wiring with separate 24V and 0V connections*

*The photo shows a servo amplifier. Check 24V and 0V separately for other components in the same way.*

<a id="plc-입력출력-모드-확인"></a>

## Checking PLC input/output modes

1. Double-click the PLC body with the Select tool.
2. Check the **Sink / Source** input mode and the output mode.
3. Connect supply voltage and return paths for the selected modes.
4. If you change the output mode, also review valve, lamp, and driver wiring.

The first exercise below uses **Sink input / Source output**. Copying its wiring with different modes
may prevent inputs or outputs from working.

<a id="배선-정돈과-태그"></a>

## Organizing wires and tags

Separate routes: power above, control around the PLC, and pneumatic hoses below the actuators.
Use right-angle routes through empty space rather than long diagonals across bodies and labels.

To shorten the display of a long connection, select **Tag** and click an existing wire.
Set the tag name and color. Tags change how an existing connection is displayed; they do not create power or signals.
Use **Alt + right-click** on the tag/wire to remove a tag.
Remove tags when you need to inspect the actual wire routes.

![Displaying long connections compactly with wire embedding](../../images/program-guide/wire-tags.gif)

*Displaying long connections compactly with wire embedding*

<a id="수정-후-확인"></a>

## Checking after edits

Use selection, deletion, and undo to fix incorrect connections. When pins are close together,
a click may select the neighboring pin. Zoom in and check which pin the wire ends on.

---

[User guide contents](README.en.md) · [← 3. Finding, placing, and labeling components](03-components.en.md) · [5. First exercise: lighting a lamp with a button →](05-first-practice.en.md)
