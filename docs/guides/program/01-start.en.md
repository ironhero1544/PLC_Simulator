[User guide contents](README.en.md) · [2. Saving and opening projects →](02-projects.en.md)

<a id="1-실행과-화면-읽기"></a>

# 1. Starting the app and reading the screen

Start with **place and connect components in Wiring → write ladder logic in Programming → Compile → Start**.
RTL is an extension for creating new Verilog components. You can begin with basic PLC exercises.

<a id="실행-파일-선택"></a>

## Choosing an executable

| Download | How to run |
| --- | --- |
| EXE or MSI installer | Install it, then launch PLC Simulator |
| No-install ZIP | Extract the entire folder, then run `PLCSimulator.exe` inside it |
| Single EXE | Run the EXE. Required files are extracted automatically to a local cache |

Moving only the EXE out of the ZIP may prevent the app from finding DLLs, fonts, or resources.
Keep practice projects in a writable location, such as your Documents folder.
Distribution filenames may differ by version.

<a id="화면의-다섯-영역"></a>

## Five areas of the screen

![Menus, tools, and component list in Wiring. The equipment shown is an industrial engineer exam example](../../images/program-guide/02-wiring-overview.png)

*Menus, tools, and component list in Wiring. The equipment shown is an industrial engineer exam example*


| Area | Purpose | First action |
| --- | --- | --- |
| Top menu | Save/open projects, RTL, settings | Use the **Project** menu to save |
| Mode buttons | Switch between Wiring and Programming | Place components in **Wiring** |
| Tools below the mode | Select/Wire or Edit/Monitor/Compile | Choose a tool for the task |
| Center canvas | Edit components/wires or ladder logic | Zoom in to select pins or cells |
| Right side | Component list or programming information | Check filters, Z/L, and the selected cell |

**STOP / RUN** at the top right indicates the run state. **Edit / Monitor** in Programming indicates editing or observation mode.
Monitor and Start have different purposes.

<a id="첫-작업-순서"></a>

## Your first workflow

1. Select **Wiring**.
2. Drag components from the list on the right onto the canvas.
3. Select **Wire** and connect the pins.
4. Switch to **Programming → Edit (F3)**.
5. Enter contacts and coils, then select **Compile**.
6. If there are no errors, select **RUN** at the top right.
7. Check conditions and outputs in **Monitor (F2)**.
8. Save with **Project → Save project…**.

```mermaid
flowchart LR
  A[Place components] --> B[Wire]
  B --> C[Write ladder logic]
  C --> D[Compile]
  D --> E[Start and monitor]
  E --> F[Save project]
```

---

[User guide contents](README.en.md) · [2. Saving and opening projects →](02-projects.en.md)
