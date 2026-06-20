---
source_type: repo
source_path: /Users/scottpresbrey/Documents/GitHub/Vario_Adafruit_Featherwing
fetched: 2026-06-20
---

# Vario Adafruit FeatherWing Repo Snapshot

This source summarizes the local `Vario_Adafruit_Featherwing` repository as of 2026-06-20.

## Purpose

The repo supports a Feather-based vario hardware build. It combines product purchasing data, Adafruit CAD retrieval, STEP model cleanup, EagleCAD/KiCad schematic sources, and a custom KiCad buzzer/power-switch PCB.

## Key Project Files

- `adafruit_purchase.xlsx`: spreadsheet used to track purchased Adafruit/product IDs and CAD availability.
- `feather_vario_buzzer_pcb_setup.md`: design brief for the custom power and buzzer PCB.
- `CAD Files/`: curated CAD assets copied or generated for the project.
- `KiCAD Files/`: imported EagleCAD schematic/board files and the native custom KiCad project.
- `KiCAD Files/Buzzer PCB/Buzzer PCB.kicad_sch`: native KiCad schematic for the custom buzzer/power PCB.
- `KiCAD Files/Buzzer PCB/Buzzer PCB.kicad_pcb`: custom PCB layout with placed components.
- `KiCAD Files/schematic_retrieval_notes.md`: record of importable schematic/CAD sources found and components where only generic symbols were available.

## Retrieved CAD and Schematic Sources

- Adafruit ESP32 Feather V2, product 5400: official EagleCAD files retrieved from Adafruit's PCB repo. Native KiCad source was not found.
- Adafruit 128x64 OLED FeatherWing, product 6313/4650 family: official EagleCAD files retrieved from Adafruit's OLED FeatherWing PCB repo. Native KiCad source was not found.
- Adafruit BMP581 temperature and pressure sensor, product 6407: official EagleCAD files retrieved from Adafruit BMP5xx PCB repo. Native KiCad source was not found.
- Adafruit SHT41 temperature and humidity sensor, product 5776: official EagleCAD files retrieved from Adafruit SHT40 PCB repo. Native KiCad source was not found.
- Custom Buzzer PCB: native KiCad project exists in `KiCAD Files/Buzzer PCB`.

## Custom Buzzer PCB Design

The custom PCB acts as a battery power switch board and passive buzzer driver board under the Feather stack. It routes a 400 mAh LiPo through JST-PH battery connectors and an SS12D00G5 slide switch, powers a small 5V boost module, and drives a passive buzzer from D13 through an S8050 NPN low-side switch.

Important PCB constraints:

- User drill sizes: `0.8`, `1.0`, `1.2`, `1.4`, `1.6`, `1.8`, `2.0`, `2.2`, `2.4`, `3.0`, `3.125`, `3.2 mm`.
- Current board drill export uses only `0.8`, `1.0`, `1.2`, and `3.0 mm`.
- Trace width presets are `1.0`, `2.0`, and `3.0 mm`; `1.0 mm` is acceptable for all traces in this low-power buzzer board.
- The D13 pad must remain at its original User.2 circle center: `(85.87, 92.84)` in board coordinates.
- Mounting holes are `3.0 mm` NPTH because the earlier `2.6 mm` conversion did not match the available drill list.
- The boost converter footprint is `11 x 11 mm`, with `1.2 mm` drilled holes at `2.54 mm` pitch and hole centers `1.35 mm` from the module edge.
- The Q1 S8050 footprint is widened to a formed-lead TO-92 footprint with `2.54 mm` adjacent pad pitch, not the as-shipped `1.27 mm` lead pitch. This requires bending the outer transistor legs outward.

## Open Mechanical Checks

- Verify the SS12D00G5 switch pinout with a multimeter before fabrication.
- Verify the exact S8050 pinout for the purchased kit before soldering.
- Verify physical clearances on the dense custom board. Automated placement checks reported overlaps because the board is small relative to the 12 mm buzzer and 11 mm boost module.
- KiCad CLI DRC crashed with exit `134` during this session, so drill and position exports were used for verification instead.
