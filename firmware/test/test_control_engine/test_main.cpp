#include <unity.h>

#include "control_engine.h"

using namespace smartlife;

namespace {

ControlInputs normalInputs(uint32_t nowMs = 0) {
  ControlInputs inputs{};
  inputs.nowMs = nowMs;
  inputs.sensors.temperatureValid = true;
  inputs.sensors.temperatureC = 25.0f;
  inputs.sensors.mq2Ready = true;
  inputs.sensors.mq2EqPpm = 100;
  inputs.sensors.lightRelative = 60;
  inputs.temperatureThresholdC = 27.0f;
  inputs.solarCurtainClosePercent = 50;
  inputs.solarLightThreshold = 35;
  inputs.buzzerEnabled = true;
  return inputs;
}

void test_boot_outputs_are_physically_safe() {
  const ControlOutputs outputs = bootSafeOutputs();
  TEST_ASSERT_EQUAL_UINT8(0, outputs.fanPercent);
  TEST_ASSERT_FALSE(outputs.relayOn);
  TEST_ASSERT_FALSE(outputs.buzzerOn);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RgbState::Off),
                          static_cast<uint8_t>(outputs.rgb));
  TEST_ASSERT_FALSE(outputs.safetyActive);
}

void test_auto_fan_uses_half_degree_hysteresis() {
  ControlState state = defaultControlState();
  ControlInputs inputs = normalInputs();

  inputs.sensors.temperatureC = 27.5f;
  ControlEvaluation evaluation = evaluateControl(inputs, state);
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);
  TEST_ASSERT_TRUE(evaluation.nextState.autoFanHigh);

  inputs.sensors.temperatureC = 27.0f;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);

  inputs.sensors.temperatureC = 26.5f;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_EQUAL_UINT8(0, evaluation.outputs.fanPercent);
  TEST_ASSERT_FALSE(evaluation.nextState.autoFanHigh);
}

void test_sleep_has_low_fan_and_yellow_light_off() {
  ControlState state = defaultControlState();
  state.targetMode = Mode::Sleep;

  const ControlEvaluation evaluation = evaluateControl(normalInputs(), state);
  TEST_ASSERT_EQUAL_UINT8(35, evaluation.outputs.fanPercent);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RgbState::Off),
                          static_cast<uint8_t>(evaluation.outputs.rgb));
  TEST_ASSERT_FALSE(evaluation.outputs.relayOn);
}

void assertPrimarySafetyOverride(const ControlInputs& inputs, AlertCode expectedAlert) {
  ControlState state = defaultControlState();
  state.targetMode = Mode::Sleep;
  const ControlEvaluation evaluation = evaluateControl(inputs, state);

  TEST_ASSERT_TRUE(evaluation.outputs.safetyActive);
  TEST_ASSERT_TRUE(hasAlert(evaluation.outputs.activeAlerts, expectedAlert));
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);
  TEST_ASSERT_FALSE(evaluation.outputs.relayOn);
  TEST_ASSERT_TRUE(evaluation.outputs.buzzerOn);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RgbState::Red),
                          static_cast<uint8_t>(evaluation.outputs.rgb));
}

void test_mq2_flame_and_water_each_apply_primary_safety_override() {
  ControlInputs mq2 = normalInputs();
  mq2.sensors.mq2EqPpm = 600;
  assertPrimarySafetyOverride(mq2, AlertCode::Mq2);

  ControlInputs flame = normalInputs();
  flame.sensors.flameDetected = true;
  assertPrimarySafetyOverride(flame, AlertCode::Flame);

  ControlInputs water = normalInputs();
  water.sensors.waterDetected = true;
  assertPrimarySafetyOverride(water, AlertCode::Water);
}

void test_intrusion_does_not_invent_fan_or_relay_actions() {
  ControlState state = defaultControlState();
  state.targetMode = Mode::Sleep;
  ControlInputs inputs = normalInputs();
  inputs.guardArmed = true;
  inputs.sensors.presenceDetected = true;

  const ControlEvaluation evaluation = evaluateControl(inputs, state);
  TEST_ASSERT_TRUE(hasAlert(evaluation.outputs.activeAlerts, AlertCode::Intrusion));
  TEST_ASSERT_EQUAL_UINT8(35, evaluation.outputs.fanPercent);
  TEST_ASSERT_FALSE(evaluation.outputs.relayOn);
  TEST_ASSERT_TRUE(evaluation.outputs.buzzerOn);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RgbState::Red),
                          static_cast<uint8_t>(evaluation.outputs.rgb));
}

