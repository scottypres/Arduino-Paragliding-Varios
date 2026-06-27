#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_BMP5xx.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SHT4x.h>
#include <ESP.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPSPlus.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <Wire.h>

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
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
constexpr uint32_t kGpsBaud = 115200;

constexpr uint8_t kBuzzerPins[] = {13, 26, 27};
constexpr uint8_t kBuzzerResolutionBits = 8;
constexpr uint8_t kPixelPin = 25;  // A1 on the SparkFun Thing Plus ESP32 WROOM-C.
constexpr uint16_t kPixelCount = 1;

constexpr uint8_t kEncoderAPin = 39;
constexpr uint8_t kEncoderBPin = 36;
constexpr uint8_t kBackButtonPin = 35;
constexpr uint8_t kEncoderButtonPin = 34;
constexpr uint8_t kConfirmButtonPin = 4;

constexpr uint32_t kDebounceMs = 35;
constexpr uint32_t kDisplayRefreshMs = 100;
constexpr uint32_t kGpsDebugMs = 1000;
constexpr uint32_t kSensorReadMs = 100;
constexpr uint32_t kBmpWarmupMs = 5000;
constexpr uint32_t kBmpRetryMs = 2000;
constexpr uint32_t kBmpPowerUpDelayMs = 50;
constexpr const char *kDataLogPath = "/vario_log.csv";
constexpr const char *kOldDataLogPath = "/vario_log_old.csv";
constexpr uint8_t kBuzzerCount = sizeof(kBuzzerPins) / sizeof(kBuzzerPins[0]);
constexpr uint32_t kWifiConnectTimeoutMs = 7000;
constexpr uint32_t kWifiRetryDelayMs = 5000;
constexpr uint16_t kWebServerPort = 80;
constexpr uint8_t kMaxWifiNetworks = 6;
constexpr uint8_t kOledAddress = 0x3C;
constexpr int8_t kOledResetPin = -1;
constexpr int16_t kOledWidth = 128;
constexpr int16_t kOledHeight = 64;
constexpr const char *kOtaHostname = "sparkfun-vario";
constexpr const char *kOtaPassword = "password";
constexpr const char *kWifiPortalSsid = "SparkFun-Vario-Setup";
constexpr const char *kPrefsNamespace = "vario";
constexpr const char *kPrefAudio = "audio";
constexpr const char *kPrefVolume = "volume";
constexpr const char *kPrefResponse = "response";
constexpr const char *kPrefHasAltitudeZero = "hasZero";
constexpr const char *kPrefAltitudeZeroFt = "zeroFt";
constexpr const char *kPrefWifiInitialized = "wifiInit";
constexpr const char *kPrefWifiCount = "wifiCount";
constexpr const char *kPrefPixelEnabled = "pixEnable";
constexpr const char *kPrefPixelMode = "pixMode";
constexpr const char *kPrefPixelColor = "pixColor";
constexpr float kMetersToFeet = 3.28084F;
constexpr float kFeetToMeters = 1.0F / kMetersToFeet;
constexpr float kSeaLevelPressureHpa = 1013.25F;
constexpr float kAltitudeSmoothingAlpha = 0.25F;
constexpr float kVarioResponseAlpha[] = {0.18F, 0.32F, 0.50F, 0.72F};
const char *const kVarioResponseLabels[] = {"Smooth", "Normal", "Quick", "Direct"};
constexpr uint8_t kVarioResponseCount = sizeof(kVarioResponseAlpha) / sizeof(kVarioResponseAlpha[0]);
constexpr float kLiftThresholdMps = 0.18F;
constexpr float kLiftOffThresholdMps = 0.08F;
constexpr float kSinkThresholdMps = -1.80F;
constexpr float kSinkOffThresholdMps = -1.40F;
constexpr uint32_t kLiftFreqBaseHz = 720;
constexpr uint32_t kLiftFreqIncrementHzPerMps = 170;
constexpr uint32_t kSinkFreqBaseHz = 360;
constexpr uint32_t kSinkFreqDecrementHzPerMps = 45;
constexpr uint32_t kMinToneHz = 130;
constexpr uint32_t kMaxToneHz = 1800;
constexpr uint32_t kToneQuantizeHz = 10;
constexpr uint8_t kDefaultBuzzerVolumePercent = 40;
constexpr uint8_t kMinBuzzerVolumePercent = 5;
constexpr uint8_t kMaxBuzzerVolumePercent = 100;
constexpr uint32_t kBuzzerTestDurationMs = 3000;
constexpr uint32_t kBuzzerTestToneHz = 1000;
constexpr uint32_t kPixelUpdateMs = 50;
constexpr uint32_t kBatteryReadMs = 1000;
constexpr uint16_t kLogTailBytes = 4096;
constexpr int8_t kBatteryVoltagePin = -1;
constexpr float kBatteryVoltageDividerRatio = 2.0F;
constexpr float kBatteryEmptyVolts = 3.30F;
constexpr float kBatteryFullVolts = 4.20F;
constexpr const char *kDataLogHeader =
    "millis,display_altitude_ft,raw_altitude_ft,vario_mps,temp_f,humidity_pct,"
    "gps_fix,latitude,longitude,gps_altitude_m,gps_speed_kmph,gps_sats_used,"
    "gps_sats_seen,gps_hdop,battery_voltage,battery_percent";

struct StoredWifiNetwork {
  String ssid;
  String password;
};

Adafruit_SH1106G oled(kOledWidth, kOledHeight, &Wire, kOledResetPin);
Adafruit_BMP5xx bmp;
Adafruit_SHT4x sht4;
Adafruit_NeoPixel statusPixel(kPixelCount, kPixelPin, NEO_GRB + NEO_KHZ800);
Preferences prefs;
TinyGPSPlus gps;
TinyGPSCustom gpsGpgsvSatellites(gps, "GPGSV", 3);
TinyGPSCustom gpsGngsvSatellites(gps, "GNGSV", 3);
TinyGPSCustom gpsGlgsvSatellites(gps, "GLGSV", 3);
TinyGPSCustom gpsGagsvSatellites(gps, "GAGSV", 3);
TinyGPSCustom gpsBdgsvSatellites(gps, "BDGSV", 3);
HardwareSerial gpsSerial(1);
WebServer webServer(kWebServerPort);
WiFiManager wifiManager;

enum VolumeLevel : uint8_t {
  kVolumeLow = 0,
  kVolumeMedium,
  kVolumeLoud,
  kVolumeCount
};

enum MenuItem : uint8_t {
  kMenuDataLogging = 0,
  kMenuSetAltitudeZero,
  kMenuClearAltitudeZero,
  kMenuAudio,
  kMenuVolume,
  kMenuResponse,
  kMenuToneTest,
  kMenuGpsLogRate,
  kMenuGpsDisplay,
  kMenuForgetWifi,
  kMenuCount
};

