#pragma once

#include "globals.h"

void initSdCard();
void writeLogHeader();
void resetDataLog();
bool deleteRecursive(const String &path);
uint32_t logFileSize();
String logTail();
uint32_t batteryLogFileSize();
String batteryLogTail();
void printCsvFloat(File &file, float value, uint8_t decimals);
void logDataIfDue();
void beginFlightLog();  // open a new per-flight CSV under /logs
void endFlightLog();    // close out the current per-flight CSV
void startBatteryLogging();
void stopBatteryLogging();
void serviceBatteryLogging();
