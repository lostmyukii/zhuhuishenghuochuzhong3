#include <unity.h>

#include <array>
#include <cstring>

#include "control_engine.h"
#include "solar_terms.h"

using namespace smartlife;

namespace {

void test_all_24_solar_terms_are_present_in_order() {
  const char* expectedNames[SOLAR_TERM_COUNT] = {
      "立春", "雨水", "惊蛰", "春分", "清明", "谷雨",
      "立夏", "小满", "芒种", "夏至", "小暑", "大暑",
      "立秋", "处暑", "白露", "秋分", "寒露", "霜降",
      "立冬", "小雪", "大雪", "冬至", "小寒", "大寒",
  };

  const auto& profiles = solarTermProfiles();
  TEST_ASSERT_EQUAL_UINT32(SOLAR_TERM_COUNT, profiles.size());
  for (size_t index = 0; index < profiles.size(); ++index) {
    TEST_ASSERT_EQUAL_STRING(expectedNames[index], profiles[index].name);
    TEST_ASSERT_EQUAL_UINT8(index, static_cast<uint8_t>(profiles[index].term));
  }
}

void test_xiaoshu_and_dahan_match_approved_strategy() {
  const SolarTermProfile& xiaoshu = solarTermProfile(SolarTerm::Xiaoshu);
  TEST_ASSERT_EQUAL_INT(26, xiaoshu.recommendedTemperatureC);
  TEST_ASSERT_EQUAL_INT(80, xiaoshu.curtainClosePercent);
  TEST_ASSERT_EQUAL_INT(30, xiaoshu.lightThreshold);

  const SolarTermProfile& dahan = solarTermProfile(SolarTerm::Dahan);
  TEST_ASSERT_EQUAL_INT(24, dahan.recommendedTemperatureC);
  TEST_ASSERT_EQUAL_INT(20, dahan.curtainClosePercent);
  TEST_ASSERT_EQUAL_INT(45, dahan.lightThreshold);
}

void test_solar_term_name_lookup_rejects_unknown_values() {
  SolarTerm term = SolarTerm::Lichun;
  TEST_ASSERT_TRUE(solarTermFromName("小暑", term));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SolarTerm::Xiaoshu),
                          static_cast<uint8_t>(term));
  TEST_ASSERT_FALSE(solarTermFromName("盛夏", term));
}

void test_knob_maps_adc_endpoints_and_midpoint_to_18_35_celsius() {
  TEST_ASSERT_EQUAL_INT(18, mapKnobRawToThresholdC(-1));
  TEST_ASSERT_EQUAL_INT(18, mapKnobRawToThresholdC(0));
  TEST_ASSERT_EQUAL_INT(27, mapKnobRawToThresholdC(2048));
  TEST_ASSERT_EQUAL_INT(35, mapKnobRawToThresholdC(4095));
  TEST_ASSERT_EQUAL_INT(35, mapKnobRawToThresholdC(5000));
}

void test_knob_filter_uses_median_and_deadband() {
  const std::array<int, 5> noisySamples = {2048, 4095, 2047, 0, 2050};
  TEST_ASSERT_EQUAL_INT(2048, medianOfFiveRaw(noisySamples));
  TEST_ASSERT_EQUAL_INT(2048, applyKnobDeadband(2048, 2055, 16));
  TEST_ASSERT_EQUAL_INT(2064, applyKnobDeadband(2048, 2064, 16));
  TEST_ASSERT_EQUAL_INT(0, applyKnobDeadband(10, -50, 16));
  TEST_ASSERT_EQUAL_INT(4095, applyKnobDeadband(4000, 5000, 16));
}

ControlInputs autoInputsFor(const SolarTermProfile& profile) {
  ControlInputs inputs{};
  inputs.sensors.temperatureValid = true;
  inputs.sensors.temperatureC = 27.5f;
  inputs.sensors.mq2Ready = true;
  inputs.sensors.mq2EqPpm = 100;
  inputs.sensors.presenceDetected = true;
  inputs.sensors.lightRelative = 40;
  inputs.temperatureThresholdC = 27.0f;
  inputs.solarCurtainClosePercent = profile.curtainClosePercent;
  inputs.solarLightThreshold = profile.lightThreshold;
  return inputs;
}

void test_solar_strategy_changes_curtain_and_light_but_not_knob_threshold() {
  ControlState state = defaultControlState();

  ControlEvaluation xiaoshu = evaluateControl(
      autoInputsFor(solarTermProfile(SolarTerm::Xiaoshu)), state);
  TEST_ASSERT_EQUAL_INT(80, xiaoshu.outputs.curtainClosePercent);
  TEST_ASSERT_TRUE(xiaoshu.outputs.curtainControlEnabled);
  TEST_ASSERT_FALSE(xiaoshu.outputs.relayOn);
  TEST_ASSERT_EQUAL_UINT8(100, xiaoshu.outputs.fanPercent);

  ControlEvaluation dahan = evaluateControl(
      autoInputsFor(solarTermProfile(SolarTerm::Dahan)), state);
  TEST_ASSERT_EQUAL_INT(20, dahan.outputs.curtainClosePercent);
  TEST_ASSERT_TRUE(dahan.outputs.relayOn);
  TEST_ASSERT_EQUAL_UINT8(100, dahan.outputs.fanPercent);
}

void test_sleep_pauses_solar_curtain_and_supplemental_light() {
  ControlState state = defaultControlState();
  state.targetMode = Mode::Sleep;
  const ControlEvaluation evaluation = evaluateControl(
      autoInputsFor(solarTermProfile(SolarTerm::Dahan)), state);

  TEST_ASSERT_FALSE(evaluation.outputs.curtainControlEnabled);
  TEST_ASSERT_FALSE(evaluation.outputs.relayOn);
  TEST_ASSERT_EQUAL_UINT8(35, evaluation.outputs.fanPercent);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_all_24_solar_terms_are_present_in_order);
  RUN_TEST(test_xiaoshu_and_dahan_match_approved_strategy);
  RUN_TEST(test_solar_term_name_lookup_rejects_unknown_values);
  RUN_TEST(test_knob_maps_adc_endpoints_and_midpoint_to_18_35_celsius);
  RUN_TEST(test_knob_filter_uses_median_and_deadband);
  RUN_TEST(test_solar_strategy_changes_curtain_and_light_but_not_knob_threshold);
  RUN_TEST(test_sleep_pauses_solar_curtain_and_supplemental_light);
  return UNITY_END();
}