enum PixelMode : uint8_t {
  kPixelModeColor = 0,
  kPixelModeRainbow,
  kPixelModeTemperature,
  kPixelModeHumidity,
  kPixelModeAltitude,
  kPixelModeSatellites,
  kPixelModeBattery,
  kPixelModeCount
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
const char *const kBuzzerTestLabels[] = {"B1 pin13", "B2 pin26", "B3 pin27", "All"};
constexpr uint8_t kLogRateCount = sizeof(kLogRatesMs) / sizeof(kLogRatesMs[0]);
constexpr uint8_t kBuzzerTestTargetCount = sizeof(kBuzzerTestLabels) / sizeof(kBuzzerTestLabels[0]);

bool oledReady = false;
bool sdReady = false;
bool bmpReady = false;
bool shtReady = false;
bool dataLoggingEnabled = true;
bool gpsDisplayEnabled = false;
bool audioEnabled = true;
bool wifiReady = false;
bool otaReady = false;
bool wifiAttemptActive = false;
bool webServerReady = false;
bool webServerRoutesConfigured = false;
bool wifiPortalActive = false;
bool altitudeFilterInitialized = false;
bool bmpWarmupComplete = false;
bool altitudeZeroSaved = false;
bool varioRateInitialized = false;
bool liftAudioActive = false;
bool sinkAudioActive = false;
bool liftBeepOn = false;
bool toneTestActive = false;
bool editingMenuItem = false;
bool pixelEnabled = false;
uint8_t selectedMenuItem = kMenuDataLogging;
uint8_t logRateIndex = 2;
VolumeLevel volumeLevel = kVolumeLow;
uint8_t buzzerVolumePercent = kDefaultBuzzerVolumePercent;
uint8_t varioResponseIndex = 1;
uint8_t toneTestPatternIndex = 0;
uint8_t buzzerTestTargetIndex = 0;
uint8_t wifiNetworkCount = 0;
uint8_t wifiAttemptIndex = 0;
uint8_t pixelMode = kPixelModeColor;
uint8_t bmpAddress = 0;
uint32_t pixelColor = 0x00FF00;

uint32_t lastDisplayMs = 0;
uint32_t lastGpsLogMs = 0;
uint32_t lastGpsDebugMs = 0;
uint32_t lastSensorReadMs = 0;
uint32_t lastBmpInitAttemptMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t wifiAttemptStartMs = 0;
uint32_t lastPixelUpdateMs = 0;
uint32_t lastBatteryReadMs = 0;
uint32_t bmpWarmupStartMs = 0;
uint32_t lastVarioRateUpdateMs = 0;
uint32_t liftPhaseStartMs = 0;
uint32_t currentToneHz = 0;
uint32_t toneTestStartMs = 0;
uint8_t currentToneMask = 0;
uint16_t rainbowHue = 0;

float altitudeFt = 0.0F;
float smoothedAltitudeFt = 0.0F;
float previousVarioAltitudeFt = 0.0F;
float baselineSmoothedAltitudeFt = 0.0F;
float displayAltitudeFt = 0.0F;
float verticalSpeedMps = 0.0F;
float temperatureF = NAN;
float humidityPercent = NAN;
float batteryVoltage = NAN;
float batteryPercent = NAN;
String connectedWifiSsid;
StoredWifiNetwork wifiNetworks[kMaxWifiNetworks];

void initOta();
void startWebServer();
void startWifiPortal();
void forgetWifiAndStartPortal();

void setBuzzersLow() {
  for (uint8_t pin : kBuzzerPins) {
    digitalWrite(pin, LOW);
  }
}

uint8_t activeBuzzerCount() {
  return static_cast<uint8_t>(volumeLevel) + 1;
}

uint8_t activeBuzzerMask() {
  uint8_t mask = 0;
  const uint8_t count = activeBuzzerCount();
  for (uint8_t index = 0; index < kBuzzerCount; index++) {
    if (index < count) {
      mask |= (1U << index);
    }
  }
  return mask;
}

float clampFloat(float value, float low, float high) {
  return min(max(value, low), high);
}

uint32_t clampFrequency(uint32_t value) {
  return min(max(value, kMinToneHz), kMaxToneHz);
}

uint32_t quantizeFrequency(uint32_t value) {
  value = clampFrequency(value);
  return ((value + kToneQuantizeHz / 2) / kToneQuantizeHz) * kToneQuantizeHz;
}

uint8_t buzzerDuty() {
  const uint8_t volume = constrain(buzzerVolumePercent,
                                   kMinBuzzerVolumePercent,
                                   kMaxBuzzerVolumePercent);
  uint16_t duty = static_cast<uint16_t>(volume) * 255U / 100U;
  return static_cast<uint8_t>(constrain(duty, 1U, 255U));
}

void setToneMask(uint32_t frequencyHz, uint8_t buzzerMask, bool honorAudioSetting = true) {
  if (honorAudioSetting && !audioEnabled) {
    frequencyHz = 0;
  }

  frequencyHz = frequencyHz > 0 ? quantizeFrequency(frequencyHz) : 0;
  if (frequencyHz == 0) {
    buzzerMask = 0;
  }

  if (frequencyHz == currentToneHz && buzzerMask == currentToneMask) {
    return;
  }

  for (uint8_t index = 0; index < kBuzzerCount; index++) {
    const bool active = frequencyHz > 0 && (buzzerMask & (1U << index));
    ledcWriteTone(kBuzzerPins[index], active ? frequencyHz : 0);
    ledcWrite(kBuzzerPins[index], active ? buzzerDuty() : 0);
    if (!active) {
      digitalWrite(kBuzzerPins[index], LOW);
    }
  }
  currentToneHz = frequencyHz;
  currentToneMask = buzzerMask;
}

void setTone(uint32_t frequencyHz) {
  setToneMask(frequencyHz, activeBuzzerMask());
}

void startBuzzers() {
  for (uint8_t pin : kBuzzerPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    if (!ledcAttach(pin, kLiftFreqBaseHz, kBuzzerResolutionBits)) {
      Serial.print("Buzzer PWM setup failed on pin ");
      Serial.println(pin);
    }
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
    case kMenuVolume:
      buzzerVolumePercent = constrain(static_cast<int>(buzzerVolumePercent) + delta * 5,
                                      kMinBuzzerVolumePercent,
                                      kMaxBuzzerVolumePercent);
      prefs.putUChar(kPrefVolume, buzzerVolumePercent);
      if (currentToneHz > 0) {
        for (uint8_t index = 0; index < activeBuzzerCount(); index++) {
          ledcWrite(kBuzzerPins[index], buzzerDuty());
        }
      }
      break;
    case kMenuResponse:
      varioResponseIndex = static_cast<uint8_t>((static_cast<int8_t>(varioResponseIndex) + delta + kVarioResponseCount) % kVarioResponseCount);
      prefs.putUChar(kPrefResponse, varioResponseIndex);
      break;
    case kMenuGpsLogRate:
      logRateIndex = static_cast<uint8_t>((static_cast<int8_t>(logRateIndex) + delta + kLogRateCount) % kLogRateCount);
      break;
    case kMenuToneTest:
      buzzerTestTargetIndex = static_cast<uint8_t>((static_cast<int8_t>(buzzerTestTargetIndex) + delta + kBuzzerTestTargetCount) % kBuzzerTestTargetCount);
      if (toneTestActive) {
        startToneTest();
      }
      break;
    default:
      break;
  }
}

void saveAltitudeZero() {
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  altitudeZeroSaved = true;
  prefs.putFloat(kPrefAltitudeZeroFt, baselineSmoothedAltitudeFt);
  prefs.putBool(kPrefHasAltitudeZero, true);
  Serial.print("Altitude zero saved at ");
  Serial.print(baselineSmoothedAltitudeFt, 2);
  Serial.println(" ft");
}

void clearAltitudeZero() {
  prefs.remove(kPrefAltitudeZeroFt);
  prefs.putBool(kPrefHasAltitudeZero, false);
  altitudeZeroSaved = false;
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  Serial.println("Saved altitude zero cleared");
}

bool i2cAddressPresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void printI2cScan() {
  Serial.print("I2C scan:");
  bool found = false;
  for (uint8_t address = 0x08; address <= 0x77; address++) {
    if (i2cAddressPresent(address)) {
      Serial.print(" 0x");
      if (address < 0x10) {
        Serial.print('0');
      }
      Serial.print(address, HEX);
      found = true;
    }
  }
  if (!found) {
    Serial.print(" none");
  }
  Serial.println();
}

bool tryBmpAddress(uint8_t address) {
  if (!i2cAddressPresent(address)) {
    return false;
  }

  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    delay(kBmpPowerUpDelayMs);
    if (bmp.begin(address, &Wire)) {
      bmpAddress = address;
      return true;
    }
  }

  Serial.print("BMP answers at 0x");
  if (address < 0x10) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
  Serial.println(" but init failed");
  return false;
}

