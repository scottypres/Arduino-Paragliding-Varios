#include "globals.h"

Adafruit_SH1106G oled(kOledWidth, kOledHeight, &Wire, kOledResetPin);
Adafruit_BMP5xx bmp;
Adafruit_SHT4x sht4;
Preferences prefs;
TinyGPSPlus gps;
TinyGPSCustom gpsGpgsvSatellites(gps, "GPGSV", 3);
TinyGPSCustom gpsGngsvSatellites(gps, "GNGSV", 3);
TinyGPSCustom gpsGlgsvSatellites(gps, "GLGSV", 3);
TinyGPSCustom gpsGagsvSatellites(gps, "GAGSV", 3);
TinyGPSCustom gpsBdgsvSatellites(gps, "BDGSV", 3);
HardwareSerial gpsSerial(1);
TwoWire batteryWire(1);
SFE_MAX1704X batteryGauge(MAX1704X_MAX17048);
#ifndef VARIO_DISABLE_WIFI
AsyncWebServer webServer(kWebServerPort);
WiFiManager wifiManager;
#endif

Button backButton = {kBackButtonPin, false, false, false, 0, false};
Button encoderButton = {kEncoderButtonPin, false, false, false, 0, false};
Button confirmButton = {kConfirmButtonPin, true, false, false, 0, false};

bool oledReady = false;
bool sdReady = false;
bool bmpReady = false;
bool shtReady = false;
bool dataLoggingEnabled = true;
bool gpsDisplayEnabled = false;
bool useGpsAltitude = false;
bool audioEnabled = true;
bool bluetoothEnabled = false;
bool batteryGaugeReady = false;
bool batteryLoggingActive = false;
bool batteryLogWifiEnabled = true;
bool batteryLogBluetoothEnabled = false;
bool batteryLogOledEnabled = true;
bool batteryLogSavedWifiEnabled = true;
bool batteryLogSavedBluetoothEnabled = false;
bool batteryLogSavedOledEnabled = true;
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
bool buzzerLabActive = false;
bool editingMenuItem = false;
bool inMenuMode = false;
bool controlsLocked = false;
bool lockBeepEnabled = true;
uint32_t lockHoldMs = kDefaultLockHoldMs;
uint32_t lockSplashUntilMs = 0;
bool pixelEnabled = false;
uint8_t activeWindow = 0;
uint8_t selectedMenuItem = kMenuDataLogging;
uint8_t selectedBatteryLogMenuItem = kBatteryLogMenuStop;
uint8_t logRateIndex = 2;
uint8_t batteryReadRateIndex = 2;
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
uint32_t batteryLogStartMs = 0;
uint32_t lastBatteryLogMs = 0;
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
uint32_t lastBuzzerLabMs = 0;
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

float clampFloat(float value, float low, float high) {
  return min(max(value, low), high);
}

String floatOrDash(float value, uint8_t decimals, const char *suffix) {
  if (isnan(value)) {
    return String("--") + suffix;
  }
  return String(value, static_cast<unsigned int>(decimals)) + suffix;
}
