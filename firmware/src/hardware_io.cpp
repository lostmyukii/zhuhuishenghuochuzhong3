#include "hardware_io.h"

#include <Arduino.h>

#include <algorithm>
#include <cmath>

#include "smartlife_config.h"
#include "solar_terms.h"

namespace smartlife {
namespace {

int clampAdc(int rawValue) {
  return std::max(0, std::min(4095, rawValue));
}

int analogRelative(int rawValue) {
  return (clampAdc(rawValue) * 100 + 2047) / 4095;
}

int mq2EquivalentPpm(int rawValue) {
  return (clampAdc(rawValue) * MQ2_EQ_PPM_MAX + 2047) / 4095;
}

bool digitalTriggered(int rawLevel, bool activeHigh) {
  return activeHigh ? rawLevel == HIGH : rawLevel == LOW;
}

uint8_t outputLevel(bool on, bool activeHigh) {
  return (on == activeHigh) ? HIGH : LOW;
}

}  // namespace

HardwareIo::HardwareIo()
    : dht_(PIN_DHT11, DHT11),
      pixels_(RGB_COUNT, PIN_RGB, NEO_GRB + NEO_KHZ800) {}

void HardwareIo::begin() {
  bootStartedMs_ = millis();

  digitalWrite(PIN_FAN, FAN_ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(PIN_RELAY, outputLevel(false, RELAY_ACTIVE_HIGH));
  digitalWrite(PIN_BUZZER, outputLevel(false, BUZZER_ACTIVE_HIGH));
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQUENCY_HZ, FAN_PWM_RESOLUTION_BITS);
  ledcAttachPin(PIN_FAN, FAN_PWM_CHANNEL);

  pixels_.begin();
  pixels_.setBrightness(RGB_STRIP_BRIGHTNESS);
  pixels_.clear();
  pixels_.show();

  writeFanPercent(BOOT_FAN_PERCENT);
  writeRelay(BOOT_RELAY_ON);
  writeBuzzer(BOOT_BUZZER_ON);
  writeRgb(RgbState::Off);

  analogReadResolution(12);
  pinMode(PIN_LIGHT, INPUT);
  pinMode(PIN_SOUND, INPUT);
  pinMode(PIN_KEYPAD, INPUT);
  pinMode(PIN_MQ2, INPUT);
  pinMode(PIN_TEMP_KNOB, INPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_WATER, INPUT_PULLDOWN);
  pinMode(PIN_FLAME, INPUT_PULLDOWN);

  dht_.begin();

  lastFastSampleMs_ = bootStartedMs_ - FAST_SENSOR_INTERVAL_MS;
  lastDhtAttemptMs_ = bootStartedMs_;
  temperatureThresholdC_ = DEFAULT_TEMPERATURE_THRESHOLD_C;
}

void HardwareIo::sample(uint32_t nowMs) {
  if (static_cast<uint32_t>(nowMs - lastFastSampleMs_) >=
      FAST_SENSOR_INTERVAL_MS) {
    lastFastSampleMs_ = nowMs;
    sampleFastSensors(nowMs);
  }

  if (static_cast<uint32_t>(nowMs - lastDhtAttemptMs_) >= DHT_INTERVAL_MS) {
    lastDhtAttemptMs_ = nowMs;
    sampleDht(nowMs);
  }

  updateDhtFreshness(nowMs);
  snapshot_.mq2Ready =
      static_cast<uint32_t>(nowMs - bootStartedMs_) >= MQ2_WARMUP_MS;
  snapshot_.mq2EqPpm =
      snapshot_.mq2Ready ? mq2EquivalentPpm(snapshot_.mq2Raw) : 0;
}

const SensorSnapshot& HardwareIo::snapshot() const {
  return snapshot_;
}

float HardwareIo::temperatureThresholdC() const {
  return temperatureThresholdC_;
}

bool HardwareIo::takeLocalEvent(LocalEvent& event) {
  if (localEventCount_ == 0) {
    return false;
  }
  event = localEvents_[localEventHead_];
  localEventHead_ = (localEventHead_ + 1) % localEvents_.size();
  --localEventCount_;
  return true;
}

void HardwareIo::sampleFastSensors(uint32_t nowMs) {
  snapshot_.lightRelative = analogRelative(analogRead(PIN_LIGHT));
  snapshot_.soundRelative = analogRelative(analogRead(PIN_SOUND));
  snapshot_.keypadRaw = clampAdc(analogRead(PIN_KEYPAD));
  const ButtonAUpdate buttonUpdate =
      buttonA_.update(snapshot_.keypadRaw, nowMs);
  if (buttonUpdate.clicked) {
    enqueueLocalEvent(LocalEventType::ButtonA);
  }
  snapshot_.mq2Raw = clampAdc(analogRead(PIN_MQ2));
  snapshot_.presenceDetected =
      digitalTriggered(digitalRead(PIN_PIR), PIR_ACTIVE_HIGH);
  snapshot_.waterDetected =
      digitalTriggered(digitalRead(PIN_WATER), WATER_ACTIVE_HIGH);
  snapshot_.flameDetected =
      digitalTriggered(digitalRead(PIN_FLAME), FLAME_ACTIVE_HIGH);

  knobSamples_[knobSampleIndex_] = clampAdc(analogRead(PIN_TEMP_KNOB));
  knobSampleIndex_ = (knobSampleIndex_ + 1) % knobSamples_.size();
  if (knobSampleCount_ < knobSamples_.size()) {
    ++knobSampleCount_;
  }

  if (knobSampleCount_ == knobSamples_.size()) {
    const int previousThreshold =
        static_cast<int>(std::lround(temperatureThresholdC_));
    const int medianRaw = medianOfFiveRaw(knobSamples_);
    acceptedKnobRaw_ = snapshot_.knobValid
                           ? applyKnobDeadband(acceptedKnobRaw_,
                                               medianRaw,
                                               KNOB_RAW_DEADBAND)
                           : medianRaw;
    snapshot_.knobRaw = acceptedKnobRaw_;
    snapshot_.knobValid = true;
    const int mappedThreshold = mapKnobRawToThresholdC(acceptedKnobRaw_);
    temperatureThresholdC_ = static_cast<float>(mappedThreshold);
    if (mappedThreshold != previousThreshold) {
      enqueueLocalEvent(LocalEventType::ThresholdChanged,
                        acceptedKnobRaw_,
                        mappedThreshold);
    }
  }
}

void HardwareIo::enqueueLocalEvent(LocalEventType type,
                                   int knobRaw,
                                   int temperatureThresholdC) {
  if (localEventCount_ == localEvents_.size()) {
    localEventHead_ = (localEventHead_ + 1) % localEvents_.size();
    --localEventCount_;
  }

  const std::size_t tail =
      (localEventHead_ + localEventCount_) % localEvents_.size();
  localEvents_[tail].type = type;
  localEvents_[tail].knobRaw = knobRaw;
  localEvents_[tail].temperatureThresholdC = temperatureThresholdC;
  ++localEventCount_;
}

void HardwareIo::sampleDht(uint32_t nowMs) {
  const float temperature = dht_.readTemperature();
  const float humidity = dht_.readHumidity();
  if (std::isnan(temperature) || std::isnan(humidity)) {
    return;
  }

  snapshot_.temperatureC = temperature;
  snapshot_.humidityRh = humidity;
  snapshot_.temperatureValid = true;
  snapshot_.humidityValid = true;
  hasDhtSuccess_ = true;
  lastDhtSuccessMs_ = nowMs;
}

void HardwareIo::updateDhtFreshness(uint32_t nowMs) {
  if (!hasDhtSuccess_ ||
      static_cast<uint32_t>(nowMs - lastDhtSuccessMs_) >= DHT_STALE_MS) {
    snapshot_.temperatureValid = false;
    snapshot_.humidityValid = false;
  }
}

void HardwareIo::applyOutputs(const ControlOutputs& outputs) {
  writeFanPercent(outputs.fanPercent);
  writeRelay(outputs.relayOn);
  writeBuzzer(outputs.buzzerOn);
  writeRgb(outputs.rgb);
  if (outputs.curtainControlEnabled) {
    writeCurtainPercent(outputs.curtainClosePercent);
  }
}

void HardwareIo::writeFanPercent(uint8_t percent) {
  const int clampedPercent = std::max(0, std::min(100, static_cast<int>(percent)));
  if (lastFanPercent_ == clampedPercent) {
    return;
  }

  int duty = (clampedPercent * 255 + 50) / 100;
  if (FAN_ACTIVE_LOW) {
    duty = 255 - duty;
  }
  ledcWrite(FAN_PWM_CHANNEL, duty);
  lastFanPercent_ = clampedPercent;
}

void HardwareIo::writeRelay(bool on) {
  if (lastRelayState_ == static_cast<int>(on)) {
    return;
  }
  digitalWrite(PIN_RELAY, outputLevel(on, RELAY_ACTIVE_HIGH));
  lastRelayState_ = static_cast<int>(on);
}

void HardwareIo::writeBuzzer(bool on) {
  if (lastBuzzerState_ == static_cast<int>(on)) {
    return;
  }
  digitalWrite(PIN_BUZZER, outputLevel(on, BUZZER_ACTIVE_HIGH));
  lastBuzzerState_ = static_cast<int>(on);
}

void HardwareIo::writeRgb(RgbState state) {
  const int numericState = static_cast<int>(state);
  if (lastRgbState_ == numericState) {
    return;
  }

  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
  if (state == RgbState::Yellow) {
    red = 255;
    green = 180;
  } else if (state == RgbState::Red) {
    red = 255;
    green = 40;
    blue = 28;
  }

  for (uint8_t index = 0; index < RGB_COUNT; ++index) {
    pixels_.setPixelColor(index, pixels_.Color(red, green, blue));
  }
  pixels_.show();
  lastRgbState_ = numericState;
}

void HardwareIo::writeCurtainPercent(int closePercent) {
  const int clampedPercent = std::max(0, std::min(100, closePercent));
  if (lastCurtainPercent_ == clampedPercent) {
    return;
  }

  if (!servoAttached_) {
    servo_.attach(PIN_SERVO, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    servoAttached_ = true;
  }
  const int angle = SERVO_OPEN_DEGREES +
                    (SERVO_CLOSED_DEGREES - SERVO_OPEN_DEGREES) *
                        clampedPercent / 100;
  servo_.write(angle);
  lastCurtainPercent_ = clampedPercent;
}

}  // namespace smartlife
