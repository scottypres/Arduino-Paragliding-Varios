#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP5xx.h>
#include <WiFiManager.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

#ifndef BAT_VOLT_PIN
#define BAT_VOLT_PIN BATT_MONITOR
#endif

namespace {
const char *const kWifiSsid = "helloworld";
const char *const kWifiPassword = "allyourbase69";
const char *const kHostname = "vario-feather-v2";
const char *const kConfigPortalSsid = "VarioFeatherSetup";
const char *const kConfigPortalPassword = "configureme";

constexpr uint8_t kBuzzerPin = 13;
constexpr uint8_t kBuzzerResolutionBits = 8;
const char *const kPrefsNamespace = "vario";
const char *const kPrefWifi = "bootWifi";
const char *const kPrefPixel = "bootPixel";
const char *const kPrefAudio = "bootAudio";

constexpr uint8_t kButtonA = 15;
constexpr uint8_t kButtonB = 32;
constexpr uint8_t kButtonC = 14;

constexpr float kMetersToFeet = 3.28084F;
constexpr float kFeetToMeters = 1.0F / kMetersToFeet;
constexpr float kSeaLevelPressureHpa = 1013.25F;
constexpr float kMinColorScaleFt = 0.1F;
constexpr float kMaxColorScaleFt = 1000.0F;
constexpr float kAltitudeSmoothingAlpha = 0.25F;
constexpr float kVarioResponseAlpha[] = {0.18F, 0.32F, 0.50F, 0.72F};
const char *const kVarioResponseLabel[] = {"Smooth", "Normal", "Quick", "Direct"};
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
constexpr uint8_t kBuzzerDuty = 96;
constexpr uint32_t kMenuLongPressMs = 800;
constexpr uint8_t kMenuItemCount = 6;
constexpr uint8_t kToneTestCount = 4;
constexpr uint32_t kToneTestDurationMs = 3000;
constexpr uint32_t kBmpWarmupMs = 5000;
constexpr uint32_t kBmpRetryMs = 2000;
constexpr uint32_t kWifiConnectTimeoutMs = 8000;
constexpr uint32_t kWifiPortalTimeoutMs = 300000;
constexpr uint32_t kWifiRetryMs = 30000;
const char *const kToneTestLabel[] = {"Ascent", "Fast up", "Descent", "Fast down"};

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_BMP5xx bmp;
Preferences prefs;
WiFiManager wifiManager;

bool bmpReady = false;
bool shtReady = false;
bool lastA = HIGH;
bool lastB = HIGH;
bool lastC = HIGH;
bool altitudeFilterInitialized = false;
bool wifiEnabled = true;
bool otaStarted = false;
bool neopixelEnabled = true;
bool audioEnabled = true;
bool menuActive = false;
bool toneOn = false;
bool liftAudioActive = false;
bool sinkAudioActive = false;
bool liftBeepOn = false;
bool varioRateInitialized = false;
bool toneTestActive = false;
bool readyBeepActive = false;
bool bmpWarmupComplete = false;
bool wifiConnectInProgress = false;
bool wifiPortalRunning = false;
uint8_t menuIndex = 0;
uint8_t varioResponseIndex = 1;
uint8_t toneTestPatternIndex = 0;
uint8_t toneTestPlayingIndex = 0;
uint8_t readyBeepCount = 0;

float altitudeFt = 0.0F;
float smoothedAltitudeFt = 0.0F;
float previousVarioAltitudeFt = 0.0F;
float baselineSmoothedAltitudeFt = 0.0F;
float displayAltitudeFt = 0.0F;
float verticalSpeedMps = 0.0F;
float temperatureF = NAN;
float humidityPercent = NAN;
float colorScaleFt = 100.0F;
float batteryVolts = NAN;
float batteryPercent = NAN;

uint32_t lastSensorReadMs = 0;
uint32_t lastDisplayUpdateMs = 0;
uint32_t lastOtaProgressMs = 0;
uint32_t lastVarioRateUpdateMs = 0;
uint32_t liftPhaseStartMs = 0;
uint32_t buttonAPressStartMs = 0;
uint32_t currentToneHz = 0;
uint32_t toneTestStartMs = 0;
uint32_t bmpWarmupStartMs = 0;
uint32_t lastBmpInitAttemptMs = 0;
uint32_t readyBeepPhaseStartMs = 0;
uint32_t wifiConnectStartMs = 0;
uint32_t wifiPortalStartMs = 0;
uint32_t lastWifiRetryMs = 0;

void disableWifi();

float clampFloat(float value, float low, float high) {
  return min(max(value, low), high);
}

void setPixelColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (!neopixelEnabled) {
    red = 0;
    green = 0;
    blue = 0;
  }
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, red, green, blue);
#endif
}

