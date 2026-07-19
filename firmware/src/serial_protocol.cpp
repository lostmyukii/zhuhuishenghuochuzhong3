#include "serial_protocol.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstdio>
#include <string>

#include "control_engine.h"
#include "smartlife_config.h"
#include "solar_terms.h"

namespace smartlife {
namespace {

const char* modeText(Mode mode) {
  return mode == Mode::Auto ? "Auto" : "Sleep";
}

const char* rgbText(RgbState rgb) {
  if (rgb == RgbState::Yellow) {
    return "yellow";
  }
  if (rgb == RgbState::Red) {
    return "red";
  }
  return "off";
}

void serializeLine(JsonDocument& doc, Stream& serial) {
  serializeJson(doc, serial);
  serial.println();
}

void addAlerts(JsonArray alerts, uint8_t activeAlerts) {
  if (hasAlert(activeAlerts, AlertCode::Mq2)) {
    alerts.add("mq2");
  }
  if (hasAlert(activeAlerts, AlertCode::Flame)) {
    alerts.add("flame");
  }
  if (hasAlert(activeAlerts, AlertCode::Water)) {
    alerts.add("water");
  }
  if (hasAlert(activeAlerts, AlertCode::Intrusion)) {
    alerts.add("intrusion");
  }
}

}  // namespace

void SerialProtocol::emitHello(Stream& serial,
                               const ProtocolRuntimeState& state,
                               bool oledReady) {
  JsonDocument doc;
  doc["type"] = "hello";
  doc["protocolVersion"] = PROTOCOL_VERSION;
  doc["project"] = PROJECT_NAME;
  doc["profileId"] = PROFILE_ID;
  doc["board"] = BOARD_ID;
  doc["baud"] = SERIAL_BAUD;
  doc["rfid"] = RFID_ENABLED;
  JsonObject health = doc["health"].to<JsonObject>();
  health["oled"] = oledReady ? "ready" : "error";
  health["buzzerEnabled"] = state.buzzerEnabled;
  serializeLine(doc, serial);
}

void SerialProtocol::poll(Stream& serial,
                          ProtocolRuntimeState& state,
                          bool safetyActive) {
  while (serial.available() > 0) {
    const int next = serial.read();
    if (next < 0) {
      return;
    }
    const char character = static_cast<char>(next);
    if (character == '\r') {
      continue;
    }
    if (character != '\n') {
      if (inputOverflow_) {
        continue;
      }
      if (inputLength_ >= SERIAL_INPUT_MAX_LENGTH) {
        inputOverflow_ = true;
        inputLength_ = 0;
        continue;
      }
      inputBuffer_[inputLength_++] = character;
      continue;
    }

    if (inputOverflow_) {
      JsonDocument doc;
      doc["type"] = "ack";
      doc["id"] = "";
      doc["ok"] = false;
      doc["error"] = "line_too_long";
      doc["message"] = "命令行过长";
      serializeLine(doc, serial);
      inputOverflow_ = false;
      continue;
    }
    if (inputLength_ == 0) {
      continue;
    }

    inputBuffer_[inputLength_] = '\0';
    const ProtocolReply reply = commandProcessor_.processLine(
        std::string(inputBuffer_.data(), inputLength_), safetyActive, state);
    serial.println(reply.line.c_str());
    if (reply.stateChanged) {
      telemetryRequested_ = true;
    }
    inputLength_ = 0;
  }
}

bool SerialProtocol::telemetryDue(uint32_t nowMs) const {
  return telemetryRequested_ || !hasTelemetry_ ||
         static_cast<uint32_t>(nowMs - lastTelemetryMs_) >=
             TELEMETRY_INTERVAL_MS;
}

void SerialProtocol::emitTelemetry(Stream& serial,
                                   uint32_t nowMs,
                                   const ProtocolRuntimeState& state,
                                   const SensorSnapshot& snapshot,
                                   float temperatureThresholdC,
                                   const ControlOutputs& outputs,
                                   bool oledReady,
                                   bool thresholdPage) {
  JsonDocument doc;
  doc["type"] = "telemetry";
  doc["protocolVersion"] = PROTOCOL_VERSION;
  doc["seq"] = ++sequence_;
  doc["uptimeMs"] = nowMs;
  doc["mode"] = modeText(outputs.mode);
  doc["solarTerm"] = solarTermProfile(state.solarTerm).name;

  JsonObject sensors = doc["sensors"].to<JsonObject>();
  if (snapshot.temperatureValid) {
    sensors["temperatureC"] = snapshot.temperatureC;
  } else {
    sensors["temperatureC"] = nullptr;
  }
  if (snapshot.humidityValid) {
    sensors["humidityRh"] = snapshot.humidityRh;
  } else {
    sensors["humidityRh"] = nullptr;
  }
  if (snapshot.mq2Ready) {
    sensors["airQualityEqPpm"] = snapshot.mq2EqPpm;
  } else {
    sensors["airQualityEqPpm"] = nullptr;
  }
  sensors["soundRelative"] = snapshot.soundRelative;
  sensors["presence"] = snapshot.presenceDetected;
  sensors["lightRelative"] = snapshot.lightRelative;
  sensors["water"] = snapshot.waterDetected;
  sensors["flame"] = snapshot.flameDetected;
  sensors["keypadRaw"] = snapshot.keypadRaw;
  if (snapshot.knobValid) {
    sensors["knobRaw"] = snapshot.knobRaw;
  } else {
    sensors["knobRaw"] = nullptr;
  }

  JsonObject thresholds = doc["thresholds"].to<JsonObject>();
  thresholds["temperatureC"] =
      static_cast<int>(std::lround(temperatureThresholdC));
  thresholds["mq2EqPpm"] = MQ2_ALERT_EQ_PPM;

  JsonObject actuators = doc["actuators"].to<JsonObject>();
  actuators["fanPercent"] = outputs.fanPercent;
  actuators["curtainClosePercent"] = outputs.curtainClosePercent;
  actuators["curtainControlEnabled"] = outputs.curtainControlEnabled;
  actuators["relay"] = outputs.relayOn;
  actuators["buzzer"] = outputs.buzzerOn;
  actuators["rgb"] = rgbText(outputs.rgb);

  char line1[32] = {};
  char line2[32] = {};
  char line3[32] = {};
  if (thresholdPage) {
    std::snprintf(line1, sizeof(line1), "Threshold");
    if (snapshot.knobValid) {
      std::snprintf(line2, sizeof(line2), "XN:%d", snapshot.knobRaw);
    } else {
      std::snprintf(line2, sizeof(line2), "XN:--");
    }
    std::snprintf(line3,
                  sizeof(line3),
                  "YZ:%d",
                  static_cast<int>(std::lround(temperatureThresholdC)));
  } else {
    if (snapshot.temperatureValid) {
      std::snprintf(line1,
                    sizeof(line1),
                    "T:%.1f c",
                    static_cast<double>(snapshot.temperatureC));
    } else {
      std::snprintf(line1, sizeof(line1), "T:-- c");
    }
    if (snapshot.mq2Ready) {
      std::snprintf(line2, sizeof(line2), "Q:%d ppm", snapshot.mq2EqPpm);
    } else {
      std::snprintf(line2, sizeof(line2), "Q:---- ppm");
    }
    std::snprintf(line3,
                  sizeof(line3),
                  "N:%d H:%d",
                  snapshot.soundRelative,
                  snapshot.presenceDetected ? 1 : 0);
  }
  JsonObject display = doc["display"].to<JsonObject>();
  display["page"] = thresholdPage ? "threshold" : "score";
  JsonArray lines = display["lines"].to<JsonArray>();
  lines.add(modeText(outputs.mode));
  lines.add(line1);
  lines.add(line2);
  lines.add(line3);

  JsonArray alerts = doc["alerts"].to<JsonArray>();
  addAlerts(alerts, outputs.activeAlerts);

  JsonObject health = doc["health"].to<JsonObject>();
  health["dht"] = snapshot.temperatureValid && snapshot.humidityValid
                      ? "ok"
                      : "stale";
  health["mq2"] = snapshot.mq2Ready ? "ready" : "warming";
  health["knob"] = snapshot.knobValid ? "ok" : "waiting";
  health["oled"] = oledReady ? "ready" : "error";
  health["buzzerEnabled"] = state.buzzerEnabled;
  health["relayOutputOnly"] = true;

  if (state.lastAppliedCommandId.empty()) {
    doc["lastAppliedCommandId"] = nullptr;
  } else {
    doc["lastAppliedCommandId"] = state.lastAppliedCommandId.c_str();
  }

  serializeLine(doc, serial);
  hasTelemetry_ = true;
  telemetryRequested_ = false;
  lastTelemetryMs_ = nowMs;
}

void SerialProtocol::emitLocalEvent(Stream& serial,
                                    const LocalEvent& event) {
  JsonDocument doc;
  doc["type"] = "event";
  if (event.type == LocalEventType::ButtonA) {
    doc["event"] = "button";
    doc["key"] = "A";
    doc["action"] = "toggle_mode";
    doc["mode"] = modeText(event.mode);
  } else if (event.type == LocalEventType::ThresholdChanged) {
    doc["event"] = "threshold_changed";
    doc["source"] = "knob";
    doc["knobRaw"] = event.knobRaw;
    doc["temperatureThresholdC"] = event.temperatureThresholdC;
  } else {
    return;
  }
  serializeLine(doc, serial);
  telemetryRequested_ = true;
}

void SerialProtocol::emitAlertChanged(Stream& serial,
                                      uint8_t activeAlerts) {
  JsonDocument doc;
  doc["type"] = "event";
  doc["event"] = "alert_changed";
  JsonArray active = doc["active"].to<JsonArray>();
  addAlerts(active, activeAlerts);
  serializeLine(doc, serial);
  telemetryRequested_ = true;
}

void SerialProtocol::requestTelemetry() {
  telemetryRequested_ = true;
}

}  // namespace smartlife
