# TODO

## Battery Voltage

- [ ] Future hardware/firmware change: remove the WS2812B NeoPixel from A1/GPIO25 and repurpose A1 for battery voltage sensing through a resistor divider. Do not remove the current NeoPixel firmware until this hardware change is ready.

## Temperature Self-Heating Compensation

- [ ] Run the vario in a constant-temperature room for 30-45 minutes while logging SHT41 temperature, SHT41 humidity, BMP temperature, BMP pressure/altitude, battery voltage, and runtime.
- [ ] Use the warmup data to estimate how much the PCB heats the sensors after startup.
- [ ] Add a firmware correction for displayed/logged temperature based on runtime or measured warmup behavior.
- [ ] Validate the correction with a second constant-temperature run.