void startToneTest() {
  toneTestStartMs = millis();
  toneTestActive = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  setToneMask(0, 0, false);
}

uint8_t buzzerTestMask() {
  if (buzzerTestTargetIndex < kBuzzerCount) {
    return 1U << buzzerTestTargetIndex;
  }
  return (1U << kBuzzerCount) - 1U;
}

void activateSelectedMenuItem() {
  switch (selectedMenuItem) {
    case kMenuDataLogging:
      dataLoggingEnabled = !dataLoggingEnabled;
      break;
    case kMenuSetAltitudeZero:
      saveAltitudeZero();
      break;
    case kMenuClearAltitudeZero:
      clearAltitudeZero();
      break;
    case kMenuAudio:
      audioEnabled = !audioEnabled;
      prefs.putBool(kPrefAudio, audioEnabled);
      if (!audioEnabled) {
        toneTestActive = false;
        setTone(0);
      }
      break;
    case kMenuGpsDisplay:
      gpsDisplayEnabled = !gpsDisplayEnabled;
      break;
    case kMenuForgetWifi:
      forgetWifiAndStartPortal();
      break;
    case kMenuVolume:
    case kMenuResponse:
    case kMenuGpsLogRate:
      editingMenuItem = !editingMenuItem;
      break;
    case kMenuToneTest:
      startToneTest();
      editingMenuItem = true;
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
    case kMenuDataLogging:
      return onOff(dataLoggingEnabled);
    case kMenuSetAltitudeZero:
      return String(displayAltitudeFt, 1) + " ft";
    case kMenuClearAltitudeZero:
      return altitudeZeroSaved ? "Saved" : "None";
    case kMenuAudio:
      return onOff(audioEnabled);
    case kMenuVolume:
      return String(buzzerVolumePercent) + "%";
    case kMenuResponse:
      return String(kVarioResponseLabels[varioResponseIndex]);
    case kMenuToneTest:
      return String(toneTestActive ? "Playing " : "") + kBuzzerTestLabels[buzzerTestTargetIndex];
    case kMenuGpsLogRate:
      return String(kLogRateLabels[logRateIndex]);
    case kMenuGpsDisplay:
      return onOff(gpsDisplayEnabled);
    case kMenuForgetWifi:
      return wifiNetworkCount == 0 ? "Cleared" : String(wifiNetworkCount) + " saved";
  }
  return "";
}

