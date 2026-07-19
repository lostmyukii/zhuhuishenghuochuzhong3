#include <unity.h>

#include "local_controls.h"
#include "smartlife_config.h"

using namespace smartlife;

namespace {

void test_button_a_adc_window_is_explicit() {
  TEST_ASSERT_TRUE(isButtonAAdc(KEYPAD_A_ADC_MIN));
  TEST_ASSERT_TRUE(isButtonAAdc(KEYPAD_A_ADC_MAX));
  TEST_ASSERT_FALSE(isButtonAAdc(KEYPAD_A_ADC_MIN - 1));
  TEST_ASSERT_FALSE(isButtonAAdc(KEYPAD_A_ADC_MAX + 1));
}

void test_button_a_debounce_emits_once_per_press_release_cycle() {
  ButtonADebouncer button;

  TEST_ASSERT_FALSE(button.update(0, 0).clicked);
  TEST_ASSERT_FALSE(button.update(KEYPAD_A_ADC_MIN, 10).clicked);
  TEST_ASSERT_FALSE(
      button.update(KEYPAD_A_ADC_MIN, 10 + KEYPAD_DEBOUNCE_MS - 1).clicked);
  TEST_ASSERT_TRUE(
      button.update(KEYPAD_A_ADC_MIN, 10 + KEYPAD_DEBOUNCE_MS).clicked);
  TEST_ASSERT_FALSE(button.update(KEYPAD_A_ADC_MIN, 1000).clicked);

  TEST_ASSERT_FALSE(button.update(0, 1010).released);
  TEST_ASSERT_TRUE(button.update(0, 1010 + KEYPAD_DEBOUNCE_MS).released);
  TEST_ASSERT_FALSE(button.update(KEYPAD_A_ADC_MAX, 1100).clicked);
  TEST_ASSERT_TRUE(
      button.update(KEYPAD_A_ADC_MAX, 1100 + KEYPAD_DEBOUNCE_MS).clicked);
}

void test_mode_toggle_is_strictly_auto_sleep_auto() {
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Sleep),
                    static_cast<int>(toggleMode(Mode::Auto)));
  TEST_ASSERT_EQUAL(static_cast<int>(Mode::Auto),
                    static_cast<int>(toggleMode(Mode::Sleep)));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_button_a_adc_window_is_explicit);
  RUN_TEST(test_button_a_debounce_emits_once_per_press_release_cycle);
  RUN_TEST(test_mode_toggle_is_strictly_auto_sleep_auto);
  return UNITY_END();
}
