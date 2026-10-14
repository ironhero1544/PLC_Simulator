[Contents](GUIDE.en.md) · [← Manual operation](logic-02-manual.en.md) · [Material branching →](logic-04-material.en.md)

<a id="3-공급가공"></a>

# 3. Supply and processing

**Operation flow**

Supply → start motor → confirm down → 3s → confirm up → transfer

> **Goal**
> Start the motor after supply extension/retraction. Confirm cylinder-down
> sensor X4, wait 3s, then raise. Transfer only after up-complete X5.

```mermaid
flowchart LR
  A[Supply extends · check X0] --> B[Supply retracts · check X1]
  B --> C[Start motor first · stage 33]
  C --> D[Processing down · check X4]
  D --> E[T0 K30 · wait 3s]
  E --> F[Processing up · check X5]
  F --> G[Transfer extends X2 · retracts X3]
  G --> H[Conveyor transport]
```

![Actual supply, processing, and transfer ladder](../../images/exam03-guide/27-ladder-feed-process.png)

<a id="이렇게-그리세요"></a>

### How to draw it

1. Put stage comparison `= D11 Kstage` on the left, then completion sensors and normally closed M204/M205 in series.
2. Set the next stage with `MOV Knext_stage D10` on the right.
3. For example, D11=3 and X4 ON writes 4 to D10.
4. D11=4 with pause OFF runs T0 K30; D11=4 with T0 complete writes 5 to D10.

<a id="단계가-바뀌면-무엇이-움직이나요"></a>

### What moves when the stage changes?

 MOV K5 D10 does not itself raise the cylinder.
Later output circuits activate Y5/Y6 for that stage.
M8000→MOV D10 D11 runs once at scan start to prevent several stage transitions
in the same scan.

<a id="완료-확인"></a>

### Checks

 Starting the timer before X4 shortens actual processing time.
Do not stop the motor or transfer before X5 turns ON.

---

[Contents](GUIDE.en.md) · [← Manual operation](logic-02-manual.en.md) · [Material branching →](logic-04-material.en.md)
