# External CAD Search Notes

Updated: 2026-06-19

## Exact or Strong Matches Copied

- Product 5776, Adafruit Sensirion SHT41 Temperature & Humidity Sensor:
  - Copied exact Adafruit EagleCAD board and schematic files from `adafruit/Adafruit-SHT40-PCB`.
  - Added a Fusion 360-compatible STEP file copied from Adafruit's `5665 SHT45 Sensor` CAD model. The SHT41 and SHT45 are in the same SHT4x breakout family; use this as same-family mechanical geometry, not a product-ID-exact exported model.
  - Folder: `5776 SHT41 Sensor PCB`
  - Source: https://github.com/adafruit/Adafruit-SHT40-PCB

- Product 6407, Adafruit BMP581 I2C or SPI Temperature and Pressure Sensor:
  - Copied exact Adafruit EagleCAD board and schematic files from `adafruit/Adafruit-BMP5xx-Temperature-and-Pressure-Sensor-PCB`.
  - Added a Fusion 360-compatible STEP file copied from Adafruit's `6413 BMP585 Pressure Sensor` CAD model. The BMP581 and BMP585 are in the same BMP5xx breakout family; use this as same-family mechanical geometry, not a product-ID-exact exported model.
  - Folder: `6407 BMP581 Sensor PCB`
  - Source: https://github.com/adafruit/Adafruit-BMP5xx-Temperature-and-Pressure-Sensor-PCB

## Reference-Only CAD Copied

- Product 261, JST PH 2-pin Cable:
  - Generated a simplified Fusion 360-compatible STEP cable assembly using the published 100 mm cable length and a JST PH 2-pin female cable plug envelope.
  - Folder: `261 JST PH 2-pin Cable`
  - Kept the earlier KiCad JST PH 2-pin board connector STEP as connector-end reference geometry in `261 JST PH 2-pin Connector Reference`.
  - Sources: https://www.adafruit.com/product/261 and https://kicad.github.io/packages3d/Connector_JST.html

- Product 2940, Short Headers Kit for Feather:
  - Copied generic KiCad 1x12 and 1x16 2.54 mm female pin socket STEP models.
  - These are reference pin sockets, not verified 5.0 mm body-height Adafruit short headers.
  - Folder: `2940 Short Feather Headers - Generic PinSocket Reference`
  - Source: https://gitlab.com/kicad/libraries/kicad-packages3D

- Standard 2.54 mm Male Headers:
  - Added generic KiCad 1x12 and 1x16 vertical male pin header STEP models for CAD assembly use.
  - Folder: `Standard 2.54mm Male Headers`
  - Source: https://gitlab.com/kicad/libraries/kicad-packages3D

- Product 3299, Black Nylon M2.5 Screw and Stand-off Set:
  - Generated Fusion 360-compatible BRep STEP files for M2.5 hex standoff variants: 6, 8, 10, and 12 mm female-female bodies, plus 6 and 12 mm male-female bodies.
  - Regenerated these on 2026-06-19 after the first faceted STEP exports imported as empty bodies in Fusion 360.
  - Kept the configurable OpenSCAD source and STL examples as references.
  - This is not the full 380-piece Adafruit kit.
  - Folder: `3299 M2.5 Standoff Reference`
  - Source: https://github.com/EvilFreelancer/hex-standoff

- Product 3366, 36-pin Stacking Header:
  - Copied a parametric FreeCAD source for 2.54 mm stackable headers.
  - A 36-pin STEP was not pre-exported in the release, and FreeCAD was not installed locally to export one.
  - Folder: `3366 36-pin Stacking Header - Parametric Source`
  - Source: https://github.com/basilhussain/stackable-headers-3d

- Products 4210 and 4399, STEMMA QT / Qwiic JST SH 4-pin Cable:
  - Generated simplified Fusion 360-compatible BRep STEP cable assemblies using the published 100 mm and 50 mm lengths and JST SH 4-pin female plug envelopes.
  - Regenerated these on 2026-06-19 after the first faceted STEP exports imported as empty bodies in Fusion 360.
  - Folder: `4210_4399 STEMMA QT Qwiic JST SH Cables`
  - Kept the earlier KiCad JST SH 4-pin board connector STEP as connector-end reference geometry in `4210_4399 STEMMA QT Cable Connector Reference`.
  - Sources: https://www.adafruit.com/product/4210, https://www.adafruit.com/product/4399, and https://gitlab.com/kicad/libraries/kicad-packages3D

- Product 4397, STEMMA QT / Qwiic JST SH 4-pin Cable with Female Sockets:
  - Generated a simplified Fusion 360-compatible STEP cable assembly using the published 150 mm cable length, one JST SH 4-pin female plug envelope, and four 0.1 inch female socket envelopes.
  - Folder: `4397 STEMMA QT to Female Sockets Cable`
  - Kept the earlier KiCad JST SH 4-pin connector STEP and generic 1x04 2.54 mm female pin socket STEP as connector-end reference geometry in `4397 STEMMA QT to Female Sockets Reference`.
  - Sources: https://www.adafruit.com/product/4397 and https://gitlab.com/kicad/libraries/kicad-packages3D

## No Downloadable CAD Found

- Product 160, Piezo Buzzer PS1240:
  - Found exact online model references for TDK PS1240P02BT, but downloadable CAD required a login/account. No file copied.

- Product 3700, Maker-Friendly Zipper Case - Red:
  - Found product details and dimensions, but no downloadable matching CAD file. No file copied.

- Product 5019, Thick Double-Sided Rectangle Foam Tape:
  - Found product dimensions and material details, but no downloadable matching CAD file. No file copied.

- Product 5719, PCB Coaster with Gold Adafruit Logo:
  - Found product dimensions, but no downloadable matching PCB/CAD design file. No file copied.