uint32_t clampFrequency(uint32_t value) {
  return min(max(value, kMinToneHz), kMaxToneHz);
}

uint32_t quantizeFrequency(uint32_t value) {
  value = clampFrequency(value);
  return ((value + kToneQuantizeHz / 2) / kToneQuantizeHz) * kToneQuantizeHz;
}

void setTone(uint32_t frequencyHz) {
  if (!audioEnabled) {
    frequencyHz = 0;
  }

  frequencyHz = frequencyHz > 0 ? quantizeFrequency(frequencyHz) : 0;
  if (frequencyHz == currentToneHz) {
    return;
  }

  if (frequencyHz > 0) {
    ledcWriteTone(kBuzzerPin, frequencyHz);
    ledcWrite(kBuzzerPin, kBuzzerDuty);
  } else {
    ledcWriteTone(kBuzzerPin, 0);
    digitalWrite(kBuzzerPin, LOW);
  }

  currentToneHz = frequencyHz;
  toneOn = frequencyHz > 0;
}

void loadSettings() {
  prefs.begin(kPrefsNamespace, false);
  wifiEnabled = prefs.getBool(kPrefWifi, true);
  neopixelEnabled = prefs.getBool(kPrefPixel, true);
  audioEnabled = prefs.getBool(kPrefAudio, true);
}

void saveBoolSetting(const char *key, bool value) {
  prefs.putBool(key, value);
}

void startBuzzer() {
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
  if (!ledcAttach(kBuzzerPin, kLiftFreqBaseHz, kBuzzerResolutionBits)) {
    Serial.println("Buzzer PWM setup failed");
  }
}

void startToneTest() {
  if (!audioEnabled || !bmpWarmupComplete) {
    return;
  }

  toneTestPlayingIndex = toneTestPatternIndex;
  toneTestPatternIndex = (toneTestPatternIndex + 1) % kToneTestCount;
  toneTestStartMs = millis();
  toneTestActive = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  setTone(0);
}

void startReadyBeeps() {
  if (!audioEnabled) {
    return;
  }

  readyBeepActive = true;
  readyBeepCount = 0;
  readyBeepPhaseStartMs = millis();
  setTone(1040);
}

bool updateReadyBeeps() {
  if (!readyBeepActive) {
    return false;
  }

  const uint32_t now = millis();
  const bool beepOn = currentToneHz > 0;
  const uint32_t phaseMs = beepOn ? 90 : 120;
  if (now - readyBeepPhaseStartMs < phaseMs) {
    return true;
  }

  readyBeepPhaseStartMs = now;
  if (beepOn) {
    setTone(0);
    readyBeepCount++;
    if (readyBeepCount >= 3) {
      readyBeepActive = false;
      return false;
    }
  } else {
    setTone(1040);
  }

  return true;
}

bool updateToneTest() {
  if (!toneTestActive) {
    return false;
  }

  const uint32_t elapsedMs = millis() - toneTestStartMs;
  if (elapsedMs >= kToneTestDurationMs) {
    toneTestActive = false;
    setTone(0);
    return false;
  }

  switch (toneTestPlayingIndex) {
    case 0: {
      const uint32_t cycleMs = 620;
      setTone(elapsedMs % cycleMs < 145 ? 860 : 0);
      break;
    }
    case 1: {
      const uint32_t cycleMs = 215;
      setTone(elapsedMs % cycleMs < 82 ? 1320 : 0);
      break;
    }
    case 2:
      setTone(320);
      break;
    case 3: {
      const uint32_t cycleMs = 430;
      setTone(elapsedMs % cycleMs < 310 ? 220 : 0);
      break;
    }
  }

  return true;
}

void updateVarioAudio() {
  const uint32_t now = millis();

  if (updateReadyBeeps()) {
    return;
  }

  if (updateToneTest()) {
    return;
  }

  if (menuActive || !audioEnabled || !bmpWarmupComplete || !varioRateInitialized) {
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
    const uint32_t beepMs =
      static_cast<uint32_t>(clampFloat(155.0F - climb * 15.0F, 65.0F, 155.0F));
    const uint32_t pauseMs =
      static_cast<uint32_t>(clampFloat(560.0F - climb * 75.0F, 95.0F, 560.0F));
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
      quantizeFrequency(kSinkFreqBaseHz -
                        min(static_cast<uint32_t>(sink * kSinkFreqDecrementHzPerMps), 180UL));
    setTone(frequency);
    return;
  }

  setTone(0);
}

