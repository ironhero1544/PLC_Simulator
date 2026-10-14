[User guide contents](README.en.md) · [← 9. Z layers: physical height and travel range](09-z-layers.en.md) · [11. Sensors, pneumatics, motors, and workpieces →](11-mechanics.en.md)

<a id="10-l-레이어-표시-순서그룹투명도"></a>

# 10. L layers: order, groups, and opacity

**L controls drawing order.** Use it when moving parts are hidden behind other components.
Visibility, opacity, and grouping do not change physical behavior or electrical connections.

<a id="창-열기와-선택"></a>

## Opening the window and selecting

1. Select **L to the right of Z** beside the component list.
2. Move or resize the window so the list is easy to read.
3. Click a component row.
4. Ctrl-click adds/removes selection; Shift-click selects a range.
5. Drag vertically through the selection boxes on the left to select multiple rows.
6. Components selected by a rectangle on the canvas can also be edited together here.

![Order, opacity, and groups in the L render layer window](../../images/program-guide/04-render-layer.png)

*Order, opacity, and groups in the L render layer window*

Components higher in the list are drawn in front. Group headings name the group;
check individual component rows for the actual drawing order.

<a id="앞뒤-순서-바꾸기"></a>

## Changing drawing order

1. Select the component that should appear in front.
2. Move one step with **Up / Down**, or use **Bring to front / Send to back**.
3. You can also drag a row before another component row.
4. Check that the moving parts and workpieces are visible on the canvas.

For example, send optical sensors installed below the conveyor behind it,
and bring cylinder rods and workpieces forward as needed to observe movement.
Moving multiple selected parts preserves their relative order.

<a id="그룹-만들기와-한-번에-조절"></a>

## Grouping and adjusting together

1. Select the cylinder, solenoid, and related sensors together.
2. Enter `Supply section` as the group name.
3. Select **Group selection / Rename**.
4. Click a group heading to select all its components.
5. Apply order, opacity, or visibility to the whole group.
6. Drag selected rows onto another group heading to add them to that group.
7. **Ungroup** removes only the selected components from their group.

Using an existing group name merges into that group. Groups are for display editing.
They do not create mechanically attached axes or components that move together.

<a id="투명도와-숨김-복구"></a>

## Opacity and restoring hidden parts

**100% opacity** is normal; **0%** is fully transparent.
Hidden or 0%-opacity components cannot be clicked on the canvas.

1. Find the missing component's row in the L window.
2. Select it and press **Show**.
3. Raise opacity to 100%.
4. If it is behind another part, adjust drawing order too.

![Hide and show a PLC in the L window](../../images/program-guide/render-visibility.gif)

*Hide and show a PLC in the L window*

Hidden components retain connections and physical behavior; **their wires remain visible**.
Hiding a component does not hide its wires.
Drawing-order/display changes can be undone and saved in the project.

---

[User guide contents](README.en.md) · [← 9. Z layers: physical height and travel range](09-z-layers.en.md) · [11. Sensors, pneumatics, motors, and workpieces →](11-mechanics.en.md)
