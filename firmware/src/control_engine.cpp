#include "control_engine.h"

#include <algorithm>

#include "smartlife_config.h"

namespace smartlife {
namespace {

constexpr uint32_t TIMER_INACTIVE = UINT32_MAX;

uint8_t alertBit(AlertCode code) {
  return static_cast<uint8_t>(code);
}

void updateLatchedAlert(bool trigger,
                        bool safeCondition,
                        uint32_t nowMs,
                        uint32_t clearDurationMs,
                        bool& latched,
                        uint32_t& safeSinceMs) {
  if (trigger) {
    latched = true;
    safeSinceMs = TIMER_INACTIVE;
    return;
  }

  if (!latched) {
    safeSinceMs = TIMER_INACTIVE;
    return;
  }

  if (!safeCondition) {
    safeSinceMs = TIMER_INACTIVE;
    return;
  }

  if (safeSinceMs == TIMER_INACTIVE) {
    safeSinceMs = nowMs;
    return;
  }

  if (static_cast<uint32_t>(nowMs - safeSinceMs) >= clearDurationMs) {
    latched = false;
    safeSinceMs = TIMER_INACTIVE;
  }
}

uint8_t collectAlertMask(const ControlState& state) {
  uint8_t alerts = 0;
  if (state.mq2Latched) {
    alerts |= alertBit(AlertCode::Mq2);
  }
  if (state.flameLatched) {
    alerts |= alertBit(AlertCode::Flame);
  }
  if (state.waterLatched) {
    alerts |= alertBit(AlertCode::Water);
  }
  if (state.intrusionLatched) {
    alerts |= alertBit(AlertCode::Intrusion);
  }
  return alerts;
}

bool hasPrimaryHomeRisk(uint8_t activeAlerts) {
  const uint8_t primaryMask = alertBit(AlertCode::Mq2) |
                              alertBit(AlertCode::Flame) |
                              alertBit(AlertCode::Water);
  return (activeAlerts & primaryMask) != 0;
}

int clampedPercent(int value) {
  return std::max(0, std::min(100, value));
}

}  // namespace

ControlState defaultControlState() {
  return ControlState{};
}

ControlOutputs bootSafeOutputs() {
  ControlOutputs outputs{};
  outputs.mode = Mode::Auto;
  outputs.fanPercent = BOOT_FAN_PERCENT;
  outputs.relayOn = BOOT_RELAY_ON;
  outputs.buzzerOn = BOOT_BUZZER_ON;
  outputs.rgb = RgbState::Off;
  outputs.curtainControlEnabled = false;
  outputs.activeAlerts = 0;
  outputs.safetyActive = false;
  return outputs;
}

ControlEvaluation evaluateControl(const ControlInputs& inputs,
                                  const ControlState& currentState) {
  ControlEvaluation evaluation{};
  evaluation.nextState = currentState;

  updateLatchedAlert(
      inputs.sensors.mq2Ready && inputs.sensors.mq2EqPpm >= MQ2_ALERT_EQ_PPM,
      inputs.sensors.mq2Ready && inputs.sensors.mq2EqPpm <= MQ2_CLEAR_EQ_PPM,
      inputs.nowMs,
      SAFETY_CLEAR_MS,
      evaluation.nextState.mq2Latched,
      evaluation.nextState.mq2SafeSinceMs);

  updateLatchedAlert(inputs.sensors.flameDetected,
                     !inputs.sensors.flameDetected,
                     inputs.nowMs,
                     SAFETY_CLEAR_MS,
                     evaluation.nextState.flameLatched,
                     evaluation.nextState.flameSafeSinceMs);

  updateLatchedAlert(inputs.sensors.waterDetected,
                     !inputs.sensors.waterDetected,
                     inputs.nowMs,
                     SAFETY_CLEAR_MS,
                     evaluation.nextState.waterLatched,
                     evaluation.nextState.waterSafeSinceMs);

  if (!inputs.guardArmed) {
    evaluation.nextState.intrusionLatched = false;
    evaluation.nextState.intrusionSafeSinceMs = TIMER_INACTIVE;
  } else {
    updateLatchedAlert(inputs.sensors.presenceDetected,
                       !inputs.sensors.presenceDetected,
                       inputs.nowMs,
                       INTRUSION_CLEAR_MS,
                       evaluation.nextState.intrusionLatched,
                       evaluation.nextState.intrusionSafeSinceMs);
  }

  ControlOutputs& outputs = evaluation.outputs;
  outputs.mode = evaluation.nextState.targetMode;
  outputs.curtainClosePercent = clampedPercent(inputs.solarCurtainClosePercent);

  if (evaluation.nextState.targetMode == Mode::Sleep) {
    outputs.fanPercent = SLEEP_FAN_PERCENT;
    outputs.relayOn = false;
    outputs.rgb = RgbState::Off;
    outputs.curtainControlEnabled = false;
  } else {
    if (inputs.sensors.temperatureValid) {
      if (inputs.sensors.temperatureC >=
          inputs.temperatureThresholdC + TEMPERATURE_HYSTERESIS_C) {
        evaluation.nextState.autoFanHigh = true;
      } else if (inputs.sensors.temperatureC <=
                 inputs.temperatureThresholdC - TEMPERATURE_HYSTERESIS_C) {
        evaluation.nextState.autoFanHigh = false;
      }
    }

    outputs.fanPercent = evaluation.nextState.autoFanHigh ? 100 : 0;
    outputs.relayOn = inputs.sensors.presenceDetected &&
                      inputs.sensors.lightRelative < inputs.solarLightThreshold;
    outputs.rgb = RgbState::Yellow;
    outputs.curtainControlEnabled = true;
  }

  outputs.activeAlerts = collectAlertMask(evaluation.nextState);
  outputs.safetyActive = outputs.activeAlerts != 0;

  if (hasPrimaryHomeRisk(outputs.activeAlerts)) {
    outputs.fanPercent = ALERT_FAN_PERCENT;
    outputs.relayOn = false;
  }

  if (outputs.safetyActive) {
    outputs.rgb = RgbState::Red;
    outputs.buzzerOn = inputs.buzzerEnabled;
  }

  return evaluation;
}

CommandDecision decideModeCommand(bool safetyActive) {
  CommandDecision decision{};
  decision.ok = true;
  decision.deferredBySafety = safetyActive;
  return decision;
}

CommandDecision decideManualActuatorCommand(bool safetyActive) {
  CommandDecision decision{};
  decision.ok = !safetyActive;
  decision.error = safetyActive ? CommandError::SafetyLock : CommandError::None;
  return decision;
}

bool hasAlert(uint8_t activeAlerts, AlertCode code) {
  return (activeAlerts & alertBit(code)) != 0;
}

}  // namespace smartlife