void updatePixel() {
  if (!neopixelEnabled) {
    setPixelColor(0, 0, 0);
    return;
  }

  const float normalized = clampFloat(displayAltitudeFt / colorScaleFt, -1.0F, 1.0F);

  uint8_t red = 0;
  uint8_t green = 12;
  uint8_t blue = 0;

  if (normalized > 0.0F) {
    red = static_cast<uint8_t>(normalized * 80.0F);
    green = static_cast<uint8_t>((1.0F - normalized) * 30.0F);
    blue = static_cast<uint8_t>((1.0F - normalized) * 20.0F);
  } else if (normalized < 0.0F) {
    const float down = -normalized;
    red = static_cast<uint8_t>((1.0F - down) * 20.0F);
    green = static_cast<uint8_t>((1.0F - down) * 30.0F);
    blue = static_cast<uint8_t>(down * 80.0F);
  }

  setPixelColor(red, green, blue);
}

void readBattery() {
  uint32_t millivolts = analogReadMilliVolts(BAT_VOLT_PIN);
  batteryVolts = millivolts * 2.0F / 1000.0F;
  batteryPercent = clampFloat((batteryVolts - 3.2F) * 100.0F / (4.2F - 3.2F), 0.0F, 100.0F);
}

void drawDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);

  if (menuActive) {
    display.setTextSize(1);
    display.println("Menu");
    display.print(menuIndex == 0 ? "> " : "  ");
    display.print("Boot WiFi:");
    display.println(wifiEnabled ? "on" : "off");
    display.print(menuIndex == 1 ? "> " : "  ");
    display.print("Boot LED:");
    display.println(neopixelEnabled ? "on" : "off");
    display.print(menuIndex == 2 ? "> " : "  ");
    display.print("Boot Buzz:");
    display.println(audioEnabled ? "on" : "off");
    display.print(menuIndex == 3 ? "> " : "  ");
    display.print("Response: ");
    display.println(kVarioResponseLabel[varioResponseIndex]);
    display.print(menuIndex == 4 ? "> " : "  ");
    display.print("Test: ");
    display.println(toneTestActive ? kToneTestLabel[toneTestPlayingIndex]
                                   : kToneTestLabel[toneTestPatternIndex]);
    display.print(menuIndex == 5 ? "> " : "  ");
    display.println("Deep sleep");
    if (menuIndex == 4) {
      display.println("A play next");
    } else {
      display.println(menuIndex == 5 ? "A sleep" : "A change");
    }
    display.println("B/C sel Hold A exit");
    display.display();
    return;
  }

  display.setTextSize(2);
  display.print(displayAltitudeFt, 2);
  display.println(" ft");

  display.setTextSize(1);
  display.print("Temp: ");
  if (isnan(temperatureF)) {
    display.println("--.- F");
  } else {
    display.print(temperatureF, 1);
    display.println(" F");
  }

  display.print("Hum:  ");
  if (isnan(humidityPercent)) {
    display.println("--.- %");
  } else {
    display.print(humidityPercent, 1);
    display.println(" %");
  }

  display.print("Batt: ");
  if (isnan(batteryPercent)) {
    display.println("--%");
  } else {
    display.print(batteryPercent, 0);
    display.print("% ");
    display.print(batteryVolts, 2);
    display.println("V");
  }

  display.print("Color: ");
  display.print(colorScaleFt, colorScaleFt < 10.0F ? 1 : 0);
  display.println(" ft");

  display.print("Vario: ");
  if (!bmpWarmupComplete) {
    display.println("warming");
  } else {
    display.print(verticalSpeedMps, 2);
    display.print(" ");
    display.println(kVarioResponseLabel[varioResponseIndex]);
  }

  display.print("BMP: ");
  display.print(bmpReady ? "ok" : "missing");
  display.print(" SHT: ");
  display.println(shtReady ? "ok" : "missing");

  display.print("WiFi: ");
  if (!wifiEnabled) {
    display.println("off");
  } else if (wifiPortalRunning) {
    display.println(kConfigPortalSsid);
  } else {
    display.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "offline");
  }

  display.println("A zero B- C+");
  display.println("Hold A menu");
  display.display();
}

void configureWifiManager() {
  wifiManager.setDebugOutput(true);
  wifiManager.setHostname(kHostname);
  wifiManager.setConnectTimeout(4);
  wifiManager.setConnectRetries(1);
  wifiManager.setSaveConnectTimeout(20);
  wifiManager.setConfigPortalTimeout(kWifiPortalTimeoutMs / 1000);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.preloadWiFi(kWifiSsid, kWifiPassword);
}

