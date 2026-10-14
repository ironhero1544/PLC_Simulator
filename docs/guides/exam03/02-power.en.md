[Contents](GUIDE.en.md) · [← Placement](01-placement.en.md) · [Control wiring →](03-control.en.md)

<a id="2-전원-배선"></a>

# 2. Power wiring

> **Goal:** finish sensor/coil power and servo power connections.
> **Target:** 51 power connections.

<a id="배선-도구와-직각-경로"></a>

## Wire tool and right-angle routes

Select **Wire**. Click starting pin → bend points in empty space → destination pin.
Empty-space clicks add waypoints. Right-click cancels an in-progress connection.
Pin types automatically determine electrical, pneumatic, or optical connections.
Route through spaces between component rows rather than through the centers of their bodies.

Use red for 24V, black for 0V, orange for PLC control, blue for air,
and purple for optical cables to make tracing easier.
After connecting, select wires to tidy their colors and waypoints.
Do not combine different signals into one wire.
A crossing that looks like a branch does not establish an electrical junction.
Actually connect each starting/destination pin listed in the tables.

<a id="실제-클릭-예-dc-24v--송출-감지-증폭부-24v"></a>

### Click example: DC 24V → transfer amplifier 24V

1. Select **Wire** and click DC supply〔1〕's red **24V** pin.
2. Click empty space above the supply to add the first bend.

![Starting pin to first bend](../../images/exam03-guide/02-wire-waypoint.png)

3. Click above transfer amplifier〔43〕 to make a horizontal route.

![Horizontal route above components](../../images/exam03-guide/03-wire-route.png)

4. Click **24V** on the amplifier's right to finish.
   Hover and check `Role: POWER_24V`, `Direction: Input`.
   SIG/0V are separate pins; do not connect them to this power wire.

![Connection completed at amplifier 24V](../../images/exam03-guide/04-24v-connected.png)

The photo's wire was created in default orange. Adjust color after connecting.
If ‘Incompatible’ appears beside a pin, recheck the terminal.
Do not infer completion from that text; check that the wire is fixed to the destination.

<a id="24v-분배"></a>

## 24V distribution

Start at DC supply〔1〕's red 24V and make each connection below.
Run horizontally above the supply, then down to each component's pin.
Do not connect solenoid coil + directly to 24V here; it is driven by PLC Y.

<details>
<summary>Expand detailed wiring table · 14 items</summary>

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | 24V DC〔1〕 · **24V** | Workpiece detector〔23〕 · **24V** |
| 2 | 24V DC〔1〕 · **24V** | Transfer detector〔43〕 · **24V** |
| 3 | 24V DC〔1〕 · **24V** | Stopper detector〔46〕 · **24V** |
| 4 | 24V DC〔1〕 · **24V** | Servo amplifier〔3〕 · **24V** |
| 5 | 24V DC〔1〕 · **24V** | Servo amplifier〔3〕 · **RESET** |
| 6 | 24V DC〔1〕 · **24V** | Supply〔10〕 · **24V** |
| 7 | 24V DC〔1〕 · **24V** | Transfer〔11〕 · **24V** |
| 8 | 24V DC〔1〕 · **24V** | Lift · Vacuum pickup〔14〕 · **24V** |
| 9 | 24V DC〔1〕 · **24V** | Six-slot rack〔16〕 · **24V** |
| 10 | 24V DC〔1〕 · **24V** | Stopper〔13〕 · **24V** |
| 11 | 24V DC〔1〕 · **24V** | Capacitive sensor〔24〕 · **24V** |
| 12 | 24V DC〔1〕 · **24V** | Passage detector〔25〕 · **24V** |
| 13 | 24V DC〔1〕 · **24V** | Processing〔12〕 · **24V** |
| 14 | 24V DC〔1〕 · **24V** | Inductive sensor〔22〕 · **24V** |

</details>

<a id="0v-공통"></a>

## 0V common

Connect each wire from DC supply〔1〕's black 0V pin.
Use a separate route one grid step below 24V.
Servo P-COM is 0V common; PE is protective earth. They are different pins.

Connect 0V to amplifier〔43〕used in the 24V example. The destination tooltip
must show `POWER_0V`. 0V is shown blue in the photo.
Distinguish electrical/pneumatic connections by pin type, not just wire color.

![Amplifier 24V and 0V connections](../../images/exam03-guide/05-power-pair.png)

