#pragma once

#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "protocol_core.h"
#include "smartlife_models.h"

namespace smartlife {

class SerialProtocol {
 public:
  void emitHello(Stream& serial,
                 const ProtocolRuntimeState& state,
                 bool oledReady);
  void poll(Stream& serial,
            ProtocolRuntimeState& state,
            bool safetyActive);
  bool telemetryDue(uint32_t nowMs) const;
  void emitTelemetry(Stream& serial,
                     uint32_t nowMs,
                     const ProtocolRuntimeState& state,
                     const SensorSnapshot& snapshot,
                     float temperatureThresholdC,
                     const ControlOutputs& outputs,
                     bool oledReady,
                     bool thresholdPage);
  void emitLocalEvent(Stream& serial, const LocalEvent& event);
  void emitAlertChanged(Stream& serial, uint8_t activeAlerts);
  void requestTelemetry();

 private:
  CommandProcessor commandProcessor_{};
  std::array<char, SERIAL_INPUT_MAX_LENGTH + 1> inputBuffer_{};
  std::size_t inputLength_ = 0;
  bool inputOverflow_ = false;
  bool hasTelemetry_ = false;
  bool telemetryRequested_ = true;
  uint32_t lastTelemetryMs_ = 0;
  uint32_t sequence_ = 0;
};

}  // namespace smartlife
