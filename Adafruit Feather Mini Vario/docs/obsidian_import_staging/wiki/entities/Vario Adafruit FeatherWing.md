---
type: entity
title: "Vario Adafruit FeatherWing"
created: 2026-06-20
updated: 2026-06-20
tags:
  - repo
  - hardware
  - feather
  - kicad
status: active
related:
  - "[[Feather Vario Buzzer PCB]]"
  - "[[Vario Adafruit FeatherWing Repo Snapshot]]"
  - "[[Hand-Drilled KiCad PCB Constraints]]"
sources:
  - "[[vario-adafruit-featherwing-repo-2026-06-20]]"
---

# Vario Adafruit FeatherWing

`Vario_Adafruit_Featherwing` is the hardware working repo for a Feather-based vario build. The repo keeps the project bill of materials, copied/generated STEP files, retrieved Adafruit EagleCAD sources, and the native KiCad custom buzzer/power board.

## Role

The repo is the source of truth for physical integration around the Feather stack:

- [[Adafruit ESP32 Feather V2]]
- OLED FeatherWing
- BMP581 pressure sensor
- SHT41 temperature/humidity sensor
- 400 mAh LiPo battery
- JST-PH battery cable path
- custom [[Feather Vario Buzzer PCB]]

## Current Status

The custom KiCad PCB has been generated and component footprints have been placed. The user intends to route traces manually. Drill sizes and trace presets have been constrained for hand fabrication.

## Important Files

- `feather_vario_buzzer_pcb_setup.md`: functional design brief.
- `KiCAD Files/Buzzer PCB/Buzzer PCB.kicad_sch`: schematic.
- `KiCAD Files/Buzzer PCB/Buzzer PCB.kicad_pcb`: placed PCB.
- `CAD Files/`: STEP and CAD assets.
- `KiCAD Files/schematic_retrieval_notes.md`: retrieved schematic source record.
