[Contents](GUIDE.en.md) · [← Pause and recovery](logic-07-pause.en.md)

<a id="6-컴파일과-시운전-확인"></a>

# 6. Compiling and trial runs

1. **Compile** in Programming. For errors, check the relevant rung's instruction/operands.
2. Return to Wiring and **RUN**. After initialization, open HMI operation.
3. Test low/medium/high speed display, held JOG, home blinking→steady, motor 5s,
   and rack forward/reverse separately first.
4. Return home, then test one nonmetal workpiece going to the box in single-cycle mode.
5. For one metal workpiece, check slot 2 storage and hidden count display.
6. Restack four workpieces in the supply section and run continuous mode.
   Metal goes to 2→5, two nonmetal pieces to the box, with no manual reloading midway.
7. Check that Finish waits for the current workpiece to be stored before stopping.
8. Pause during post-pickup travel; check initialization after 5s and subsequent restart.

| Stopped at | Connections/conditions to check first |
| --- | --- |
| Waiting for supply | X7, supply retracted X1, gravity-feed Z, home D8340=0 |
| After processing | X4/X5, T0, motor ON through up-complete |
| Stopper | X6 and X16, stopper A/B, optical TX/RX |
| Just after pickup | VAC tubing, X20, X11, servo U/V/W/ENC and SON |
| Lift travel | DDRVA condition, M8340 OFF and X17 ON, absolute target position |
| Rack forward/reverse | Y15/Y16, coil common, crossed A/B, X12/X13 |
| Nonmetal box | X22 ON→OFF, box/workpiece Z, conveyor Y7 |
| Initialization stays active | X1/X3/X5/X11/X13/X21, T13, home/INP/BUSY |

---

[Contents](GUIDE.en.md) · [← Pause and recovery](logic-07-pause.en.md)
