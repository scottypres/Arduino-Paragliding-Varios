#pragma once

#include "radio_config.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_BMP5xx.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SHT4x.h>
#include <ESP.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <TinyGPSPlus.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <Wire.h>

// SparkFun Thing Plus ESP32 WROOM-C pin mapping for this carrier board.
constexpr uint8_t kQwiicPowerPin = 0;
constexpr uint8_t kSdCsPin = 5;
constexpr uint8_t kSdSckPin = 18;
constexpr uint8_t kSdMisoPin = 19;
constexpr uint8_t kSdMosiPin = 23;

constexpr uint8_t kI2cSdaPin = 33;
constexpr uint8_t kI2cSclPin = 32;

constexpr uint8_t kBatteryI2cSdaPin = 21;  // Onboard MAX17048 default I2C SDA.
constexpr uint8_t kBatteryI2cSclPin = 22;  // Onboard MAX17048 default I2C SCL.
constexpr uint8_t kGpsRxPin = 21;  // GPS TX on Qwiic SDA into ESP32 RX.
constexpr uint8_t kGpsTxPin = 22;  // GPS RX on Qwiic SCL from ESP32 TX.
constexpr uint32_t kGpsBaud = 115200;

constexpr uint8_t kBuzzerPins[] = {13, 26, 27};
constexpr uint8_t kBuzzerResolutionBits = 8;

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
constexpr const char *kBatteryLogPath = "/battery_log.csv";
constexpr uint8_t kBuzzerCount = sizeof(kBuzzerPins) / sizeof(kBuzzerPins[0]);
constexpr uint32_t kWifiConnectTimeoutMs = 7000;
constexpr uint32_t kWifiRetryDelayMs = 5000;
constexpr uint16_t kWebServerPort = 80;
constexpr uint8_t kMaxWifiNetworks = 6;
constexpr uint8_t kOledAddress = 0x3C;
constexpr int8_t kOledResetPin = -1;
constexpr int16_t kOledWidth = 128;
constexpr int16_t kOledHeight = 64;
constexpr uint8_t kOledWindowCount = 3;  // data screens cycled by the encoder in view mode
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
constexpr const char *kPrefDataLogging = "dataLog";
constexpr const char *kPrefLogRate = "logRate";
constexpr const char *kPrefGpsEnabled = "gpsEn";
constexpr const char *kPrefBluetooth = "btClassic";
constexpr const char *kPrefAltitudeSource = "altSrc";  // false=baro, true=GPS
constexpr const char *kPrefBatteryReadRate = "batRate";
constexpr const char *kPrefLocked = "ctlLock";
constexpr const char *kPrefLockHoldMs = "lockHold";
constexpr const char *kPrefLockBeep = "lockBeep";
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
// Button lock (hold Select+Back to toggle; hold time set in the web UI).
constexpr uint32_t kDefaultLockHoldMs = 3000;
constexpr uint32_t kMinLockHoldMs = 1000;
constexpr uint32_t kMaxLockHoldMs = 10000;
constexpr uint32_t kLockSplashMs = 1500;
constexpr uint32_t kLockBeepHz = 1000;
constexpr uint32_t kLockBeepMs = 100;
constexpr uint32_t kPixelUpdateMs = 50;
constexpr uint16_t kLogTailBytes = 4096;
constexpr uint32_t kBatteryLogIntervalMs = 30000;
constexpr const char *kDataLogHeader =
    "millis,iso_utc,display_altitude_ft,raw_altitude_ft,vario_mps,temp_f,humidity_pct,"
    "gps_fix,latitude,longitude,gps_altitude_m,gps_speed_kmph,gps_sats_used,"
    "gps_sats_seen,gps_hdop,battery_voltage,battery_percent";
constexpr const char *kBatteryLogHeader =
    "millis,elapsed_s,iso_utc,battery_voltage,battery_percent,wifi_enabled,wifi_status,"
    "wifi_ssid,bluetooth_enabled,oled_enabled";