String menuLabel(uint8_t item) {
  switch (item) {
    case kMenuDataLogging:
      return "SD logging";
    case kMenuSetAltitudeZero:
      return "Set zero";
    case kMenuClearAltitudeZero:
      return "Clear zero";
    case kMenuAudio:
      return "Vario audio";
    case kMenuVolume:
      return "Volume";
    case kMenuResponse:
      return "Response";
    case kMenuToneTest:
      return "Buzzer test";
    case kMenuGpsLogRate:
      return "Log rate";
    case kMenuGpsDisplay:
      return "GPS display";
    case kMenuForgetWifi:
      return "Forget WiFi";
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

int gpsCustomInt(TinyGPSCustom &field) {
  if (!field.isValid()) {
    return -1;
  }
  return atoi(field.value());
}

int gpsSatellitesUsed() {
  return gps.satellites.isValid() ? static_cast<int>(gps.satellites.value()) : -1;
}

int gpsSatellitesSeen() {
  int seen = gpsCustomInt(gpsGngsvSatellites);
  if (seen >= 0) {
    return seen;
  }

  seen = max(gpsCustomInt(gpsGpgsvSatellites), gpsCustomInt(gpsGlgsvSatellites));
  seen = max(seen, gpsCustomInt(gpsGagsvSatellites));
  seen = max(seen, gpsCustomInt(gpsBdgsvSatellites));
  return seen;
}

String gpsSatSummary() {
  const int used = gpsSatellitesUsed();
  const int seen = gpsSatellitesSeen();
  String value = used >= 0 ? String(used) : String("--");
  value += "/";
  value += seen >= 0 ? String(seen) : String("--");
  return value;
}

String floatOrDash(float value, uint8_t decimals, const char *suffix = "") {
  if (isnan(value)) {
    return String("--") + suffix;
  }
  return String(value, static_cast<unsigned int>(decimals)) + suffix;
}

String batterySummary() {
  if (isnan(batteryVoltage)) {
    return "not wired";
  }
  return String(batteryVoltage, 2) + "V " + String(batteryPercent, 0) + "%";
}

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (uint16_t index = 0; index < value.length(); index++) {
    const char c = value[index];
    if (c == '&') {
      escaped += F("&amp;");
    } else if (c == '<') {
      escaped += F("&lt;");
    } else if (c == '>') {
      escaped += F("&gt;");
    } else if (c == '"') {
      escaped += F("&quot;");
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (uint16_t index = 0; index < value.length(); index++) {
    const char c = value[index];
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += F("\\n");
    } else if (c == '\r') {
      escaped += F("\\r");
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String jsonFloat(float value, uint8_t decimals) {
  if (isnan(value)) {
    return F("null");
  }
  return String(value, static_cast<unsigned int>(decimals));
}

String wifiKey(const char *prefix, uint8_t index) {
  return String(prefix) + String(index);
}

const char *pixelModeName(uint8_t mode) {
  switch (mode) {
    case kPixelModeColor:
      return "color";
    case kPixelModeRainbow:
      return "rainbow";
    case kPixelModeTemperature:
      return "temp";
    case kPixelModeHumidity:
      return "humidity";
    case kPixelModeAltitude:
      return "altitude";
    case kPixelModeSatellites:
      return "satellites";
    case kPixelModeBattery:
      return "battery";
  }
  return "color";
}

uint8_t pixelModeFromName(const String &mode) {
  if (mode == "rainbow") {
    return kPixelModeRainbow;
  }
  if (mode == "temp") {
    return kPixelModeTemperature;
  }
  if (mode == "humidity") {
    return kPixelModeHumidity;
  }
  if (mode == "altitude") {
    return kPixelModeAltitude;
  }
  if (mode == "satellites") {
    return kPixelModeSatellites;
  }
  if (mode == "battery") {
    return kPixelModeBattery;
  }
  return kPixelModeColor;
}

String colorToHex(uint32_t color) {
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
           static_cast<unsigned int>((color >> 16) & 0xFF),
           static_cast<unsigned int>((color >> 8) & 0xFF),
           static_cast<unsigned int>(color & 0xFF));
  return String(buffer);
}

uint32_t parseHtmlColor(const String &value, uint32_t fallback) {
  if (value.length() != 7 || value[0] != '#') {
    return fallback;
  }

  char *end = nullptr;
  const uint32_t parsed = strtoul(value.c_str() + 1, &end, 16);
  if (end == value.c_str() + 7) {
    return parsed & 0xFFFFFF;
  }
  return fallback;
}

uint32_t gradientColor(float value, float low, float high) {
  if (isnan(value)) {
    return statusPixel.Color(20, 20, 20);
  }
  const float normalized = clampFloat((value - low) / (high - low), 0.0F, 1.0F);
  const uint16_t hue = static_cast<uint16_t>((1.0F - normalized) * 43690.0F);
  return statusPixel.gamma32(statusPixel.ColorHSV(hue, 255, 80));
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

  if (gpsDisplayEnabled) {
    oledText(0, String("GPS ") + (gps.location.isValid() ? "fix" : "no fix") + " sat " + gpsSatSummary());
    if (gps.location.isValid()) {
      oledText(1, "Lat " + String(gps.location.lat(), 6));
      oledText(2, "Lng " + String(gps.location.lng(), 6));
    } else {
      oledText(1, "Sat fixed/seen " + gpsSatSummary());
      String sensorLine = "BMP ";
      sensorLine += bmpReady ? String("0x") + String(bmpAddress, HEX) : String("Off");
      sensorLine += " SHT ";
      sensorLine += onOff(shtReady);
      sensorLine += " SD ";
      sensorLine += onOff(sdReady);
      oledText(2, sensorLine);
    }
  } else {
    oled.setTextSize(2);
    oled.setCursor(0, 0);
    oled.print(displayAltitudeFt, 1);
    oled.println(" ft");
    oled.setTextSize(1);
    oledText(2, "Vario " + String(verticalSpeedMps, 2) + " m/s Sat " + gpsSatSummary());
  }

  oledText(3, "T " + floatOrDash(temperatureF, 1, "F") + " H " +
                floatOrDash(humidityPercent, 0, "%") + " B " +
                (isnan(batteryVoltage) ? String("--") : String(batteryVoltage, 2) + "V"));
  oledText(4, "SD " + onOff(sdReady && dataLoggingEnabled) + " WiFi " +
                (wifiReady ? WiFi.localIP().toString() : (wifiPortalActive ? String(kWifiPortalSsid) : String("off"))));

  const uint8_t firstMenuRow = 5;
  for (uint8_t row = 0; row < 1; row++) {
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

  if (!SD.exists(kDataLogPath)) {
    File file = SD.open(kDataLogPath, FILE_WRITE);
    if (file) {
      file.println(kDataLogHeader);
      file.close();
    }
    return;
  }

  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return;
  }
  const String firstLine = file.readStringUntil('\n');
  file.close();
  if (firstLine.indexOf("battery_voltage") >= 0) {
    return;
  }

  if (!SD.exists(kOldDataLogPath)) {
    SD.rename(kDataLogPath, kOldDataLogPath);
  } else {
    SD.remove(kDataLogPath);
  }

  file = SD.open(kDataLogPath, FILE_WRITE);
  if (file) {
    file.println(kDataLogHeader);
    file.close();
  }
}

void saveWifiNetworks() {
  wifiNetworkCount = min(wifiNetworkCount, kMaxWifiNetworks);
  prefs.putUChar(kPrefWifiCount, wifiNetworkCount);
  prefs.putBool(kPrefWifiInitialized, true);
  for (uint8_t index = 0; index < kMaxWifiNetworks; index++) {
    if (index < wifiNetworkCount) {
      prefs.putString(wifiKey("wifiS", index).c_str(), wifiNetworks[index].ssid);
      prefs.putString(wifiKey("wifiP", index).c_str(), wifiNetworks[index].password);
    } else {
      prefs.remove(wifiKey("wifiS", index).c_str());
      prefs.remove(wifiKey("wifiP", index).c_str());
    }
  }
}

bool addWifiNetwork(const String &ssid, const String &password) {
  if (ssid.length() == 0) {
    return false;
  }

  for (uint8_t index = 0; index < wifiNetworkCount; index++) {
    if (wifiNetworks[index].ssid == ssid) {
      wifiNetworks[index].password = password;
      saveWifiNetworks();
      return true;
    }
  }

  if (wifiNetworkCount >= kMaxWifiNetworks) {
    return false;
  }

  wifiNetworks[wifiNetworkCount].ssid = ssid;
  wifiNetworks[wifiNetworkCount].password = password;
  wifiNetworkCount++;
  saveWifiNetworks();
  return true;
}

bool removeWifiNetwork(uint8_t removeIndex) {
  if (removeIndex >= wifiNetworkCount) {
    return false;
  }

  for (uint8_t index = removeIndex; index + 1 < wifiNetworkCount; index++) {
    wifiNetworks[index] = wifiNetworks[index + 1];
  }
  wifiNetworkCount--;
  saveWifiNetworks();
  if (wifiAttemptIndex >= wifiNetworkCount) {
    wifiAttemptIndex = 0;
  }
  return true;
}

void clearWifiNetworks() {
  wifiNetworkCount = 0;
  wifiAttemptIndex = 0;
  connectedWifiSsid = "";
  saveWifiNetworks();
  wifiManager.resetSettings();
  WiFi.disconnect(false, false);
  wifiReady = false;
  wifiAttemptActive = false;
}

void loadWifiNetworks() {
  wifiNetworkCount = 0;

  if (!prefs.getBool(kPrefWifiInitialized, false)) {
    saveWifiNetworks();
    return;
  }

  const uint8_t storedCount = min(prefs.getUChar(kPrefWifiCount, 0), kMaxWifiNetworks);
  for (uint8_t index = 0; index < storedCount; index++) {
    const String ssid = prefs.getString(wifiKey("wifiS", index).c_str(), "");
    if (ssid.length() == 0) {
      continue;
    }
    wifiNetworks[wifiNetworkCount].ssid = ssid;
    wifiNetworks[wifiNetworkCount].password = prefs.getString(wifiKey("wifiP", index).c_str(), "");
    wifiNetworkCount++;
  }
}

void loadSettings() {
  prefs.begin(kPrefsNamespace, false);
  audioEnabled = prefs.getBool(kPrefAudio, true);
  buzzerVolumePercent = constrain(prefs.getUChar(kPrefVolume, kDefaultBuzzerVolumePercent),
                                  kMinBuzzerVolumePercent,
                                  kMaxBuzzerVolumePercent);
  varioResponseIndex = prefs.getUChar(kPrefResponse, varioResponseIndex);
  if (varioResponseIndex >= kVarioResponseCount) {
    varioResponseIndex = 1;
  }
  altitudeZeroSaved = prefs.getBool(kPrefHasAltitudeZero, false);
  baselineSmoothedAltitudeFt = prefs.getFloat(kPrefAltitudeZeroFt, 0.0F);
  pixelEnabled = prefs.getBool(kPrefPixelEnabled, false);
  pixelMode = prefs.getUChar(kPrefPixelMode, kPixelModeColor);
  if (pixelMode >= kPixelModeCount) {
    pixelMode = kPixelModeColor;
  }
  pixelColor = prefs.getUInt(kPrefPixelColor, pixelColor) & 0xFFFFFF;
  loadWifiNetworks();
}

void startBmp() {
  lastBmpInitAttemptMs = millis();
  bmpAddress = 0;
  static bool printedInitialScan = false;
  if (!printedInitialScan) {
    printI2cScan();
    printedInitialScan = true;
  }
  bmpReady = tryBmpAddress(BMP5XX_ALTERNATIVE_ADDRESS);
  if (!bmpReady) {
    bmpReady = tryBmpAddress(BMP5XX_DEFAULT_ADDRESS);
  }

  if (bmpReady) {
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_2X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
    bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
    bmp.enablePressure(true);
    bmpWarmupStartMs = millis();
    bmpWarmupComplete = false;
    altitudeFilterInitialized = false;
    varioRateInitialized = false;
    verticalSpeedMps = 0.0F;
    Serial.print("BMP581 ready at 0x");
    if (bmpAddress < 0x10) {
      Serial.print('0');
    }
    Serial.print(bmpAddress, HEX);
    Serial.println("; warming up");
  } else {
    Serial.println("BMP missing; will retry");
  }
}

void startSht() {
  shtReady = sht4.begin(&Wire);
  if (shtReady) {
    sht4.setPrecision(SHT4X_MED_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  } else {
    Serial.println("SHT missing");
  }
}

void initSensors() {
  startBmp();
  startSht();
}

void completeBmpWarmup() {
  if (!altitudeZeroSaved) {
    baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  }
  previousVarioAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = smoothedAltitudeFt - baselineSmoothedAltitudeFt;
  verticalSpeedMps = 0.0F;
  lastVarioRateUpdateMs = millis();
  varioRateInitialized = true;
  bmpWarmupComplete = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  Serial.println(altitudeZeroSaved ? "BMP warmup complete; saved altitude zero restored"
                                   : "BMP warmup complete; using temporary altitude zero");
}

void readSensors() {
  if (!bmpReady && millis() - lastBmpInitAttemptMs >= kBmpRetryMs) {
    startBmp();
  }

  bool bmpReadOk = false;
  float bmpTemperatureF = NAN;
  if (bmpReady && bmp.performReading()) {
    bmpReadOk = true;
    bmpTemperatureF = bmp.temperature * 9.0F / 5.0F + 32.0F;
    const uint32_t now = millis();
    altitudeFt = bmp.readAltitude(kSeaLevelPressureHpa) * kMetersToFeet;

    if (!altitudeFilterInitialized) {
      smoothedAltitudeFt = altitudeFt;
      previousVarioAltitudeFt = smoothedAltitudeFt;
      if (!altitudeZeroSaved) {
        baselineSmoothedAltitudeFt = smoothedAltitudeFt;
      }
      lastVarioRateUpdateMs = now;
      altitudeFilterInitialized = true;
    } else {
      smoothedAltitudeFt += kAltitudeSmoothingAlpha * (altitudeFt - smoothedAltitudeFt);
      if (!bmpWarmupComplete && now - bmpWarmupStartMs >= kBmpWarmupMs) {
        completeBmpWarmup();
      } else if (bmpWarmupComplete) {
        const uint32_t dtMs = now - lastVarioRateUpdateMs;
        if (dtMs > 0) {
          const float dtSeconds = dtMs / 1000.0F;
          const float measuredMps =
              (smoothedAltitudeFt - previousVarioAltitudeFt) * kFeetToMeters / dtSeconds;
          const float responseAlpha = kVarioResponseAlpha[varioResponseIndex];
          verticalSpeedMps += responseAlpha * (measuredMps - verticalSpeedMps);
          previousVarioAltitudeFt = smoothedAltitudeFt;
          lastVarioRateUpdateMs = now;
        }
      }
    }
    displayAltitudeFt = smoothedAltitudeFt - baselineSmoothedAltitudeFt;
  }

  bool shtTempReadOk = false;
  if (shtReady) {
    sensors_event_t humidity;
    sensors_event_t temp;
    if (sht4.getEvent(&humidity, &temp)) {
      temperatureF = temp.temperature * 9.0F / 5.0F + 32.0F;
      humidityPercent = humidity.relative_humidity;
      shtTempReadOk = true;
    }
  }
  if (!shtTempReadOk) {
    temperatureF = bmpReadOk ? bmpTemperatureF : NAN;
    humidityPercent = NAN;
  }
}

void initBatteryMonitor() {
  if (kBatteryVoltagePin >= 0) {
    pinMode(kBatteryVoltagePin, INPUT);
    analogSetPinAttenuation(kBatteryVoltagePin, ADC_11db);
  }
}

void readBatteryIfDue() {
  const uint32_t nowMs = millis();
  if (nowMs - lastBatteryReadMs < kBatteryReadMs) {
    return;
  }
  lastBatteryReadMs = nowMs;

  if (kBatteryVoltagePin < 0) {
    batteryVoltage = NAN;
    batteryPercent = NAN;
    return;
  }

  const uint32_t millivolts = analogReadMilliVolts(kBatteryVoltagePin);
  batteryVoltage = millivolts * kBatteryVoltageDividerRatio / 1000.0F;
  batteryPercent = clampFloat((batteryVoltage - kBatteryEmptyVolts) * 100.0F /
                                  (kBatteryFullVolts - kBatteryEmptyVolts),
                              0.0F,
                              100.0F);
}

void initPixel() {
  statusPixel.begin();
  statusPixel.clear();
  statusPixel.show();
  statusPixel.setBrightness(80);
}

void savePixelSettings() {
  prefs.putBool(kPrefPixelEnabled, pixelEnabled);
  prefs.putUChar(kPrefPixelMode, pixelMode);
  prefs.putUInt(kPrefPixelColor, pixelColor & 0xFFFFFF);
}

void setPixelColor(uint32_t color) {
  statusPixel.setPixelColor(0,
                            static_cast<uint8_t>((color >> 16) & 0xFF),
                            static_cast<uint8_t>((color >> 8) & 0xFF),
                            static_cast<uint8_t>(color & 0xFF));
  statusPixel.show();
}

void servicePixel() {
  const uint32_t nowMs = millis();
  if (nowMs - lastPixelUpdateMs < kPixelUpdateMs) {
    return;
  }
  lastPixelUpdateMs = nowMs;

  if (!pixelEnabled) {
    statusPixel.clear();
    statusPixel.show();
    return;
  }

  uint32_t color = pixelColor;
  switch (pixelMode) {
    case kPixelModeRainbow:
      rainbowHue += 256;
      color = statusPixel.gamma32(statusPixel.ColorHSV(rainbowHue, 255, 80));
      break;
    case kPixelModeTemperature:
      color = gradientColor(temperatureF, 32.0F, 100.0F);
      break;
    case kPixelModeHumidity:
      color = gradientColor(humidityPercent, 0.0F, 100.0F);
      break;
    case kPixelModeAltitude:
      color = gradientColor(displayAltitudeFt, -500.0F, 5000.0F);
      break;
    case kPixelModeSatellites:
      color = gradientColor(static_cast<float>(max(gpsSatellitesUsed(), gpsSatellitesSeen())), 0.0F, 20.0F);
      break;
    case kPixelModeBattery:
      color = gradientColor(batteryPercent, 0.0F, 100.0F);
      break;
    case kPixelModeColor:
    default:
      color = pixelColor;
      break;
  }
  setPixelColor(color);
}

void stopWebServer() {
  if (!webServerReady) {
    return;
  }
  webServer.stop();
  webServerReady = false;
}

void stopWifiPortal() {
  if (!wifiPortalActive) {
    return;
  }
  wifiManager.stopConfigPortal();
  wifiPortalActive = false;
}

void rememberWifiManagerCredentials() {
  const String ssid = wifiManager.getWiFiSSID();
  if (ssid.length() == 0) {
    return;
  }
  addWifiNetwork(ssid, wifiManager.getWiFiPass());
}

void startWifiPortal() {
  if (wifiPortalActive) {
    return;
  }

  stopWebServer();
  wifiAttemptActive = false;
  wifiReady = false;
  otaReady = false;
  connectedWifiSsid = "";
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);

  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(0);
  wifiManager.setConnectTimeout(kWifiConnectTimeoutMs / 1000);
  wifiManager.setBreakAfterConfig(true);
  wifiManager.setClass("invert");
  wifiManager.setSaveConfigCallback([]() {
    rememberWifiManagerCredentials();
  });

  Serial.print("Starting WiFi setup AP: ");
  Serial.println(kWifiPortalSsid);
  wifiManager.startConfigPortal(kWifiPortalSsid);
  wifiPortalActive = wifiManager.getConfigPortalActive();
  if (!wifiPortalActive) {
    Serial.println("WiFi setup portal failed to start");
  }
}

void forgetWifiAndStartPortal() {
  clearWifiNetworks();
  startWifiPortal();
}

void startWifiAttempt(uint8_t index) {
  if (wifiNetworkCount == 0 || index >= wifiNetworkCount) {
    wifiAttemptActive = false;
    startWifiPortal();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(1);
  connectedWifiSsid = "";
  wifiAttemptIndex = index;
  wifiAttemptStartMs = millis();
  wifiAttemptActive = true;

  Serial.print("Connecting WiFi: ");
  Serial.println(wifiNetworks[index].ssid);
  WiFi.begin(wifiNetworks[index].ssid.c_str(), wifiNetworks[index].password.c_str());
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  wifiReady = false;
  otaReady = false;
  wifiAttemptActive = false;
  wifiAttemptIndex = 0;
  lastWifiAttemptMs = 0;

  if (wifiNetworkCount == 0) {
    Serial.println("No saved WiFi networks; starting setup portal");
    startWifiPortal();
    return;
  }

  startWifiAttempt(0);
}

void serviceWifi() {
  const uint32_t nowMs = millis();

  if (wifiPortalActive) {
    wifiManager.process();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiReady) {
      if (wifiPortalActive) {
        rememberWifiManagerCredentials();
        stopWifiPortal();
      }
      wifiReady = true;
      wifiAttemptActive = false;
      connectedWifiSsid = WiFi.SSID();
      Serial.print("WiFi connected: ");
      Serial.print(connectedWifiSsid);
      Serial.print(" ");
      Serial.println(WiFi.localIP());
      initOta();
      startWebServer();
    }
    return;
  }

  if (wifiReady) {
    wifiReady = false;
    otaReady = false;
    connectedWifiSsid = "";
    lastWifiAttemptMs = nowMs;
    Serial.println("WiFi disconnected");
  }

  if (wifiNetworkCount == 0) {
    if (!wifiPortalActive) {
      startWifiPortal();
    }
    return;
  }

  if (wifiAttemptActive) {
    if (nowMs - wifiAttemptStartMs < kWifiConnectTimeoutMs) {
      return;
    }
    Serial.print("WiFi timeout: ");
    Serial.println(wifiNetworks[wifiAttemptIndex].ssid);
    WiFi.disconnect(false, false);
    wifiAttemptActive = false;
    const uint8_t nextIndex = static_cast<uint8_t>((wifiAttemptIndex + 1) % wifiNetworkCount);
    if (nextIndex == 0) {
      startWifiPortal();
    }
    wifiAttemptIndex = nextIndex;
    lastWifiAttemptMs = nowMs;
    return;
  }

  if (!wifiPortalActive && nowMs - lastWifiAttemptMs >= kWifiRetryDelayMs) {
    startWifiAttempt(wifiAttemptIndex);
  }
}

void redirectHome() {
  webServer.sendHeader("Location", "/", true);
  webServer.send(303, "text/plain", "See /");
}

void writeLogHeader() {
  File file = SD.open(kDataLogPath, FILE_WRITE);
  if (file) {
    file.println(kDataLogHeader);
    file.close();
  }
}

void resetDataLog() {
  if (!sdReady) {
    return;
  }
  SD.remove(kDataLogPath);
  writeLogHeader();
}

bool deleteRecursive(const String &path) {
  File entry = SD.open(path);
  if (!entry) {
    return false;
  }

  if (!entry.isDirectory()) {
    entry.close();
    return SD.remove(path);
  }

  File child = entry.openNextFile();
  while (child) {
    const String childPath = child.path();
    const bool childIsDirectory = child.isDirectory();
    child.close();
    if (childIsDirectory) {
      deleteRecursive(childPath);
    } else {
      SD.remove(childPath);
    }
    child = entry.openNextFile();
  }
  entry.close();

  if (path != "/") {
    return SD.rmdir(path);
  }
  return true;
}

uint32_t logFileSize() {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    return 0;
  }
  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return 0;
  }
  const uint32_t size = file.size();
  file.close();
  return size;
}

String logTail() {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    return "No log file";
  }

  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return "Log open failed";
  }

  const size_t size = file.size();
  const size_t start = size > kLogTailBytes ? size - kLogTailBytes : 0;
  file.seek(start);
  if (start > 0) {
    file.readStringUntil('\n');
  }
  String tail = file.readString();
  file.close();
  return tail;
}

