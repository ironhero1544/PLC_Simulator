[User guide contents](README.en.md) · [← 1. Starting the app and reading the screen](01-start.en.md) · [3. Finding, placing, and labeling components →](03-components.en.md)

<a id="2-프로젝트-저장불러오기"></a>

# 2. Saving and opening projects

Save a **`.plcproj` integrated project** to continue working on the entire exercise.
JSON in Wiring and CSV in Programming save each area separately.

<a id="파일과-여는-위치"></a>

## File types and where to open them

| File | Contents | Open from |
| --- | --- | --- |
| `.plcproj` | Components, wires, ladder logic, HMI settings, layers, and project RTL data | **Project → Open project…** |
| `.json` | Wiring layout and its settings | **Wiring → Load** |
| `.csv` | Ladder program | **Programming → Load** |
| `.plccomp` | Reusable RTL component package | **RTL Library → Import component** |

![Project menu with integrated save/open commands](../../images/program-guide/01-project-menu.png)

*Project menu with integrated save/open commands*


<a id="전체-실습-저장"></a>

## Saving the entire exercise

1. Select **Project → Save project…** at the top. `Ctrl+S` opens the same dialog.
2. Choose a folder.
3. Enter a recognizable filename, such as `button_lamp_01.plcproj`.
4. Save. Before overwriting an existing file, check the selected filename.
5. Before major changes, save another copy, such as `button_lamp_02.plcproj`.

<a id="다시-열어-이어서-작업"></a>

## Reopening and continuing

1. Save your current work first.
2. Select **Project → Open project…**.
3. Choose the `.plcproj` file.
4. Check components, wires, and HMI settings in Wiring.
5. Check that the ladder program loaded in Programming.
6. Check the compilation state. Recompile if you changed the program or compilation is required.
7. Before starting, check workpiece positions and homing state.

Opening a project does not merely add a few components to the current work;
it opens the exercise saved in that file.

<a id="배치-또는-래더만-따로-저장할-때"></a>

## Saving only the layout or ladder program

**Save** in the Wiring toolbar saves a JSON layout. It does not save the ladder program.
**Save** in the Programming toolbar saves CSV. It does not include wiring.

The industrial engineer guide's **placement-only JSON** is also a file for practicing from component placement.
Opening it does not complete power wiring, control wiring, HMI, or ladder logic.

---

[User guide contents](README.en.md) · [← 1. Starting the app and reading the screen](01-start.en.md) · [3. Finding, placing, and labeling components →](03-components.en.md)
