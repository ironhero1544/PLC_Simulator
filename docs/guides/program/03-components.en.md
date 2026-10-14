[User guide contents](README.en.md) · [← 2. Saving and opening projects](02-projects.en.md) · [4. Electrical, pneumatic, and optical wiring →](04-wiring.en.md)

<a id="3-부품-찾기배치이름-표시"></a>

# 3. Finding, placing, and labeling components

Find components in the list on the right. Filters are divided into **Certification** and **Function**.
After placement, adjust positions with the Select tool.

<a id="필요한-부품만-찾기"></a>

## Finding the components you need

1. Open the certification filter at the top of the list.
2. Choose **All** for general practice, or the relevant exam, such as **Industrial Engineer in Automation Equipment**.
3. Open the function filter below it.
4. Narrow the list to power, control, pneumatic, sensors, or another required category.
5. If a component is missing, reset both filters to **All / All functions**.

![Opening the certification filter](../../images/automation-exam-03/04-certificate-filter-menu.png)

*Opening the certification filter*

<a id="부품-배치와-이동"></a>

## Placing and moving components

1. Drag a component card from the list onto the canvas.
2. Select the **Select**. With Wire selected, clicking a pin may start a connection.
3. Click a component body to select it, then drag it to a new position.
4. Drag a rectangle in empty space to select multiple components.
5. Ctrl-click a component to add it to or remove it from the selection.
6. Move the selected components. If they are wired, check the connection routes afterward.

Crowding the PLC, power supply, and actuators together makes pin selection and observation difficult.
Place the power supply and PLC above the actuators, leaving space for hoses and wires.

<a id="확대축소와-화면-이동"></a>

## Zooming and panning

| Action | Result |
| --- | --- |
| Mouse wheel / trackpad pinch | Zoom in/out |
| Middle-button drag | Pan |
| Alt + right-button drag | Pan |
| Shift + wheel | Horizontal pan |
| Two-finger trackpad scroll | Pan |

Zoom in until pin names are readable. Two-finger trackpad scrolling may
pan rather than zoom, so check the Zoom value at the bottom of the screen.

<a id="회전반전삭제"></a>

## Rotating, flipping, and deleting

1. Select a component with the Select tool.
2. `R` rotates 90 degrees; `Shift+R` rotates in the opposite direction.
3. The body's double-click menu also offers rotate, horizontal flip, and vertical flip.
4. To delete, select the component and press `Delete`.
5. Undo with `Ctrl+Z`; redo with `Ctrl+Y` or `Ctrl+Shift+Z`.

![Select the PLC and rotate it with R or Shift+R](../../images/program-guide/component-rotate.gif)

*Select the PLC and rotate it with R or Shift+R*

Rotation changes the rod direction and pin positions, so recheck wiring and mechanical movement.

<a id="캔버스에-글씨-넣기"></a>

## Adding text to the canvas

Use **Show component name** for explanatory labels.

1. In the Select tool, double-click the component body.
2. Open **Edit component name**.
3. Enter a name such as `Supply cylinder` or `Process detection X2`.
4. Press Enter to apply.
5. Enable **Show name** in the same menu.
6. Clear the name or disable the display checkbox to hide the label.

![Show name in Edit component name, using an industrial engineer example](../../images/automation-exam-03/part-name-display-menu.jpg)

*Show name in Edit component name, using an industrial engineer example*

Text inside an HMI screen is created separately with **Edit screen → + Text**.
Component names and HMI widget text are separate settings.

---

[User guide contents](README.en.md) · [← 2. Saving and opening projects](02-projects.en.md) · [4. Electrical, pneumatic, and optical wiring →](04-wiring.en.md)
