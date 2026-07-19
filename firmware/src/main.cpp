#include <Arduino.h>

#include "control_engine.h"
#include "display_controller.h"
#include "hardware_io.h"
#include "local_controls.h"
#include "protocol_core.h"
#include "serial_protocol.h"
#include "smartlife_config.h"
#include "solar_terms.h"

namespace {

smartlife::HardwareIo hardware;
smartlife::DisplayController display;
smartlife::SerialProtocol protocol;
smartlife::ProtocolRuntimeState protocolState{};
smartlife::ControlState controlState = smartlife::defaultControlState();
uint8_t lastAlertMask = 0;
bool alertMaskInitialized = false;

smartlife::ControlInputs makeControlInputs(uint32_t nowMs) {
  const smartlife::SolarTermProfile& profile =
      smartlife::solarTermProfile(protocolState.solarTerm);
  smartlife::ControlInputs inputs{};
  inputs.nowMs = nowMs;
  inputs.sensors = hardware.snapshot();
  inputs.temperatureThresholdC = hardware.temperatureThresholdC();
  inputs.solarCurtainClosePercent = profile.curtainClosePercent;
  inputs.solarLightThreshold = profile.lightThreshold;
  inputs.guardArmed = protocolState.guardArmed;
  inputs.buzzerEnabled = protocolState.buzzerEnabled;
  return inputs;
}

}  // namespace

void setup() {
  hardware.begin();
  Serial.begin(smartlife::SERIAL_BAUD);
  display.begin();
  protocol.emitHello(Serial, protocolState, display.ready());
}

void loop() {
  const uint32_t nowMs = millis();
  hardware.sample(nowMs);

  smartlife::LocalEvent localEvent{};
  while (hardware.takeLocalEvent(localEvent)) {
    if (localEvent.type == smartlife::LocalEventType::ButtonA) {
      protocolState.targetMode =
          smartlife::toggleMode(protocolState.targetMode);
      smartlife::clearManualOverrides(protocolState);
      localEvent.mode = protocolState.targetMode;
    }
    protocol.emitLocalEvent(Serial, localEvent);
  }

  controlState.targetMode = protocolState.targetMode;
  const smartlife::ControlEvaluation beforeCommands =
      smartlife::evaluateControl(makeControlInputs(nowMs), controlState);
  protocol.poll(Serial, protocolState, beforeCommands.outputs.safetyActive);

  controlState.targetMode = protocolState.targetMode;
  smartlife::ControlEvaluation evaluation =
      smartlife::evaluateControl(makeControlInputs(nowMs), controlState);
  if (evaluation.outputs.safetyActive) {
    smartlife::clearManualOverrides(protocolState);
  }
  smartlife::applyManualOverrides(protocolState, evaluation.outputs);
  controlState = evaluation.nextState;
  hardware.applyOutputs(evaluation.outputs);
  display.render(nowMs,
                 evaluation.outputs.mode,
                 hardware.snapshot(),
                 hardware.temperatureThresholdC());

  if (!alertMaskInitialized) {
    alertMaskInitialized = true;
    if (evaluation.outputs.activeAlerts != 0) {
      protocol.emitAlertChanged(Serial, evaluation.outputs.activeAlerts);
    }
  } else if (evaluation.outputs.activeAlerts != lastAlertMask) {
    protocol.emitAlertChanged(Serial, evaluation.outputs.activeAlerts);
  }
  lastAlertMask = evaluation.outputs.activeAlerts;

  if (protocol.telemetryDue(nowMs)) {
    protocol.emitTelemetry(Serial,
                           nowMs,
                           protocolState,
                           hardware.snapshot(),
                           hardware.temperatureThresholdC(),
                           evaluation.outputs,
                           display.ready(),
                           display.showingThresholdPage(nowMs));
  }
  yield();
}
