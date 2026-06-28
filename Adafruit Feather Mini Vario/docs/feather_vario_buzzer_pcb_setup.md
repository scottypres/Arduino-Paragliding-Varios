# Feather Vario Buzzer Power/Sound PCB Setup

## Purpose

This small PCB acts as a **battery power switch board** and **loud passive buzzer driver board** for the Feather-based vario build.

The system stack is:

```text
FeatherWing OLED
        ↓
400 mAh LiPo battery (sandwiched between adafruit feather and featherwing)
        ↓
Adafruit Feather
        ↓
custom power/buzzer PCB with switch + boost + buzzer driver
        ↓
BMP581 connected to featherwing through Qwiic/STEMMA QT (BMP sits adjacent to our custom PCB)
```

The custom PCB will sit underneath the Feather, adjacent to the BMP581 board.

---

## Important Connector Correction

For the battery path, use **JST-PH 2.0 mm**

```text
JST-PH 2.0 mm = Feather LiPo battery connector
JST-SH 1.0 mm = Qwiic/STEMMA QT connector
```

Do **not** connect the LiPo battery through a Qwiic/JST-SH connector. Qwiic is for 3.3V I2C devices only.

Use the Amazon parts you bought:

- **20 Sets JST PH 2.0 Connector 2 Pin Male/Female**
- **PH 2.0 mm Socket Kit, through-hole side-entry/right-angle PCB connectors**

The custom PCB should have:

```text
JST-PH 2mm 2pin female connector for battery input
JST-PH 2mm 2pin female connector for Feather battery output
switch to turn off all components (SS12D00G5)
```

---

## Main Components

### Purchased components

| Component | Purchased item / source description | Use |
|---|---|---|
| LiPo battery | 400 mAh battery | Main power source |
| Buzzer/transducer | 12085 passive buzzer, 12 mm × 8.5 mm, 42Ω | Loud variable-pitch sounder |
| Boost converter | Comidox 0.9–5V to 5V step-up module | Boosts LiPo/BAT voltage to 5V for buzzer | dimensions: 11x10.5mm (pin location to be determined, but order will be Vin, GND, Vout vertically)
| Transistor kit | ALLECIN 24-value BJT kit | S8050 as buzzer driver |
| Battery connectors | JST-PH 2.0 connector pigtails and PCB sockets | Battery in/out wiring |
| Switch | SS12D00G5 slide switch | Main power switch |
| Flyback diode | Existing 1N5819 Schottky diode | Protects driver transistor from buzzer coil kickback |

### Additional small parts needed

| Qty | Part | Suggested value |
|---:|---|---|
| 1 | Base resistor | 330Ω to 470Ω |
| 1 | Base pulldown resistor | 100kΩ |
| 1 | Bulk capacitor | 47µF to 100µF electrolytic |
| 1 | Ceramic decoupling capacitor | 0.1µF |
| 1 | single-pin header | D13 signal from Feather |

---

## Board Function Blocks

The PCB has two main sections:

1. **Power switching section**
2. **Buzzer driver section**

---

## 1. Power Switching Section

The LiPo battery plugs into the custom PCB first. The PCB switch controls power to the entire device.

### Battery path

```text
LiPo battery +
        ↓
JST-PH battery input +
        ↓
SS12D00G5 slide switch
        ↓
switched BAT+
        ↓
JST-PH output +
        ↓
Feather battery input +
```

Ground is not switched:

```text
LiPo battery -
        ↓
JST-PH battery input -
        ↓
PCB GND
        ↓
JST-PH output -
        ↓
Feather battery input -
```

### Switch wiring

Use the SS12D00G5 as a simple inline power switch on the **positive battery lead**.

```text
Battery + → switch common
switch ON terminal → switched BAT+
unused switch terminal → not connected
```

Before final soldering, verify the switch pins with a multimeter. Do not assume the pinout from the package shape.

---

## 2. Buzzer Driver Section

The buzzer should be driven from the boosted 5V rail through an NPN low-side switch.

### Recommended transistor

Use this one from from the ALLECIN kit:

```text
S8050 NPN

```


---

## Electrical Connections

### Boost converter

The Comidox boost converter has:

```text
VIN
GND
VOUT
```

Wire it like this:

```text
switched BAT+ → boost VIN
PCB GND       → boost GND
boost VOUT    → BOOST_5V
```

The boost converter should receive power **after the main switch**, so it is fully off when the device switch is off.

---

## Buzzer circuit

```text
BOOST_5V → buzzer +
buzzer - → transistor collector
transistor emitter → GND
```

### Transistor base drive

```text
Feather D13 → 330Ω–470Ω resistor → transistor base
transistor base → 100kΩ resistor → GND
```

### Flyback diode

Use your existing **1N5819** across the buzzer.

```text
1N5819 stripe/cathode side → BOOST_5V / buzzer +
1N5819 non-stripe/anode    → buzzer - / transistor collector
```

The diode is normally reverse-biased during operation. It protects the transistor from the magnetic buzzer coil’s voltage spike.

### Capacitors

Place these close to the boost converter output and buzzer driver:

```text
47–100µF electrolytic:
+ → BOOST_5V
- → GND

0.1µF ceramic:
BOOST_5V → GND
```

---

## Feather Connections

The custom PCB connects to the Feather using:

