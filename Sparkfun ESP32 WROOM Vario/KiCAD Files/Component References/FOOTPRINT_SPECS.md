# Footprint build specs — from DXF + PDF (Buzzer PCB V2)

Source: `Component References/*.pdf` (dims, pin names) + `*Sheet1.dxf` (geometry).
All headers are **2.54 mm pitch, single row, THT**. Pin 1 = square pad.
Exact pad centers to be verified visually against the drawing in KiCad at build time
(DXFs are drawn at a per-file scale + include both front/back views — counts below are de-duplicated).

## Sides (per user; do NOT flip others)
- SD card reader → **BACK (B.Cu)**
- JST plug → **BACK** (existing part)
- Buzzer → **FRONT (F.Cu)** (existing part)
- OLED / Comidox booster / TP4057 charger → **keep current side** (unspecified)

## 1. M16968 OLED + Rotary Knob — `M16968 OLED and Rotary Knob.step`
- Board outline: **59.3 × 35.3 mm**
- Mounting: **4 × Ø3.25** holes (corners), vertical span 29.5
- Header: **9 pins**, header offset 11.5 from datum, 2 from top edge
- Pin order (BACK view, L→R): `3v3, GND, BACK, ENCODER_B, ENCODER_A, ENCODER_PUSH, OLED_SCL, OLED_SDA, CONFIRM`
- Note: front/back pin order mirrors — confirm orientation when placing.

## 2. WWZMDiB SD TF Card Adaptor — `WWZMDiB SD TF Card Adaptor - SPI - Arduino.step`  → BACK
- Board outline: **17.9 × 17.75 mm**, thickness 1.6
- Card window: 13.8 wide
- Mounting: **2 × Ø2.5**, 14.5 apart (horizontal), near bottom
- Header: **6 pins** along bottom edge, row-to-mount offset 4.5
- Pin order (front view, L→R): `3v3, CS, MOSI, CLK, MISO, GND`

## 3. Comidox 5V Booster — `Comidox 5V Booster Board.step`
- Board outline: **10.75 × 10.75 mm**
- Pot/trimmer circle: Ø5.25
- Header: **3 pins** (Ø1 holes): `Vin, GND, Vout` (L→R), offsets 8.12 / 2.84
- Pad pitch 2.54.

## 4. HiLetGo TP4057 LiPo/LiIon Charger — `HiLetGo TP4057 LiPo_LiIon Charger.step`
- Board envelope: **12.2 × 18.5 mm** including the USB-C protrusion.
- Main pad columns span **10.45 mm**; pad rows span **9.0 mm**. Holes are **6 × Ø0.8**.
- Pads at USB end: `In-`, `In+`.
- Pads at opposite end: `Out-`, `Batt-`, `Batt+`, `Out+`.
