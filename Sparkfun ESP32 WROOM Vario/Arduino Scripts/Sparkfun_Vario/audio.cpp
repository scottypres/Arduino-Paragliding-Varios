#include "audio.h"

void setBuzzersLow() {
  for (uint8_t pin : kBuzzerPins) {
    digitalWrite(pin, LOW);
  }
}

uint8_t activeBuzzerCount() {
  return static_cast<uint8_t>(volumeLevel) + 1;
}

uint8_t activeBuzzerMask() {
  uint8_t mask = 0;
  const uint8_t count = activeBuzzerCount();
  for (uint8_t index = 0; index < kBuzzerCount; index++) {
    if (index < count) {
      mask |= (1U << index);
    }
  }
  return mask;
}

uint32_t clampFrequency(uint32_t value) {
  return min(max(value, kMinToneHz), kMaxToneHz);
}

uint32_t quantizeFrequency(uint32_t value) {
  value = clampFrequency(value);
  return ((value + kToneQuantizeHz / 2) / kToneQuantizeHz) * kToneQuantizeHz;
}

uint8_t buzzerDuty() {
  const uint8_t volume = constrain(buzzerVolumePercent,
                                   kMinBuzzerVolumePercent,
                                   kMaxBuzzerVolumePercent);
  uint16_t duty = static_cast<uint16_t>(volume) * 255U / 100U;
  return static_cast<uint8_t>(constrain(duty, 1U, 255U));
}

void setToneMask(uint32_t frequencyHz, uint8_t buzzerMask, bool honorAudioSetting) {
  if (honorAudioSetting && !audioEnabled) {
    frequencyHz = 0;
  }

  frequencyHz = frequencyHz > 0 ? quantizeFrequency(frequencyHz) : 0;
  if (frequencyHz == 0) {
    buzzerMask = 0;
  }

  if (frequencyHz == currentToneHz && buzzerMask == currentToneMask) {
    return;
  }

  for (uint8_t index = 0; index < kBuzzerCount; index++) {
    const bool active = frequencyHz > 0 && (buzzerMask & (1U << index));
    ledcWriteTone(kBuzzerPins[index], active ? frequencyHz : 0);
    ledcWrite(kBuzzerPins[index], active ? buzzerDuty() : 0);
    if (!active) {
      digitalWrite(kBuzzerPins[index], LOW);
    }
  }
  currentToneHz = frequencyHz;
  currentToneMask = buzzerMask;
}

void setTone(uint32_t frequencyHz) {
  setToneMask(frequencyHz, activeBuzzerMask());
}

void startBuzzers() {
  for (uint8_t pin : kBuzzerPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    if (!ledcAttach(pin, kLiftFreqBaseHz, kBuzzerResolutionBits)) {
      Serial.print("Buzzer PWM setup failed on pin ");
      Serial.println(pin);
    }
  }
}

void startToneTest() {
  toneTestStartMs = millis();
  toneTestActive = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  setToneMask(0, 0, false);
}

uint8_t buzzerTestMask() {
  if (buzzerTestTargetIndex < kBuzzerCount) {
    return 1U << buzzerTestTargetIndex;
  }
  return (1U << kBuzzerCount) - 1U;
}

bool updateToneTest() {
  if (!toneTestActive) {
    return false;
  }

  const uint32_t elapsedMs = millis() - toneTestStartMs;
  if (elapsedMs >= kBuzzerTestDurationMs) {
    toneTestActive = false;
    setToneMask(0, 0, false);
    return false;
  }

  setToneMask(kBuzzerTestToneHz, buzzerTestMask(), false);
  return true;
}

// ---- jingles (3-buzzer chords / melodies, on-demand) ----

struct JingleNote {
  uint16_t f0;
  uint16_t f1;
  uint16_t f2;
  uint16_t ms;
};

// Note frequencies (Hz): C5 523 D5 587 E5 659 F5 698 G5 784 A5 880 B5 988 C6 1047
static const JingleNote kChimeNotes[] = {
    {659, 0, 0, 130}, {784, 0, 0, 130}, {1047, 0, 0, 150},
    {523, 659, 784, 440}, {0, 0, 0, 60}};
static const JingleNote kArpNotes[] = {
    {523, 0, 0, 105}, {659, 0, 0, 105}, {784, 0, 0, 105}, {1047, 0, 0, 105},
    {784, 0, 0, 105}, {1047, 659, 523, 380}, {0, 0, 0, 40}};
static const JingleNote kChordNotes[] = {
    {523, 659, 784, 280}, {587, 740, 880, 280}, {659, 831, 988, 280},
    {523, 659, 1047, 520}, {0, 0, 0, 40}};

struct Jingle {
  const JingleNote *notes;
  uint8_t len;
};
static const Jingle kJingles[] = {
    {kChimeNotes, sizeof(kChimeNotes) / sizeof(kChimeNotes[0])},
    {kArpNotes, sizeof(kArpNotes) / sizeof(kArpNotes[0])},
    {kChordNotes, sizeof(kChordNotes) / sizeof(kChordNotes[0])}};
static const uint8_t kJingleCount = sizeof(kJingles) / sizeof(kJingles[0]);

