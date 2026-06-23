#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
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
constexpr uint32_t kBuzzerToneHz = 4000;
constexpr uint8_t kBuzzerResolutionBits = 8;

constexpr uint8_t kButtonA = 15;
constexpr uint8_t kButtonB = 32;
constexpr uint8_t kButtonC = 14;

constexpr float kMetersToFeet = 3.28084F;
constexpr float kSeaLevelPressureHpa = 1013.25F;
constexpr float kMinColorScaleFt = 0.1F;
constexpr float kMaxColorScaleFt = 1000.0F;
constexpr float kAltitudeSmoothingAlpha = 0.25F;
constexpr uint32_t kMenuLongPressMs = 800;
constexpr uint8_t kMenuItemCount = 3;

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_BMP5xx bmp;

bool bmpReady = false;
bool shtReady = false;
bool lastA = HIGH;
bool lastB = HIGH;
bool lastC = HIGH;
bool altitudeFilterInitialized = false;
bool wifiEnabled = true;
bool otaStarted = false;
bool neopixelEnabled = true;
bool menuActive = false;
bool buzzerOn = false;
uint8_t menuIndex = 0;

float altitudeFt = 0.0F;
float smoothedAltitudeFt = 0.0F;
float baselineSmoothedAltitudeFt = 0.0F;
float displayAltitudeFt = 0.0F;
float temperatureF = NAN;
float humidityPercent = NAN;
float colorScaleFt = 100.0F;
float batteryVolts = NAN;
float batteryPercent = NAN;

uint32_t lastSensorReadMs = 0;
uint32_t lastDisplayUpdateMs = 0;
uint32_t lastOtaProgressMs = 0;
uint32_t buttonAPressStartMs = 0;

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

void setBuzzer(bool enabled) {
  if (enabled == buzzerOn) {
    return;
  }

  if (enabled) {
    ledcWriteTone(kBuzzerPin, kBuzzerToneHz);
  } else {
    ledcWriteTone(kBuzzerPin, 0);
    digitalWrite(kBuzzerPin, LOW);
  }

  buzzerOn = enabled;
}

void startBuzzer() {
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
  if (!ledcAttach(kBuzzerPin, kBuzzerToneHz, kBuzzerResolutionBits)) {
    Serial.println("Buzzer PWM setup failed");
  }
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
    display.println();
    display.print(menuIndex == 0 ? "> " : "  ");
    display.print("WiFi: ");
    display.println(wifiEnabled ? "on" : "off");
    display.print(menuIndex == 1 ? "> " : "  ");
    display.print("NeoPixel: ");
    display.println(neopixelEnabled ? "on" : "off");
    display.print(menuIndex == 2 ? "> " : "  ");
    display.println("Deep sleep");
    display.println();
    display.println(menuIndex == 2 ? "A sleep" : "A toggle");
    display.println("B/C select");
    display.println("Hold A exit");
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

  display.print("BMP: ");
  display.print(bmpReady ? "ok" : "missing");
  display.print(" SHT: ");
  display.println(shtReady ? "ok" : "missing");

  display.print("WiFi: ");
  if (!wifiEnabled) {
    display.println("off");
  } else {
    display.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "offline");
  }

  display.println("A zero B- C+");
  display.println("Hold A menu");
  display.display();
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.begin(kWifiSsid, kWifiPassword);

  Serial.printf("Connecting to WiFi SSID: %s\n", kWifiSsid);
  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 30000) {
    setBuzzer(digitalRead(kButtonB) == LOW);
    delay(10);
  }
  setBuzzer(false);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return;
  }

  Serial.println("Requested WiFi failed; starting setup portal");
  WiFi.disconnect(false);

  WiFiManager wm;
  wm.setDebugOutput(true);
  wm.setHostname(kHostname);
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180);
  wm.preloadWiFi(kWifiSsid, kWifiPassword);

  if (!wm.autoConnect(kConfigPortalSsid, kConfigPortalPassword)) {
    ESP.restart();
  }
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
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void enableWifi() {
  connectWifi();
  startOta();
}

void startSensors() {
  Wire.begin();

  bmpReady = bmp.begin(BMP5XX_ALTERNATIVE_ADDRESS, &Wire);
  if (bmpReady) {
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_2X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
    bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
    bmp.enablePressure(true);
  }

  shtReady = sht4.begin();
  if (shtReady) {
    sht4.setPrecision(SHT4X_MED_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }
}

void readSensors() {
  if (bmpReady && bmp.performReading()) {
    altitudeFt = bmp.readAltitude(kSeaLevelPressureHpa) * kMetersToFeet;
    if (!altitudeFilterInitialized) {
      smoothedAltitudeFt = altitudeFt;
      baselineSmoothedAltitudeFt = smoothedAltitudeFt;
      altitudeFilterInitialized = true;
    } else {
      smoothedAltitudeFt += kAltitudeSmoothingAlpha * (altitudeFt - smoothedAltitudeFt);
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

  setBuzzer(b == LOW);

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
        if (wifiEnabled) {
          enableWifi();
        } else {
          disableWifi();
        }
      } else if (menuIndex == 1) {
        neopixelEnabled = !neopixelEnabled;
        updatePixel();
      } else if (menuIndex == 2) {
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

  connectWifi();
  startOta();
}

void loop() {
  if (wifiEnabled && otaStarted) {
    ArduinoOTA.handle();
  }
  serviceButtons();

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
