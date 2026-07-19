#pragma once

#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include <ESP32Servo.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "smartlife_models.h"

namespace smartlife {

class HardwareIo {
 public:
  HardwareIo();

  void begin();
  void sample(uint32_t nowMs);
  const SensorSnapshot& snapshot() const;
  float temperatureThresholdC() const;
  void applyOutputs(const ControlOutputs& outputs);

 private:
  void sampleFastSensors(uint32_t nowMs);
  void sampleDht(uint32_t nowMs);
  void updateDhtFreshness(uint32_t nowMs);

  void writeFanPercent(uint8_t percent);
  void writeRelay(bool on);
  void writeBuzzer(bool on);
  void writeRgb(RgbState state);
  void writeCurtainPercent(int closePercent);

  DHT dht_;
  Servo servo_;
  Adafruit_NeoPixel pixels_;
  SensorSnapshot snapshot_{};

  uint32_t bootStartedMs_ = 0;
  uint32_t lastFastSampleMs_ = 0;
  uint32_t lastDhtAttemptMs_ = 0;
  uint32_t lastDhtSuccessMs_ = 0;
  bool hasDhtSuccess_ = false;

  std::array<int, 5> knobSamples_{{0, 0, 0, 0, 0}};
  std::size_t knobSampleIndex_ = 0;
  std::size_t knobSampleCount_ = 0;
  int acceptedKnobRaw_ = 0;
  float temperatureThresholdC_ = 27.0f;

  bool servoAttached_ = false;
  int lastFanPercent_ = -1;
  int lastCurtainPercent_ = -1;
  int lastRelayState_ = -1;
  int lastBuzzerState_ = -1;
  int lastRgbState_ = -1;
};

}  // namespace smartlife
