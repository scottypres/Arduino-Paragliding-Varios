#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <Wire.h>

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#endif

#ifndef WIFI_NETWORKS
#define WIFI_NETWORKS {"", ""}
#define WIFI_NETWORKS_CONFIGURED 0
#else
#define WIFI_NETWORKS_CONFIGURED 1
#endif

// SparkFun Thing Plus ESP32 WROOM-C pin mapping for this carrier board.
constexpr uint8_t kQwiicPowerPin = 0;
constexpr uint8_t kSdCsPin = 5;
constexpr uint8_t kSdSckPin = 18;
constexpr uint8_t kSdMisoPin = 19;
constexpr uint8_t kSdMosiPin = 23;

constexpr uint8_t kI2cSdaPin = 33;
constexpr uint8_t kI2cSclPin = 32;

constexpr uint8_t kGpsRxPin = 21;  // GPS TX on Qwiic SDA into ESP32 RX.
constexpr uint8_t kGpsTxPin = 22;  // GPS RX on Qwiic SCL from ESP32 TX.
constexpr uint32_t kGpsBaud = 9600;

constexpr uint8_t kBuzzerPins[] = {13, 26, 27};
constexpr uint32_t kToneHalfPeriodUs = 125;  // 4 kHz square wave.
constexpr uint32_t kToneOnMs = 500;
constexpr uint32_t kToneOffMs = 2500;

constexpr uint8_t kEncoderAPin = 39;
constexpr uint8_t kEncoderBPin = 36;
constexpr uint8_t kBackButtonPin = 35;
constexpr uint8_t kEncoderButtonPin = 34;
constexpr uint8_t kConfirmButtonPin = 4;

constexpr uint32_t kDebounceMs = 35;
constexpr uint32_t kDisplayRefreshMs = 200;
constexpr const char *kGpsLogPath = "/gps_log.csv";
constexpr uint8_t kBuzzerCount = sizeof(kBuzzerPins) / sizeof(kBuzzerPins[0]);
constexpr uint32_t kWifiConnectTimeoutMs = 7000;
constexpr uint8_t kOledAddress = 0x3C;
constexpr int8_t kOledResetPin = -1;
constexpr int16_t kOledWidth = 128;
constexpr int16_t kOledHeight = 64;

struct WifiNetwork {
  const char *ssid;
  const char *password;
};

const WifiNetwork kWifiNetworks[] = {
    WIFI_NETWORKS
};
constexpr uint8_t kWifiNetworkCount = sizeof(kWifiNetworks) / sizeof(kWifiNetworks[0]);

Adafruit_SH1106G oled(kOledWidth, kOledHeight, &Wire, kOledResetPin);
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

enum VolumeLevel : uint8_t {
  kVolumeLow = 0,
  kVolumeMedium,
  kVolumeLoud,
  kVolumeCount
};

enum MenuItem : uint8_t {
  kMenuGpsLogging = 0,
  kMenuGpsDisplay,
  kMenuGpsLogRate,
  kMenuVolume,
  kMenuBuzzer,
  kMenuCount
};

struct Button {
  uint8_t pin;
  bool usePullup;
  bool stablePressed;
  bool lastRawPressed;
  uint32_t lastChangeMs;
  bool pressedEvent;
};

Button backButton = {kBackButtonPin, false, false, false, 0, false};
Button encoderButton = {kEncoderButtonPin, false, false, false, 0, false};
Button confirmButton = {kConfirmButtonPin, true, false, false, 0, false};

const uint32_t kLogRatesMs[] = {1000, 2000, 5000, 10000, 30000, 60000};
const char *const kLogRateLabels[] = {"1 sec", "2 sec", "5 sec", "10 sec", "30 sec", "60 sec"};
const char *const kVolumeLabels[] = {"Low", "Medium", "Loud"};
constexpr uint8_t kLogRateCount = sizeof(kLogRatesMs) / sizeof(kLogRatesMs[0]);

