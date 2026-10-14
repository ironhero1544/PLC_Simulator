[Contents](GUIDE.en.md) · [← Entry procedure](05-ladder-basics.en.md)

<a id="전체-회로와-명령어-참고"></a>

# Complete circuit and instruction reference

Check contact conditions and instructions in the circuit tables.
The listings below express the same circuits as instructions.

<a id="전체-회로-입력표"></a>

### Complete circuit entry table

Draw complex conditions with the exact series/parallel grouping in parentheses. Table circuit numbers are explanatory, not actual PLC instruction step numbers.

<details>
<summary>Expand complete circuit table · 107 items</summary>

| Circuit | Contact conditions (series/parallel) | Final instruction |
| --- | --- | --- |
| 1 | M8002 | `MOV K500 D100` |
| 2 | M8002 | `MOV K0 D10` |
| 3 | M8002 | `MOV K0 D101` |
| 4 | M8002 | `RST M200` |
| 5 | M8002 | `RST M201` |
| 6 | M8002 | `SET M205` |
| 7 | M106 | `MOV K500 D100` |
| 8 | M107 | `MOV K1000 D100` |
| 9 | M108 | `MOV K1500 D100` |
| 10 | M8000 | `DMOV D100 D200` |
| 11 | M8000 | `MOV K0 D201` |
| 12 | M8000 | `DMOV D200 D202` |
| 13 | M8000 | `DNEG D202` |
| 14 | (((↑M111 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `SET M200` |
| 15 | (((↑M111 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `SET M210` |
| 16 | (((↑M111 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `MOV K0 D101` |
| 17 | (((↑M111 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `SET M201` |
| 18 | (((↑M110 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `RST M200` |
| 19 | (((↑M110 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `RST M210` |
| 20 | (((↑M110 ∧ ¬(M201)) ∧ ¬(M205)) ∧ ¬(M206)) | `SET M201` |
| 21 | M112 | `SET M203` |
| 22 | M113 | `SET M204` |
| 23 | M204 | `OUT T3 K50` |
| 24 | T3 | `SET M205` |
| 25 | T3 | `RST M201` |
| 26 | T3 | `RST M200` |
| 27 | T3 | `RST M204` |
| 28 | T3 | `RST M206` |
| 29 | T3 | `RST M212` |
| 30 | M8000 | `MOV D10 D11` |
| 31 | (((((((D11 = K0 ∧ M201) ∧ X7) ∧ ¬(M204)) ∧ ¬(M205)) ∧ D8340 = K0) ∧ X17) ∧ ¬(M8340)) | `MOV K1 D10` |
| 32 | (((D11 = K1 ∧ X0) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K2 D10` |
| 33 | (((D11 = K2 ∧ X1) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K33 D10` |
| 34 | (D11 = K33 ∧ ¬(M204)) | `MOV K3 D10` |
| 35 | (((D11 = K3 ∧ X4) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K4 D10` |
| 36 | (D11 = K4 ∧ ¬(M204)) | `OUT T0 K30` |
| 37 | (((D11 = K4 ∧ T0) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K5 D10` |
| 38 | (((D11 = K5 ∧ X5) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K6 D10` |
| 39 | (((D11 = K6 ∧ X2) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K7 D10` |
| 40 | (((D11 = K7 ∧ X3) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K8 D10` |
| 41 | (((D11 = K8 ∧ X14) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K9 D10` |
| 42 | (D11 = K9 ∧ X15) | `MOV K10 D10` |
| 43 | (D11 = K9 ∧ ¬(X15)) | `MOV K30 D10` |
| 44 | ((D11 = K10 ∧ X6) ∧ X16) | `MOV K11 D10` |
| 45 | D11 = K11 | `OUT T4 K10` |
| 46 | (((((D11 = K11 ∧ T4) ∧ ¬(M204)) ∧ ¬(M205)) ∧ X17) ∧ ¬(M8340)) | `MOV K12 D10` |
| 47 | (((D11 = K12 ∧ X20) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K13 D10` |
| 48 | ((D11 = K13 ∧ D101 = K0) ∧ X13) | `MOV K14 D10` |
| 49 | ((D11 = K13 ∧ D101 > K0) ∧ X12) | `MOV K14 D10` |
| 50 | D11 = K14 | `OUT T6 K10` |
| 51 | (((D11 = K14 ∧ T6) ∧ X17) ∧ ¬(M8340)) | `MOV K15 D10` |
| 52 | (((D11 = K15 ∧ X10) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K16 D10` |
| 53 | ((D11 = K16 ∧ ¬(X20)) ∧ ¬(M204)) | `MOV K17 D10` |
| 54 | (((D11 = K17 ∧ X11) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K18 D10` |
| 55 | D11 = K18 | `OUT T8 K10` |
| 56 | (((D11 = K18 ∧ T8) ∧ X17) ∧ ¬(M8340)) | `MOV K19 D10` |
| 57 | (((D11 = K19 ∧ X21) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K20 D10` |
| 58 | (D11 = K20 ∧ M200) | `INC D101` |
| 59 | D11 = K20 | `MOV K0 D10` |
| 60 | (((D11 = K30 ∧ X22) ∧ ¬(M204)) ∧ ¬(M205)) | `MOV K32 D10` |
| 61 | ((D11 = K32 ∧ ¬(X22)) ∧ ¬(M204)) | `MOV K31 D10` |
| 62 | D11 = K31 | `MOV K0 D10` |
| 63 | ((((D11 = K20 ∧ ¬(M200)) ∨ (D11 = K20 ∧ M203)) ∨ (D11 = K20 ∧ D101 >= K2)) ∨ (D11 = K20 ∧ ¬(X7))) | `RST M201` |
| 64 | ((((D11 = K20 ∧ ¬(M200)) ∨ (D11 = K20 ∧ M203)) ∨ (D11 = K20 ∧ D101 >= K2)) ∨ (D11 = K20 ∧ ¬(X7))) | `RST M200` |
| 65 | ((((D11 = K31 ∧ ¬(M200)) ∨ (D11 = K31 ∧ M203)) ∨ (D11 = K31 ∧ D101 >= K2)) ∨ (D11 = K31 ∧ ¬(X7))) | `RST M201` |
| 66 | ((((D11 = K31 ∧ ¬(M200)) ∨ (D11 = K31 ∧ M203)) ∨ (D11 = K31 ∧ D101 >= K2)) ∨ (D11 = K31 ∧ ¬(X7))) | `RST M200` |
| 67 | ¬(M201) | `RST M203` |
| 68 | (↑M103 ∧ ¬(M201)) | `SET M212` |
| 69 | (M212 ∧ ¬(M204)) | `OUT T11 K50` |
| 70 | T11 | `RST M212` |
| 71 | (((↑M102 ∧ ¬(M201)) ∧ ¬(M205)) ∧ X11) | `SET M206` |
| 72 | M206 | `OUT T12 K10` |
| 73 | ((((M206 ∧ T12) ∧ D8340 = K0) ∧ X17) ∧ ¬(M8340)) | `SET M208` |
| 74 | ((((M206 ∧ T12) ∧ D8340 = K0) ∧ X17) ∧ ¬(M8340)) | `RST M206` |
| 75 | (M104 ∨ M105) | `RST M208` |
| 76 | ((M206 ∧ M8013) ∨ M208) | `OUT M209` |
| 77 | (((M8340 ∨ ¬(X17)) ∧ M200) ∧ M8013) | `OUT M211` |
| 78 | M8000 | `MOV K0 D103` |
| 79 | M200 | `MOV K1 D103` |
| 80 | M204 | `MOV K2 D103` |
| 81 | ((D11 = K11 ∧ ¬(M204)) ∧ X11) | `DDRVA K-6000 K1500 Y0 Y4` |
| 82 | (((D11 = K14 ∧ D101 = K0) ∧ ¬(M204)) ∧ X11) | `DDRVA K-6000 K1500 Y0 Y4` |
| 83 | (((D11 = K14 ∧ D101 > K0) ∧ ¬(M204)) ∧ X11) | `DDRVA K0 K1500 Y0 Y4` |
| 84 | ((D11 = K18 ∧ ¬(M204)) ∧ X11) | `DDRVA K0 K1500 Y0 Y4` |
| 85 | ((((D11 = K0 ∧ M201) ∧ ¬(M205)) ∧ ¬(M204)) ∧ X11) | `DDRVA K0 K1500 Y0 Y4` |
| 86 | (((M206 ∧ ¬(M205)) ∧ ¬(M204)) ∧ X11) | `DDRVA K0 K1500 Y0 Y4` |
| 87 | ((M205 ∧ ¬(M204)) ∧ X11) | `DDRVA K0 K1500 Y0 Y4` |
| 88 | ((((((M104 ∧ ¬(M201)) ∧ ¬(M206)) ∧ ¬(M205)) ∧ D8340 < K0) ∧ ¬(M204)) ∧ X11) | `DPLSV D200 Y0 Y4` |
| 89 | ((((((M105 ∧ ¬(M201)) ∧ ¬(M206)) ∧ ¬(M205)) ∧ D8340 > K-6000) ∧ ¬(M204)) ∧ X11) | `DPLSV D202 Y0 Y4` |
| 90 | (M8000 ∧ ¬(M204)) | `OUT Y17` |
| 91 | ((D11 = K1 ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y1` |
| 92 | ((D11 = K6 ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y2` |
| 93 | (((((D11 >= K3 ∧ D11 <= K5) ∨ D11 = K33) ∨ M212) ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y3` |
| 94 | (((D11 >= K3 ∧ D11 <= K4) ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y5` |
| 95 | ((D11 = K5 ∨ M205) ∧ ¬(M204)) | `OUT Y6` |
| 96 | (((((D11 >= K8 ∧ D11 <= K10) ∨ D11 = K30) ∨ D11 = K32) ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y7` |
| 97 | (((D11 >= K10 ∧ D11 <= K18) ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y10` |
| 98 | ((D11 = K19 ∨ M205) ∧ ¬(M204)) | `OUT Y11` |
| 99 | (((D11 >= K15 ∧ D11 <= K16) ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y12` |
| 100 | ((D11 = K17 ∨ M205) ∧ ¬(M204)) | `OUT Y13` |
| 101 | ((D11 >= K12 ∧ D11 <= K15) ∧ ¬(M205)) | `OUT Y14` |
| 102 | ((((D11 = K13 ∧ D101 > K0) ∨ (M100 ∧ ¬(M201))) ∧ ¬(M205)) ∧ ¬(M204)) | `OUT Y15` |
| 103 | ((((D11 = K13 ∧ D101 = K0) ∨ (M101 ∧ ¬(M201))) ∨ M205) ∧ ¬(M204)) | `OUT Y16` |
| 104 | M205 | `OUT T13 K20` |
| 105 | ((((((((((M205 ∧ X1) ∧ X3) ∧ X5) ∧ X11) ∧ X13) ∧ X21) ∧ D8340 = K0) ∧ X17) ∧ ¬(M8340)) ∧ T13) | `MOV K0 D10` |
| 106 | ((((((((((M205 ∧ X1) ∧ X3) ∧ X5) ∧ X11) ∧ X13) ∧ X21) ∧ D8340 = K0) ∧ X17) ∧ ¬(M8340)) ∧ T13) | `RST M205` |
| 107 | END | `END` |

</details>

<details>
<summary>Circuit 1: MOV K500 D100</summary>

```text
LD M8002
MOV K500 D100
```

</details>

<details>
<summary>Circuit 2: MOV K0 D10</summary>

```text
LD M8002
MOV K0 D10
```

</details>

<details>
<summary>Circuit 3: MOV K0 D101</summary>

```text
LD M8002
MOV K0 D101
```

</details>

<details>
<summary>Circuit 4: RST M200</summary>

```text
LD M8002
RST M200
```

</details>

<details>
<summary>Circuit 5: RST M201</summary>

```text
LD M8002
RST M201
```

</details>

<details>
<summary>Circuit 6: SET M205</summary>

```text
LD M8002
SET M205
```

</details>

<details>
<summary>Circuit 7: MOV K500 D100</summary>

```text
LD M106
MOV K500 D100
```

</details>

<details>
<summary>Circuit 8: MOV K1000 D100</summary>

```text
LD M107
MOV K1000 D100
```

</details>

<details>
<summary>Circuit 9: MOV K1500 D100</summary>

```text
LD M108
MOV K1500 D100
```

</details>

<details>
<summary>Circuit 10: DMOV D100 D200</summary>

```text
LD M8000
DMOV D100 D200
```

</details>

<details>
<summary>Circuit 11: MOV K0 D201</summary>

```text
LD M8000
MOV K0 D201
```

</details>

<details>
<summary>Circuit 12: DMOV D200 D202</summary>

```text
LD M8000
DMOV D200 D202
```

</details>

<details>
<summary>Circuit 13: DNEG D202</summary>

```text
LD M8000
DNEG D202
```

</details>

<details>
<summary>Circuit 14: SET M200</summary>

```text
LDP M111
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
SET M200
```

</details>

<details>
<summary>Circuit 15: SET M210</summary>

```text
LDP M111
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
SET M210
```

</details>

<details>
<summary>Circuit 16: MOV K0 D101</summary>

```text
LDP M111
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
MOV K0 D101
```

</details>

<details>
<summary>Circuit 17: SET M201</summary>

```text
LDP M111
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
SET M201
```

</details>

<details>
<summary>Circuit 18: RST M200</summary>

```text
LDP M110
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
RST M200
```

</details>

<details>
<summary>Circuit 19: RST M210</summary>

```text
LDP M110
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
RST M210
```

</details>

<details>
<summary>Circuit 20: SET M201</summary>

```text
LDP M110
LD M201
INV
ANB
LD M205
INV
ANB
LD M206
INV
ANB
SET M201
```

</details>

<details>
<summary>Circuit 21: SET M203</summary>

```text
LD M112
SET M203
```

</details>

<details>
<summary>Circuit 22: SET M204</summary>

```text
LD M113
SET M204
```

</details>

<details>
<summary>Circuit 23: OUT T3 K50</summary>

```text
LD M204
OUT T3 K50
```

</details>

<details>
<summary>Circuit 24: SET M205</summary>

```text
LD T3
SET M205
```

</details>

<details>
<summary>Circuit 25: RST M201</summary>

```text
LD T3
RST M201
```

</details>

<details>
<summary>Circuit 26: RST M200</summary>

```text
LD T3
RST M200
```

</details>

<details>
<summary>Circuit 27: RST M204</summary>

```text
LD T3
RST M204
```

</details>

<details>
<summary>Circuit 28: RST M206</summary>

```text
LD T3
RST M206
```

</details>

<details>
<summary>Circuit 29: RST M212</summary>

```text
LD T3
RST M212
```

</details>

<details>
<summary>Circuit 30: MOV D10 D11</summary>

```text
LD M8000
MOV D10 D11
```

</details>

<details>
<summary>Circuit 31: MOV K1 D10</summary>

```text
LD= D11 K0
LD M201
ANB
LD X7
ANB
LD M204
INV
ANB
LD M205
INV
ANB
LDD= D8340 K0
ANB
LD X17
ANB
LD M8340
INV
ANB
MOV K1 D10
```

</details>

<details>
<summary>Circuit 32: MOV K2 D10</summary>

```text
LD= D11 K1
LD X0
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K2 D10
```

</details>

<details>
<summary>Circuit 33: MOV K33 D10</summary>

```text
LD= D11 K2
LD X1
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K33 D10
```

</details>

<details>
<summary>Circuit 34: MOV K3 D10</summary>

```text
LD= D11 K33
LD M204
INV
ANB
MOV K3 D10
```

</details>

<details>
<summary>Circuit 35: MOV K4 D10</summary>

```text
LD= D11 K3
LD X4
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K4 D10
```

</details>

<details>
<summary>Circuit 36: OUT T0 K30</summary>

```text
LD= D11 K4
LD M204
INV
ANB
OUT T0 K30
```

</details>

<details>
<summary>Circuit 37: MOV K5 D10</summary>

```text
LD= D11 K4
LD T0
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K5 D10
```

</details>

<details>
<summary>Circuit 38: MOV K6 D10</summary>

```text
LD= D11 K5
LD X5
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K6 D10
```

</details>

<details>
<summary>Circuit 39: MOV K7 D10</summary>

```text
LD= D11 K6
LD X2
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K7 D10
```

</details>

<details>
<summary>Circuit 40: MOV K8 D10</summary>

```text
LD= D11 K7
LD X3
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K8 D10
```

</details>

<details>
<summary>Circuit 41: MOV K9 D10</summary>

```text
LD= D11 K8
LD X14
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K9 D10
```

</details>

<details>
<summary>Circuit 42: MOV K10 D10</summary>

```text
LD= D11 K9
LD X15
ANB
MOV K10 D10
```

</details>

<details>
<summary>Circuit 43: MOV K30 D10</summary>

```text
LD= D11 K9
LD X15
INV
ANB
MOV K30 D10
```

</details>

<details>
<summary>Circuit 44: MOV K11 D10</summary>

```text
LD= D11 K10
LD X6
ANB
LD X16
ANB
MOV K11 D10
```

</details>

<details>
<summary>Circuit 45: OUT T4 K10</summary>

```text
LD= D11 K11
OUT T4 K10
```

</details>

<details>
<summary>Circuit 46: MOV K12 D10</summary>

```text
LD= D11 K11
LD T4
ANB
LD M204
INV
ANB
LD M205
INV
ANB
LD X17
ANB
LD M8340
INV
ANB
MOV K12 D10
```

</details>

<details>
<summary>Circuit 47: MOV K13 D10</summary>

```text
LD= D11 K12
LD X20
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K13 D10
```

</details>

<details>
<summary>Circuit 48: MOV K14 D10</summary>

```text
LD= D11 K13
LD= D101 K0
ANB
LD X13
ANB
MOV K14 D10
```

</details>

<details>
<summary>Circuit 49: MOV K14 D10</summary>

```text
LD= D11 K13
LD> D101 K0
ANB
LD X12
ANB
MOV K14 D10
```

</details>

<details>
<summary>Circuit 50: OUT T6 K10</summary>

```text
LD= D11 K14
OUT T6 K10
```

</details>

<details>
<summary>Circuit 51: MOV K15 D10</summary>

```text
LD= D11 K14
LD T6
ANB
LD X17
ANB
LD M8340
INV
ANB
MOV K15 D10
```

</details>

<details>
<summary>Circuit 52: MOV K16 D10</summary>

```text
LD= D11 K15
LD X10
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K16 D10
```

</details>

<details>
<summary>Circuit 53: MOV K17 D10</summary>

```text
LD= D11 K16
LD X20
INV
ANB
LD M204
INV
ANB
MOV K17 D10
```

</details>

<details>
<summary>Circuit 54: MOV K18 D10</summary>

```text
LD= D11 K17
LD X11
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K18 D10
```

</details>

<details>
<summary>Circuit 55: OUT T8 K10</summary>

```text
LD= D11 K18
OUT T8 K10
```

</details>

<details>
<summary>Circuit 56: MOV K19 D10</summary>

```text
LD= D11 K18
LD T8
ANB
LD X17
ANB
LD M8340
INV
ANB
MOV K19 D10
```

</details>

<details>
<summary>Circuit 57: MOV K20 D10</summary>

```text
LD= D11 K19
LD X21
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K20 D10
```

</details>

<details>
<summary>Circuit 58: INC D101</summary>

```text
LD= D11 K20
LD M200
ANB
INC D101
```

</details>

<details>
<summary>Circuit 59: MOV K0 D10</summary>

```text
LD= D11 K20
MOV K0 D10
```

</details>

<details>
<summary>Circuit 60: MOV K32 D10</summary>

```text
LD= D11 K30
LD X22
ANB
LD M204
INV
ANB
LD M205
INV
ANB
MOV K32 D10
```

</details>

<details>
<summary>Circuit 61: MOV K31 D10</summary>

```text
LD= D11 K32
LD X22
INV
ANB
LD M204
INV
ANB
MOV K31 D10
```

</details>

<details>
<summary>Circuit 62: MOV K0 D10</summary>

```text
LD= D11 K31
MOV K0 D10
```

</details>

<details>
<summary>Circuit 63: RST M201</summary>

```text
LD= D11 K20
LD M200
INV
ANB
LD= D11 K20
LD M203
ANB
ORB
LD= D11 K20
LD>= D101 K2
ANB
ORB
LD= D11 K20
LD X7
INV
ANB
ORB
RST M201
```

</details>

<details>
<summary>Circuit 64: RST M200</summary>

```text
LD= D11 K20
LD M200
INV
ANB
LD= D11 K20
LD M203
ANB
ORB
LD= D11 K20
LD>= D101 K2
ANB
ORB
LD= D11 K20
LD X7
INV
ANB
ORB
RST M200
```

</details>

<details>
<summary>Circuit 65: RST M201</summary>

```text
LD= D11 K31
LD M200
INV
ANB
LD= D11 K31
LD M203
ANB
ORB
LD= D11 K31
LD>= D101 K2
ANB
ORB
LD= D11 K31
LD X7
INV
ANB
ORB
RST M201
```

</details>

<details>
<summary>Circuit 66: RST M200</summary>

```text
LD= D11 K31
LD M200
INV
ANB
LD= D11 K31
LD M203
ANB
ORB
LD= D11 K31
LD>= D101 K2
ANB
ORB
LD= D11 K31
LD X7
INV
ANB
ORB
RST M200
```

</details>

<details>
<summary>Circuit 67: RST M203</summary>

```text
LD M201
INV
RST M203
```

</details>

<details>
<summary>Circuit 68: SET M212</summary>

```text
LDP M103
LD M201
INV
ANB
SET M212
```

</details>

<details>
<summary>Circuit 69: OUT T11 K50</summary>

```text
LD M212
LD M204
INV
ANB
OUT T11 K50
```

</details>

<details>
<summary>Circuit 70: RST M212</summary>

```text
LD T11
RST M212
```

</details>

<details>
<summary>Circuit 71: SET M206</summary>

```text
LDP M102
LD M201
INV
ANB
LD M205
INV
ANB
LD X11
ANB
SET M206
```

</details>

<details>
<summary>Circuit 72: OUT T12 K10</summary>

```text
LD M206
OUT T12 K10
```

</details>

<details>
<summary>Circuit 73: SET M208</summary>

```text
LD M206
LD T12
ANB
LDD= D8340 K0
ANB
LD X17
ANB
LD M8340
INV
ANB
SET M208
```

</details>

<details>
<summary>Circuit 74: RST M206</summary>

```text
LD M206
LD T12
ANB
LDD= D8340 K0
ANB
LD X17
ANB
LD M8340
INV
ANB
RST M206
```

</details>

<details>
<summary>Circuit 75: RST M208</summary>

```text
LD M104
LD M105
ORB
RST M208
```

</details>

<details>
<summary>Circuit 76: OUT M209</summary>

```text
LD M206
LD M8013
ANB
LD M208
ORB
OUT M209
```

</details>

<details>
<summary>Circuit 77: OUT M211</summary>

```text
LD M8340
LD X17
INV
ORB
LD M200
ANB
LD M8013
ANB
OUT M211
```

</details>

<details>
<summary>Circuit 78: MOV K0 D103</summary>

```text
LD M8000
MOV K0 D103
```

</details>

<details>
<summary>Circuit 79: MOV K1 D103</summary>

```text
LD M200
MOV K1 D103
```

</details>

<details>
<summary>Circuit 80: MOV K2 D103</summary>

```text
LD M204
MOV K2 D103
```

</details>

<details>
<summary>Circuit 81: DDRVA K-6000 K1500 Y0 Y4</summary>

```text
LD= D11 K11
LD M204
INV
ANB
LD X11
ANB
DDRVA K-6000 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 82: DDRVA K-6000 K1500 Y0 Y4</summary>

```text
LD= D11 K14
LD= D101 K0
ANB
LD M204
INV
ANB
LD X11
ANB
DDRVA K-6000 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 83: DDRVA K0 K1500 Y0 Y4</summary>

```text
LD= D11 K14
LD> D101 K0
ANB
LD M204
INV
ANB
LD X11
ANB
DDRVA K0 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 84: DDRVA K0 K1500 Y0 Y4</summary>

```text
LD= D11 K18
LD M204
INV
ANB
LD X11
ANB
DDRVA K0 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 85: DDRVA K0 K1500 Y0 Y4</summary>

```text
LD= D11 K0
LD M201
ANB
LD M205
INV
ANB
LD M204
INV
ANB
LD X11
ANB
DDRVA K0 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 86: DDRVA K0 K1500 Y0 Y4</summary>

```text
LD M206
LD M205
INV
ANB
LD M204
INV
ANB
LD X11
ANB
DDRVA K0 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 87: DDRVA K0 K1500 Y0 Y4</summary>

```text
LD M205
LD M204
INV
ANB
LD X11
ANB
DDRVA K0 K1500 Y0 Y4
```

</details>

<details>
<summary>Circuit 88: DPLSV D200 Y0 Y4</summary>

```text
LD M104
LD M201
INV
ANB
LD M206
INV
ANB
LD M205
INV
ANB
LDD< D8340 K0
ANB
LD M204
INV
ANB
LD X11
ANB
DPLSV D200 Y0 Y4
```

</details>

<details>
<summary>Circuit 89: DPLSV D202 Y0 Y4</summary>

```text
LD M105
LD M201
INV
ANB
LD M206
INV
ANB
LD M205
INV
ANB
LDD> D8340 K-6000
ANB
LD M204
INV
ANB
LD X11
ANB
DPLSV D202 Y0 Y4
```

</details>

<details>
<summary>Circuit 90: OUT Y17</summary>

```text
LD M8000
LD M204
INV
ANB
OUT Y17
```

</details>

<details>
<summary>Circuit 91: OUT Y1</summary>

```text
LD= D11 K1
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y1
```

</details>

<details>
<summary>Circuit 92: OUT Y2</summary>

```text
LD= D11 K6
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y2
```

</details>

<details>
<summary>Circuit 93: OUT Y3</summary>

```text
LD>= D11 K3
LD<= D11 K5
ANB
LD= D11 K33
ORB
LD M212
ORB
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y3
```

</details>

<details>
<summary>Circuit 94: OUT Y5</summary>

```text
LD>= D11 K3
LD<= D11 K4
ANB
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y5
```

</details>

<details>
<summary>Circuit 95: OUT Y6</summary>

```text
LD= D11 K5
LD M205
ORB
LD M204
INV
ANB
OUT Y6
```

</details>

<details>
<summary>Circuit 96: OUT Y7</summary>

```text
LD>= D11 K8
LD<= D11 K10
ANB
LD= D11 K30
ORB
LD= D11 K32
ORB
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y7
```

</details>

<details>
<summary>Circuit 97: OUT Y10</summary>

```text
LD>= D11 K10
LD<= D11 K18
ANB
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y10
```

</details>

<details>
<summary>Circuit 98: OUT Y11</summary>

```text
LD= D11 K19
LD M205
ORB
LD M204
INV
ANB
OUT Y11
```

</details>

<details>
<summary>Circuit 99: OUT Y12</summary>

```text
LD>= D11 K15
LD<= D11 K16
ANB
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y12
```

</details>

<details>
<summary>Circuit 100: OUT Y13</summary>

```text
LD= D11 K17
LD M205
ORB
LD M204
INV
ANB
OUT Y13
```

</details>

<details>
<summary>Circuit 101: OUT Y14</summary>

```text
LD>= D11 K12
LD<= D11 K15
ANB
LD M205
INV
ANB
OUT Y14
```

</details>

<details>
<summary>Circuit 102: OUT Y15</summary>

```text
LD= D11 K13
LD> D101 K0
ANB
LD M100
LD M201
INV
ANB
ORB
LD M205
INV
ANB
LD M204
INV
ANB
OUT Y15
```

</details>

<details>
<summary>Circuit 103: OUT Y16</summary>

```text
LD= D11 K13
LD= D101 K0
ANB
LD M101
LD M201
INV
ANB
ORB
LD M205
ORB
LD M204
INV
ANB
OUT Y16
```

</details>

<details>
<summary>Circuit 104: OUT T13 K20</summary>

```text
LD M205
OUT T13 K20
```

</details>

<details>
<summary>Circuit 105: MOV K0 D10</summary>

```text
LD M205
LD X1
ANB
LD X3
ANB
LD X5
ANB
LD X11
ANB
LD X13
ANB
LD X21
ANB
LDD= D8340 K0
ANB
LD X17
ANB
LD M8340
INV
ANB
LD T13
ANB
MOV K0 D10
```

</details>

<details>
<summary>Circuit 106: RST M205</summary>

```text
LD M205
LD X1
ANB
LD X3
ANB
LD X5
ANB
LD X11
ANB
LD X13
ANB
LD X21
ANB
LDD= D8340 K0
ANB
LD X17
ANB
LD M8340
INV
ANB
LD T13
ANB
RST M205
```

</details>

<details>
<summary>Circuit 107: END</summary>

```text
END
```

</details>

---

[Contents](GUIDE.en.md) · [← Entry procedure](05-ladder-basics.en.md)
