<a id="rtl-tool-doctor-wsl2-환경-준비와-오류-해결"></a>

# RTL Tool Doctor: WSL2 preparation and troubleshooting

[User guide contents](README.en.md) · [Creating RTL components](12-rtl.en.md)

Tool Doctor checks and manages the **environment for RTL analysis, builds, and testbenches**.
Use it to create Verilog components or rebuild modified source.
Running already built Windows RTL component EXEs does not require WSL.

<a id="창을-열고-결과-읽기"></a>

## Opening the window and reading results

Select **RTL → Toolchain** to open **RTL Tool Doctor (WSL2)**.
Opening it automatically checks saved settings. Buttons are disabled during diagnosis.

![Tool Doctor diagnostic results](../../images/program-guide/06-rtl-doctor.png)

| Display | Meaning |
| --- | --- |
| Distribution | WSL environment for analysis/builds; choose an installed name |
| Default distribution | Uses the Windows WSL default; not necessarily Ubuntu |
| Build CPU usage | **Concurrent build jobs**, not CPU %. Default 2, range 1–64 |
| Save and diagnose | Save selected distribution/CPU values, then check them |
| Diagnose saved environment | Check last saved settings; unsaved choices are ignored |
| Found / Missing | Pass/fail; expand an item for command output and versions |

After WSL/distribution checks pass, seven items cover the kernel and build tools.
**If all show ‘Found’, no installation is needed.** Close the window and
[analyze/build in the RTL library](12-rtl.en.md).

<a id="wsl-환경이-없는-pc에서-처음-준비하기"></a>

## Initial setup on a PC without WSL

1. Select **Install WSL2 / Ubuntu**. This requests Ubuntu installation.
   Follow Windows administrator approval or restart prompts.
2. When installation finishes, select **Diagnose saved environment** to refresh the distribution list.
   After restarting, reopen the app and Tool Doctor.
3. Choose the installed **Ubuntu [WSL2]** in **Distribution**.
   You may select an existing Ubuntu/Debian WSL2 environment instead.
4. Set build jobs and select **Save and diagnose**.
5. If tools are missing, select **Install / Repair RTL tools**.
   This installs Verilator/g++/MinGW-w64/make inside the selected distribution.
6. Read the automatic diagnosis afterward. Use **Diagnose saved environment** again if needed.

An installation-complete message alone does not prove readiness. Check each diagnostic item.
If no distribution appears or initial setup fails after installing Ubuntu, open Ubuntu directly,
complete its initial setup, and diagnose again.

Tool installation uses **Ubuntu/Debian apt**. Selecting another distribution
does not adapt installation to its package manager.
Downloads/installations take time; wait for completion logs.

<a id="없음이-나왔을-때"></a>

## When an item is ‘Missing’

Expand the failed item and read details first, then choose the matching action below.

| Failed item / situation | Action |
| --- | --- |
| WSL host | WSL list query failed. Read the error; if not installed, use **Install WSL2 / Ubuntu** |
| WSL2 distribution: name missing | Select an installed name and **Save and diagnose**; install Ubuntu if none exists |
| Distribution shows WSL1 | Select it, **Convert selected distribution to WSL2**, then diagnose |
| Linux kernel | Read the distribution startup error; check whether WSL update/restart is required |
| Verilator | Requires **5.x or newer**. Older executables show Missing; inspect version if repair still fails |
| C++ compiler / make | Requires g++/make in the selected Linux environment; use **Install / Repair RTL tools** on Ubuntu/Debian |
| MinGW Windows compiler | Requires a **Windows cross-compiler inside WSL**; Windows-installed MinGW does not satisfy this check |
| Installation/repair fails | Read apt/download/distribution errors in the bottom log, fix the cause, then retry |

Checks stop at a failed WSL host or WSL2 distribution.
It is normal for tool items not to appear yet. Fix the earlier failure and diagnose again.

‘Missing’ also includes command failures and unsupported versions, not only absent files.
Diagnosis checks tools in the selected distribution's `/usr/bin`; manually installed tools elsewhere
are not automatically selected.

<a id="관리-버튼을-쓰는-경우"></a>

## Using management buttons

**Explicitly select and save the distribution name** before management to identify the target clearly.

| Button | Action / use |
| --- | --- |
| Install WSL2 / Ubuntu | Requests WSL/Ubuntu installation; does not install a distribution matching the selected name |
| Install / Repair RTL tools | Install build tools in selected WSL2 Ubuntu/Debian |
| Start selected distribution | Start the selected distribution |
| Stop selected distribution | Stop it, including other programs running inside it |
| Convert selected distribution to WSL2 | Convert an installed WSL1 distribution to WSL2 |
| Update WSL | Update Windows WSL; separate from RTL tool installation |

**Diagnosis may start the distribution.** Diagnosing after Stop can start it again.
After successful Stop, the old diagnostic list is cleared without automatic rediagnosis.

The bottom log shows **management results**, such as install/start/stop;
expanded diagnostic items show **that check command's result**.
Closing the window during management does not cancel the operation.
Doctor settings/management buttons may be disabled during diagnosis, management, or RTL analysis/build.

<a id="환경이-준비된-다음"></a>

## After the environment is ready

Close Doctor and analyze/build source in **RTL → RTL Library**.
WSL Linux tools run analysis/testbenches; WSL MinGW-w64 builds
**native Windows component EXEs** for the Wiring simulation.

Continue with [Creating and using RTL components](12-rtl.en.md).

[User guide contents](README.en.md)
