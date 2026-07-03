#include "imu.h"

#include <SparkFun_LSM6DSV16X.h>
#include <math.h>

static SparkFun_LSM6DSV16X lsm;

// Unsmoothed attitude, kept for level-to-horizon capture.
static float rawPitchDeg = 0.0F;
static float rawRollDeg = 0.0F;

constexpr float kImuSmoothingAlpha = 0.30F;

static void startLsm() {
  imuReady = lsm.begin(Wire);  // defaults to I2C address 0x6B (SDO high)
  if (!imuReady) {
    return;
  }
  lsm.deviceReset();
  const uint32_t resetStartMs = millis();
  while (!lsm.getDeviceReset() && millis() - resetStartMs < 100) {
    delay(1);
  }
  lsm.enableBlockDataUpdate();
  lsm.setAccelDataRate(LSM6DSV16X_ODR_AT_120Hz);
  lsm.setAccelFullScale(LSM6DSV16X_4g);
  lsm.setGyroDataRate(LSM6DSV16X_ODR_AT_120Hz);
  lsm.setGyroFullScale(LSM6DSV16X_500dps);
  lsm.enableFilterSettling();
  lsm.enableAccelLP2Filter();
  lsm.setAccelLP2Bandwidth(LSM6DSV16X_XL_STRONG);
  lsm.enableGyroLP1Filter();
  lsm.setGyroLP1Bandwidth(LSM6DSV16X_GY_ULTRA_LIGHT);
  Serial.println("LSM6DSV16X ready");
}

void initImu() {
  lastImuInitAttemptMs = millis();
  if (!imuEnabled) {
    return;
  }
  startLsm();
  if (!imuReady) {
    Serial.println("LSM6DSV16X missing; will retry");
  }
}

void setImuEnabled(bool enabled) {
  imuEnabled = enabled;
  prefs.putBool(kPrefImuEnabled, imuEnabled);
  if (enabled) {
    initImu();
  } else {
    imuReady = false;
    imuPitchDeg = NAN;
    imuRollDeg = NAN;
    imuGForce = NAN;
  }
}

void saveImuLevel() {
  imuPitchOffsetDeg = rawPitchDeg;
  imuRollOffsetDeg = rawRollDeg;
  imuLevelSaved = true;
  prefs.putFloat(kPrefImuPitchZero, imuPitchOffsetDeg);
  prefs.putFloat(kPrefImuRollZero, imuRollOffsetDeg);
  prefs.putBool(kPrefImuHasLevel, true);
  Serial.print("IMU level saved: pitch ");
  Serial.print(imuPitchOffsetDeg, 1);
  Serial.print(" roll ");
  Serial.println(imuRollOffsetDeg, 1);
}

void clearImuLevel() {
  imuPitchOffsetDeg = 0.0F;
  imuRollOffsetDeg = 0.0F;
  imuLevelSaved = false;
  prefs.remove(kPrefImuPitchZero);
  prefs.remove(kPrefImuRollZero);
  prefs.putBool(kPrefImuHasLevel, false);
  Serial.println("IMU level cleared");
}

void readImu() {
  if (!imuEnabled) {
    return;
  }
  if (!imuReady && millis() - lastImuInitAttemptMs >= kBmpRetryMs) {
    lastImuInitAttemptMs = millis();
    startLsm();
  }
  if (!imuReady) {
    return;
  }

  if (!lsm.checkStatus()) {
    return;
  }
  sfe_lsm_data_t accel;
  if (!lsm.getAccel(&accel)) {
    return;
  }

  // Library returns milli-g.
  float ax = accel.xData;
  float ay = accel.yData;
  const float az = accel.zData;
  const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
  if (magnitude < 1.0F) {
    return;
  }
  imuGForce = magnitude / 1000.0F;

  // Mounting orientation fix-up. imuSwapAxes exchanges the two horizontal axes
  // for a unit rotated 90 degrees on the airframe (pitch reads on the roll axis
  // and vice versa); the mirror flags flip each sign independently.
  if (imuSwapAxes) {
    const float tmp = ax;
    ax = ay;
    ay = tmp;
  }
  rawPitchDeg = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
  rawRollDeg = atan2f(ay, az) * RAD_TO_DEG;
  if (imuMirrorPitch) {
    rawPitchDeg = -rawPitchDeg;
  }
  if (imuMirrorRoll) {
    rawRollDeg = -rawRollDeg;
  }

  const float levelPitch = rawPitchDeg - imuPitchOffsetDeg;
  const float levelRoll = rawRollDeg - imuRollOffsetDeg;
  if (isnan(imuPitchDeg) || isnan(imuRollDeg)) {
    imuPitchDeg = levelPitch;
    imuRollDeg = levelRoll;
  } else {
    imuPitchDeg += kImuSmoothingAlpha * (levelPitch - imuPitchDeg);
    imuRollDeg += kImuSmoothingAlpha * (levelRoll - imuRollDeg);
  }
}