```text
D13
GND
```

The power path to the Feather is handled through the Feather’s battery input connector.

### Signal connection

Use **D13** as the buzzer PWM/tone signal.

```text
Feather D13 → custom PCB BUZZER_SIG
```


---

## FeatherWing / Feather Header Plan

The OLED FeatherWing is pre-soldered and connects to the Feather through female headers.

The OLED FeatherWing uses:

```text
3V
GND
SDA
SCL
```

The BMP581 connects through the OLED FeatherWing’s Qwiic/STEMMA QT port.

The buzzer PCB does **not** connect through Qwiic. It uses D13 for the buzzer signal and the switched battery path for power.

### D27 note

Since the buzzer signal is now planned for **D13**, removing the D27 header pin is not electrically required for the buzzer circuit.

If D27 removal is only for physical clearance, it is acceptable. If there is no clearance issue, leave D27 installed.

---

## Overall Wiring Diagram

```text
                      ┌──────────────────────────┐
                      │  Custom Power/Buzzer PCB │
                      │                          │
LiPo Battery + ───────┤ JST-PH IN +              │
LiPo Battery - ───────┤ JST-PH IN -              │
                      │                          │
                      │ IN+ → switch → BAT_SW+   │
                      │ IN- → GND                │
                      │                          │
                      │ BAT_SW+ → JST-PH OUT + ──┼────→ Feather battery +
                      │ GND     → JST-PH OUT - ──┼────→ Feather battery -
                      │                          │
                      │ BAT_SW+ → boost VIN      │
                      │ GND     → boost GND      │
                      │ boost VOUT → BOOST_5V    │
                      │                          │
Feather D13 ──────────┤ BUZZER_SIG               │
Feather GND ──────────┤ optional extra GND       │
                      │                          │
                      │ BOOST_5V → buzzer +      │
                      │ buzzer - → transistor C  │
                      │ transistor E → GND       │
                      └──────────────────────────┘
```

---

## Suggested Net Names

Use these net names in KiCad/Fusion:

```text
BAT_IN
BAT_SW
GND
BOOST_5V
BUZZER_SIG
BUZZER_LOW
```

Connections:

```text
BAT_IN      = battery input positive before switch
BAT_SW      = switched battery positive after switch
GND         = battery negative / system ground
BOOST_5V    = 5V boost output
BUZZER_SIG  = D13 signal from Feather
BUZZER_LOW  = buzzer negative / transistor collector
```

---

## Suggested Net Classes / Trace Widths

For a CNC-milled single-sided board:

| Net class | Nets | Suggested trace width |
|---|---|---:|
| Power | BAT_IN, BAT_SW, BOOST_5V, GND, BUZZER_LOW | 2.0 mm |
| Signal | BUZZER_SIG | 0.3–0.5 mm |
Please create pad diameters that are as large as possible without overlapping eachother. There should be 0.55mm between each pad minimum. When possible, if 3mm diameter pads that are adjacent allows for 0.55mm clearance between them, when possible make the pads slot shaped (5mm long and 3mm wide for example)

Use wider traces if space allows, especially on:

```text
BAT_SW → boost VIN
boost VOUT → buzzer +
buzzer - → transistor collector
transistor emitter → GND
```

---



---

## Example ESP32 Arduino Test Code

```cpp
const int BUZZER_PIN = 13;
const int BUZZER_CH = 0;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  ledcSetup(BUZZER_CH, 3000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CH);
}

void loop() {
  ledcWriteTone(BUZZER_CH, 2500);
  delay(150);

  ledcWriteTone(BUZZER_CH, 0);
  digitalWrite(BUZZER_PIN, LOW);
  delay(250);

  ledcWriteTone(BUZZER_CH, 4000);
  delay(150);

  ledcWriteTone(BUZZER_CH, 0);
  digitalWrite(BUZZER_PIN, LOW);
  delay(500);
}
```

---

## Sleep / Off Behavior

The main slide switch cuts power from the LiPo to:

```text
Feather
OLED FeatherWing
BMP581
boost converter
buzzer driver
```

So when the switch is off, there should be essentially no battery drain from the electronics.

Deep sleep is separate from true off. The switch is what provides true off.

Before entering ESP32 deep sleep, the firmware should still stop the buzzer:

```cpp
ledcWriteTone(BUZZER_CH, 0);
digitalWrite(BUZZER_PIN, LOW);
pinMode(BUZZER_PIN, OUTPUT);
```

Also turn off the OLED display before sleep if using deep sleep without switching the device fully off.

---

## Critical Warnings

1. **Battery polarity matters.**  
   Some Amazon JST-PH cables may have reversed polarity compared with Adafruit/Feather wiring. Verify with a multimeter before plugging into the Feather.

2. **Do not use JST-SH/Qwiic for battery power.**  
   Qwiic/STEMMA QT is 3.3V I2C only.

3. **Do not drive the 42Ω buzzer directly from a Feather GPIO.**  
   Use the transistor driver.

4. **Verify transistor pinout before milling.**  
   S8050 may have different pin orders depending on package/vendor.

5. **Verify switch pinout before milling.**  
   Use continuity mode to identify common and switched terminals.

6. **Use a shared ground.**  
   Feather GND, boost GND, buzzer driver GND, and battery negative must all be common.