bool oledReady = false;
bool sdReady = false;
bool gpsLoggingEnabled = false;
bool gpsDisplayEnabled = true;
bool buzzerEnabled = true;
bool wifiReady = false;
bool editingMenuItem = false;
uint8_t selectedMenuItem = kMenuGpsLogging;
uint8_t logRateIndex = 2;
VolumeLevel volumeLevel = kVolumeLow;

uint32_t lastDisplayMs = 0;
uint32_t lastGpsLogMs = 0;
uint32_t lastBuzzerCycleMs = 0;
uint32_t lastToneToggleUs = 0;
bool buzzerDriveHigh = false;

void setBuzzersLow() {
  for (uint8_t pin : kBuzzerPins) {
    digitalWrite(pin, LOW);
  }
}

uint8_t activeBuzzerCount() {
  return static_cast<uint8_t>(volumeLevel) + 1;
}

void serviceBuzzers() {
  if (!buzzerEnabled) {
    setBuzzersLow();
    lastBuzzerCycleMs = millis();
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastBuzzerCycleMs >= kToneOnMs + kToneOffMs) {
    lastBuzzerCycleMs = nowMs;
  }

  const bool toneActive = (nowMs - lastBuzzerCycleMs) < kToneOnMs;
  if (!toneActive) {
    setBuzzersLow();
    return;
  }

  const uint32_t nowUs = micros();
  if (nowUs - lastToneToggleUs < kToneHalfPeriodUs) {
    return;
  }

  lastToneToggleUs = nowUs;
  buzzerDriveHigh = !buzzerDriveHigh;
  const uint8_t count = activeBuzzerCount();
  for (uint8_t index = 0; index < kBuzzerCount; index++) {
    digitalWrite(kBuzzerPins[index], index < count && buzzerDriveHigh ? HIGH : LOW);
  }
}

void initButton(Button &button) {
  pinMode(button.pin, button.usePullup ? INPUT_PULLUP : INPUT);
  const bool rawPressed = digitalRead(button.pin) == LOW;
  button.stablePressed = rawPressed;
  button.lastRawPressed = rawPressed;
  button.lastChangeMs = millis();
  button.pressedEvent = false;
}

void updateButton(Button &button) {
  const bool rawPressed = digitalRead(button.pin) == LOW;
  button.pressedEvent = false;

  if (rawPressed != button.lastRawPressed) {
    button.lastRawPressed = rawPressed;
    button.lastChangeMs = millis();
  }

  if (millis() - button.lastChangeMs < kDebounceMs) {
    return;
  }

  if (rawPressed != button.stablePressed) {
    button.stablePressed = rawPressed;
    if (button.stablePressed) {
      button.pressedEvent = true;
    }
  }
}

int8_t readEncoderDelta() {
  static bool initialized = false;
  static uint8_t lastState = 0;
  static int8_t accumulator = 0;
  constexpr int8_t transitionTable[16] = {
      0, -1, 1, 0,
      1, 0, 0, -1,
      -1, 0, 0, 1,
      0, 1, -1, 0};

  const uint8_t state = (digitalRead(kEncoderAPin) ? 0x02 : 0x00) |
                        (digitalRead(kEncoderBPin) ? 0x01 : 0x00);

  if (!initialized) {
    lastState = state;
    initialized = true;
    return 0;
  }

  const int8_t movement = transitionTable[(lastState << 2) | state];
  lastState = state;

  if (movement == 0) {
    return 0;
  }

  accumulator += movement;
  if (accumulator >= 4) {
    accumulator = 0;
    return 1;
  }
  if (accumulator <= -4) {
    accumulator = 0;
    return -1;
  }

  return 0;
}

