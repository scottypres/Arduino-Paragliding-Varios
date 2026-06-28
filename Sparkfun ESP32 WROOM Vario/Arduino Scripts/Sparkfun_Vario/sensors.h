#pragma once

#include "globals.h"

bool i2cAddressPresent(uint8_t address);
void printI2cScan();
bool tryBmpAddress(uint8_t address);
void startBmp();
void startSht();
void initSensors();
void completeBmpWarmup();
void readSensors();