struct StoredWifiNetwork {
  String ssid;
  String password;
};

enum VolumeLevel : uint8_t {
  kVolumeLow = 0,
  kVolumeMedium,
  kVolumeLoud,
  kVolumeCount
};

// Radios are split across firmwares (see radio_config.h): the WiFi build has
// no Bluetooth and the BT build has no WiFi. Hide the menu items a given build
// can't act on so the OLED never offers a dead toggle.
enum MenuItem : uint8_t {
  kMenuLock = 0,
  kMenuOled,
  kMenuDataLogging,
  kMenuSetAltitudeZero,
  kMenuClearAltitudeZero,
  kMenuAudio,
  kMenuVolume,
  kMenuResponse,
  kMenuToneTest,
  kMenuGpsLogRate,
  kMenuBatteryReadRate,
  kMenuGpsEnabled,
  kMenuAltitudeSource,
#ifndef VARIO_DISABLE_BT
  kMenuBluetooth,
#endif
  kMenuBatteryLogging,
#ifndef VARIO_DISABLE_WIFI
  kMenuWifiSetup,
  kMenuForgetWifi,
#endif
  kMenuSwitchFirmware,
  kMenuCount
};

enum BatteryLogMenuItem : uint8_t {
  kBatteryLogMenuStop = 0,
  kBatteryLogMenuWifi,
#ifndef VARIO_DISABLE_BT
  kBatteryLogMenuBluetooth,
#endif
  kBatteryLogMenuOled,
  kBatteryLogMenuCount
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

const uint32_t kLogRatesMs[] = {1000, 2000, 5000, 10000, 30000, 60000};
const char *const kLogRateLabels[] = {"1 sec", "2 sec", "5 sec", "10 sec", "30 sec", "60 sec"};
const uint32_t kBatteryReadRatesMs[] = {5000, 10000, 30000, 60000, 120000, 300000};
const char *const kBatteryReadRateLabels[] = {"5 sec", "10 sec", "30 sec", "60 sec", "2 min", "5 min"};
const char *const kVolumeLabels[] = {"Low", "Medium", "Loud"};
const char *const kBuzzerTestLabels[] = {"B1 pin13", "B2 pin26", "B3 pin27", "All"};
constexpr uint8_t kLogRateCount = sizeof(kLogRatesMs) / sizeof(kLogRatesMs[0]);
constexpr uint8_t kBatteryReadRateCount = sizeof(kBatteryReadRatesMs) / sizeof(kBatteryReadRatesMs[0]);
constexpr uint8_t kBuzzerTestTargetCount = sizeof(kBuzzerTestLabels) / sizeof(kBuzzerTestLabels[0]);

// Shared library objects (defined in globals.cpp).
extern Adafruit_SH1106G oled;
extern Adafruit_BMP5xx bmp;
extern Adafruit_SHT4x sht4;
extern Preferences prefs;
extern TinyGPSPlus gps;
extern TinyGPSCustom gpsGpgsvSatellites;
extern TinyGPSCustom gpsGngsvSatellites;
extern TinyGPSCustom gpsGlgsvSatellites;
extern TinyGPSCustom gpsGagsvSatellites;
extern TinyGPSCustom gpsBdgsvSatellites;
extern HardwareSerial gpsSerial;
extern TwoWire batteryWire;
extern SFE_MAX1704X batteryGauge;
extern AsyncWebServer webServer;
extern DNSServer dnsServer;  // captive-portal DNS while broadcasting the AP

extern Button backButton;
extern Button encoderButton;
extern Button confirmButton;

// Shared mutable state (defined in globals.cpp).
extern bool oledReady;
extern bool oledDisplayEnabled;  // OLED menu toggle; any button press re-enables
extern bool sdReady;
extern bool bmpReady;
extern bool shtReady;
extern bool dataLoggingEnabled;
extern bool gpsEnabled;  // false = GPS UART never claimed, frees the shared pins for the battery gauge
extern bool useGpsAltitude;
extern bool audioEnabled;
extern bool bluetoothEnabled;
extern bool batteryGaugeReady;
extern bool batteryLoggingActive;
extern bool batteryLogWifiEnabled;
extern bool batteryLogBluetoothEnabled;
extern bool batteryLogOledEnabled;
extern bool batteryLogSavedWifiEnabled;
extern bool batteryLogSavedBluetoothEnabled;
extern bool batteryLogSavedOledEnabled;
extern bool wifiReady;
extern bool otaReady;
extern bool wifiAttemptActive;
extern bool webServerReady;
extern bool webServerRoutesConfigured;
extern bool wifiPortalActive;
extern bool altitudeFilterInitialized;
extern bool bmpWarmupComplete;
extern bool altitudeZeroSaved;
extern bool varioRateInitialized;
extern bool liftAudioActive;
extern bool sinkAudioActive;
extern bool liftBeepOn;
extern bool toneTestActive;
extern bool buzzerLabActive;   // web Buzzer Lab is driving the buzzers; vario audio paused
extern bool editingMenuItem;
extern bool inMenuMode;       // false = data windows, true = settings menu
extern bool controlsLocked;   // buttons/encoder ignored except the unlock hold
extern bool lockBeepEnabled;
extern uint32_t lockHoldMs;
extern uint32_t lockSplashUntilMs;  // show Locked/Unlocked splash until this time
extern bool pixelEnabled;
extern uint8_t activeWindow;  // 0..kOledWindowCount-1, cycled by encoder in view mode
extern uint8_t selectedMenuItem;
extern uint8_t selectedBatteryLogMenuItem;
extern uint8_t logRateIndex;
extern uint8_t batteryReadRateIndex;
extern VolumeLevel volumeLevel;
extern uint8_t buzzerVolumePercent;
extern uint8_t varioResponseIndex;
extern uint8_t toneTestPatternIndex;
extern uint8_t buzzerTestTargetIndex;
extern uint8_t wifiNetworkCount;
extern uint8_t wifiAttemptIndex;
extern uint8_t pixelMode;
extern uint8_t bmpAddress;
extern uint32_t pixelColor;

extern uint32_t lastDisplayMs;
extern uint32_t lastGpsLogMs;
extern uint32_t batteryLogStartMs;
extern uint32_t lastBatteryLogMs;
extern uint32_t lastGpsDebugMs;
extern uint32_t lastSensorReadMs;
extern uint32_t lastBmpInitAttemptMs;
extern uint32_t lastWifiAttemptMs;
extern uint32_t wifiAttemptStartMs;
extern uint32_t lastPixelUpdateMs;
extern uint32_t lastBatteryReadMs;
extern uint32_t bmpWarmupStartMs;
extern uint32_t lastVarioRateUpdateMs;
extern uint32_t liftPhaseStartMs;
extern uint32_t currentToneHz;
extern uint32_t toneTestStartMs;
extern uint32_t lastBuzzerLabMs;  // last Buzzer Lab keepalive; auto-stops if browser goes away
extern uint8_t currentToneMask;
extern uint16_t rainbowHue;

extern float altitudeFt;
extern float smoothedAltitudeFt;
extern float previousVarioAltitudeFt;
extern float baselineSmoothedAltitudeFt;
extern float displayAltitudeFt;
extern float verticalSpeedMps;
extern float temperatureF;
extern float humidityPercent;
extern float batteryVoltage;
extern float batteryPercent;
extern String connectedWifiSsid;
extern StoredWifiNetwork wifiNetworks[kMaxWifiNetworks];

// Shared utility helpers (defined in globals.cpp).
float clampFloat(float value, float low, float high);
String floatOrDash(float value, uint8_t decimals, const char *suffix = "");