void adjustSelectedValue(int8_t delta) {
  if (delta == 0) {
    return;
  }

  switch (selectedMenuItem) {
    case kMenuGpsLogging:
      gpsLoggingEnabled = !gpsLoggingEnabled;
      break;
    case kMenuGpsDisplay:
      gpsDisplayEnabled = !gpsDisplayEnabled;
      break;
    case kMenuGpsLogRate:
      logRateIndex = static_cast<uint8_t>((static_cast<int8_t>(logRateIndex) + delta + kLogRateCount) % kLogRateCount);
      break;
    case kMenuVolume:
      volumeLevel = static_cast<VolumeLevel>((static_cast<int8_t>(volumeLevel) + delta + kVolumeCount) % kVolumeCount);
      break;
    case kMenuBuzzer:
      buzzerEnabled = !buzzerEnabled;
      break;
  }
}

void activateSelectedMenuItem() {
  switch (selectedMenuItem) {
    case kMenuGpsLogging:
      gpsLoggingEnabled = !gpsLoggingEnabled;
      break;
    case kMenuGpsDisplay:
      gpsDisplayEnabled = !gpsDisplayEnabled;
      break;
    case kMenuGpsLogRate:
    case kMenuVolume:
      editingMenuItem = !editingMenuItem;
      break;
    case kMenuBuzzer:
      buzzerEnabled = !buzzerEnabled;
      break;
  }
}

void serviceControls() {
  updateButton(backButton);
  updateButton(encoderButton);
  updateButton(confirmButton);

  const int8_t encoderDelta = readEncoderDelta();
  if (encoderDelta != 0) {
    if (editingMenuItem) {
      adjustSelectedValue(encoderDelta);
    } else {
      selectedMenuItem = static_cast<uint8_t>((selectedMenuItem + encoderDelta + kMenuCount) % kMenuCount);
    }
  }

  if (encoderButton.pressedEvent || confirmButton.pressedEvent) {
    activateSelectedMenuItem();
  }

  if (backButton.pressedEvent) {
    editingMenuItem = false;
  }
}

String onOff(bool value) {
  return value ? "On" : "Off";
}

String menuValue(uint8_t item) {
  switch (item) {
    case kMenuGpsLogging:
      return onOff(gpsLoggingEnabled);
    case kMenuGpsDisplay:
      return onOff(gpsDisplayEnabled);
    case kMenuGpsLogRate:
      return String(kLogRateLabels[logRateIndex]);
    case kMenuVolume:
      return String(kVolumeLabels[volumeLevel]);
    case kMenuBuzzer:
      return onOff(buzzerEnabled);
  }
  return "";
}

String menuLabel(uint8_t item) {
  switch (item) {
    case kMenuGpsLogging:
      return "GPS logging";
    case kMenuGpsDisplay:
      return "GPS display";
    case kMenuGpsLogRate:
      return "Log rate";
    case kMenuVolume:
      return "Volume";
    case kMenuBuzzer:
      return "Buzzer";
  }
  return "";
}

void oledText(uint8_t row, const String &text) {
  if (!oledReady) {
    return;
  }
  oled.setCursor(0, row * 10);
  oled.print(text);
}

void updateDisplay(bool force = false) {
  if (!oledReady) {
    return;
  }

  const uint32_t nowMs = millis();
  if (!force && nowMs - lastDisplayMs < kDisplayRefreshMs) {
    return;
  }
  lastDisplayMs = nowMs;

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  String gpsStatus = "GPS ";
  gpsStatus += gps.location.isValid() ? "fix" : "no fix";
  gpsStatus += " sat:";
  gpsStatus += gps.satellites.isValid() ? String(gps.satellites.value()) : "--";
  oledText(0, gpsStatus);

  if (gpsDisplayEnabled && gps.location.isValid()) {
    oledText(1, "Lat " + String(gps.location.lat(), 6));
    oledText(2, "Lng " + String(gps.location.lng(), 6));
  } else {
    oledText(1, "Logging " + onOff(gpsLoggingEnabled) + " SD " + onOff(sdReady));
    oledText(2, "Buzz " + String(activeBuzzerCount()) + "/3 " + onOff(buzzerEnabled));
  }

  const uint8_t firstMenuRow = 3;
  for (uint8_t row = 0; row < 3; row++) {
    const uint8_t item = (selectedMenuItem + row) % kMenuCount;
    String line = item == selectedMenuItem ? ">" : " ";
    line += menuLabel(item);
    line += ": ";
    line += menuValue(item);
    if (item == selectedMenuItem && editingMenuItem) {
      line += " *";
    }
    oledText(firstMenuRow + row, line);
  }

  oled.display();
}

