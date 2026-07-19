#pragma once

#include <Adafruit_SSD1306.h>

#include <cstdint>

#include "smartlife_models.h"

namespace smartlife {

class DisplayController {
 public:
  DisplayController();

  void begin();
  bool ready() const;
  void render(uint32_t nowMs,
              Mode mode,
              const SensorSnapshot& snapshot,
              float temperatureThresholdC);

 private:
  void renderNormal(Mode mode, const SensorSnapshot& snapshot);
  void renderThreshold(Mode mode,
                       const SensorSnapshot& snapshot,
                       int temperatureThresholdC);
  void setLine(uint8_t line);
  void printMode(Mode mode);

  Adafruit_SSD1306 display_;
  bool ready_ = false;
  bool hasRendered_ = false;
  bool thresholdInitialized_ = false;
  uint32_t lastRenderMs_ = 0;
  uint32_t thresholdPageUntilMs_ = 0;
  int lastThresholdC_ = 27;
};

}  // namespace smartlife
