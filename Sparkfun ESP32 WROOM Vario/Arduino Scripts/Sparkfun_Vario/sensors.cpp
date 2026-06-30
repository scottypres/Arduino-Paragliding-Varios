#include "sensors.h"

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
  if (!useGpsAltitude) {
    displayAltitudeFt = smoothedAltitudeFt - baselineSmoothedAltitudeFt;
  }
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
    if (!useGpsAltitude) {
      displayAltitudeFt = smoothedAltitudeFt - baselineSmoothedAltitudeFt;
    }
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
