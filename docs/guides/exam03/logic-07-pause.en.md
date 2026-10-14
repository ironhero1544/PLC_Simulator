[Contents](GUIDE.en.md) · [← Discharge/finish](logic-06-finish.en.md) · [Trial run →](06-checks.en.md)

<a id="7-일시정지복귀"></a>

# 7. Pause and recovery

**Operation flow**

Pause → 5s → initialize → verify all returns → ready to restart

> **Goal**
> Latch the pause request and start initialization after 5s.
> Completion requires all cylinder return sensors and servo home.

```mermaid
flowchart LR
  A[M113 Pause] --> B[SET M204 · cut run air and SON]
  B --> C[T3 K50 · 5s]
  C --> D[SET M205 · clear automatic states]
  D --> E[Cylinders return · servo home]
  E --> F[All return sensors · position 0 · INP · BUSY OFF · T13]
  F --> G[Stage 0 · RST M205]
```

![Actual pause and 5-second timer ladder](../../images/exam03-guide/26-ladder-pause.png)

<a id="이렇게-그리세요"></a>

### How to draw it

1. Draw M113→SET M204 and M204→OUT T3 K50 first.
2. With T3 completion, add SET M205 and RST rows for each run state.
3. Omit normally closed M204 from vacuum Y14 to retain the workpiece during pickup pauses.

![Actual initialization completion and END](../../images/exam03-guide/37-ladder-reset-complete.png)

The last two rows place M205, X1/X3/X5/X11/X13/X21, D8340=0, X17,
normally closed M8340, and T13 in series. Outputs are MOV K0 D10 and RST M205.
If any sensor is OFF, initialization waits rather than completes.

<a id="완료-확인"></a>

### Checks

 Check initialization after pause and restarting afterward.
`CONVERTED` in the photo means compilation succeeded, not that the complete equipment trial passed.

![Complete circuit compilation](../../images/exam03-guide/35-ladder-compiled.png)

---

[Contents](GUIDE.en.md) · [← Discharge/finish](logic-06-finish.en.md) · [Trial run →](06-checks.en.md)
