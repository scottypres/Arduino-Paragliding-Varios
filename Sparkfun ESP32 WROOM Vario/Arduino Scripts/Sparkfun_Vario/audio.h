#pragma once

#include "globals.h"

void setBuzzersLow();
uint8_t activeBuzzerCount();
uint8_t activeBuzzerMask();
uint32_t clampFrequency(uint32_t value);
uint32_t quantizeFrequency(uint32_t value);
uint8_t buzzerDuty();
void setToneMask(uint32_t frequencyHz, uint8_t buzzerMask, bool honorAudioSetting = true);
void setTone(uint32_t frequencyHz);
void startBuzzers();
void startToneTest();
uint8_t buzzerTestMask();
bool updateToneTest();
void updateVarioAudio();

// Jingles: drive the 3 buzzers independently for chords/melodies. On-demand
// only (never at boot, per the no-boot-chirp rule). Non-blocking: playJingle()
// starts a tune, serviceJingle() advances it from the main loop.
void setChord(uint32_t f0, uint32_t f1, uint32_t f2);
void playJingle(uint8_t index);
void serviceJingle();
uint8_t jingleCount();
bool jingleActive();