<details>
<summary>Expand detailed wiring table · 28 items</summary>

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | 24V DC〔1〕 · **0V** | Workpiece detector〔23〕 · **0V** |
| 2 | 24V DC〔1〕 · **0V** | Transfer detector〔43〕 · **0V** |
| 3 | 24V DC〔1〕 · **0V** | Stopper detector〔46〕 · **0V** |
| 4 | 24V DC〔1〕 · **0V** | Servo amplifier〔3〕 · **0V** |
| 5 | 24V DC〔1〕 · **0V** | Servo amplifier〔3〕 · **P-COM** |
| 6 | 24V DC〔1〕 · **0V** | Supply〔10〕 · **0V** |
| 7 | 24V DC〔1〕 · **0V** | Transfer〔11〕 · **0V** |
| 8 | 24V DC〔1〕 · **0V** | Lift · Vacuum pickup〔14〕 · **0V** |
| 9 | 24V DC〔1〕 · **0V** | Six-slot rack〔16〕 · **0V** |
| 10 | 24V DC〔1〕 · **0V** | Stopper〔13〕 · **0V** |
| 11 | 24V DC〔1〕 · **0V** | Capacitive sensor〔24〕 · **0V** |
| 12 | 24V DC〔1〕 · **0V** | Passage detector〔25〕 · **0V** |
| 13 | 24V DC〔1〕 · **0V** | Processing〔12〕 · **0V** |
| 14 | 24V DC〔1〕 · **0V** | Inductive sensor〔22〕 · **0V** |
| 15 | 24V DC〔1〕 · **0V** | Conveyor〔17〕 · **0V** |
| 16 | 24V DC〔1〕 · **0V** | Air supply〔6〕 · **Coil A 0V** |
| 17 | 24V DC〔1〕 · **0V** | Supply〔30〕 · **Coil A 0V** |
| 18 | 24V DC〔1〕 · **0V** | Transfer〔31〕 · **Coil A 0V** |
| 19 | 24V DC〔1〕 · **0V** | Processing〔32〕 · **Coil A 0V** |
| 20 | 24V DC〔1〕 · **0V** | Processing〔32〕 · **Coil B 0V** |
| 21 | 24V DC〔1〕 · **0V** | Stopper〔33〕 · **Coil A 0V** |
| 22 | 24V DC〔1〕 · **0V** | Stopper〔33〕 · **Coil B 0V** |
| 23 | 24V DC〔1〕 · **0V** | Vacuum pickup〔34〕 · **Coil A 0V** |
| 24 | 24V DC〔1〕 · **0V** | Vacuum pickup〔34〕 · **Coil B 0V** |
| 25 | 24V DC〔1〕 · **0V** | Storage rack〔35〕 · **Coil A 0V** |
| 26 | 24V DC〔1〕 · **0V** | Storage rack〔35〕 · **Coil B 0V** |
| 27 | 24V DC〔1〕 · **0V** | Vacuum〔36〕 · **Coil A 0V** |
| 28 | 24V DC〔1〕 · **0V** | Transfer conveyor〔18〕 · **0V** |

</details>

<a id="서보-ac-전원"></a>

## Servo AC power

Connect L1/L2/L3/PE on AC supply〔4〕and servo amplifier〔3〕by matching names.
Do not mix 24V sensor supply with AC terminals. This wiring follows the simulator model.

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | AC supply〔4〕 · **L1** | Servo amplifier〔3〕 · **L1** |
| 2 | AC supply〔4〕 · **L2** | Servo amplifier〔3〕 · **L2** |
| 3 | AC supply〔4〕 · **L3** | Servo amplifier〔3〕 · **L3** |
| 4 | AC supply〔4〕 · **PE** | Servo amplifier〔3〕 · **PE** |

<a id="서보-동력피드백"></a>

## Servo motor power and feedback

Connect U/V/W/PE/ENC from servo amplifier〔3〕to lift/pickup〔14〕.
This is a U/V/W servo model, different from an A+/A-/B+/B- stepper.
Distinguish the lift's upper motor terminals from the cylinder sensor terminals.

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | Servo amplifier〔3〕 · **U** | Lift · Vacuum pickup〔14〕 · **U** |
| 2 | Servo amplifier〔3〕 · **V** | Lift · Vacuum pickup〔14〕 · **V** |
| 3 | Servo amplifier〔3〕 · **W** | Lift · Vacuum pickup〔14〕 · **W** |
| 4 | Servo amplifier〔3〕 · **PE** | Lift · Vacuum pickup〔14〕 · **PE** |
| 5 | Servo amplifier〔3〕 · **ENC** | Lift · Vacuum pickup〔14〕 · **ENC** |

<a id="hmi-연결"></a>

## HMI connection

HMI communication connects to PLC automatically. Connect DC supply〔1〕24V/0V to HMI 24V/0V respectively. Add these two power wires separately from the 51 connections above.
Edit HMI buttons to write M addresses and numeric displays to read D addresses.

<a id="전원동력-배선-완료-참고-사진"></a>

## Completed power wiring reference

Only the **51 power connections** from this chapter's tables are shown.
Check 24V/0V distribution, AC supply, and servo power/feedback.
PLC I/O, pneumatic, and optical links are added next and are absent from this photo.
HMI is empty because its screens have not been designed yet.

![Completed power wiring reference](../../images/exam03-guide/22-power-complete.png)

---

[Contents](GUIDE.en.md) · [← Placement](01-placement.en.md) · [Control wiring →](03-control.en.md)