void test_explicit_buzzer_mute_keeps_visual_safety_actions() {
  ControlState state = defaultControlState();
  ControlInputs inputs = normalInputs();
  inputs.sensors.waterDetected = true;
  inputs.buzzerEnabled = false;

  const ControlEvaluation evaluation = evaluateControl(inputs, state);
  TEST_ASSERT_TRUE(evaluation.outputs.safetyActive);
  TEST_ASSERT_FALSE(evaluation.outputs.buzzerOn);
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RgbState::Red),
                          static_cast<uint8_t>(evaluation.outputs.rgb));
}

void test_command_decisions_respect_safety_lock_and_deferred_mode() {
  const CommandDecision safeMode = decideModeCommand(false);
  TEST_ASSERT_TRUE(safeMode.ok);
  TEST_ASSERT_FALSE(safeMode.deferredBySafety);

  const CommandDecision deferredMode = decideModeCommand(true);
  TEST_ASSERT_TRUE(deferredMode.ok);
  TEST_ASSERT_TRUE(deferredMode.deferredBySafety);

  const CommandDecision blockedManual = decideManualActuatorCommand(true);
  TEST_ASSERT_FALSE(blockedManual.ok);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandError::SafetyLock),
                          static_cast<uint8_t>(blockedManual.error));
}

void test_water_clears_after_three_seconds_and_recomputes_sleep() {
  ControlState state = defaultControlState();
  state.targetMode = Mode::Sleep;
  ControlInputs inputs = normalInputs(10);
  inputs.sensors.waterDetected = true;

  ControlEvaluation evaluation = evaluateControl(inputs, state);
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);

  inputs.nowMs = 20;
  inputs.sensors.waterDetected = false;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);

  inputs.nowMs = 3019;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_EQUAL_UINT8(100, evaluation.outputs.fanPercent);

  inputs.nowMs = 3020;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_FALSE(evaluation.outputs.safetyActive);
  TEST_ASSERT_EQUAL_UINT8(35, evaluation.outputs.fanPercent);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RgbState::Off),
                          static_cast<uint8_t>(evaluation.outputs.rgb));
}

void test_mq2_clear_uses_lower_threshold_and_stable_window() {
  ControlState state = defaultControlState();
  ControlInputs inputs = normalInputs(100);
  inputs.sensors.mq2EqPpm = 600;
  ControlEvaluation evaluation = evaluateControl(inputs, state);
  TEST_ASSERT_TRUE(hasAlert(evaluation.outputs.activeAlerts, AlertCode::Mq2));

  inputs.nowMs = 200;
  inputs.sensors.mq2EqPpm = 575;
  evaluation = evaluateControl(inputs, evaluation.nextState);

  inputs.nowMs = 300;
  inputs.sensors.mq2EqPpm = 550;
  evaluation = evaluateControl(inputs, evaluation.nextState);

  inputs.nowMs = 3299;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_TRUE(hasAlert(evaluation.outputs.activeAlerts, AlertCode::Mq2));

  inputs.nowMs = 3300;
  evaluation = evaluateControl(inputs, evaluation.nextState);
  TEST_ASSERT_FALSE(hasAlert(evaluation.outputs.activeAlerts, AlertCode::Mq2));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_outputs_are_physically_safe);
  RUN_TEST(test_auto_fan_uses_half_degree_hysteresis);
  RUN_TEST(test_sleep_has_low_fan_and_yellow_light_off);
  RUN_TEST(test_mq2_flame_and_water_each_apply_primary_safety_override);
  RUN_TEST(test_intrusion_does_not_invent_fan_or_relay_actions);
  RUN_TEST(test_explicit_buzzer_mute_keeps_visual_safety_actions);
  RUN_TEST(test_command_decisions_respect_safety_lock_and_deferred_mode);
  RUN_TEST(test_water_clears_after_three_seconds_and_recomputes_sleep);
  RUN_TEST(test_mq2_clear_uses_lower_threshold_and_stable_window);
  return UNITY_END();
}
