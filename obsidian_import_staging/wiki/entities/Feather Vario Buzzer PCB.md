---
type: entity
title: "Feather Vario Buzzer PCB"
created: 2026-06-20
updated: 2026-06-20
tags:
  - kicad
  - pcb
  - feather
  - buzzer
status: active
related:
  - "[[Vario Adafruit FeatherWing]]"
  - "[[Hand-Drilled KiCad PCB Constraints]]"
  - "[[KiCad Single-Sided PCB Milling Workflow]]"
sources:
  - "[[vario-adafruit-featherwing-repo-2026-06-20]]"
---

# Feather Vario Buzzer PCB

The Feather Vario Buzzer PCB is a small custom KiCad board that provides battery switching and a louder passive buzzer driver for the Feather vario build.

## Function

The board receives a 400 mAh LiPo battery through JST-PH, switches the positive battery lead through `SW1`, feeds the Feather battery input, powers a 5V step-up converter, and drives a passive buzzer through an S8050 NPN low-side switch controlled by Feather D13.

## Main Nets

- `BAT_IN`: battery positive before the switch.
- `SW_BAT`: switched battery positive after `SW1`.
- `BOOST_5V`: 5V boost output for the buzzer and capacitors.
- `BUZZER_LOW`: buzzer low side, switched by Q1 collector.
- `BUZZER_BASE`: base-drive node from D13 through R1 with R2 pulldown.
- `D13`: Feather signal input.
- `GND`: common ground.

## Key Footprints

- `J1`: JST-PH battery input.
- `SW1`: SS12D00G5 slide switch used as an inline positive battery switch.
- `J2`: JST-PH Feather battery output.
- `J3`: custom 11 x 11 mm Comidox boost module footprint.
- `BZ1`: 12085 passive buzzer footprint.
- `J4`: Feather D13 pad at the original User.2 position `(85.87, 92.84)`.
- `Q1`: S8050 NPN TO-92 footprint widened for formed leads.
- `D1`: 1N5819 Schottky diode.
- `C1`: 47-100 uF bulk capacitor across `BOOST_5V` and `GND`.
- `C2`: 0.1 uF ceramic decoupling capacitor across `BOOST_5V` and `GND`.

## Assembly Notes

`SW1` must be verified with a multimeter before fabrication or soldering because the exact switch pinout should not be assumed from package shape.

The Q1 footprint uses `2.54 mm` adjacent pad pitch for easier drilling and soldering. The real unformed S8050 TO-92 lead pitch is about `1.27 mm` between adjacent leads, so the two outside leads are bent outward during assembly.

`C2` is the small capacitor between `BOOST_5V` and `GND`; it smooths high-frequency boost/buzzer noise. `C1` is the larger bulk capacitor on the same rail.
