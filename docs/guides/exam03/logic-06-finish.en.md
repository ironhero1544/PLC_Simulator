[Contents](GUIDE.en.md) · [← Pickup/storage](logic-05-storage.en.md) · [Pause/recovery →](logic-07-pause.en.md)

<a id="6-배출종료"></a>

# 6. Discharge and finish

**Operation flow**

Finish workpiece → check count/finish conditions → stop or next workpiece

> **Goal**
> For nonmetal, wait for the box-passage sensor to turn ON, then OFF.
> Finish queues a stop after completing the current workpiece.

```mermaid
flowchart TD
  A[Nonmetal stage 30] --> B[X22 ON · entering box]
  B --> C[Wait X22 OFF at stage 32]
  C --> D[Stage 31 · discharge complete]
  E[Metal stored · stage 20] --> F[Increment D101 in continuous mode]
  F --> G{Single cycle OR finish queued OR 2 metals OR no supply?}
  D --> G
  G -->|Yes| H[Clear M201 · M200]
  G -->|No| I[Stage 0 · next workpiece]
```

![Actual box passage and finish branches](../../images/exam03-guide/30-ladder-box-finish.png)

<a id="이렇게-그리세요"></a>

### How to draw it

1. D11=30 followed by normally open X22 enters stage 32.
2. D11=32 followed by normally closed X22 enters stage 31.
3. Finish conditions are parallel branches.
4. Split paths across lower rows as shown and join at the common RST M201 on the right.
5. Putting all finish conditions in series incorrectly requires them all ON to stop.

<a id="완료-확인"></a>

### Checks

 Finish M112 sets M203. Do not directly clear M201 from the button
to interrupt immediately. In continuous mode, increment D101 once per completed metal storage.

---

[Contents](GUIDE.en.md) · [← Pickup/storage](logic-05-storage.en.md) · [Pause/recovery →](logic-07-pause.en.md)
