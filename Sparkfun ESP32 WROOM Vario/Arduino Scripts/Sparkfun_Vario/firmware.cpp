#include "firmware.h"

#include <Update.h>

#include "audio.h"

// This build switches TO the "other" radio firmware. A WiFi-less (BT) build
// loads the WiFi image; everything else loads the BT image. Both .bin files
// live in the SD card root; copy them there once (Arduino IDE: Sketch ->
// Export Compiled Binary).
#if defined(VARIO_DISABLE_WIFI)
constexpr const char *kSwitchFirmwarePath = "/fw_wifi.bin";
constexpr const char *kSwitchFirmwareLabel = "WiFi FW";
#else
constexpr const char *kSwitchFirmwarePath = "/fw_bt.bin";
constexpr const char *kSwitchFirmwareLabel = "BT FW";
#endif

const char *switchFirmwareTargetLabel() {
  return kSwitchFirmwareLabel;
}

// Two centered text lines plus a hold so the user can read a result/error.
static void showFlashMessage(const String &line1, const String &line2, uint32_t holdMs) {
  Serial.println(line1 + " " + line2);
  if (!oledReady) {
    delay(holdMs);
    return;
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 16);
  oled.print(line1);
  oled.setCursor(0, 32);
  oled.print(line2);
  oled.display();
  delay(holdMs);
}

// Title, "Flashing NN%", and a filled progress bar. Redrawn each whole percent.
static void drawFlashProgress(uint8_t pct) {
  if (!oledReady) {
    return;
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);
  oled.setCursor(0, 0);
  oled.print("Load ");
  oled.print(kSwitchFirmwareLabel);
  oled.setCursor(0, 18);
  oled.print("Flashing ");
  oled.print(pct);
  oled.print('%');

  constexpr int16_t kBarY = 36;
  constexpr int16_t kBarH = 14;
  oled.drawRect(0, kBarY, kOledWidth, kBarH, SH110X_WHITE);
  const int16_t fillW = static_cast<int16_t>((kOledWidth - 4) * static_cast<int32_t>(pct) / 100);
  oled.fillRect(2, kBarY + 2, fillW, kBarH - 4, SH110X_WHITE);

  oled.setCursor(0, kOledHeight - 8);
  oled.print("Do not power off");
  oled.display();
}

void flashFirmwareFromSd() {
  if (!sdReady) {
    showFlashMessage("No SD card", "Cannot switch FW", 2500);
    return;
  }

  File file = SD.open(kSwitchFirmwarePath, FILE_READ);
  if (!file || file.isDirectory()) {
    showFlashMessage("Missing file:", String(kSwitchFirmwarePath), 3000);
    if (file) {
      file.close();
    }
    return;
  }

  const size_t size = file.size();
  if (size == 0 || !Update.begin(size)) {
    Update.printError(Serial);
    showFlashMessage("Flash init failed", String(kSwitchFirmwareLabel), 3000);
    file.close();
    return;
  }

  // Silence the buzzers; the loop won't run again until after reboot.
  setTone(0);

  uint8_t buffer[1024];
  size_t written = 0;
  uint8_t lastPct = 255;
  drawFlashProgress(0);

  while (file.available()) {
    const size_t n = file.read(buffer, sizeof(buffer));
    if (n == 0) {
      break;
    }
    if (Update.write(buffer, n) != n) {
      Update.printError(Serial);
      Update.abort();
      file.close();
      showFlashMessage("Write failed", "FW not changed", 3000);
      return;
    }
    written += n;
    const uint8_t pct = static_cast<uint8_t>(static_cast<uint64_t>(written) * 100 / size);
    if (pct != lastPct) {
      lastPct = pct;
      drawFlashProgress(pct);
    }
    yield();  // keep the system task / watchdog happy during the long write
  }
  file.close();

  if (written != size || !Update.end(true)) {
    Update.printError(Serial);
    showFlashMessage("Verify failed", "FW not changed", 3000);
    return;
  }

  drawFlashProgress(100);
  showFlashMessage("Done.", "Rebooting...", 900);
  ESP.restart();
}
