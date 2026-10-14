<a id="공개문제-3번-직접-제작-가이드"></a>

# Public exam 3: build-it-yourself guide

Follow **placement → power wiring → control wiring → HMI/ladder creation**.
Continue with the **Next →** link at the bottom of each page.

<a id="실습-시작"></a>

## Starting the exercise

| Order | Task | Target |
| --- | --- | --- |
| 1 | [Placement](01-placement.en.md) | 39 parts; no wires or HMI widgets |
| 2 | [Power wiring](02-power.en.md) | 51 power connections |
| 3 | [Control wiring](03-control.en.md) | 65 control/pneumatic/optical connections |
| 4 | [HMI screens](04-hmi.en.md) | Two screens: Test operation / Equipment trial run |
| 5 | [Ladder entry](05-ladder-basics.en.md) | Learn contact/instruction entry |

[Download placement-only JSON](exam03-placement-only.json)

Build wiring, HMI, and program yourself using the guide.

<a id="동작별-래더-작성"></a>

## Ladder logic by operation

Each page covers **flow → logic diagram → actual ladder → entry procedure → checks**.

1. [Initialization](logic-01-init.en.md)
2. [Manual operation](logic-02-manual.en.md)
3. [Supply and processing](logic-03-process.en.md)
4. [Material branching](logic-04-material.en.md)
5. [Pickup and storage](logic-05-storage.en.md)
6. [Discharge and finish](logic-06-finish.en.md)
7. [Pause and recovery](logic-07-pause.en.md)

[Reading ladder logic](reference-ladder-reading.en.md) · [Compile and trial runs](06-checks.en.md)

<a id="필요한-때만-보는-참고-자료"></a>

## References when needed

- [Component coordinates and Z settings](reference-placement.en.md)
- [Address roles](reference-devices.en.md)
- [Stage, servo, and recovery conditions](reference-sequence.en.md)
- [Complete circuit tables and instructions](reference-program.en.md)
