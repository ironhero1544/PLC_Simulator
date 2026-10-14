[Contents](GUIDE.en.md) · [← Ladder entry](05-ladder-basics.en.md) · [Manual operation →](logic-02-manual.en.md)

<a id="1-초기화"></a>

# 1. Initialization

**Operation flow**

First scan → initialize numbers/states → return mechanisms → ready

> **Goal**
> On the first RUN scan, set speed 500, stage 0, and stored count 0,
> clear previous run states, and set initialization M205.

```mermaid
flowchart LR
  A[First RUN scan M8002] --> B[Speed 500 · stage 0 · count 0]
  B --> C[Clear continuous M200 and run M201]
  C --> D[Start initialization M205]
  D --> E[Check cylinder returns and servo home]
  E --> F[Clear initialization · ready]
```

![Actual initialization/default-speed ladder](../../images/exam03-guide/24-ladder-init-speed.png)

<a id="이렇게-그리세요"></a>

### How to draw it

1. Put an M8002 normally open contact on the left and one right-side instruction per row.
2. Use `MOV K500 D100`, `MOV K0 D10`, `MOV K0 D101`, `RST M200`, `RST M201`, `SET M205` in order.
3. M8002 repeats because several values are initialized by the same first-scan condition.

<a id="완료-확인"></a>

### Checks

 D10=0 and D101=0 do not prove that every axis is home.
Clear M205 only after checking return sensors and servo position.

---

[Contents](GUIDE.en.md) · [← Ladder entry](05-ladder-basics.en.md) · [Manual operation →](logic-02-manual.en.md)
