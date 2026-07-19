#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "smartlife_config.h"
#include "smartlife_models.h"
#include "solar_terms.h"

namespace smartlife {

struct ManualActuatorOverrides {
  bool fanPercentSet = false;
  int fanPercent = 0;
  bool curtainClosePercentSet = false;
  int curtainClosePercent = 50;
  bool relaySet = false;
  bool relayOn = false;
  bool buzzerSet = false;
  bool buzzerOn = false;
  bool rgbSet = false;
  RgbState rgb = RgbState::Off;
};

struct ProtocolRuntimeState {
  Mode targetMode = Mode::Auto;
  SolarTerm solarTerm = SolarTerm::Lichun;
  bool guardArmed = false;
  bool buzzerEnabled = true;
  ManualActuatorOverrides manual{};
  std::string lastAppliedCommandId{};
  uint32_t appliedCommandCount = 0;
};

struct ProtocolReply {
  std::string line{};
  bool duplicate = false;
  bool stateChanged = false;

  ProtocolReply() = default;
  ProtocolReply(const std::string& replyLine,
                bool isDuplicate,
                bool changed)
      : line(replyLine), duplicate(isDuplicate), stateChanged(changed) {}
};

class CommandProcessor {
 public:
  ProtocolReply processLine(const std::string& line,
                            bool safetyActive,
                            ProtocolRuntimeState& state);

 private:
  struct CachedAck {
    bool valid = false;
    std::string id{};
    std::string line{};
  };

  const std::string* findCachedAck(const std::string& id) const;
  void cacheAck(const std::string& id, const std::string& line);

  std::array<CachedAck, COMMAND_ACK_CACHE_SIZE> ackCache_{};
  std::size_t nextCacheIndex_ = 0;
};

void clearManualOverrides(ProtocolRuntimeState& state);
void applyManualOverrides(const ProtocolRuntimeState& state,
                          ControlOutputs& outputs);

}  // namespace smartlife
