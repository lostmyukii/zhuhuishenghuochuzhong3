#include <Arduino.h>

#include "control_engine.h"
#include "hardware_io.h"
#include "smartlife_config.h"
#include "solar_terms.h"

namespace {

smartlife::HardwareIo hardware;
smartlife::ControlState controlState = smartlife::defaultControlState();
smartlife::SolarTerm activeSolarTerm = smartlife::SolarTerm::Lichun;

}  // namespace

void setup() {
  hardware.begin();
  Serial.begin(smartlife::SERIAL_BAUD);
}

void loop() {
  const uint32_t nowMs = millis();
  hardware.sample(nowMs);

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
  yield();
}
