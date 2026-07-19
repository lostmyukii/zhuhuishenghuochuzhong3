#include "protocol_core.h"

#include <ArduinoJson.h>

#include <cstring>

namespace smartlife {
namespace {

std::string serializeDocument(JsonDocument& doc) {
  std::string line;
  serializeJson(doc, line);
  return line;
}

std::string errorAck(const char* id,
                     const char* error,
                     const char* message) {
  JsonDocument ack;
  ack["type"] = "ack";
  ack["id"] = id == nullptr ? "" : id;
  ack["ok"] = false;
  ack["error"] = error;
  ack["message"] = message;
  return serializeDocument(ack);
}

void successAckBase(const char* id, JsonDocument& ack) {
  ack["type"] = "ack";
  ack["id"] = id;
  ack["ok"] = true;
}

bool supportedOrigin(const char* origin) {
  return origin != nullptr &&
         (std::strcmp(origin, "dashboard") == 0 ||
          std::strcmp(origin, "voice") == 0 ||
          std::strcmp(origin, "test") == 0);
}

bool parseMode(const char* value, Mode& mode) {
  if (value != nullptr && std::strcmp(value, "Auto") == 0) {
    mode = Mode::Auto;
    return true;
  }
  if (value != nullptr && std::strcmp(value, "Sleep") == 0) {
    mode = Mode::Sleep;
    return true;
  }
  return false;
}

bool parseRgb(const char* value, RgbState& rgb) {
  if (value != nullptr && std::strcmp(value, "off") == 0) {
    rgb = RgbState::Off;
    return true;
  }
  if (value != nullptr && std::strcmp(value, "yellow") == 0) {
    rgb = RgbState::Yellow;
    return true;
  }
  if (value != nullptr && std::strcmp(value, "red") == 0) {
    rgb = RgbState::Red;
    return true;
  }
  return false;
}

std::size_t objectSize(JsonObjectConst object) {
  std::size_t size = 0;
  for (JsonPairConst ignored : object) {
    static_cast<void>(ignored);
    ++size;
  }
  return size;
}

bool objectHasKey(JsonObjectConst object, const char* expectedKey) {
  for (JsonPairConst pair : object) {
    if (std::strcmp(pair.key().c_str(), expectedKey) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

ProtocolReply CommandProcessor::processLine(const std::string& line,
                                            bool safetyActive,
                                            ProtocolRuntimeState& state) {
  JsonDocument command;
  const DeserializationError parseError = deserializeJson(command, line);
  if (parseError) {
    return {errorAck("", "invalid_json", "命令不是有效 JSON"), false, false};
  }

  const JsonObjectConst root = command.as<JsonObjectConst>();
  const char* id = root["id"].is<const char*>()
                       ? root["id"].as<const char*>()
                       : nullptr;
  if (id == nullptr || id[0] == '\0') {
    return {errorAck("", "missing_id", "命令缺少非空 id"), false, false};
  }
  if (std::strlen(id) > COMMAND_ID_MAX_LENGTH) {
    return {errorAck(id, "out_of_range", "命令 id 过长"), false, false};
  }

  if (const std::string* cached = findCachedAck(id)) {
    return {*cached, true, false};
  }

  const auto fail = [&](const char* error, const char* message) {
    const std::string ack = errorAck(id, error, message);
    cacheAck(id, ack);
    return ProtocolReply{ack, false, false};
  };

  const char* type = root["type"].is<const char*>()
                         ? root["type"].as<const char*>()
                         : nullptr;
  if (type == nullptr || std::strcmp(type, "command") != 0) {
    return fail("unsupported_command", "type 必须是 command");
  }

  const char* origin = root["origin"].is<const char*>()
                           ? root["origin"].as<const char*>()
                           : nullptr;
  if (!supportedOrigin(origin)) {
    return fail("unsupported_origin", "origin 不在白名单");
  }

  const bool hasMode = objectHasKey(root, "mode");
  const bool hasSet = objectHasKey(root, "set");
  const bool hasActuator = objectHasKey(root, "actuator");
  if (static_cast<int>(hasMode) + static_cast<int>(hasSet) +
          static_cast<int>(hasActuator) !=
      1) {
    return fail("unsupported_command", "每条命令只能包含一个动作");
  }

  const auto accept = [&](JsonDocument& ack) {
    state.lastAppliedCommandId = id;
    ++state.appliedCommandCount;
    const std::string line = serializeDocument(ack);
    cacheAck(id, line);
    return ProtocolReply{line, false, true};
  };

  if (hasMode) {
    Mode requestedMode = Mode::Auto;
    const char* modeValue = root["mode"].is<const char*>()
                                ? root["mode"].as<const char*>()
                                : nullptr;
    if (!parseMode(modeValue, requestedMode)) {
      return fail("invalid_mode", "mode 只接受 Auto 或 Sleep");
    }

    state.targetMode = requestedMode;
    clearManualOverrides(state);
    JsonDocument ack;
    successAckBase(id, ack);
    JsonObject applied = ack["applied"].to<JsonObject>();
    applied["mode"] = requestedMode == Mode::Auto ? "Auto" : "Sleep";
    if (safetyActive) {
      ack["deferredBy"] = "safety";
    }
    return accept(ack);
  }

  if (hasSet) {
    if (!root["set"].is<JsonObjectConst>()) {
      return fail("unsupported_command", "set 必须是对象");
    }
    const JsonObjectConst set = root["set"].as<JsonObjectConst>();
    if (objectSize(set) != 1) {
      return fail("unsupported_command", "set 每次只允许一个字段");
    }

    JsonDocument ack;
    successAckBase(id, ack);
    JsonObject applied = ack["applied"].to<JsonObject>();
    if (objectHasKey(set, "solarTerm")) {
      const char* name = set["solarTerm"].is<const char*>()
                             ? set["solarTerm"].as<const char*>()
                             : nullptr;
      SolarTerm term = SolarTerm::Lichun;
      if (!solarTermFromName(name, term)) {
        return fail("invalid_solar_term", "solarTerm 必须是准确节气名称");
      }
      state.solarTerm = term;
      applied["solarTerm"] = solarTermProfile(term).name;
      return accept(ack);
    }
    if (objectHasKey(set, "guardArmed")) {
      if (!set["guardArmed"].is<bool>()) {
        return fail("unsupported_command", "guardArmed 必须是布尔值");
      }
      state.guardArmed = set["guardArmed"].as<bool>();
      applied["guardArmed"] = state.guardArmed;
      return accept(ack);
    }
    if (objectHasKey(set, "buzzerEnabled")) {
      if (!set["buzzerEnabled"].is<bool>()) {
        return fail("unsupported_command", "buzzerEnabled 必须是布尔值");
      }
      state.buzzerEnabled = set["buzzerEnabled"].as<bool>();
      applied["buzzerEnabled"] = state.buzzerEnabled;
      return accept(ack);
    }
    return fail("unsupported_command", "未知 set 字段");
  }

  if (!root["actuator"].is<JsonObjectConst>()) {
    return fail("unsupported_command", "actuator 必须是对象");
  }
  const JsonObjectConst actuator = root["actuator"].as<JsonObjectConst>();
  if (objectSize(actuator) != 1) {
    return fail("unsupported_command", "actuator 每次只允许一个字段");
  }

  enum class ManualKind : uint8_t {
    Fan,
    Curtain,
    Relay,
    Buzzer,
    Rgb,
  };
  ManualKind kind = ManualKind::Fan;
  int integerValue = 0;
  bool booleanValue = false;
  RgbState rgbValue = RgbState::Off;

  if (objectHasKey(actuator, "fanPercent")) {
    if (!actuator["fanPercent"].is<int>()) {
      return fail("unsupported_command", "fanPercent 必须是整数");
    }
    integerValue = actuator["fanPercent"].as<int>();
    if (integerValue < 0 || integerValue > 100) {
      return fail("out_of_range", "fanPercent 范围为 0 到 100");
    }
    kind = ManualKind::Fan;
  } else if (objectHasKey(actuator, "curtainClosePercent")) {
    if (!actuator["curtainClosePercent"].is<int>()) {
      return fail("unsupported_command", "curtainClosePercent 必须是整数");
    }
    integerValue = actuator["curtainClosePercent"].as<int>();
    if (integerValue < 0 || integerValue > 100) {
      return fail("out_of_range", "curtainClosePercent 范围为 0 到 100");
    }
    kind = ManualKind::Curtain;
  } else if (objectHasKey(actuator, "relay")) {
    if (!actuator["relay"].is<bool>()) {
      return fail("unsupported_command", "relay 必须是布尔值");
    }
    booleanValue = actuator["relay"].as<bool>();
    kind = ManualKind::Relay;
  } else if (objectHasKey(actuator, "buzzer")) {
    if (!actuator["buzzer"].is<bool>()) {
      return fail("unsupported_command", "buzzer 必须是布尔值");
    }
    booleanValue = actuator["buzzer"].as<bool>();
    kind = ManualKind::Buzzer;
  } else if (objectHasKey(actuator, "rgb")) {
    const char* rgb = actuator["rgb"].is<const char*>()
                          ? actuator["rgb"].as<const char*>()
                          : nullptr;
    if (!parseRgb(rgb, rgbValue)) {
      return fail("unsupported_command", "rgb 只接受 off/yellow/red");
    }
    kind = ManualKind::Rgb;
  } else {
    return fail("unsupported_command", "未知 actuator 字段");
  }

  if (safetyActive) {
    return fail("safety_lock", "安全告警期间不能覆盖执行器");
  }

  JsonDocument ack;
  successAckBase(id, ack);
  JsonObject applied = ack["applied"].to<JsonObject>();
  JsonObject appliedActuator = applied["actuator"].to<JsonObject>();
  switch (kind) {
    case ManualKind::Fan:
      state.manual.fanPercentSet = true;
      state.manual.fanPercent = integerValue;
      appliedActuator["fanPercent"] = integerValue;
      break;
    case ManualKind::Curtain:
      state.manual.curtainClosePercentSet = true;
      state.manual.curtainClosePercent = integerValue;
      appliedActuator["curtainClosePercent"] = integerValue;
      break;
    case ManualKind::Relay:
      state.manual.relaySet = true;
      state.manual.relayOn = booleanValue;
      appliedActuator["relay"] = booleanValue;
      break;
    case ManualKind::Buzzer:
      state.manual.buzzerSet = true;
      state.manual.buzzerOn = booleanValue;
      appliedActuator["buzzer"] = booleanValue;
      break;
    case ManualKind::Rgb:
      state.manual.rgbSet = true;
      state.manual.rgb = rgbValue;
      appliedActuator["rgb"] = rgbValue == RgbState::Off
                                    ? "off"
                                    : (rgbValue == RgbState::Yellow ? "yellow"
                                                                    : "red");
      break;
  }
  return accept(ack);
}

const std::string* CommandProcessor::findCachedAck(
    const std::string& id) const {
  for (const CachedAck& cached : ackCache_) {
    if (cached.valid && cached.id == id) {
      return &cached.line;
    }
  }
  return nullptr;
}

void CommandProcessor::cacheAck(const std::string& id,
                                const std::string& line) {
  CachedAck& slot = ackCache_[nextCacheIndex_];
  slot.valid = true;
  slot.id = id;
  slot.line = line;
  nextCacheIndex_ = (nextCacheIndex_ + 1) % ackCache_.size();
}

void clearManualOverrides(ProtocolRuntimeState& state) {
  state.manual = ManualActuatorOverrides{};
}

void applyManualOverrides(const ProtocolRuntimeState& state,
                          ControlOutputs& outputs) {
  if (outputs.safetyActive) {
    return;
  }
  if (state.manual.fanPercentSet) {
    outputs.fanPercent = static_cast<uint8_t>(state.manual.fanPercent);
  }
  if (state.manual.curtainClosePercentSet) {
    outputs.curtainControlEnabled = true;
    outputs.curtainClosePercent = state.manual.curtainClosePercent;
  }
  if (state.manual.relaySet) {
    outputs.relayOn = state.manual.relayOn;
  }
  if (state.manual.buzzerSet) {
    outputs.buzzerOn = state.manual.buzzerOn;
  }
  if (state.manual.rgbSet) {
    outputs.rgb = state.manual.rgb;
  }
}

}  // namespace smartlife
