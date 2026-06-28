---
type: synthesis
title: "Vario FeatherWing PCB Layout Notes"
created: 2026-06-20
updated: 2026-06-20
tags:
  - kicad
  - pcb
  - feather
  - buzzer
status: developing
related:
  - "[[Vario Adafruit FeatherWing]]"
  - "[[Feather Vario Buzzer PCB]]"
  - "[[Hand-Drilled KiCad PCB Constraints]]"
sources:
  - "[[vario-adafruit-featherwing-repo-2026-06-20]]"
question: "What PCB placement and hand-fabrication decisions were made for the Feather vario buzzer board?"
answer_quality: solid
---

# Vario FeatherWing PCB Layout Notes

The Feather vario custom PCB is a compact power-switch and passive-buzzer driver board. It is designed for hand fabrication with known drill bits and manual routing.

## Functional Design

The battery path uses JST-PH 2.0 mm, not JST-SH/Qwiic. The LiPo battery positive goes into `J1`, through `SW1`, then out through `J2` to the Feather battery connector. Ground is common and is not switched.

The buzzer path uses a 5V boost converter and an NPN low-side switch:

```text
BOOST_5V -> buzzer + 
buzzer - -> Q1 collector
Q1 emitter -> GND
D13 -> R1 -> Q1 base
R2 pulls Q1 base to GND
```

## Current PCB Constraints

- Trace width presets: `1`, `2`, and `3 mm`.
- `1 mm` traces are acceptable for the whole board.
- Available drill sizes: `0.8`, `1.0`, `1.2`, `1.4`, `1.6`, `1.8`, `2.0`, `2.2`, `2.4`, `3.0`, `3.125`, `3.2 mm`.
- Current drill export: `0.8`, `1.0`, `1.2`, and `3.0 mm` only.
- Mounting holes are `3.0 mm` NPTH.

## Fixed Placement Detail

The D13 footprint must remain at the original User.2 circle center:

```text
J4 / Feather D13: (85.87, 92.84)
```

This was restored after it was accidentally moved during automated placement.

## Step-Up Converter Footprint

The boost converter footprint is custom:

- Module body: `11 x 11 mm`.
- Pin holes: `1.2 mm`.
- Pitch: `2.54 mm`.
- Hole centers: `1.35 mm` from the module edge.
- Pin order: `VIN`, `GND`, `VOUT`.

## Q1 Footprint Decision

The S8050 TO-92 component normally has about `1.27 mm` adjacent lead pitch, with the outer leads about `2.54 mm` apart. That exact footprint is cramped for hand drilling and soldering.

The board uses a widened formed-lead TO-92 footprint instead:

- Adjacent pad pitch: `2.54 mm`.
- Outer pad spacing: `5.08 mm`.
- Drill: `0.8 mm`.

This requires bending the two outside transistor legs outward. That is normal for TO-92 hand assembly and should be easy with needle-nose pliers. Bend a few millimeters below the plastic body, not at the body.

## SW1 Meaning

`SW1` is the main power switch. It is an SPDT slide switch used as a simple inline positive battery switch:

- Pad 1: `SW_BAT`.
- Pad 2: `BAT_IN`.
- Pad 3: unused.

The exact SS12D00G5 pinout should be verified with a multimeter before fabrication or soldering.

## Capacitor Labels

The small rectangle labeled between `GND` and `BOOST_5V` is the decoupling capacitor, likely `C2`, a `0.1 uF` ceramic capacitor across the boost output. `C1` is the larger `47-100 uF` bulk capacitor on the same rail.

## Routing Guidance

All traces can be `1 mm` wide. The power path should still be short and direct:

- Battery positive to switch.
- Switch to boost input.
- Boost output to buzzer and capacitors.
- Buzzer return through Q1 to ground.
- Main ground return.

Wider traces are optional margin, not a requirement for this low-power board.

## Verification Caveats

KiCad CLI DRC crashed with exit `134`, so verification used drill export, position export, and direct file inspection. Automated placement checks also reported overlaps because the physical board is dense. Human mechanical review is still required before fabrication.
