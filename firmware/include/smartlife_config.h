#pragma once

#include <cstddef>
#include <cstdint>

namespace smartlife {

constexpr char PROJECT_NAME[] = "smartlife-junior";
constexpr char PROFILE_ID[] = "smartlife-junior-solar-home-v1";
constexpr char BOARD_ID[] = "n16r8_esp32s3";
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr bool RFID_ENABLED = false;

constexpr uint8_t PIN_LIGHT = 1;
constexpr uint8_t PIN_SOUND = 4;
constexpr uint8_t PIN_DHT11 = 14;
constexpr uint8_t PIN_PIR = 5;
constexpr uint8_t PIN_KEYPAD = 10;
constexpr uint8_t PIN_MQ2 = 2;
constexpr uint8_t PIN_WATER = 8;
constexpr uint8_t PIN_FLAME = 45;
constexpr uint8_t PIN_SERVO = 9;
constexpr uint8_t PIN_FAN = 11;
constexpr uint8_t PIN_RELAY = 12;
constexpr uint8_t PIN_BUZZER = 13;
constexpr uint8_t PIN_RGB = 46;
constexpr uint8_t PIN_OLED_SDA = 41;
constexpr uint8_t PIN_OLED_SCL = 42;
constexpr uint8_t PIN_TEMP_KNOB = 17;

constexpr uint32_t FAST_SENSOR_INTERVAL_MS = 200;
constexpr uint32_t DHT_INTERVAL_MS = 2000;
constexpr uint32_t DHT_STALE_MS = 6000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr uint32_t MQ2_WARMUP_MS = 60000;
constexpr uint32_t SAFETY_CLEAR_MS = 3000;
constexpr uint32_t INTRUSION_CLEAR_MS = 5000;

constexpr float TEMPERATURE_HYSTERESIS_C = 0.5f;
constexpr float DEFAULT_TEMPERATURE_THRESHOLD_C = 27.0f;
constexpr int TEMPERATURE_THRESHOLD_MIN_C = 18;
constexpr int TEMPERATURE_THRESHOLD_MAX_C = 35;
constexpr int MQ2_EQ_PPM_MAX = 1000;
constexpr int MQ2_ALERT_EQ_PPM = 600;
constexpr int MQ2_CLEAR_EQ_PPM = 550;

constexpr uint8_t BOOT_FAN_PERCENT = 0;
constexpr bool BOOT_RELAY_ON = false;
constexpr bool BOOT_BUZZER_ON = false;
constexpr uint8_t SLEEP_FAN_PERCENT = 35;
constexpr uint8_t ALERT_FAN_PERCENT = 100;

constexpr bool PIR_ACTIVE_HIGH = true;
constexpr bool WATER_ACTIVE_HIGH = true;
constexpr bool FLAME_ACTIVE_HIGH = true;
constexpr bool RELAY_ACTIVE_HIGH = true;
constexpr bool BUZZER_ACTIVE_HIGH = true;
constexpr bool FAN_ACTIVE_LOW = false;

constexpr uint8_t FAN_PWM_CHANNEL = 0;
constexpr uint16_t FAN_PWM_FREQUENCY_HZ = 25000;
constexpr uint8_t FAN_PWM_RESOLUTION_BITS = 8;
constexpr uint8_t RGB_COUNT = 12;
constexpr uint8_t RGB_STRIP_BRIGHTNESS = 12;

constexpr int SERVO_OPEN_DEGREES = 20;
constexpr int SERVO_CLOSED_DEGREES = 110;
constexpr int SERVO_MIN_PULSE_US = 500;
constexpr int SERVO_MAX_PULSE_US = 2400;
constexpr int KNOB_RAW_DEADBAND = 16;

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr int8_t OLED_RESET_PIN = -1;
constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;
constexpr uint32_t OLED_REFRESH_INTERVAL_MS = 250;
constexpr uint32_t OLED_THRESHOLD_PAGE_MS = 2500;

// Provisional A-key window from the read-only reference hardware. Freeze a
// narrower range only after GPIO10 is sampled on this exact board at G2.
constexpr int KEYPAD_A_ADC_MIN = 1950;
constexpr int KEYPAD_A_ADC_MAX = 2300;
constexpr uint32_t KEYPAD_DEBOUNCE_MS = 60;
constexpr std::size_t LOCAL_EVENT_QUEUE_CAPACITY = 6;

}  // namespace smartlife
