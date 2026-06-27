#pragma once

#include "globals.h"

void initSdCard();
void writeLogHeader();
void resetDataLog();
bool deleteRecursive(const String &path);
uint32_t logFileSize();
String logTail();
void printCsvFloat(File &file, float value, uint8_t decimals);
void logDataIfDue();