void initDisplay() {
  Wire.begin(kI2cSdaPin, kI2cSclPin);
  oledReady = oled.begin(kOledAddress, true);
  if (oledReady) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);
    oledText(0, "SparkFun Vario");
    oledText(1, "Starting...");
    oled.display();
  }
}

void initSdCard() {
  SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  sdReady = SD.begin(kSdCsPin);
  if (!sdReady) {
    Serial.println("SD init failed");
    return;
  }

  if (!SD.exists(kGpsLogPath)) {
    File file = SD.open(kGpsLogPath, FILE_WRITE);
    if (file) {
      file.println("millis,latitude,longitude,altitude_m,speed_kmph,satellites,hdop");
      file.close();
    }
  }
}

void initWifi() {
  if (!WIFI_NETWORKS_CONFIGURED) {
    wifiReady = false;
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi credentials not configured");
    return;
  }

  WiFi.mode(WIFI_STA);

  for (uint8_t index = 0; index < kWifiNetworkCount; index++) {
    Serial.print("Connecting WiFi: ");
    Serial.println(kWifiNetworks[index].ssid);
    WiFi.begin(kWifiNetworks[index].ssid, kWifiNetworks[index].password);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < kWifiConnectTimeoutMs) {
      serviceBuzzers();
      serviceGps();
      delay(10);
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiReady = true;
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.localIP());
      return;
    }

    WiFi.disconnect(true);
    delay(100);
  }

  wifiReady = false;
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi not connected");
}

void serviceGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
}

void logGpsIfDue() {
  if (!gpsLoggingEnabled || !sdReady || !gps.location.isValid()) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastGpsLogMs < kLogRatesMs[logRateIndex]) {
    return;
  }
  lastGpsLogMs = nowMs;

  File file = SD.open(kGpsLogPath, FILE_APPEND);
  if (!file) {
    sdReady = false;
    Serial.println("GPS log open failed");
    return;
  }

  file.print(nowMs);
  file.print(',');
  file.print(gps.location.lat(), 6);
  file.print(',');
  file.print(gps.location.lng(), 6);
  file.print(',');
  file.print(gps.altitude.isValid() ? gps.altitude.meters() : 0.0, 2);
  file.print(',');
  file.print(gps.speed.isValid() ? gps.speed.kmph() : 0.0, 2);
  file.print(',');
  file.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
  file.print(',');
  file.println(gps.hdop.isValid() ? gps.hdop.hdop() : 0.0, 2);
  file.flush();
  file.close();
}

void setup() {
  Serial.begin(115200);

  pinMode(kQwiicPowerPin, OUTPUT);
  digitalWrite(kQwiicPowerPin, HIGH);

  for (uint8_t pin : kBuzzerPins) {
    pinMode(pin, OUTPUT);
  }
  setBuzzersLow();

  pinMode(kEncoderAPin, INPUT);
  pinMode(kEncoderBPin, INPUT);
  initButton(backButton);
  initButton(encoderButton);
  initButton(confirmButton);

  gpsSerial.begin(kGpsBaud, SERIAL_8N1, kGpsRxPin, kGpsTxPin);
  initDisplay();
  initSdCard();
  initWifi();
  updateDisplay(true);
}

void loop() {
  serviceGps();
  serviceControls();
  serviceBuzzers();
  logGpsIfDue();
  updateDisplay();
}
