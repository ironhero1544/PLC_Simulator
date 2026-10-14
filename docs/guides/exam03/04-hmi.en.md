[Contents](GUIDE.en.md) · [← Control wiring](03-control.en.md) · [Ladder entry →](05-ladder-basics.en.md)

<a id="4-hmi-화면-만들기"></a>

# 4. Creating HMI screens

With the **Select**, double-click HMI → **Edit screen**.
You can also use the edit button in **Operate HMI**.

![HMI double-click menu](../../images/exam03-guide/08-hmi-menu.png)

![Empty HMI screen editor](../../images/exam03-guide/09-hmi-editor.png)

Editing pauses simulation; closing the editor restores the preceding run state.
Keep the screen at 288×216, with a 4:3 ratio.
Name the first screen `Test operation` and the second `Equipment trial run`.
Enter each widget's type, address, position, and size from the tables below.
These coordinates/sizes are inside the HMI screen, not the program canvas.
Use momentary buttons. In particular, JOG M104/M105 must be ON only while pressed.
For navigation, enable **Screen switch button** and choose **Target screen**.
The screen selector replaces the PLC address field in this mode.

<a id="버튼-한-개를-만드는-순서"></a>

### Creating one button

1. Select **+ Button**. A button appears in the preview and widget list.
2. Select it to show label, PLC address, position, and size on the right.

![New button properties](../../images/exam03-guide/10-hmi-button.png)

3. Set **Label** to `Rack forward` and **PLC address** to `M100`.
   Use Ctrl+A to select and replace existing field text.
4. Enable **Momentary buttons** (ON only while pressed).
5. Enter position/size from the final settings table below. The photo shows
   naming at the default position; use the table for final coordinates.

![Label and momentary settings](../../images/exam03-guide/11-hmi-button-name.png)

<a id="두-번째-화면과-전환-버튼"></a>

### Second screen and navigation button

1. Select **Add screen** to create `Screen 2`.
2. Rename it `Equipment trial run` in **Screen name**.

![Adding the second screen](../../images/exam03-guide/12-hmi-page.png)

3. Add a button there and enable **Screen switch button**.

![Navigation button properties](../../images/exam03-guide/13-hmi-transition.png)

4. Choose `Test operation` as **Target screen**. Set the label
   to `<- Test operation`, and use the table's position/size.
   On the first screen, target `Equipment trial run` instead.

![Navigation target set to Test operation](../../images/exam03-guide/14-hmi-transition-target.png)

5. After all settings, select **Apply screen** at the bottom left.
   Cancel discards the edits.
6. Add numbers, lamps, and status messages with their add buttons; set
   addresses and visibility conditions from the table. Reopen the editor using the same menu.

<a id="화면-1-테스트-동작"></a>

### Screen 1: Test operation

<details>
<summary>Expand detailed settings table · 13 items</summary>

| Content | Type | Address | X,Y | Width×height | Additional settings |
| --- | --- | --- | --- | --- | --- |
| Test operation | text | Empty | 8,4 | 200×20 |  |
| Rack forward | button | M100 | 8,28 | 85×28 |  |
| Rack reverse | button | M101 | 98,28 | 85×28 |  |
| Processing motor 5s | button | M103 | 188,28 | 92×28 |  |
| JOG up | button | M104 | 8,64 | 85×28 |  |
| JOG down | button | M105 | 98,64 | 85×28 |  |
| Homing | button | M102 | 188,64 | 92×28 |  |
| Low speed | button | M106 | 8,100 | 85×28 |  |
| Medium speed | button | M107 | 98,100 | 85×28 |  |
| High speed | button | M108 | 188,100 | 92×28 |  |
| JOG Speed | number | D100 | 8,138 | 175×42 | Unit mm/min |
| Home | lamp | M209 | 188,138 | 92×42 |  |
| Equipment trial run -> | button | Empty | 8,185 | 272×24 | Navigate → Equipment trial run |

</details>

<a id="화면-2-장비-시운전"></a>

### Screen 2: Equipment trial run

| Content | Type | Address | X,Y | Width×height | Additional settings |
| --- | --- | --- | --- | --- | --- |
| Equipment trial run | text | Empty | 8,4 | 200×20 |  |
| Single cycle | button | M110 | 8,28 | 60×30 |  |
| Continuous | button | M111 | 78,28 | 60×30 |  |
| Finish | button | M112 | 148,28 | 60×30 |  |
| Pause | button | M113 | 218,28 | 62×30 |  |
| Operation state | message | D103 | 8,70 | 272×35 | 0=Stopped / 1=Continuous operation / 2=Pause |
| Metal stored | number | D101 | 8,115 | 125×44 | Visibility condition M210, Unit: pieces |
| Continuous | lamp | M200 | 148,115 | 132×24 |  |
| Servo motion | lamp | M211 | 148,144 | 132×24 |  |
| <- Test operation | button | Empty | 8,185 | 272×24 | Navigate → Test operation |

Always set `M210` as the metal-count display's visibility condition.
M210 is OFF in single-cycle mode, hiding the count; continuous mode shows it.
PLC logic generates blinking for home lamp M209 and servo-motion lamp M211.
Enter the three status-message strings directly in the HMI widget.

---

[Contents](GUIDE.en.md) · [← Control wiring](03-control.en.md) · [Ladder entry →](05-ladder-basics.en.md)
