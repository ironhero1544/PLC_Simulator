[User guide contents](README.en.md) · [← 10. L layers: order, groups, and opacity](10-render-layers.en.md) · [12. Creating and using RTL components →](12-rtl.en.md)

<a id="11-센서공압모터공작물-실습"></a>

# 11. Sensors, pneumatics, motors, and workpieces

Mechanical parts require correct **position, direction, and height** as well as wiring and signal conditions.
Verify each movement before connecting the next actuator to simplify automatic-sequence troubleshooting.

<a id="공압-공급과-복동실린더"></a>

## Air supply and double-acting cylinders

1. Place an air service unit (FRL), solenoid valve, and double-acting cylinder.
2. Set FRL pressure. The industrial engineer example uses 0.5MPa.
3. Connect FRL OUT to solenoid P.
4. Connect solenoid A/B to cylinder A/B.
5. Connect the solenoid coil's power, PLC control wire, and 0V.
6. Activate only one direction output at a time and test extension/retraction.
7. For built-in sensors, connect 24V/0V and wire FWD/REV to PLC X.

![Pneumatic wiring from FRL supply to solenoid P](../../images/exam03-guide/07-pneumatic.png)

*Pneumatic wiring from FRL supply to solenoid P*


Read working ports and supply/exhaust routes from the solenoid's pneumatic symbol.
Swapping cylinder A/B can reverse the expected rod direction.
Interlock opposite directions to avoid energizing opposing coils simultaneously.

<a id="센서와-광화이버"></a>

## Sensors and fiber optics

Power ordinary sensors first, then connect their signal pin to X.
Inductive sensors detect metal; capacitive sensors have different targets. Read the component descriptions.
Sensors clustered along a workpiece path can detect simultaneously and obscure the sequence.

Fiber-optic sensing uses **an amplifier, transmitter head, and receiver head together**.

1. Connect 24V/0V to the amplifier.
2. Connect optical TX to the transmitter and RX to the receiver.
3. Connect amplifier SIG to PLC X.
4. Align head positions/directions so workpieces enter the detection path between them.
5. Check sensor/workpiece Z heights.
6. Check that amplifier readings/indicators and X input change together.

If the amplifier stays at 0000, check power, both optical links, and head placement first.
If its display changes but PLC X does not, check SIG wiring, input mode, and address.

<a id="dc-모터-드라이버-plc-속도-제어"></a>

## DC motor driver: PLC speed control

| Driver pin | Connection / role |
| --- | --- |
| 24V / 0V | Driver power |
| EN | PLC enable output |
| DIR | PLC direction output |
| BRK | PLC brake output |
| PWM | PLC PWM output |
| M+ / M− | DC motor + / − |

1. Connect the motor to driver M+/M−.
2. Connect 24V/0V power.
3. Connect EN/DIR/BRK to selected PLC outputs.
4. Connect PWM to **one output from Y0–Y3**.
5. Generate PWM in ladder logic. With the condition ON, `PWM K5 K10 Y0` means
   5ms ON time / 10ms period, or 50% duty.
6. Enable EN, release BRK, then check direction and speed.

EN/DIR/BRK are not fixed to Y0. However, **PWM supports only Y0–Y3**.
Do not connect multiple outputs to one PWM pin.
Without a PWM instruction, a normal Y ON is treated as 100% and OFF as 0%.
This is not a potentiometer exercise. PWM control needs no separate analog output module.
Do not assign PWM and ordinary OUT or servo-motion instructions to the same Y simultaneously.

<a id="서보-리프트와-원점"></a>

## Servo lift and homing

Servo motors use **U/V/W, PE, and ENC**. Do not confuse them with stepper A+/A−/B+/B−.
Connect amplifier power, SON/pulse/direction/status signals, motor drive, and ENC according to pin names.

A lift converts rotation to ball-screw linear travel. Follow [Z and servo axis settings](09-z-layers.en.md).
Test homing and short JOG movements before automatic positioning.
JOG at an intermediate position can invalidate the next move if old automatic-stage M bits or coordinate references remain.

<a id="흡착스토퍼"></a>

## Vacuum pickup and stopper

Check rod movement and vacuum together. The pad must reach the workpiece's actual position and height.
Vacuum ON alone does not attract a distant workpiece.

A stopper blocks a workpiece when its pin reaches the conveyor path's height.
At an inactive height, the pin is hidden in top view, as on the real equipment; it is not a permanently protruding rod.
Align stopper, conveyor, and workpiece heights, then test passing/stopping with ON/OFF.

<a id="공작물-공급보관"></a>

## Supplying and storing workpieces

Stack workpieces on the workpiece detection sensor.
The first workpiece is at working height; waiting workpieces stay above. After the first leaves
and related actuators stop, the next falls into place. A separate magazine does not lift them.

A storage box arranges and stores multiple workpieces entering at the same working height.
The six-slot rack has two slots per level. Use hover/slot previews to inspect stored workpieces.
Drag workpieces out with the Select tool. If selection fails, check L visibility, opacity, and covering components.

For horizontal rack travel, use **servo-driven storage** or another matching actuator component.
Pneumatic storage is a different model moved by cylinder A/B pressure.
A static rack does not move merely by attaching a signal wire.

For a complete equipment exercise, continue with [industrial engineer exam 3](../exam03/GUIDE.en.md).

---

[User guide contents](README.en.md) · [← 10. L layers: order, groups, and opacity](10-render-layers.en.md) · [12. Creating and using RTL components →](12-rtl.en.md)