String dataJson() {
  String json;
  json.reserve(768);
  json += "{";
  json += "\"uptime_ms\":" + String(millis());
  json += ",\"altitude_ft\":" + jsonFloat(displayAltitudeFt, 2);
  json += ",\"raw_altitude_ft\":" + jsonFloat(altitudeFt, 2);
  json += ",\"vario_mps\":" + jsonFloat(verticalSpeedMps, 2);
  json += ",\"temp_f\":" + jsonFloat(temperatureF, 1);
  json += ",\"humidity_pct\":" + jsonFloat(humidityPercent, 1);
  json += ",\"gps_fix\":" + String(gps.location.isValid() ? "true" : "false");
  json += ",\"gps_sats_used\":" + String(gpsSatellitesUsed());
  json += ",\"gps_sats_seen\":" + String(gpsSatellitesSeen());
  json += ",\"gps_hdop\":" + (gps.hdop.isValid() ? String(gps.hdop.hdop(), 2) : String("null"));
  json += ",\"latitude\":" + (gps.location.isValid() ? String(gps.location.lat(), 6) : String("null"));
  json += ",\"longitude\":" + (gps.location.isValid() ? String(gps.location.lng(), 6) : String("null"));
  json += ",\"battery_voltage\":" + jsonFloat(batteryVoltage, 2);
  json += ",\"battery_percent\":" + jsonFloat(batteryPercent, 0);
  json += ",\"sd_ready\":" + String(sdReady ? "true" : "false");
  json += ",\"logging_enabled\":" + String(dataLoggingEnabled ? "true" : "false");
  json += ",\"log_size\":" + String(logFileSize());
  json += ",\"wifi_ready\":" + String(wifiReady ? "true" : "false");
  json += ",\"wifi_portal\":" + String(wifiPortalActive ? "true" : "false");
  json += ",\"wifi_portal_ssid\":\"" + String(kWifiPortalSsid) + "\"";
  json += ",\"wifi_ssid\":\"" + jsonEscape(connectedWifiSsid) + "\"";
  json += ",\"ip\":\"" + (wifiReady ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"pixel_enabled\":" + String(pixelEnabled ? "true" : "false");
  json += ",\"pixel_mode\":\"" + String(pixelModeName(pixelMode)) + "\"";
  json += "}";
  return json;
}

void handleDataJson() {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", dataJson());
}

String optionForPixelMode(uint8_t mode, const char *label) {
  String option = "<option value=\"";
  option += pixelModeName(mode);
  option += "\"";
  if (pixelMode == mode) {
    option += " selected";
  }
  option += ">";
  option += label;
  option += "</option>";
  return option;
}

void handleRoot() {
  String page;
  page.reserve(9500);
  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>SparkFun Vario</title><style>");
  page += F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:0;background:#111;color:#eee}");
  page += F("main{max-width:960px;margin:auto;padding:16px}h1{font-size:24px;margin:0 0 12px}");
  page += F("section{border:1px solid #333;border-radius:8px;padding:12px;margin:12px 0;background:#181818}");
  page += F(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px}");
  page += F(".metric{background:#222;border:1px solid #333;border-radius:6px;padding:8px}.metric b{display:block;font-size:12px;color:#aaa}.metric span{font-size:20px}");
  page += F("label{display:block;margin:8px 0 4px;color:#bbb}input,select,button{font:inherit}input,select{box-sizing:border-box;width:100%;padding:7px;background:#0d0d0d;color:#eee;border:1px solid #444;border-radius:4px}");
  page += F("button{padding:8px 10px;margin:4px 4px 4px 0;border:1px solid #555;border-radius:4px;background:#2b2b2b;color:#fff}button.danger{background:#5a1f1f;border-color:#8b3535}");
  page += F("form{margin:8px 0}.row{display:flex;gap:8px;align-items:end;flex-wrap:wrap}.row>*{flex:1;min-width:160px}");
  page += F("pre{white-space:pre-wrap;word-break:break-word;max-height:320px;overflow:auto;background:#080808;border:1px solid #333;border-radius:6px;padding:8px}");
  page += F("a{color:#8cc8ff}.wifi{display:flex;gap:8px;align-items:center;justify-content:space-between;border-top:1px solid #333;padding:6px 0}");
  page += F("</style></head><body><main><h1>SparkFun Vario</h1>");

  page += F("<section><div class='grid' id='metrics'>");
  page += F("<div class='metric'><b>Altitude</b><span id='alt'>--</span></div>");
  page += F("<div class='metric'><b>Vario</b><span id='vario'>--</span></div>");
  page += F("<div class='metric'><b>Temp</b><span id='temp'>--</span></div>");
  page += F("<div class='metric'><b>Humidity</b><span id='hum'>--</span></div>");
  page += F("<div class='metric'><b>Sat fixed / seen</b><span id='sats'>--</span></div>");
  page += F("<div class='metric'><b>Battery</b><span id='bat'>--</span></div>");
  page += F("<div class='metric'><b>WiFi</b><span id='wifi'>--</span></div>");
  page += F("<div class='metric'><b>Log</b><span id='logsize'>--</span></div>");
  page += F("</div><p><a href='/log'>View CSV</a> &nbsp; <a href='/download'>Download CSV</a></p></section>");

  page += F("<section><h2>WiFi</h2>");
  for (uint8_t index = 0; index < wifiNetworkCount; index++) {
    page += F("<div class='wifi'><span>");
    page += htmlEscape(wifiNetworks[index].ssid);
    page += F("</span><form method='post' action='/wifi/forget'><input type='hidden' name='i' value='");
    page += String(index);
    page += F("'><button class='danger'>Forget</button></form></div>");
  }
  page += F("<form method='post' action='/wifi/add'><div class='row'><div><label>SSID</label><input name='ssid' maxlength='63'></div><div><label>Password</label><input name='pass' type='password' maxlength='63'></div><div><button>Add / Update</button></div></div></form>");
  page += F("<form method='post' action='/wifi/forget-all'><button class='danger'>Forget all WiFi</button></form></section>");

  page += F("<section><h2>Pixel</h2><form method='post' action='/pixel'><label><input style='width:auto' type='checkbox' name='enable' value='1'");
  if (pixelEnabled) {
    page += F(" checked");
  }
  page += F("> Enable pixel</label><div class='row'><div><label>Mode</label><select name='mode'>");
  page += optionForPixelMode(kPixelModeColor, "Fixed color");
  page += optionForPixelMode(kPixelModeRainbow, "Rainbow");
  page += optionForPixelMode(kPixelModeTemperature, "Temperature");
  page += optionForPixelMode(kPixelModeHumidity, "Humidity");
  page += optionForPixelMode(kPixelModeAltitude, "Altitude");
  page += optionForPixelMode(kPixelModeSatellites, "Satellites");
  page += optionForPixelMode(kPixelModeBattery, "Battery");
  page += F("</select></div><div><label>Color</label><input name='color' type='color' value='");
  page += colorToHex(pixelColor);
  page += F("'></div><div><button>Save pixel</button></div></div></form></section>");

  page += F("<section><h2>SD card</h2>");
  page += F("<form method='post' action='/sd/clear'><button class='danger'>Clear log</button></form>");
  page += F("<form method='post' action='/sd/format'><button class='danger'>Clear / format SD</button></form>");
  page += F("<pre id='tail'>Loading log...</pre></section>");

  page += F("<section><h2>Board</h2><form method='post' action='/reset'><button class='danger'>Reset board</button></form></section>");

  page += F("<script>");
  page += F("function fmt(v,d,u){return v===null?'--':Number(v).toFixed(d)+u}");
  page += F("async function data(){let r=await fetch('/data.json',{cache:'no-store'});let j=await r.json();");
  page += F("alt.textContent=fmt(j.altitude_ft,1,' ft');vario.textContent=fmt(j.vario_mps,2,' m/s');");
  page += F("temp.textContent=fmt(j.temp_f,1,' F');hum.textContent=fmt(j.humidity_pct,0,' %');");
  page += F("sats.textContent=j.gps_sats_used+'/'+j.gps_sats_seen;");
  page += F("bat.textContent=j.battery_voltage===null?'not wired':fmt(j.battery_voltage,2,' V')+' '+fmt(j.battery_percent,0,' %');");
  page += F("wifi.textContent=j.wifi_ready?(j.wifi_ssid+' '+j.ip):(j.wifi_portal?'setup AP '+j.wifi_portal_ssid:'connecting');logsize.textContent=j.sd_ready?j.log_size+' bytes':'SD off'}");
  page += F("async function tailLog(){let r=await fetch('/tail',{cache:'no-store'});tail.textContent=await r.text()}");
  page += F("data();tailLog();setInterval(data,1000);setInterval(tailLog,2000);");
  page += F("</script></main></body></html>");
  webServer.send(200, "text/html", page);
}

void handleLogView() {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    webServer.send(404, "text/plain", "No log file");
    return;
  }
  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    webServer.send(500, "text/plain", "Log open failed");
    return;
  }
  webServer.streamFile(file, "text/csv");
  file.close();
}

