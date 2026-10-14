[Contents](GUIDE.en.md) · [← Power wiring](02-power.en.md) · [HMI screens →](04-hmi.en.md)

<a id="3-제어-배선"></a>

# 3. Control wiring

> **Goal:** finish sensor inputs, actuator outputs, and pneumatic/optical links.
> **Target:** 65 control connections; 116 including power.

<a id="plc-입력-x"></a>

## PLC inputs X

Connect cylinder completion sensors, optical amplifier outputs, and capacitive/inductive signals to PLC X.
Set sensor outputs to PNP and PLC inputs to Sink.
Use the placement JSON settings as supplied.
PLC X/Y addresses are octal: X7 → X10 and X17 → X20.
Use a PLC version with input rows supporting X20–X22.

Route each signal above the input rows, then down to its X pin.
Zoom into pins to distinguish sensor power from signal connections.

Start at transfer amplifier〔43〕SIG and route below the controls,
then connect to PLC〔0〕X23. Check the destination tooltip `Role: X23`.
On this PLC, it is the fourth pin of the third input row.

![Optical amplifier SIG connected to PLC X23](../../images/exam03-guide/06-sensor-plc.png)

<details>
<summary>Expand detailed input table · 20 items</summary>

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | Workpiece detector〔23〕 · **SIG** | PLC〔0〕 · **X7** |
| 2 | Transfer detector〔43〕 · **SIG** | PLC〔0〕 · **X23** |
| 3 | Stopper detector〔46〕 · **SIG** | PLC〔0〕 · **X16** |
| 4 | Servo amplifier〔3〕 · **INP** | PLC〔0〕 · **X17** |
| 5 | Supply〔10〕 · **FWD** | PLC〔0〕 · **X0** |
| 6 | Supply〔10〕 · **REV** | PLC〔0〕 · **X1** |
| 7 | Transfer〔11〕 · **FWD** | PLC〔0〕 · **X2** |
| 8 | Transfer〔11〕 · **REV** | PLC〔0〕 · **X3** |
| 9 | Lift · Vacuum pickup〔14〕 · **FWD** | PLC〔0〕 · **X10** |
| 10 | Lift · Vacuum pickup〔14〕 · **REV** | PLC〔0〕 · **X11** |
| 11 | Six-slot rack〔16〕 · **FWD** | PLC〔0〕 · **X12** |
| 12 | Six-slot rack〔16〕 · **REV** | PLC〔0〕 · **X13** |
| 13 | Stopper〔13〕 · **DOWN** | PLC〔0〕 · **X6** |
| 14 | Stopper〔13〕 · **UP** | PLC〔0〕 · **X21** |
| 15 | Lift · Vacuum pickup〔14〕 · **VAC confirmation** | PLC〔0〕 · **X20** |
| 16 | Capacitive sensor〔24〕 · **SIG** | PLC〔0〕 · **X14** |
| 17 | Passage detector〔25〕 · **SIG** | PLC〔0〕 · **X22** |
| 18 | Processing〔12〕 · **FWD** | PLC〔0〕 · **X4** |
| 19 | Processing〔12〕 · **REV** | PLC〔0〕 · **X5** |
| 20 | Inductive sensor〔22〕 · **SIG** | PLC〔0〕 · **X15** |

</details>

<a id="plc-출력-y"></a>

## PLC outputs Y

Use Source outputs. Connect Y to coil + or a control pin.
Connect double-solenoid A/B coils to different Y outputs.
Y17 branches to both run air supply and servo SON.
Y7 branches to both conveyor drive pins.
Use the processing cylinder's built-in MOTOR pin for motor control.

For example, start at Y1 and make two bends between the PLC and valve rows,
then connect to supply valve〔30〕's red coil + pin.
The destination tooltip must be `SOL_A_24V`, not the black `SOL_A_0V`
just below. Zoom in if pins look joined; undo a wrong connection with Ctrl+Z.
Connect coil 0V separately following the table in chapter 2's 0V section.

![Y1 connected to supply valve coil +](../../images/exam03-guide/21-plc-output.png)

<details>
<summary>Expand detailed output table · 18 items</summary>

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | PLC〔0〕 · **Y17** | Servo amplifier〔3〕 · **SON** |
| 2 | PLC〔0〕 · **Y0** | Servo amplifier〔3〕 · **PULSE** |
| 3 | PLC〔0〕 · **Y4** | Servo amplifier〔3〕 · **DIR** |
| 4 | PLC〔0〕 · **Y3** | Processing〔12〕 · **MOTOR** |
| 5 | PLC〔0〕 · **Y7** | Conveyor〔17〕 · **24V** |
| 6 | PLC〔0〕 · **Y17** | Air supply〔6〕 · **Coil A +** |
| 7 | PLC〔0〕 · **Y1** | Supply〔30〕 · **Coil A +** |
| 8 | PLC〔0〕 · **Y2** | Transfer〔31〕 · **Coil A +** |
| 9 | PLC〔0〕 · **Y5** | Processing〔32〕 · **Coil A +** |
| 10 | PLC〔0〕 · **Y6** | Processing〔32〕 · **Coil B +** |
| 11 | PLC〔0〕 · **Y10** | Stopper〔33〕 · **Coil A +** |
| 12 | PLC〔0〕 · **Y11** | Stopper〔33〕 · **Coil B +** |
| 13 | PLC〔0〕 · **Y12** | Vacuum pickup〔34〕 · **Coil A +** |
| 14 | PLC〔0〕 · **Y13** | Vacuum pickup〔34〕 · **Coil B +** |
| 15 | PLC〔0〕 · **Y15** | Storage rack〔35〕 · **Coil A +** |
| 16 | PLC〔0〕 · **Y16** | Storage rack〔35〕 · **Coil B +** |
| 17 | PLC〔0〕 · **Y14** | Vacuum〔36〕 · **Coil A +** |
| 18 | PLC〔0〕 · **Y7** | Transfer conveyor〔18〕 · **24V** |

