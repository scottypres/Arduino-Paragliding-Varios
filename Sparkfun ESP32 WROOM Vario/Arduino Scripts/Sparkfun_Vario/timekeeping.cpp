#include "timekeeping.h"

#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "esp_sntp.h"

#include "gps_mod.h"

// ponytail: store UTC only; a TZ-offset knob can come with manual-location work.
// GPS and NTP both write the RTC; the clock reflects whichever synced most
// recently, so time stays correct whether you last had a fix or a network.
static const char *source = "none";
static volatile uint32_t lastNtpSyncMs = 0;  // set from the SNTP task callback
static uint32_t lastGpsSyncMs = 0;

// ESP-IDF's SNTP client calls this each time it successfully sets the RTC.
static void onNtpSync(struct timeval *) {
  const uint32_t ms = millis();
  lastNtpSyncMs = ms == 0 ? 1 : ms;  // reserve 0 as the "never synced" sentinel
}

// Which source last set the clock. Signed subtraction is wraparound-safe: the
// two syncs are always far closer together than the ~49-day millis() period.
static const char *moreRecentSource(uint32_t ntpMs, uint32_t gpsMs) {
  if (ntpMs == 0 && gpsMs == 0) return "none";
  if (gpsMs == 0) return "ntp";
  if (ntpMs == 0) return "gps";
  return static_cast<int32_t>(ntpMs - gpsMs) >= 0 ? "ntp" : "gps";
}

void initTimekeeping() {
  setenv("TZ", "UTC0", 1);
  tzset();
}

void onWifiConnectedTime() {
  // SNTP sets the ESP32 RTC, which keeps ticking after WiFi drops. Register the
  // sync callback before configTime so we learn each time NTP refreshes the clock.
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

bool timeKnown() {
  // Epoch < ~2023-11 means the clock was never set.
  return time(nullptr) > 1700000000;
}

const char *clockSource() { return source; }

void serviceClock() {
  const uint32_t nowMs = millis();

  // Re-seed the RTC from a fresh GPS fix on an interval. GPS is an absolute UTC
  // reference, so this keeps time correct with no WiFi and stops the RTC drifting
  // on a long flight. Correcting for the fix's centiseconds and parse age means a
  // re-seed never makes an already-NTP-synced clock visibly stutter.
  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024 &&
      gps.time.age() < kGpsFixMaxAgeMs &&
      (lastGpsSyncMs == 0 || nowMs - lastGpsSyncMs >= kGpsClockResyncMs)) {
    struct tm t = {};
    t.tm_year = gps.date.year() - 1900;
    t.tm_mon = gps.date.month() - 1;
    t.tm_mday = gps.date.day();
    t.tm_hour = gps.time.hour();
    t.tm_min = gps.time.minute();
    t.tm_sec = gps.time.second();
    const time_t epoch = mktime(&t);  // TZ=UTC, so this is a UTC epoch
    const int64_t us = static_cast<int64_t>(epoch) * 1000000LL +
                       static_cast<int64_t>(gps.time.centisecond()) * 10000LL +
                       static_cast<int64_t>(gps.time.age()) * 1000LL;
    const struct timeval tv = {static_cast<time_t>(us / 1000000LL),
                               static_cast<suseconds_t>(us % 1000000LL)};
    settimeofday(&tv, nullptr);
    lastGpsSyncMs = nowMs == 0 ? 1 : nowMs;  // reserve 0 as the "never" sentinel
  }

  const char *label = moreRecentSource(lastNtpSyncMs, lastGpsSyncMs);
  if (strcmp(label, "none") == 0 && timeKnown()) {
    label = "ntp";  // SNTP set the clock before its callback recorded a stamp
  }
  source = label;
}

String isoTimestamp() {
  if (!timeKnown()) {
    return String();
  }
  const time_t now = time(nullptr);
  struct tm tmNow;
  gmtime_r(&now, &tmNow);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmNow);
  return String(buf);
}

// Local (tzOffsetMinutes-shifted) wall clock, independent of isoTimestamp()'s
// UTC — logging always stays UTC, only the OLED/web display is localized.
static struct tm localBrokenDownTime() {
  const time_t local = time(nullptr) + static_cast<time_t>(tzOffsetMinutes) * 60;
  struct tm t;
  gmtime_r(&local, &t);  // shift already applied; gmtime_r just splits the fields
  return t;
}

String localTimeString() {
  if (!timeKnown()) {
    return String("--:--:--");
  }
  const struct tm t = localBrokenDownTime();
  char buf[16];
  if (clock12h) {
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) {
      hour12 = 12;
    }
    snprintf(buf, sizeof(buf), "%d:%02d:%02d %s", hour12, t.tm_min, t.tm_sec,
             t.tm_hour < 12 ? "AM" : "PM");
  } else {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  }
  return String(buf);
}

String localDateString() {
  if (!timeKnown()) {
    return String("----");
  }
  const struct tm t = localBrokenDownTime();
  char buf[12];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return String(buf);
}

String tzOffsetString() {
  if (tzOffsetMinutes == 0) {
    return String("UTC");
  }
  const int16_t absMinutes = tzOffsetMinutes < 0 ? -tzOffsetMinutes : tzOffsetMinutes;
  char buf[8];
  snprintf(buf, sizeof(buf), "%c%02d:%02d", tzOffsetMinutes < 0 ? '-' : '+',
           absMinutes / 60, absMinutes % 60);
  return String(buf);
}