void handleLogDownload() {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    webServer.send(404, "text/plain", "No log file");
    return;
  }
  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    webServer.send(500, "text/plain", "Log open failed");
    return;
  }
  webServer.sendHeader("Content-Disposition", "attachment; filename=vario_log.csv");
  webServer.streamFile(file, "text/csv");
  file.close();
}

void handleTail() {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "text/plain", logTail());
}

void handleWifiAdd() {
  addWifiNetwork(webServer.arg("ssid"), webServer.arg("pass"));
  if (!wifiReady && !wifiAttemptActive && wifiNetworkCount > 0) {
    startWifiAttempt(wifiAttemptIndex);
  }
  redirectHome();
}

void handleWifiForget() {
  const uint8_t index = static_cast<uint8_t>(webServer.arg("i").toInt());
  const String removedSsid = index < wifiNetworkCount ? wifiNetworks[index].ssid : String("");
  const bool disconnectRemovedNetwork =
      removedSsid.length() &&
      (connectedWifiSsid == removedSsid ||
       (wifiAttemptActive && wifiAttemptIndex < wifiNetworkCount && wifiNetworks[wifiAttemptIndex].ssid == removedSsid));
  removeWifiNetwork(index);
  if (disconnectRemovedNetwork) {
    WiFi.disconnect(false, false);
    wifiReady = false;
    wifiAttemptActive = false;
  }
  redirectHome();
}