void startWifiConnection() {
  if (!wifiEnabled) {
    disableWifi();
    return;
  }

  if (wifiPortalRunning) {
    wifiManager.stopConfigPortal();
    wifiPortalRunning = false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  if (WiFi.SSID().length() > 0) {
    Serial.print("Starting WiFi with saved SSID: ");
    Serial.println(WiFi.SSID());
    WiFi.begin();
  } else {
    Serial.print("Starting WiFi with default SSID: ");
    Serial.println(kWifiSsid);
    WiFi.begin(kWifiSsid, kWifiPassword);
  }

  wifiConnectInProgress = true;
  wifiConnectStartMs = millis();
  lastWifiRetryMs = wifiConnectStartMs;
}

void startWifiPortal() {
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP_STA);
  Serial.print("Starting setup AP: ");
  Serial.println(kConfigPortalSsid);
  wifiManager.startConfigPortal(kConfigPortalSsid, kConfigPortalPassword);
  wifiPortalRunning = wifiManager.getConfigPortalActive();
  wifiPortalStartMs = millis();
  wifiConnectInProgress = false;
}

void startOta() {
  if (otaStarted || !wifiEnabled || WiFi.status() != WL_CONNECTED) {
    return;
  }

  ArduinoOTA.setHostname(kHostname);
  ArduinoOTA.setPassword("password");
  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA update started");
      setPixelColor(40, 20, 0);
    })
    .onEnd([]() {
      Serial.println("\nOTA update finished");
      setPixelColor(0, 40, 0);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - lastOtaProgressMs > 500) {
        Serial.printf("OTA progress: %u%%\n", progress / (total / 100));
        lastOtaProgressMs = millis();
      }
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA error[%u]\n", error);
      setPixelColor(80, 0, 0);
    });
  ArduinoOTA.begin();
  otaStarted = true;
}

