---
type: source
title: "Vario Adafruit FeatherWing Repo Snapshot"
created: 2026-06-20
updated: 2026-06-20
tags:
  - hardware
  - kicad
  - cad
  - feather
status: developing
related:
  - "[[Vario Adafruit FeatherWing]]"
  - "[[Feather Vario Buzzer PCB]]"
  - "[[Hand-Drilled KiCad PCB Constraints]]"
sources:
  - "[[.raw/repos/vario-adafruit-featherwing-2026-06-20.md]]"
---

# Vario Adafruit FeatherWing Repo Snapshot

The [[Vario Adafruit FeatherWing]] repo is the working hardware folder for a Feather-based vario build. It combines Adafruit purchase tracking, CAD acquisition, schematic source retrieval, and a custom KiCad power/buzzer PCB.

## Repository Shape

- `adafruit_purchase.xlsx` tracks purchased product IDs and CAD availability.
- `CAD Files/` contains curated STEP and source CAD for project components.
- `KiCAD Files/` contains imported Adafruit EagleCAD files plus the native custom KiCad buzzer board.
- `feather_vario_buzzer_pcb_setup.md` is the functional design brief for the custom PCB.
- `KiCAD Files/schematic_retrieval_notes.md` records which components have official schematic/board sources and which require generic KiCad symbols.

## Retrieved Sources

Official Adafruit EagleCAD sources were retrieved for product 5400 ([[Adafruit ESP32 Feather V2]]), product 6313/4650 family OLED FeatherWing, product 6407 BMP581, and product 5776 SHT41. Native KiCad project sources were not found for those Adafruit boards, so their EagleCAD files are the clean import source.

The custom buzzer board is native KiCad and lives at `KiCAD Files/Buzzer PCB`.

## Current Custom PCB State

The custom PCB has placed footprints for the battery JST input, switch, Feather battery JST output, boost module, passive buzzer, D13 input pad, S8050 transistor, base/pulldown resistors, diode, bulk capacitor, ceramic decoupling capacitor, and mounting holes.

Current durable layout decisions:

- Drill export uses only `0.8`, `1.0`, `1.2`, and `3.0 mm` holes.
- Trace presets are `1`, `2`, and `3 mm`.
- `1 mm` traces are acceptable everywhere for this low-power board.
- D13 must stay at `(85.87, 92.84)`, the original User.2 circle center.
- The boost converter footprint is a custom `11 x 11 mm` module with `1.2 mm` holes at `2.54 mm` pitch, hole centers `1.35 mm` from the edge.
- Q1 uses a hand-assembly-friendly formed-lead TO-92 footprint at `2.54 mm` pad pitch. The real unformed TO-92 adjacent lead pitch is about `1.27 mm`, so the outer legs must be bent outward.

## Verification Caveats

KiCad CLI DRC crashed with exit `134`, so the session used drill export, position export, and direct file inspection for checks. Placement quality still needs human review because the physical board is very dense.