static bool jinglePlaying = false;
static uint8_t jingleIdx = 0;
static uint8_t jingleNote = 0;
static uint32_t jingleNoteStartMs = 0;

void setChord(uint32_t f0, uint32_t f1, uint32_t f2) {
  const uint32_t freqs[3] = {f0, f1, f2};
  uint8_t mask = 0;
  for (uint8_t index = 0; index < kBuzzerCount; index++) {
    const uint32_t fr = freqs[index];
    ledcWriteTone(kBuzzerPins[index], fr);
    ledcWrite(kBuzzerPins[index], fr ? buzzerDuty() : 0);
    if (fr) {
      mask |= (1U << index);
    } else {
      digitalWrite(kBuzzerPins[index], LOW);
    }
  }
  currentToneHz = f0;
  currentToneMask = mask;
}

uint8_t jingleCount() { return kJingleCount; }
bool jingleActive() { return jinglePlaying; }

void playJingle(uint8_t index) {
  if (index >= kJingleCount) {
    return;
  }
  jingleIdx = index;
  jingleNote = 0;
  jingleNoteStartMs = millis();
  jinglePlaying = true;
  toneTestActive = false;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  const JingleNote &n = kJingles[index].notes[0];
  setChord(n.f0, n.f1, n.f2);
}

void serviceJingle() {
  if (!jinglePlaying) {
    return;
  }
  const Jingle &tune = kJingles[jingleIdx];
  if (millis() - jingleNoteStartMs < tune.notes[jingleNote].ms) {
    return;
  }
  jingleNote++;
  if (jingleNote >= tune.len) {
    jinglePlaying = false;
    setToneMask(0, 0, false);
    return;
  }
  jingleNoteStartMs = millis();
  const JingleNote &n = tune.notes[jingleNote];
  setChord(n.f0, n.f1, n.f2);
}

void labTone(bool on, uint32_t freq, uint8_t mask, uint8_t duty) {
  buzzerLabActive = on;
  if (!on) {
    setToneMask(0, 0, false);  // resets currentTone state + silences
    return;
  }
  lastBuzzerLabMs = millis();
  freq = constrain(freq, 50U, 6000U);
  if (duty < 1) duty = 1;
  for (uint8_t i = 0; i < kBuzzerCount; i++) {
    const bool active = mask & (1U << i);
    ledcWriteTone(kBuzzerPins[i], active ? freq : 0);
    ledcWrite(kBuzzerPins[i], active ? duty : 0);
    if (!active) {
      digitalWrite(kBuzzerPins[i], LOW);
    }
  }
}

void updateVarioAudio() {
  const uint32_t now = millis();

  if (buzzerLabActive) {
    if (now - lastBuzzerLabMs <= 8000) {
      return;  // lab in control; keepalives keep it alive
    }
    labTone(false, 0, 0, 0);  // browser went away — release buzzers to the vario
  }

  if (jinglePlaying) {
    return;
  }

  if (updateToneTest()) {
    return;
  }

  if (!audioEnabled || !bmpWarmupComplete || !varioRateInitialized) {
    setTone(0);
    liftAudioActive = false;
    sinkAudioActive = false;
    liftBeepOn = false;
    return;
  }

  if (!liftAudioActive && verticalSpeedMps >= kLiftThresholdMps) {
    liftAudioActive = true;
    sinkAudioActive = false;
    liftBeepOn = true;
    liftPhaseStartMs = now;
  } else if (liftAudioActive && verticalSpeedMps < kLiftOffThresholdMps) {
    liftAudioActive = false;
    liftBeepOn = false;
  }

  if (!sinkAudioActive && verticalSpeedMps <= kSinkThresholdMps) {
    sinkAudioActive = true;
    liftAudioActive = false;
    liftBeepOn = false;
  } else if (sinkAudioActive && verticalSpeedMps > kSinkOffThresholdMps) {
    sinkAudioActive = false;
  }

  if (liftAudioActive) {
    const float climb = clampFloat(verticalSpeedMps, 0.0F, 8.0F);
    const uint32_t frequency =
        quantizeFrequency(kLiftFreqBaseHz + static_cast<uint32_t>(climb * kLiftFreqIncrementHzPerMps));
    const uint32_t beepMs = static_cast<uint32_t>(clampFloat(155.0F - climb * 15.0F, 65.0F, 155.0F));
    const uint32_t pauseMs = static_cast<uint32_t>(clampFloat(560.0F - climb * 75.0F, 95.0F, 560.0F));
    const uint32_t phaseMs = liftBeepOn ? beepMs : pauseMs;

    if (now - liftPhaseStartMs >= phaseMs) {
      liftBeepOn = !liftBeepOn;
      liftPhaseStartMs = now;
    }

    setTone(liftBeepOn ? frequency : 0);
    return;
  }

  if (sinkAudioActive) {
    const float sink = clampFloat(-verticalSpeedMps, 0.0F, 8.0F);
    const uint32_t frequency =
        quantizeFrequency(kSinkFreqBaseHz - min(static_cast<uint32_t>(sink * kSinkFreqDecrementHzPerMps), 180UL));
    setTone(frequency);
    return;
  }

  setTone(0);
}
