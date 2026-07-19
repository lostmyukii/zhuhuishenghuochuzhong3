#pragma once

#include <cstdint>

namespace smartlife {

enum class Mode : uint8_t {
  Auto,
  Sleep,
};

enum class RgbState : uint8_t {
  Off,
  Yellow,
  Red,
};

enum class AlertCode : uint8_t {
  None = 0,
  Mq2 = 1 << 0,
  Flame = 1 << 1,
  Water = 1 << 2,
  Intrusion = 1 << 3,
};

}  // namespace smartlife
