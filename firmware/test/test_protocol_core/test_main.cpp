#include <ArduinoJson.h>
#include <unity.h>

#include <string>

#include "protocol_core.h"

using namespace smartlife;

namespace {

JsonDocument parseAck(const std::string& line) {
  JsonDocument doc;
  TEST_ASSERT_FALSE(static_cast<bool>(deserializeJson(doc, line)));
  return doc;
}

void assertStateIsDefault(const ProtocolRuntimeState& state) {
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Auto),
                    static_cast<int>(state.targetMode));
  TEST_ASSERT_EQUAL(static_cast<int>(SolarTerm::Lichun),
                    static_cast<int>(state.solarTerm));
  TEST_ASSERT_FALSE(state.guardArmed);
  TEST_ASSERT_TRUE(state.buzzerEnabled);
  TEST_ASSERT_EQUAL_UINT32(0, state.appliedCommandCount);
  TEST_ASSERT_TRUE(state.lastAppliedCommandId.empty());
}

void test_valid_commands_apply_only_whitelisted_state() {
  CommandProcessor processor;
  ProtocolRuntimeState state;

  ProtocolReply mode = processor.processLine(
      R"({"type":"command","id":"cmd-001","origin":"dashboard","mode":"Sleep"})",
      false,
      state);
  JsonDocument modeAck = parseAck(mode.line);
  TEST_ASSERT_TRUE(modeAck["ok"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("Sleep", modeAck["applied"]["mode"]);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Sleep),
                    static_cast<int>(state.targetMode));

  ProtocolReply term = processor.processLine(
      R"({"type":"command","id":"cmd-002","origin":"voice","set":{"solarTerm":"大寒"}})",
      false,
      state);
  TEST_ASSERT_TRUE(parseAck(term.line)["ok"].as<bool>());
  TEST_ASSERT_EQUAL(static_cast<int>(SolarTerm::Dahan),
                    static_cast<int>(state.solarTerm));

  ProtocolReply guard = processor.processLine(
      R"({"type":"command","id":"cmd-003","origin":"test","set":{"guardArmed":true}})",
      false,
      state);
  TEST_ASSERT_TRUE(parseAck(guard.line)["ok"].as<bool>());
  TEST_ASSERT_TRUE(state.guardArmed);
  TEST_ASSERT_EQUAL_STRING("cmd-003", state.lastAppliedCommandId.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, state.appliedCommandCount);
}

void test_missing_id_bad_origin_mode_term_and_range_do_not_mutate_state() {
  const char* invalidLines[] = {
      R"({"type":"command","origin":"dashboard","mode":"Sleep"})",
      R"({"type":"command","id":"bad-origin","origin":"cloud","mode":"Sleep"})",
      R"({"type":"command","id":"bad-mode","origin":"dashboard","mode":"Away"})",
      R"({"type":"command","id":"bad-term","origin":"dashboard","set":{"solarTerm":"盛夏"}})",
      R"({"type":"command","id":"bad-range","origin":"dashboard","actuator":{"fanPercent":101}})",
  };
  const char* expectedErrors[] = {
      "missing_id", "unsupported_origin", "invalid_mode",
      "invalid_solar_term", "out_of_range",
  };

  CommandProcessor processor;
  ProtocolRuntimeState state;
  for (size_t index = 0; index < 5; ++index) {
    ProtocolReply reply = processor.processLine(invalidLines[index], false, state);
    JsonDocument ack = parseAck(reply.line);
    TEST_ASSERT_FALSE(ack["ok"].as<bool>());
    TEST_ASSERT_EQUAL_STRING(expectedErrors[index], ack["error"]);
    assertStateIsDefault(state);
  }
}

void test_duplicate_id_returns_exact_first_ack_without_reapplying() {
  CommandProcessor processor;
  ProtocolRuntimeState state;
  const std::string command =
      R"({"type":"command","id":"same-id","origin":"dashboard","set":{"guardArmed":true}})";

  ProtocolReply first = processor.processLine(command, false, state);
  ProtocolReply duplicate = processor.processLine(command, false, state);
  TEST_ASSERT_FALSE(first.duplicate);
  TEST_ASSERT_TRUE(duplicate.duplicate);
  TEST_ASSERT_EQUAL_STRING(first.line.c_str(), duplicate.line.c_str());
  TEST_ASSERT_EQUAL_UINT32(1, state.appliedCommandCount);
}

void test_safety_defers_mode_and_rejects_manual_actuator() {
  CommandProcessor processor;
  ProtocolRuntimeState state;

  ProtocolReply mode = processor.processLine(
      R"({"type":"command","id":"safe-mode","origin":"dashboard","mode":"Sleep"})",
      true,
      state);
  JsonDocument modeAck = parseAck(mode.line);
  TEST_ASSERT_TRUE(modeAck["ok"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("safety", modeAck["deferredBy"]);
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Sleep),
                    static_cast<int>(state.targetMode));

  ProtocolReply actuator = processor.processLine(
      R"({"type":"command","id":"blocked-fan","origin":"dashboard","actuator":{"fanPercent":0}})",
      true,
      state);
  JsonDocument actuatorAck = parseAck(actuator.line);
  TEST_ASSERT_FALSE(actuatorAck["ok"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("safety_lock", actuatorAck["error"]);
  TEST_ASSERT_FALSE(state.manual.fanPercentSet);
}

void test_manual_outputs_never_override_a_safety_evaluation() {
  CommandProcessor processor;
  ProtocolRuntimeState state;
  processor.processLine(
      R"({"type":"command","id":"manual-fan","origin":"test","actuator":{"fanPercent":10}})",
      false,
      state);

  ControlOutputs normal{};
  normal.fanPercent = 100;
  applyManualOverrides(state, normal);
  TEST_ASSERT_EQUAL_UINT8(10, normal.fanPercent);

  ControlOutputs safety{};
  safety.safetyActive = true;
  safety.fanPercent = 100;
  safety.rgb = RgbState::Red;
  applyManualOverrides(state, safety);
  TEST_ASSERT_EQUAL_UINT8(100, safety.fanPercent);
  TEST_ASSERT_EQUAL(static_cast<int>(RgbState::Red),
                    static_cast<int>(safety.rgb));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_commands_apply_only_whitelisted_state);
  RUN_TEST(test_missing_id_bad_origin_mode_term_and_range_do_not_mutate_state);
  RUN_TEST(test_duplicate_id_returns_exact_first_ack_without_reapplying);
  RUN_TEST(test_safety_defers_mode_and_rejects_manual_actuator);
  RUN_TEST(test_manual_outputs_never_override_a_safety_evaluation);
  return UNITY_END();
}
