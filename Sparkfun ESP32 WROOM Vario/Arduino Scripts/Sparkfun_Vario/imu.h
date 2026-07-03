#pragma once

#include "globals.h"

// SparkFun 6DoF LSM6DSV16X breakout (accel + gyro) on the shared Qwiic bus,
// daisy-chained with the BMP/SHT sensors. Pitch and roll come from the
// accelerometer with a stored level-to-horizon offset; there is no
// magnetometer on this board, so magnetic heading is not available.
void initImu();
void setImuEnabled(bool enabled);
void readImu();          // call on the sensor-read cadence
void saveImuLevel();     // capture current attitude as "level"
void clearImuLevel();
