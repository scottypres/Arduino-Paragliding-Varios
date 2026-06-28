#pragma once

#include "globals.h"

String batterySummary();
uint32_t gradientColor(float value, float low, float high);
void initBatteryMonitor();
void readBatteryIfDue();
void initPixel();
void savePixelSettings();
void setPixelColor(uint32_t color);
void servicePixel();
void enterDeepSleep();  // silences outputs, sleeps; wakes on encoder-button press
