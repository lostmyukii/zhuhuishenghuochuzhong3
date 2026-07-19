#include <Arduino.h>

#include "control_engine.h"
#include "display_controller.h"
#include "hardware_io.h"
#include "local_controls.h"
#include "smartlife_config.h"
#include "solar_terms.h"

namespace {

smartlife::HardwareIo hardware;
smartlife::DisplayController display;
smartlife::ControlState controlState = smartlife::defaultControlState();
smartlife::SolarTerm activeSolarTerm = smartlife::SolarTerm::Lichun;

const char* modeText(smartlife::Mode mode) {
  return mode == smartlife::Mode::Auto ? "Auto" : "Sleep";
}

void emitLocalEvent(const smartlife::LocalEvent& event) {
  if (event.type == smartlife::LocalEventType::ButtonA) {
    Serial.print(F("{\"type\":\"event\",\"event\":\"button\",\"key\":\"A\",\"action\":\"toggle_mode\",\"mode\":\""));
    Serial.print(modeText(event.mode));
    Serial.println(F("\"}"));
    return;
  }
  if (event.type == smartlife::LocalEventType::ThresholdChanged) {
    Serial.print(F("{\"type\":\"event\",\"event\":\"threshold_changed\",\"source\":\"knob\",\"knobRaw\":"));
    Serial.print(event.knobRaw);
    Serial.print(F(",\"temperatureThresholdC\":"));
    Serial.print(event.temperatureThresholdC);
    Serial.println(F("}"));
  }
}

}  // namespace

void setup() {
  hardware.begin();
  Serial.begin(smartlife::SERIAL_BAUD);
  display.begin();
}

void loop() {
  const uint32_t nowMs = millis();
  hardware.sample(nowMs);

  smartlife::LocalEvent localEvent{};
  while (hardware.takeLocalEvent(localEvent)) {
    if (localEvent.type == smartlife::LocalEventType::ButtonA) {
      controlState.targetMode =
          smartlife::toggleMode(controlState.targetMode);
      localEvent.mode = controlState.targetMode;
    }
    emitLocalEvent(localEvent);
  }

  const smartlife::SolarTermProfile& profile =
      smartlife::solarTermProfile(activeSolarTerm);
  smartlife::ControlInputs inputs{};
  inputs.nowMs = nowMs;
  inputs.sensors = hardware.snapshot();
  inputs.temperatureThresholdC = hardware.temperatureThresholdC();
  inputs.solarCurtainClosePercent = profile.curtainClosePercent;
  inputs.solarLightThreshold = profile.lightThreshold;
  inputs.guardArmed = false;
  inputs.buzzerEnabled = true;

  const smartlife::ControlEvaluation evaluation =
      smartlife::evaluateControl(inputs, controlState);
  controlState = evaluation.nextState;
  hardware.applyOutputs(evaluation.outputs);
  display.render(nowMs,
                 evaluation.outputs.mode,
                 hardware.snapshot(),
                 hardware.temperatureThresholdC());
  yield();
}