</details>

<a id="공압-배관"></a>

## Pneumatic tubing

FRL〔5〕uses setting 5, equivalent to 0.5 MPa.
Supply distributor〔7〕through the run air valve〔6〕.
The single-solenoid supply valve's B port connects to the distributor because
that is the pressurized port in this model's run-ON state.
Cross processing/stopper/pickup/rack valve ports with cylinder A/B as listed below.
Follow terminal names, not their apparent left/right positions.

Take the vacuum branch from **FRL before the supply shutoff valve**.
This branch position is required to retain the workpiece when pausing during pickup.
Do not add a separate vacuum pad. Use VAC on the lift's integrated cylinder.

Start the first hose at FRL〔5〕's lower OUT. Route below the valve row,
then horizontally to the lower P_IN of run air valve〔6〕.
Use the same **Wire** tool as for electrical wiring.

![FRL OUT connected to run supply valve P](../../images/exam03-guide/07-pneumatic.png)

<details>
<summary>Expand detailed tubing table · 23 items</summary>

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | 0.5MPa〔5〕 · **OUT** | Air supply〔6〕 · **P** |
| 2 | Air supply〔6〕 · **B** | Distribution〔7〕 · **IN** |
| 3 | Distribution〔7〕 · **OUT1** | Supply〔30〕 · **P** |
| 4 | Supply〔30〕 · **A** | Supply〔10〕 · **A** |
| 5 | Supply〔30〕 · **B** | Supply〔10〕 · **B** |
| 6 | Distribution〔7〕 · **OUT1** | Transfer〔31〕 · **P** |
| 7 | Transfer〔31〕 · **A** | Transfer〔11〕 · **A** |
| 8 | Transfer〔31〕 · **B** | Transfer〔11〕 · **B** |
| 9 | Distribution〔7〕 · **OUT2** | Processing〔32〕 · **P** |
| 10 | Processing〔32〕 · **A** | Processing〔12〕 · **B** |
| 11 | Processing〔32〕 · **B** | Processing〔12〕 · **A** |
| 12 | Distribution〔7〕 · **OUT2** | Stopper〔33〕 · **P** |
| 13 | Stopper〔33〕 · **A** | Stopper〔13〕 · **B** |
| 14 | Stopper〔33〕 · **B** | Stopper〔13〕 · **A** |
| 15 | Distribution〔7〕 · **OUT2** | Vacuum pickup〔34〕 · **P** |
| 16 | Vacuum pickup〔34〕 · **A** | Lift · Vacuum pickup〔14〕 · **B** |
| 17 | Vacuum pickup〔34〕 · **B** | Lift · Vacuum pickup〔14〕 · **A** |
| 18 | Distribution〔7〕 · **OUT2** | Storage rack〔35〕 · **P** |
| 19 | Storage rack〔35〕 · **A** | Six-slot rack〔16〕 · **B** |
| 20 | Storage rack〔35〕 · **B** | Six-slot rack〔16〕 · **A** |
| 21 | 0.5MPa〔5〕 · **OUT** | Vacuum〔36〕 · **P** |
| 22 | Vacuum〔36〕 · **B** | Vacuum generator〔21〕 · **P** |
| 23 | Vacuum generator〔21〕 · **VAC** | Lift · Vacuum pickup〔14〕 · **VAC** |

</details>

<a id="광-케이블"></a>

## Optical cables

Connect amplifier TX to the transmitter head and RX to the receiver head.
Do not wire heads directly to PLC X. The PLC reads amplifier SIG.
Face the heads toward each other at the same Z, with workpieces passing between them.
SIG wiring without optical cables does not detect workpieces.

| Order | Starting terminal | Destination terminal |
| --- | --- | --- |
| 1 | Transfer detector〔43〕 · **TX** | Transfer Workpiece TX〔44〕 · **24V** |
| 2 | Transfer detector〔43〕 · **RX** | Transfer Workpiece RX〔45〕 · **24V** |
| 3 | Stopper detector〔46〕 · **TX** | Stopper TX〔47〕 · **24V** |
| 4 | Stopper detector〔46〕 · **RX** | Stopper RX〔48〕 · **24V** |

<a id="배선-완료-확인"></a>

## Checking completed wiring

1. Check each table row. There are 116 connections excluding the two HMI power wires.
2. Distinguish supply/transfer/pickup/rack FWD/REV from stopper DOWN/UP.
3. Check servo PULSE=Y0, DIR=Y4, INP=X17, SON=Y17.
4. Check vacuum confirmation=X20, stopper up=X21, box passage=X22.
5. Before using `Tag`, check table tag names and actual connections.
   Keep direct wiring initially in this guide to trace connections.
6. Save your progress with Wiring **Save** under a separate name.
   Do not overwrite the provided placement JSON.

<a id="제어-배선-완료-참고-사진"></a>

## Completed control wiring reference

This shows chapter 3's **65 control/pneumatic/optical connections**.
PLC X/Y wires are gray, pneumatic hoses blue, and optical cables purple.
Check control, pneumatic, and optical wiring at this stage.
Your actual circuit must also include the preceding 51 power connections.
Together these total 116. Finish all wiring before starting operation.

![Completed control, pneumatic, and optical wiring reference](../../images/exam03-guide/23-control-complete.png)

---

[Contents](GUIDE.en.md) · [← Power wiring](02-power.en.md) · [HMI screens →](04-hmi.en.md)