void disableWifi() {
  ArduinoOTA.end();
  otaStarted = false;
  wifiConnectInProgress = false;
  wifiPortalRunning = false;
  wifiManager.stopConfigPortal();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void enableWifi() {
  startWifiConnection();
}

void serviceWifi() {
  if (!wifiEnabled) {
    return;
  }

  if (!bmpWarmupComplete) {
    return;
  }

  if (wifiPortalRunning) {
    wifiManager.process();
    if (WiFi.status() == WL_CONNECTED) {
      wifiPortalRunning = false;
      wifiConnectInProgress = false;
      Serial.print("Connected to SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      startOta();
      return;
    }

    if (!wifiManager.getConfigPortalActive() ||
        millis() - wifiPortalStartMs >= kWifiPortalTimeoutMs) {
      wifiManager.stopConfigPortal();
      wifiPortalRunning = false;
      lastWifiRetryMs = millis();
      Serial.println("WiFi setup AP closed; continuing offline");
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiPortalRunning = false;
    wifiConnectInProgress = false;
    startOta();
    return;
  }

  if (wifiConnectInProgress) {
    if (millis() - wifiConnectStartMs >= kWifiConnectTimeoutMs) {
      Serial.println("WiFi connect timed out");
      startWifiPortal();
    }
    return;
  }

  if (lastWifiRetryMs == 0 || millis() - lastWifiRetryMs >= kWifiRetryMs) {
    startWifiConnection();
  }
}

void startBmp() {
  lastBmpInitAttemptMs = millis();
  bmpReady = bmp.begin(BMP5XX_ALTERNATIVE_ADDRESS, &Wire);
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
    Serial.println("BMP ready; warming up");
  } else {
    Serial.println("BMP missing; will retry");
  }
}

void startSht() {
  shtReady = sht4.begin();
  if (shtReady) {
    sht4.setPrecision(SHT4X_MED_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }
}

void startSensors() {
  Wire.begin();
  startBmp();
  startSht();
}

void completeBmpWarmup() {
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  previousVarioAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  verticalSpeedMps = 0.0F;
  lastVarioRateUpdateMs = millis();
  varioRateInitialized = true;
  bmpWarmupComplete = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  startReadyBeeps();
  Serial.println("BMP warmup complete; altitude zeroed");
}

void readSensors() {
  if (!bmpReady && millis() - lastBmpInitAttemptMs >= kBmpRetryMs) {
    startBmp();
  }

  if (bmpReady && bmp.performReading()) {
    const uint32_t now = millis();
    altitudeFt = bmp.readAltitude(kSeaLevelPressureHpa) * kMetersToFeet;
    if (!altitudeFilterInitialized) {
      smoothedAltitudeFt = altitudeFt;
      previousVarioAltitudeFt = smoothedAltitudeFt;
      baselineSmoothedAltitudeFt = smoothedAltitudeFt;
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

  if (shtReady) {
    sensors_event_t humidity;
    sensors_event_t temp;
    if (sht4.getEvent(&humidity, &temp)) {
      temperatureF = temp.temperature * 9.0F / 5.0F + 32.0F;
      humidityPercent = humidity.relative_humidity;
    }
  }

  readBattery();
  updatePixel();
}

void enterDeepSleep() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.println("Deep sleep");
  display.println();
  display.println("Press A");
  display.println("to wake");
  display.display();

  disableWifi();
  setPixelColor(0, 0, 0);

  rtc_gpio_pullup_en(static_cast<gpio_num_t>(kButtonA));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(kButtonA));
  const esp_err_t wakeResult =
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(kButtonA), 0);
  if (wakeResult != ESP_OK) {
    Serial.printf("Deep sleep wake config failed: %d\n", wakeResult);
  }

  delay(500);
  display.clearDisplay();
  display.display();
  display.oled_command(SH110X_DISPLAYOFF);
  Serial.flush();
  esp_deep_sleep_start();
}

void serviceButtons() {
  const bool a = digitalRead(kButtonA);
  const bool b = digitalRead(kButtonB);
  const bool c = digitalRead(kButtonC);

  if (lastA == HIGH && a == LOW) {
    buttonAPressStartMs = millis();
  }

  if (lastA == LOW && a == HIGH) {
    const bool longPress = millis() - buttonAPressStartMs >= kMenuLongPressMs;
    if (longPress) {
      menuActive = !menuActive;
    } else if (menuActive) {
      if (menuIndex == 0) {
        wifiEnabled = !wifiEnabled;
        saveBoolSetting(kPrefWifi, wifiEnabled);
        if (wifiEnabled) {
          enableWifi();
        } else {
          disableWifi();
        }
      } else if (menuIndex == 1) {
        neopixelEnabled = !neopixelEnabled;
        saveBoolSetting(kPrefPixel, neopixelEnabled);
        updatePixel();
      } else if (menuIndex == 2) {
        audioEnabled = !audioEnabled;
        saveBoolSetting(kPrefAudio, audioEnabled);
        if (!audioEnabled) {
          setTone(0);
          toneTestActive = false;
          readyBeepActive = false;
        }
      } else if (menuIndex == 3) {
        varioResponseIndex = (varioResponseIndex + 1) % kVarioResponseCount;
      } else if (menuIndex == 4) {
        startToneTest();
      } else if (menuIndex == 5) {
        enterDeepSleep();
      }
    } else {
      baselineSmoothedAltitudeFt = smoothedAltitudeFt;
      displayAltitudeFt = 0.0F;
    }
  }

  if (lastB == HIGH && b == LOW) {
    if (menuActive) {
      menuIndex = (menuIndex + kMenuItemCount - 1) % kMenuItemCount;
    } else {
      colorScaleFt = clampFloat(colorScaleFt / 1.5F, kMinColorScaleFt, kMaxColorScaleFt);
    }
  }

  if (lastC == HIGH && c == LOW) {
    if (menuActive) {
      menuIndex = (menuIndex + 1) % kMenuItemCount;
    } else {
      colorScaleFt = clampFloat(colorScaleFt * 1.5F, kMinColorScaleFt, kMaxColorScaleFt);
    }
  }

  lastA = a;
  lastB = b;
  lastC = c;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  loadSettings();

  pinMode(kButtonA, INPUT_PULLUP);
  pinMode(kButtonB, INPUT_PULLUP);
  pinMode(kButtonC, INPUT_PULLUP);
  startBuzzer();
  analogSetPinAttenuation(BAT_VOLT_PIN, ADC_11db);

  setPixelColor(0, 0, 20);

  display.begin(0x3C, true);
  display.setRotation(1);
  display.clearDisplay();
  display.display();

  startSensors();
  readSensors();
  drawDisplay();

  configureWifiManager();
  if (!wifiEnabled) {
    disableWifi();
  }
}

void loop() {
  serviceWifi();

  if (wifiEnabled && otaStarted) {
    ArduinoOTA.handle();
  }
  serviceButtons();
  updateVarioAudio();

  if (millis() - lastSensorReadMs >= 100) {
    lastSensorReadMs = millis();
    readSensors();
  }

  if (millis() - lastDisplayUpdateMs >= 100) {
    lastDisplayUpdateMs = millis();
    drawDisplay();
  }

  delay(5);
}
