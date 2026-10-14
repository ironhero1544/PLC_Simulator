[Contents](GUIDE.en.md) · [← Entry procedure](05-ladder-basics.en.md)

<a id="래더를-읽는-방법"></a>

# Reading ladder logic

Read the operation sequence in the logic diagram, then check ladder contacts and instructions.
See [contact/MOV entry](05-ladder-basics.en.md) for input steps and [circuit tables](reference-program.en.md) for all conditions.

Read ladder as **execute the right-side instruction when all left-side conditions are true**.
Series contacts mean AND, parallel OR, and slashed contacts pass when OFF.
Screenshot row numbers include parallel branches and may differ from appendix circuit numbers.
Match contact addresses and right-side instructions rather than row numbers.

`X` is sensor input, `Y` actuator output, `M` internal state, `D` numeric data,
and `T` timer. `SET` latches a state; `RST` clears it.
`MOV` copies a value; `DMOV` copies a 32-bit value.
`K` values are constants; T0/T3 here use 0.1-second units.

| Diagram | Meaning |
| --- | --- |
| Series contacts | All conditions must be true |
| Parallel contacts | Any condition may be true |
| Slashed contact | Passes when the signal is OFF |
| Right-side coil/instruction | Action performed when conditions are true |

---

[Contents](GUIDE.en.md) · [← Entry procedure](05-ladder-basics.en.md)
