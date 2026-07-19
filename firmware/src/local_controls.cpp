#include "local_controls.h"

#include "smartlife_config.h"

namespace smartlife {

bool isButtonAAdc(int rawValue) {
  return rawValue >= KEYPAD_A_ADC_MIN && rawValue <= KEYPAD_A_ADC_MAX;
}

Mode toggleMode(Mode currentMode) {
  return currentMode == Mode::Auto ? Mode::Sleep : Mode::Auto;
}

ButtonAUpdate ButtonADebouncer::update(int rawValue, uint32_t nowMs) {
  ButtonAUpdate update{};
  const bool rawPressed = isButtonAAdc(rawValue);

  if (!candidateInitialized_) {
    candidateInitialized_ = true;
    candidatePressed_ = rawPressed;
    candidateSinceMs_ = nowMs;
    update.stablePressed = stablePressed_;
    return update;
  }

  if (rawPressed != candidatePressed_) {
    candidatePressed_ = rawPressed;
    candidateSinceMs_ = nowMs;
    update.stablePressed = stablePressed_;
    return update;
  }

  if (candidatePressed_ != stablePressed_ &&
      static_cast<uint32_t>(nowMs - candidateSinceMs_) >=
          KEYPAD_DEBOUNCE_MS) {
    stablePressed_ = candidatePressed_;
    if (stablePressed_) {
      update.clicked = true;
    } else {
      update.released = true;
    }
  }

  update.stablePressed = stablePressed_;
  return update;
}

}  // namespace smartlife