void handleWifiForgetAll() {
  clearWifiNetworks();
  redirectHome();
}

void handlePixelSave() {
  pixelEnabled = webServer.hasArg("enable");
  pixelMode = pixelModeFromName(webServer.arg("mode"));
  pixelColor = parseHtmlColor(webServer.arg("color"), pixelColor);
  savePixelSettings();
  servicePixel();
  redirectHome();
}

void handleSdClear() {
  resetDataLog();
  redirectHome();
}

void handleSdFormat() {
  if (sdReady) {
    deleteRecursive("/");
    writeLogHeader();
  }
  redirectHome();
}

void handleReset() {
  webServer.send(200, "text/plain", "Resetting");
  webServer.client().stop();
  delay(100);
  ESP.restart();
}

void startWebServer() {
  if (webServerReady) {
    return;
  }

  if (!webServerRoutesConfigured) {
    webServer.on("/", HTTP_GET, handleRoot);
    webServer.on("/data.json", HTTP_GET, handleDataJson);
    webServer.on("/log", HTTP_GET, handleLogView);
    webServer.on("/download", HTTP_GET, handleLogDownload);
    webServer.on("/tail", HTTP_GET, handleTail);
    webServer.on("/wifi/add", HTTP_POST, handleWifiAdd);
    webServer.on("/wifi/forget", HTTP_POST, handleWifiForget);
    webServer.on("/wifi/forget-all", HTTP_POST, handleWifiForgetAll);
    webServer.on("/pixel", HTTP_POST, handlePixelSave);
    webServer.on("/sd/clear", HTTP_POST, handleSdClear);
    webServer.on("/sd/format", HTTP_POST, handleSdFormat);
    webServer.on("/reset", HTTP_POST, handleReset);
    webServer.onNotFound([]() {
      webServer.send(404, "text/plain", "Not found");
    });
    webServerRoutesConfigured = true;
  }
  webServer.begin();
  webServerReady = true;
  Serial.print("Web server ready: http://");
  Serial.println(WiFi.localIP());
}

void serviceWebServer() {
  if (webServerReady && wifiReady) {
    webServer.handleClient();
  }
}

