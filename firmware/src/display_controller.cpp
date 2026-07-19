#include "display_controller.h"

#include <Arduino.h>
#include <Wire.h>

#include <cmath>

#include "smartlife_config.h"

namespace smartlife {

DisplayController::DisplayController()
    : display_(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN) {}

void DisplayController::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  ready_ = display_.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
  if (!ready_) {
    return;
  }

  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);
  display_.display();
}

bool DisplayController::ready() const {
  return ready_;
}

void DisplayController::render(uint32_t nowMs,
                               Mode mode,
                               const SensorSnapshot& snapshot,
                               float temperatureThresholdC) {
  const int roundedThreshold = static_cast<int>(std::lround(temperatureThresholdC));
  if (!thresholdInitialized_) {
    thresholdInitialized_ = true;
    lastThresholdC_ = roundedThreshold;
  } else if (roundedThreshold != lastThresholdC_) {
    lastThresholdC_ = roundedThreshold;
    thresholdPageUntilMs_ = nowMs + OLED_THRESHOLD_PAGE_MS;
  }

  if (!ready_) {
    return;
  }
  if (hasRendered_ &&
      static_cast<uint32_t>(nowMs - lastRenderMs_) <
          OLED_REFRESH_INTERVAL_MS) {
    return;
  }

  hasRendered_ = true;
  lastRenderMs_ = nowMs;
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);

  const bool thresholdPageActive =
      static_cast<int32_t>(thresholdPageUntilMs_ - nowMs) > 0;
  if (thresholdPageActive) {
    renderThreshold(mode, snapshot, roundedThreshold);
  } else {
    renderNormal(mode, snapshot);
  }
  display_.display();
}

void DisplayController::renderNormal(Mode mode,
                                     const SensorSnapshot& snapshot) {
  setLine(0);
  printMode(mode);

  setLine(1);
  display_.print("T:");
  if (snapshot.temperatureValid) {
    display_.print(snapshot.temperatureC, 1);
  } else {
    display_.print("--");
  }
  display_.print(" c");

  setLine(2);
  display_.print("Q:");
  if (snapshot.mq2Ready) {
    display_.print(snapshot.mq2EqPpm);
  } else {
    display_.print("----");
  }
  display_.print(" ppm");

  setLine(3);
  display_.print("N:");
  display_.print(snapshot.soundRelative);
  display_.print(" H:");
  display_.print(snapshot.presenceDetected ? 1 : 0);
}

void DisplayController::renderThreshold(Mode mode,
                                        const SensorSnapshot& snapshot,
                                        int temperatureThresholdC) {
  setLine(0);
  printMode(mode);

  setLine(1);
  display_.print("Threshold");

  setLine(2);
  display_.print("XN:");
  if (snapshot.knobValid) {
    display_.print(snapshot.knobRaw);
  } else {
    display_.print("--");
  }

  setLine(3);
  display_.print("YZ:");
  display_.print(temperatureThresholdC);
}

void DisplayController::setLine(uint8_t line) {
  display_.setCursor(0, static_cast<int16_t>(line) * 16);
}

void DisplayController::printMode(Mode mode) {
  display_.print(mode == Mode::Auto ? "Auto" : "Sleep");
}

}  // namespace smartlife
