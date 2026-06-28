---
type: concept
title: "Hand-Drilled KiCad PCB Constraints"
created: 2026-06-20
updated: 2026-06-20
tags:
  - pcb
  - kicad
  - fabrication
status: developing
related:
  - "[[Feather Vario Buzzer PCB]]"
  - "[[KiCad Single-Sided PCB Milling Workflow]]"
sources:
  - "[[vario-adafruit-featherwing-repo-2026-06-20]]"
---

# Hand-Drilled KiCad PCB Constraints

For a hand-drilled PCB, footprints should use drill sizes that match the available bit set instead of library-default or mathematically convenient holes.

## Drill Strategy

The active drill list for the Feather vario buzzer board is:

`0.8`, `1.0`, `1.2`, `1.4`, `1.6`, `1.8`, `2.0`, `2.2`, `2.4`, `3.0`, `3.125`, `3.2 mm`.

The current board export uses only:

`0.8`, `1.0`, `1.2`, `3.0 mm`.

This keeps fabrication simple and avoids odd drill sizes such as `2.6 mm` that cannot be made directly with the available bits.

## Trace Strategy

For a low-power buzzer board, `1 mm` traces are acceptable everywhere. Wider traces can still be used for extra margin and easier milling:

- `3 mm`: battery, boost, buzzer, and main ground paths if there is space.
- `2 mm`: shorter current branches where space is tight.
- `1 mm`: signal traces such as D13 to the transistor base.

The important routing property is directness and short return paths, not maximum width.

## Footprint Strategy

Use footprints that match fabrication and assembly realities:

- TO-92 transistor footprints can be widened to `2.54 mm` pitch for hand drilling, but this requires bending the outside leads outward.
- Mounting holes should be converted to allowed drill sizes, such as `3.0 mm` NPTH instead of non-listed clearances.
- Module footprints should encode real mechanical constraints, including board size and hole-to-edge distances.
