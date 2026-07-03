#include "globals.h"

#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#endif

#include "audio.h"
#include "controls.h"
#include "display.h"
#include "flight.h"
#include "gps_mod.h"
#include "imu.h"
#include "logging.h"
#include "power.h"
#include "sensors.h"
#include "settings.h"
#include "timekeeping.h"
#include "web.h"
#include "wifi_net.h"
#include "windows.h"

void setup() {
  Serial.begin(115200);
  delay(250);
  loadSettings();
  initTimekeeping();

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
  initImu();
  initFlight();
  readSensors();
  readBatteryIfDue();
  initSdCard();
  initWindowConfig();
  initWifi();
  updateDisplay(true);
}

void loop() {
  serviceWifi();
  serviceWebServer();
  serviceWebPush();
  serviceOta();
  serviceGps();
  serviceFlight();
  serviceClock();
  printGpsDebugIfDue();
  serviceControls();
  if (millis() - lastSensorReadMs >= kSensorReadMs) {
    lastSensorReadMs = millis();
    readSensors();
    readImu();
  }
  readBatteryIfDue();
  servicePixel();
  serviceJingle();
  updateVarioAudio();
  serviceBatteryLogging();
  logDataIfDue();
  updateDisplay();
}
