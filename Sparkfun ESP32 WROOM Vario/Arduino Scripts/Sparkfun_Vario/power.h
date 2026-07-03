#pragma once

#include "globals.h"

String batterySummary();
uint32_t gradientColor(float value, float low, float high);
void initBatteryMonitor();
void readBatteryIfDue();
void readBatteryNow();   // take one ADC reading immediately (ignores implausible values)
void refreshBattery();   // drop WiFi briefly so ADC2 is free, take a clean reading, restore WiFi
void initPixel();
void savePixelSettings();
void setPixelColor(uint32_t color);
void servicePixel();
void enterDeepSleep();  // silences outputs, sleeps; wakes on encoder-button press