void initOta() {
  if (!wifiReady) {
    otaReady = false;
    return;
  }
  if (otaReady) {
    return;
  }

  ArduinoOTA.setHostname(kOtaHostname);
  ArduinoOTA.setPassword(kOtaPassword);
  ArduinoOTA.onStart([]() {
    setTone(0);
    Serial.println("OTA start");
  });
  ArduinoOTA.onEnd([]() {
    setTone(0);
    Serial.println("OTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    setTone(0);
    Serial.printf("OTA error[%u]\n", error);
  });
  ArduinoOTA.begin();
  otaReady = true;
  Serial.print("OTA ready: ");
  Serial.println(kOtaHostname);
}

void serviceOta() {
  if (otaReady) {
    ArduinoOTA.handle();
  }
}

void serviceGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
}

void printGpsDebugIfDue() {
  const uint32_t nowMs = millis();
  if (nowMs - lastGpsDebugMs < kGpsDebugMs) {
    return;
  }
  lastGpsDebugMs = nowMs;

  Serial.print("GPS chars=");
  Serial.print(gps.charsProcessed());
  Serial.print(" sentences=");
  Serial.print(gps.sentencesWithFix());
  Serial.print(" failed_checksum=");
  Serial.print(gps.failedChecksum());
  Serial.print(" fix=");
  Serial.print(gps.location.isValid() ? "yes" : "no");
  Serial.print(" sats=");
  Serial.print(gpsSatellitesUsed());
  Serial.print(" sats_seen=");
  Serial.print(gpsSatellitesSeen());
  Serial.print(" age_ms=");
  Serial.print(gps.location.isValid() ? gps.location.age() : ULONG_MAX);
  Serial.print(" hdop=");
  if (gps.hdop.isValid()) {
    Serial.print(gps.hdop.hdop(), 2);
  } else {
    Serial.print("--");
  }
  Serial.print(" rx_pin=");
  Serial.print(kGpsRxPin);
  Serial.print(" tx_pin=");
  Serial.print(kGpsTxPin);
  Serial.print(" baud=");
  Serial.println(kGpsBaud);
}

bool updateToneTest() {
  if (!toneTestActive) {
    return false;
  }

  const uint32_t elapsedMs = millis() - toneTestStartMs;
  if (elapsedMs >= kBuzzerTestDurationMs) {
    toneTestActive = false;
    setToneMask(0, 0, false);
    return false;
  }

  setToneMask(kBuzzerTestToneHz, buzzerTestMask(), false);
  return true;
}

void updateVarioAudio() {
  const uint32_t now = millis();

  if (updateToneTest()) {
    return;
  }

  if (!audioEnabled || !bmpWarmupComplete || !varioRateInitialized) {
    setTone(0);
    liftAudioActive = false;
    sinkAudioActive = false;
    liftBeepOn = false;
    return;
  }

  if (!liftAudioActive && verticalSpeedMps >= kLiftThresholdMps) {
    liftAudioActive = true;
    sinkAudioActive = false;
    liftBeepOn = true;
    liftPhaseStartMs = now;
  } else if (liftAudioActive && verticalSpeedMps < kLiftOffThresholdMps) {
    liftAudioActive = false;
    liftBeepOn = false;
  }

  if (!sinkAudioActive && verticalSpeedMps <= kSinkThresholdMps) {
    sinkAudioActive = true;
    liftAudioActive = false;
    liftBeepOn = false;
  } else if (sinkAudioActive && verticalSpeedMps > kSinkOffThresholdMps) {
    sinkAudioActive = false;
  }

  if (liftAudioActive) {
    const float climb = clampFloat(verticalSpeedMps, 0.0F, 8.0F);
    const uint32_t frequency =
        quantizeFrequency(kLiftFreqBaseHz + static_cast<uint32_t>(climb * kLiftFreqIncrementHzPerMps));
    const uint32_t beepMs = static_cast<uint32_t>(clampFloat(155.0F - climb * 15.0F, 65.0F, 155.0F));
    const uint32_t pauseMs = static_cast<uint32_t>(clampFloat(560.0F - climb * 75.0F, 95.0F, 560.0F));
    const uint32_t phaseMs = liftBeepOn ? beepMs : pauseMs;

    if (now - liftPhaseStartMs >= phaseMs) {
      liftBeepOn = !liftBeepOn;
      liftPhaseStartMs = now;
    }

    setTone(liftBeepOn ? frequency : 0);
    return;
  }

  if (sinkAudioActive) {
    const float sink = clampFloat(-verticalSpeedMps, 0.0F, 8.0F);
    const uint32_t frequency =
        quantizeFrequency(kSinkFreqBaseHz - min(static_cast<uint32_t>(sink * kSinkFreqDecrementHzPerMps), 180UL));
    setTone(frequency);
    return;
  }

  setTone(0);
}

void printCsvFloat(File &file, float value, uint8_t decimals) {
  if (!isnan(value)) {
    file.print(value, decimals);
  }
}

void logDataIfDue() {
  if (!dataLoggingEnabled || !sdReady) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastGpsLogMs < kLogRatesMs[logRateIndex]) {
    return;
  }
  lastGpsLogMs = nowMs;

  File file = SD.open(kDataLogPath, FILE_APPEND);
  if (!file) {
    sdReady = false;
    Serial.println("Data log open failed");
    return;
  }

  file.print(nowMs);
  file.print(',');
  printCsvFloat(file, displayAltitudeFt, 2);
  file.print(',');
  printCsvFloat(file, altitudeFt, 2);
  file.print(',');
  printCsvFloat(file, verticalSpeedMps, 2);
  file.print(',');
  printCsvFloat(file, temperatureF, 1);
  file.print(',');
  printCsvFloat(file, humidityPercent, 1);
  file.print(',');
  file.print(gps.location.isValid() ? 1 : 0);
  file.print(',');
  if (gps.location.isValid()) {
    file.print(gps.location.lat(), 6);
  }
  file.print(',');
  if (gps.location.isValid()) {
    file.print(gps.location.lng(), 6);
  }
  file.print(',');
  if (gps.altitude.isValid()) {
    file.print(gps.altitude.meters(), 2);
  }
  file.print(',');
  if (gps.speed.isValid()) {
    file.print(gps.speed.kmph(), 2);
  }
  file.print(',');
  file.print(gpsSatellitesUsed());
  file.print(',');
  file.print(gpsSatellitesSeen());
  file.print(',');
  if (gps.hdop.isValid()) {
    file.print(gps.hdop.hdop(), 2);
  }
  file.print(',');
  printCsvFloat(file, batteryVoltage, 2);
  file.print(',');
  printCsvFloat(file, batteryPercent, 0);
  file.println();
  file.flush();
  file.close();
}

void setup() {
  Serial.begin(115200);
  delay(250);
  loadSettings();

  pinMode(kQwiicPowerPin, OUTPUT);
  digitalWrite(kQwiicPowerPin, HIGH);

  startBuzzers();

  pinMode(kEncoderAPin, INPUT);
  pinMode(kEncoderBPin, INPUT);
  initButton(backButton);
  initButton(encoderButton);
  initButton(confirmButton);

  gpsSerial.begin(kGpsBaud, SERIAL_8N1, kGpsRxPin, kGpsTxPin);
  initPixel();
  initBatteryMonitor();
  initDisplay();
  Wire.setClock(400000);
  initSensors();
  readSensors();
  readBatteryIfDue();
  initSdCard();
  initWifi();
  updateDisplay(true);
}

void loop() {
  serviceWifi();
  serviceWebServer();
  serviceOta();
  serviceGps();
  printGpsDebugIfDue();
  serviceControls();
  if (millis() - lastSensorReadMs >= kSensorReadMs) {
    lastSensorReadMs = millis();
    readSensors();
  }
  readBatteryIfDue();
  servicePixel();
  updateVarioAudio();
  logDataIfDue();
  updateDisplay();
}
