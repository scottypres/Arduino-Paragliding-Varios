#pragma once

#include "globals.h"
#include "driving_math.h"
#include <atomic>

extern bool drivingActive;
extern bool drivingBeeps;
extern uint8_t drivingAxis;
extern float drivingAccelLimit;
extern float drivingBrakeLimit;
extern driving::Detector drivingDetector;
extern String drivingLogPath;
extern bool drivingLogOk;
extern std::atomic<int32_t> drivingRequest;  // 1=start, -1=stop, 2=zero; main loop only

void initDriving();
bool startDriving();
void stopDriving();
void serviceDriving();
void calibrateDriving();  // operator must be parked
void processDrivingImu(driving::Vec accel, driving::Vec gyro);
void drivingImuFault(const char *reason);  // reason must have static lifetime
void saveDrivingSettings();
bool serviceDrivingAudio();  // true while driving owns the vario audio channel
String drivingStatus();
String drivingJson();
