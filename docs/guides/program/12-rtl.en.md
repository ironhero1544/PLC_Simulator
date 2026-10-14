[User guide contents](README.en.md) · [← 11. Sensors, pneumatics, motors, and workpieces](11-mechanics.en.md) · [13. Settings, shortcuts, and troubleshooting →](13-settings-shortcuts.en.md)

<a id="12-rtl-부품-만들기와-사용"></a>

# 12. Creating and using RTL components

RTL activates Verilog circuits as Wiring components.
Basic ladder exercises do not require it; you can learn it afterward.

For placement and wiring, continue with [Full Adder wiring practice](rtl-full-adder.en.md).
It covers source code, power/I/O wiring, button operation, and the truth table.

<a id="먼저-빌드-환경-준비하기"></a>

## Prepare the build environment first

Diagnose WSL2 in **RTL → Toolchain**.
See [Tool Doctor](rtl-tool-doctor.en.md) for initial installation and error-specific fixes.
When all diagnostic items show ‘Found’, proceed to component creation below.

Building or testing new components requires WSL2.
Already built components work without WSL installed.

<a id="1단계-rtl-모듈-준비"></a>

## Step 1: preparing an RTL module

![New RTL Module in the RTL library](../../images/program-guide/rtl-01-library.jpg)

*Open RTL → RTL Library and select ‘New RTL Module’ at the top left.*

1. Open **RTL → RTL Library**.
2. Select **New RTL Module** and give it a recognizable name.
3. For a new source file, **right-click the source list → Add file**.
   Import an existing `.v` with **Import** above the editor.
4. Select the file and **Set as TOP**. Match the TOP module name to the source's module name.
5. If you have a testbench file, add it and **Set as TB**.
   Enter that source's module name in **Testbench top module** above.
6. Edit the source and run **Analyze**.
7. If there are no errors, run **Build**.

![TOP source and editor for a new module](../../images/program-guide/rtl-02-source-top.jpg)

*Select files on the left and edit source on the right. [TOP] marks the file used for the component.*

![Add file in the source list's context menu](../../images/program-guide/rtl-03-add-file.jpg)

*‘Add file’ is in the source list's right-click menu. Create an empty file, then enter the code.*

![TOP/TB files and testbench module name](../../images/program-guide/rtl-05-testbench-top.jpg)

*Match the TB source's `module guide_tb` to `guide_tb` in ‘Testbench top module’.*

![Successful analysis and input/output port list](../../images/program-guide/rtl-04-analysis.jpg)

*After analysis, ‘Analysis status: Success’ and the input/output ports appear below.*

Analysis checks source and ports; building creates the runnable component.
Successful analysis alone does not create a new executable.

![Successful build and component activation setting](../../images/program-guide/rtl-06-build.jpg)

*‘Build status: Success’ means an executable has been created.
‘Enable As Component’ below registers it in the Wiring list.*

<a id="2단계-테스트와-실배선-사용"></a>

## Step 2: testing and Wiring use

1. If TB is assigned, select **Run testbench**.
2. Check PASS/FAIL/TIMEOUT and the testbench log.
3. If a waveform is generated, inspect signal changes with **Open waveform**.
4. Enable **Enable As Component** on the built module.
5. Place the RTL component from the Wiring component list.
6. Connect input/output ports and test operation.
7. Save the entire project as `.plcproj`.

![Passing testbench result](../../images/program-guide/rtl-07-testbench-pass.jpg)

*PASS means the test passed. Scroll inside the lower panel if the result is clipped.*

Reanalyze and rebuild after source changes. NOT RUN does not mean the test passed.
Check that GUI port names match the actual TOP inputs/outputs.

<a id="부품-공유와-다시-편집하기"></a>

## Sharing and editing components again

| Purpose | Save method |
| --- | --- |
| Reuse in another project | Export component → `.plccomp` |
| Allow source editing/rebuilding | Include source when exporting |
| Distribute for execution only | Runtime-only package |
| Keep wiring, ladder, and RTL together | Integrated `.plcproj` save |

Open `.plccomp` with the library's **Import component**.
Do not import a build-folder worker EXE again as a new RTL module.
Runtime packages without source can run but cannot be source-edited or rebuilt.

Packages containing built Windows component EXEs do not use WSL at runtime.
Prepare WSL2 to build new components or run testbenches.

For environment issues, see [Tool Doctor troubleshooting](rtl-tool-doctor.en.md).

---

[User guide contents](README.en.md) · [← 11. Sensors, pneumatics, motors, and workpieces](11-mechanics.en.md) · [13. Settings, shortcuts, and troubleshooting →](13-settings-shortcuts.en.md)
