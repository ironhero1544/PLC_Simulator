[User guide contents](README.en.md) · [← 12. Creating and using RTL components](12-rtl.en.md)

<a id="13-설정단축키문제-해결"></a>

# 13. Settings, shortcuts, and troubleshooting

If the screen or text is too small, adjust canvas zoom and UI settings separately.
Canvas Zoom changes component magnification; UI scale changes menus and windows.

<a id="언어와-ui-크기"></a>

## Language and UI size

1. Select Korean, English, or Japanese in **Settings → Language**.
2. Open **Settings → UI**.
3. Adjust overall size with **UI scale**.
4. Adjust text with **Font scale**.
5. Adjust spacing/window layout with **Layout size**.
6. Save and restart when prompted.
7. Use **Defaults** to reset sizes.

![UI, font, layout, and frame settings](../../images/program-guide/05-ui-settings.png)

*UI, font, layout, and frame settings*


UI/font/layout scales currently range from 0.75 to 1.5 and require a restart.
**VSync** and **Frame limit (monitor)** are rendering settings.
The frame limit is not a servo-speed or PLC-timer setting.

<a id="공통-단축키"></a>

## Common shortcuts

| Key | Action |
| --- | --- |
| Tab | Switch Wiring / Programming |
| Ctrl+S | Integrated project save dialog |
| Ctrl+, / Ctrl+/ | Help and user guide |
| Ctrl+Z | Undo |
| Ctrl+Y / Ctrl+Shift+Z | Redo |

While editing text fields or popups, keys may enter text or shortcuts may be limited.
If a shortcut does not work, confirm/close the dialog and select the canvas or ladder again.

<a id="실배선-단축키"></a>

## Wiring shortcuts

| Key / action | Result |
| --- | --- |
| Q | Cycle Select → Wire → Wire embedding |
| R / Shift+R | Rotate 90 degrees / opposite direction |
| Delete | Delete selection |
| Ctrl-click | Add/remove component selection |
| Drag empty space | Rectangle multi-selection |
| Right-click while connecting | Cancel connection |
| Alt-right-click | Remove a wire tag |

<a id="프로그래밍-단축키"></a>

## Programming shortcuts

| Key | Action |
| --- | --- |
| F2 / F3 | Monitor / Edit |
| F4 | Compile (Edit mode) |
| F5 / F6 | Normally open / normally closed contact |
| F7 | Output coil |
| F8 | Application instruction |
| Shift+F7 / Shift+F8 | Rising / falling pulse contact |
| Enter | Edit instruction / confirm input |
| Insert | Add circuit |
| Delete | Delete selected area |
| Ctrl+C / Ctrl+V | Copy / paste selected rows/cells |
| Arrow keys | Move selected cell |
| Shift+Up/Down | Extend row selection |
| Backtick key | Circuit note popup |

Add horizontal lines with **F9**, vertical connections with **Shift+F9**.
See [branch editing](06-programming.en.md) for connections.

<a id="증상별-확인-순서"></a>

## Troubleshooting by symptom

| Symptom | Check order | Details |
| --- | --- | --- |
| Cannot start / compilation warning | Edit → fix errors → compile → start | [Ladder](06-programming.en.md) |
| Missing component | Reset both certification/function filters to All | [Components](03-components.en.md) |
| Pin hard to select | Zoom → pin name → wire end position | [Wiring](04-wiring.en.md) |
| Y ON but no movement | Power/0V → output mode → valve/driver → physical conditions | [Mechanics](11-mechanics.en.md) |
| Sensor does not detect | Power → target → position/direction → Z → SIG/X | [Sensors](11-mechanics.en.md) |
| Component disappears | L visibility/opacity/order → canvas position | [L](10-render-layers.en.md) |
| Overlap without storage/collision | Contact Z, working position, mechanical movement | [Z](09-z-layers.en.md) |
| HMI button does not work | 24V/0V → one PLC → RUN → pause → M address | [HMI](08-hmi.en.md) |
| Cannot edit HMI number | Numeric displays are read-only; use M buttons and ladder logic | [HMI](08-hmi.en.md) |
| Ladder/wiring missing after save | Check JSON/CSV-only save → save integrated project | [Files](02-projects.en.md) |
| Automatic sequence stops midway | Request → stage M → instruction → actuator → completion input | [Monitor](07-monitoring.en.md) |
| RTL analysis/build failure | Saved distribution → failed Doctor item → build log | [RTL](12-rtl.en.md) |

When reporting issues, include the version, project filename, RUN/STOP, last working action,
stopped component, X/Y/M states, error message, and screenshot.

<a id="다음-실습"></a>

## Next exercise

After learning the basics, follow [industrial engineer exam 3](../exam03/GUIDE.en.md)
in this order: **placement → power wiring → control wiring → HMI → ladder logic**.


<a id="프로그램-안에서-가이드-읽기"></a>

## Reading guides inside the app

Open Help with **Ctrl+,**, then select **Open user guide**. Help provides quick basic controls and shortcuts; the guide is a separate window with detailed photos and GIFs.

- Keep editing Wiring/ladder logic and running the simulation with the guide open. It does not pause automatically.
- Drag window edges to resize, and move it beside the app.
- Select a chapter in the left contents or enter a search term.
- Previous/next return through documents you have read. A−/A+ change body text size.
- Click images/GIFs to enlarge them. Documents and exercise images work offline.
- Closing the guide leaves the app running. Reopen it from Help.

If the guide does not open, check `resources/help` in the installation and Microsoft Edge WebView2 Runtime.
---

[User guide contents](README.en.md) · [← 12. Creating and using RTL components](12-rtl.en.md)
