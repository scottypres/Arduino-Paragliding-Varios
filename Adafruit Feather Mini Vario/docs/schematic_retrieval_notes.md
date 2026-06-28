# Schematic Retrieval Notes

Updated: 2026-06-20

## Files Retrieved

- Adafruit ESP32 Feather V2, product 5400:
  - Retrieved official EagleCAD schematic and board files.
  - Folder: `5400 ESP32 Feather V2 EagleCAD`
  - Source: https://github.com/adafruit/Adafruit-ESP32-Feather-V2-PCB
  - Native KiCad source not found.

- Adafruit 128x64 OLED FeatherWing, product 6313 / 4650 family:
  - Retrieved official EagleCAD schematic and board files for the 128x64 OLED FeatherWing.
  - Folder: `6313 OLED FeatherWing EagleCAD`
  - Source: https://github.com/adafruit/Adafruit-OLED-FeatherWing-PCB
  - Native KiCad source not found.

- Adafruit BMP581 Temperature and Pressure Sensor, product 6407:
  - Retrieved official EagleCAD schematic and board files.
  - Folder: `6407 BMP581 Sensor EagleCAD`
  - Source: https://github.com/adafruit/Adafruit-BMP5xx-Temperature-and-Pressure-Sensor-PCB
  - Native KiCad source not found.

- Adafruit SHT41 Temperature and Humidity Sensor, product 5776:
  - Retrieved official EagleCAD schematic and board files.
  - Folder: `5776 SHT41 Sensor EagleCAD`
  - Source: https://github.com/adafruit/Adafruit-SHT40-PCB
  - Native KiCad source not found.

- Custom Buzzer PCB:
  - Existing native KiCad project already present.
  - Folder: `Buzzer PCB`

## No Downloadable Schematic Source Found

- 400 mAh LiPo battery:
  - Battery only; no schematic source needed. Use connector symbols and physical battery envelope.

- 12085 passive buzzer / Adafruit product 160 PS1240-style piezo buzzer:
  - No downloadable schematic source found. Use a generic KiCad buzzer/speaker symbol and a footprint based on measured/datasheet pin spacing.

- Comidox 0.9-5V to 5V boost converter module:
  - No exact schematic source found for the purchased module. Treat as a 3-pin module: `VIN`, `GND`, `VOUT`, with a custom footprint based on measured board and pin spacing.

- ALLECIN S8050 transistor:
  - No external schematic source needed. Use KiCad built-in BJT symbol and match the footprint to the actual package/pinout.

- JST-PH 2.0 battery connectors:
  - No external schematic source needed. Use generic 2-pin connector symbols and KiCad JST-PH footprints.

- SS12D00G5 slide switch:
  - No exact importable schematic source found. Use a KiCad SPDT switch symbol and verify/create the footprint from the measured switch pinout.

- 1N5819 Schottky diode:
  - No external schematic source needed. Use KiCad built-in diode symbol and match the footprint to the physical package.

- Resistors, capacitors, single-pin header:
  - No external schematic source needed. Use KiCad built-in symbols and footprints.

- STEMMA QT / Qwiic JST-SH cables:
  - Cable assemblies only; no schematic source needed. Use generic 4-pin connector symbols and KiCad JST-SH footprints for board-side connectors.
