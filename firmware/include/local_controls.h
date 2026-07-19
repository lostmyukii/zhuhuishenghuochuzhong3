#pragma once

#include <cstdint>

#include "smartlife_models.h"

namespace smartlife {

struct ButtonAUpdate {
  bool stablePressed = false;
  bool clicked = false;
  bool released = false;
};

bool isButtonAAdc(int rawValue);
Mode toggleMode(Mode currentMode);

class ButtonADebouncer {
 public:
  ButtonAUpdate update(int rawValue, uint32_t nowMs);

 private:
  bool candidateInitialized_ = false;
  bool candidatePressed_ = false;
  bool stablePressed_ = false;
  uint32_t candidateSinceMs_ = 0;
};

}  // namespace smartlife
