#include "imu.h"
#include "driving.h"

#include <SparkFun_LSM6DSV16X.h>
#include <math.h>

static SparkFun_LSM6DSV16X lsm;
static bool drivingFifo = false;
static bool haveAccel = false, haveGyro = false;
static bool accelSettling = false, gyroSettling = false;
static uint8_t accelCounter = 0, gyroCounter = 0;
static driving::Vec fifoAccel, fifoGyro;
static uint32_t lastPollMs = 0, lastAttitudeMs = 0;

static int32_t fifoRead(void *, uint8_t reg, uint8_t *data, uint16_t len) {
  return lsm.readRegisterRegion(reg, data, len);
}
static int32_t fifoWrite(void *, uint8_t reg, const uint8_t *data, uint16_t len) {
  return lsm.writeRegisterRegion(reg, const_cast<uint8_t *>(data), len);
}
static stmdev_ctx_t fifoContext = {fifoWrite, fifoRead, nullptr, nullptr};

// Unsmoothed attitude, kept for level-to-horizon capture.
static float rawPitchDeg = 0.0F;
static float rawRollDeg = 0.0F;

constexpr float kImuSmoothingAlpha = 0.30F;

static void startLsm() {
  drivingFifo = haveAccel = haveGyro = false;
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
  if (drivingActive) return;  // practice owns the IMU until its session ends
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

static void updateAttitude(const sfe_lsm_data_t &accel) {
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

bool beginDrivingImu() {
  drivingFifo = haveAccel = haveGyro = false;
  if (!imuReady) return false;
  // Hardware FIFO retains samples while display or SD writes block the loop.
  // Disable the strong legacy horizon filter; the detector has a 60 ms filter.
  drivingFifo = lsm.setFifoMode(LSM6DSV16X_BYPASS_MODE) &&
      lsm.setAccelFullScale(LSM6DSV16X_16g) &&
      lsm.setGyroFullScale(LSM6DSV16X_2000dps) &&
      lsm.enableAccelLP2Filter(false) &&
      lsm.setAccelFifoBatchSet(LSM6DSV16X_XL_BATCHED_AT_120Hz) &&
      lsm.setGyroFifoBatchSet(LSM6DSV16X_GY_BATCHED_AT_120Hz) &&
      lsm.setFifoMode(LSM6DSV16X_STREAM_MODE);
  return drivingFifo;
}

void endDrivingImu() {
  if (imuReady) {
    lsm.setFifoMode(LSM6DSV16X_BYPASS_MODE);
    lsm.setAccelFifoBatchSet(LSM6DSV16X_XL_NOT_BATCHED);
    lsm.setGyroFifoBatchSet(LSM6DSV16X_GY_NOT_BATCHED);
    lsm.setAccelFullScale(LSM6DSV16X_4g);
    lsm.setGyroFullScale(LSM6DSV16X_500dps);
    lsm.enableAccelLP2Filter();
  }
  drivingFifo = haveAccel = haveGyro = false;
}

void readImu() {
  if (!imuEnabled) return;
  const uint32_t now = millis();
  if (now - lastPollMs < (drivingActive ? 4U : 100U)) return;
  lastPollMs = now;
  if (!imuReady && now - lastImuInitAttemptMs >= kBmpRetryMs) {
    lastImuInitAttemptMs = now;
    startLsm();
  }
  if (!imuReady) return;
  if (!drivingActive) {
    if (!lsm.checkAccelStatus()) return;
    sfe_lsm_data_t accel;
    if (lsm.getAccel(&accel)) updateAttitude(accel);
    return;
  }
  if (!drivingFifo && !beginDrivingImu()) {
    drivingImuFault("fifo_config"); return;
  }
  lsm6dsv16x_fifo_status_t status{};
  if (lsm6dsv16x_fifo_status_get(&fifoContext, &status) != 0) {
    drivingImuFault("imu_read"); imuReady = false; return;
  }
  // Refuse late alerts or integration across lost data. Explicit parked zero
  // is required after any fault; resetting the FIFO is not recalibration.
  if (status.fifo_ovr || status.fifo_full || status.fifo_level > 120) {
    drivingImuFault("fifo_gap");
    beginDrivingImu();
    return;
  }
  for (uint16_t i = 0; i < status.fifo_level; ++i) {
    lsm6dsv16x_fifo_out_raw_t raw{};
    if (lsm6dsv16x_fifo_out_raw_get(&fifoContext, &raw) != 0) {
      drivingImuFault("fifo_read"); imuReady = false; return;
    }
    const int16_t x = static_cast<int16_t>(raw.data[0] | (uint16_t(raw.data[1]) << 8));
    const int16_t y = static_cast<int16_t>(raw.data[2] | (uint16_t(raw.data[3]) << 8));
    const int16_t z = static_cast<int16_t>(raw.data[4] | (uint16_t(raw.data[5]) << 8));
    // ST FIFO tag counter identifies measurements from the same ODR tick.
    if (raw.tag == lsm6dsv16x_fifo_out_raw_t::LSM6DSV16X_XL_NC_TAG) {
      if (haveAccel) drivingImuFault("accel_gap");
      fifoAccel = driving::Vec(lsm.convert16gToMg(x), lsm.convert16gToMg(y),
                               lsm.convert16gToMg(z)) * 0.001F;
      accelCounter = raw.cnt;
      accelSettling = driving::fifoSettlingSample(x, y, z);
      haveAccel = true;
    } else if (raw.tag == lsm6dsv16x_fifo_out_raw_t::LSM6DSV16X_GY_NC_TAG) {
      if (haveGyro) drivingImuFault("gyro_gap");
      fifoGyro = driving::Vec(lsm.convert2000dpsToMdps(x), lsm.convert2000dpsToMdps(y),
                              lsm.convert2000dpsToMdps(z)) * 0.001F;
      gyroCounter = raw.cnt;
      gyroSettling = driving::fifoSettlingSample(x, y, z);
      haveGyro = true;
    }
    if (haveAccel && haveGyro) {
      const bool settling = drivingDetector.state == driving::State::Calibrating &&
          (accelSettling || gyroSettling);
      if (accelCounter != gyroCounter) drivingImuFault("sample_pair_gap");
      else if (settling) processDrivingImuSettling();
      else processDrivingImu(fifoAccel, fifoGyro);
      haveAccel = haveGyro = false;
      if (!settling && now - lastAttitudeMs >= 100) {
        lastAttitudeMs = now;
        sfe_lsm_data_t accel;
        accel.xData = fifoAccel.x * 1000;
        accel.yData = fifoAccel.y * 1000;
        accel.zData = fifoAccel.z * 1000;
        updateAttitude(accel);
      }
    }
  }
}
