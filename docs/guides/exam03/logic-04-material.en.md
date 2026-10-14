[Contents](GUIDE.en.md) · [← Supply/processing](logic-03-process.en.md) · [Pickup/storage →](logic-05-storage.en.md)

<a id="4-재질-분기"></a>

# 4. Material branching

**Operation flow**

Confirm arrival → metal to rack / nonmetal to box

> **Goal**
> Confirm arrival X14 at the material sensor and enter stage 9.
> In that stage, X15 ON selects storage; OFF selects box discharge.

```mermaid
flowchart TD
  A[Transport stage 8] --> B[Confirm workpiece arrival X14]
  B --> C[Material stage 9]
  C --> D{Metal detected X15?}
  D -->|ON| E[Stage 10 · stopper and pickup]
  D -->|OFF| F[Stage 30 · box discharge]
```

![Actual material branching and position confirmation ladder](../../images/exam03-guide/28-ladder-material-pick.png)

<a id="이렇게-그리세요"></a>

### How to draw it

1. After D11=9, put X15 normally open and normally closed contacts on separate rows.
2. End the normally open row with MOV K10 D10 and normally closed row with MOV K30 D10.
3. Evaluate material only at the arrival stage; otherwise X15 OFF with no workpiece could be mistaken for nonmetal.

<a id="완료-확인"></a>

### Checks

 Metal must select stage 10; nonmetal stage 30.
Metal positioning requires both stopper-down X6 and optical detection X16.

---

[Contents](GUIDE.en.md) · [← Supply/processing](logic-03-process.en.md) · [Pickup/storage →](logic-05-storage.en.md)
