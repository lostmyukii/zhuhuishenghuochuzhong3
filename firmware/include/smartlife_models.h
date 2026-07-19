#pragma once

#include <cstdint>

namespace smartlife {

enum class Mode : uint8_t {
  Auto,
  Sleep,
};

enum class RgbState : uint8_t {
  Off,
  Yellow,
  Red,
};

enum class AlertCode : uint8_t {
  None = 0,
  Mq2 = 1 << 0,
  Flame = 1 << 1,
  Water = 1 << 2,
  Intrusion = 1 << 3,
};

struct SensorSnapshot {
  bool temperatureValid = false;
  float temperatureC = 0.0f;
  bool mq2Ready = false;
  int mq2EqPpm = 0;
  bool waterDetected = false;
  bool flameDetected = false;
  bool presenceDetected = false;
  int lightRelative = 0;
};

struct ControlInputs {
  uint32_t nowMs = 0;
  SensorSnapshot sensors{};
  float temperatureThresholdC = 27.0f;
  int solarCurtainClosePercent = 50;
  int solarLightThreshold = 35;
  bool guardArmed = false;
  bool buzzerEnabled = true;
};

struct ControlState {
  Mode targetMode = Mode::Auto;
  bool autoFanHigh = false;
  bool mq2Latched = false;
  bool flameLatched = false;
  bool waterLatched = false;
  bool intrusionLatched = false;
  uint32_t mq2SafeSinceMs = UINT32_MAX;
  uint32_t flameSafeSinceMs = UINT32_MAX;
  uint32_t waterSafeSinceMs = UINT32_MAX;
  uint32_t intrusionSafeSinceMs = UINT32_MAX;
};

struct ControlOutputs {
  Mode mode = Mode::Auto;
  uint8_t fanPercent = 0;
  int curtainClosePercent = 50;
  bool curtainControlEnabled = false;
  bool relayOn = false;
  bool buzzerOn = false;
  RgbState rgb = RgbState::Off;
  uint8_t activeAlerts = 0;
  bool safetyActive = false;
};

struct ControlEvaluation {
  ControlState nextState{};
  ControlOutputs outputs{};
};

enum class CommandError : uint8_t {
  None,
  SafetyLock,
};

struct CommandDecision {
  bool ok = false;
  bool deferredBySafety = false;
  CommandError error = CommandError::None;
};

}  // namespace smartlife
