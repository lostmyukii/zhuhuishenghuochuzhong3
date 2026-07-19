import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
CONFIG = FIRMWARE / "include" / "smartlife_config.h"
MODELS = FIRMWARE / "include" / "smartlife_models.h"
PLATFORMIO = FIRMWARE / "platformio.ini"
BOARD = FIRMWARE / "boards" / "n16r8_esp32s3.json"
MAIN = FIRMWARE / "src" / "main.cpp"
HARDWARE_HEADER = FIRMWARE / "include" / "hardware_io.h"
HARDWARE_SOURCE = FIRMWARE / "src" / "hardware_io.cpp"
DISPLAY_HEADER = FIRMWARE / "include" / "display_controller.h"
DISPLAY_SOURCE = FIRMWARE / "src" / "display_controller.cpp"
LOCAL_CONTROLS_HEADER = FIRMWARE / "include" / "local_controls.h"
LOCAL_CONTROLS_SOURCE = FIRMWARE / "src" / "local_controls.cpp"
PROTOCOL_HEADER = FIRMWARE / "include" / "serial_protocol.h"
PROTOCOL_SOURCE = FIRMWARE / "src" / "serial_protocol.cpp"
PROTOCOL_CORE_HEADER = FIRMWARE / "include" / "protocol_core.h"
PROTOCOL_CORE_SOURCE = FIRMWARE / "src" / "protocol_core.cpp"


class FirmwareContractTest(unittest.TestCase):
    def read_required(self, path: Path) -> str:
        self.assertTrue(path.is_file(), f"missing required file: {path.relative_to(ROOT)}")
        return path.read_text(encoding="utf-8")

    def test_project_identity_is_unique_and_current(self) -> None:
        config = self.read_required(CONFIG)
        self.assertIn('PROJECT_NAME[] = "smartlife-junior"', config)
        self.assertIn('PROFILE_ID[] = "smartlife-junior-solar-home-v1"', config)
        self.assertIn("PROTOCOL_VERSION = 1", config)
        self.assertIn("SERIAL_BAUD = 115200", config)
        self.assertIn("RFID_ENABLED = false", config)
        self.assertNotIn("smartlife-junior-home-v1", config)

    def test_fixed_gpio_map_has_no_drift_or_duplicates(self) -> None:
        config = self.read_required(CONFIG)
        discovered = {
            name: int(value)
            for name, value in re.findall(r"constexpr\s+uint8_t\s+(PIN_[A-Z0-9_]+)\s*=\s*(\d+)\s*;", config)
        }
        expected = {
            "PIN_LIGHT": 1,
            "PIN_SOUND": 4,
            "PIN_DHT11": 14,
            "PIN_PIR": 5,
            "PIN_KEYPAD": 10,
            "PIN_MQ2": 2,
            "PIN_WATER": 8,
            "PIN_FLAME": 45,
            "PIN_SERVO": 9,
            "PIN_FAN": 11,
            "PIN_RELAY": 12,
            "PIN_BUZZER": 13,
            "PIN_RGB": 46,
            "PIN_OLED_SDA": 41,
            "PIN_OLED_SCL": 42,
            "PIN_TEMP_KNOB": 17,
        }
        self.assertEqual(expected, discovered)
        self.assertEqual(len(discovered.values()), len(set(discovered.values())))

    def test_timing_threshold_and_boot_defaults_are_frozen(self) -> None:
        config = self.read_required(CONFIG)
        expected_fragments = (
            "FAST_SENSOR_INTERVAL_MS = 200",
            "DHT_INTERVAL_MS = 2000",
            "DHT_STALE_MS = 6000",
            "TELEMETRY_INTERVAL_MS = 1000",
            "MQ2_WARMUP_MS = 60000",
            "TEMPERATURE_HYSTERESIS_C = 0.5f",
            "DEFAULT_TEMPERATURE_THRESHOLD_C = 27.0f",
            "SLEEP_FAN_PERCENT = 35",
            "ALERT_FAN_PERCENT = 100",
            "MQ2_ALERT_EQ_PPM = 600",
            "MQ2_CLEAR_EQ_PPM = 550",
            "SAFETY_CLEAR_MS = 3000",
            "INTRUSION_CLEAR_MS = 5000",
            "BOOT_FAN_PERCENT = 0",
            "BOOT_RELAY_ON = false",
            "BOOT_BUZZER_ON = false",
        )
        for fragment in expected_fragments:
            self.assertIn(fragment, config)

    def test_platformio_uses_custom_n16r8_board_and_required_libraries(self) -> None:
        platformio = self.read_required(PLATFORMIO)
        self.assertIn("boards_dir = boards", platformio)
        self.assertIn("[env:n16r8_esp32s3]", platformio)
        self.assertIn("board = n16r8_esp32s3", platformio)
        self.assertIn("monitor_speed = 115200", platformio)
        for library in (
            "DHT sensor library",
            "Adafruit Unified Sensor",
            "Adafruit SSD1306",
            "Adafruit GFX Library",
            "ESP32Servo",
            "ArduinoJson",
            "Adafruit NeoPixel",
        ):
            self.assertIn(library, platformio)

    def test_board_definition_matches_n16r8_hardware(self) -> None:
        board = json.loads(self.read_required(BOARD))
        self.assertEqual("esp32s3", board["build"]["mcu"])
        self.assertEqual("esp32_s3r8n16", board["build"]["variant"])
        self.assertEqual("16MB", board["upload"]["flash_size"])
        self.assertEqual(16_777_216, board["upload"]["maximum_size"])
        self.assertIn("-DBOARD_HAS_PSRAM", board["build"]["extra_flags"])

    def test_minimal_source_and_models_exist(self) -> None:
        main = self.read_required(MAIN)
        models = self.read_required(MODELS)
        self.assertIn("void setup()", main)
        self.assertIn("void loop()", main)
        self.assertIn("enum class Mode", models)
        self.assertIn("enum class RgbState", models)

    def test_hardware_adapter_reads_every_installed_sensor(self) -> None:
        source = self.read_required(HARDWARE_SOURCE)
        for fragment in (
            "analogRead(PIN_LIGHT)",
            "analogRead(PIN_SOUND)",
            "analogRead(PIN_KEYPAD)",
            "analogRead(PIN_MQ2)",
            "analogRead(PIN_TEMP_KNOB)",
            "digitalRead(PIN_PIR)",
            "digitalRead(PIN_WATER)",
            "digitalRead(PIN_FLAME)",
            "dht_.readTemperature()",
            "dht_.readHumidity()",
        ):
            self.assertIn(fragment, source)

    def test_boot_safe_outputs_precede_sensor_startup(self) -> None:
        source = self.read_required(HARDWARE_SOURCE)
        begin_at = source.index("void HardwareIo::begin()")
        dht_at = source.index("dht_.begin()", begin_at)
        for fragment in (
            "writeFanPercent(BOOT_FAN_PERCENT)",
            "writeRelay(BOOT_RELAY_ON)",
            "writeBuzzer(BOOT_BUZZER_ON)",
            "writeRgb(RgbState::Off)",
        ):
            self.assertGreater(source.index(fragment, begin_at), begin_at)
            self.assertLess(source.index(fragment, begin_at), dht_at)
        self.assertNotIn("servo_.attach", source[begin_at:dht_at])

    def test_sampling_uses_independent_non_blocking_schedules(self) -> None:
        main = self.read_required(MAIN)
        source = self.read_required(HARDWARE_SOURCE)
        header = self.read_required(HARDWARE_HEADER)
        self.assertNotIn("delay(", main)
        self.assertNotIn("delay(", source)
        self.assertIn("const uint32_t nowMs = millis()", main)
        self.assertIn("hardware.sample(nowMs)", main)
        self.assertIn("FAST_SENSOR_INTERVAL_MS", source)
        self.assertIn("DHT_INTERVAL_MS", source)
        self.assertIn("lastFastSampleMs_", header)
        self.assertIn("lastDhtAttemptMs_", header)

    def test_dht_mq2_and_knob_health_contract_is_explicit(self) -> None:
        source = self.read_required(HARDWARE_SOURCE)
        models = self.read_required(MODELS)
        for fragment in (
            "DHT_STALE_MS",
            "MQ2_WARMUP_MS",
            "snapshot_.mq2Ready",
            "snapshot_.knobValid",
            "medianOfFiveRaw",
            "applyKnobDeadband",
            "mapKnobRawToThresholdC",
        ):
            self.assertIn(fragment, source)
        for field in (
            "humidityValid",
            "humidityRh",
            "soundRelative",
            "lightRelative",
            "mq2Raw",
            "knobRaw",
            "knobValid",
            "keypadRaw",
        ):
            self.assertIn(field, models)

    def test_actuator_layer_only_accepts_control_engine_outputs(self) -> None:
        header = self.read_required(HARDWARE_HEADER)
        main = self.read_required(MAIN)
        self.assertIn("void applyOutputs(const ControlOutputs& outputs)", header)
        self.assertIn("hardware.applyOutputs(evaluation.outputs)", main)
        self.assertIn("evaluateControl(", main)
        self.assertIn("controlState)", main)

    def test_oled_uses_fixed_bus_score_fields_and_honest_placeholders(self) -> None:
        source = self.read_required(DISPLAY_SOURCE)
        for fragment in (
            "Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL)",
            "display_.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)",
            'display_.print("T:")',
            'display_.print(" c")',
            'display_.print("Q:")',
            'display_.print(" ppm")',
            'display_.print("N:")',
            'display_.print(" H:")',
            'display_.print("XN:")',
            'display_.print("YZ:")',
            'display_.print("----")',
            "snapshot.temperatureValid",
            "snapshot.mq2Ready",
            "snapshot.knobValid",
        ):
            self.assertIn(fragment, source)

    def test_oled_refresh_and_threshold_page_are_non_blocking(self) -> None:
        source = self.read_required(DISPLAY_SOURCE)
        header = self.read_required(DISPLAY_HEADER)
        main = self.read_required(MAIN)
        self.assertNotIn("delay(", source)
        self.assertIn("OLED_REFRESH_INTERVAL_MS", source)
        self.assertIn("OLED_THRESHOLD_PAGE_MS", source)
        self.assertIn("bool ready() const", header)
        self.assertIn("display.begin()", main)
        self.assertIn("display.render(", main)

    def test_gpio10_button_a_has_configurable_window_and_single_click_debounce(self) -> None:
        config = self.read_required(CONFIG)
        source = self.read_required(LOCAL_CONTROLS_SOURCE)
        hardware = self.read_required(HARDWARE_SOURCE)
        main = self.read_required(MAIN)
        for fragment in (
            "KEYPAD_A_ADC_MIN",
            "KEYPAD_A_ADC_MAX",
            "KEYPAD_DEBOUNCE_MS",
        ):
            self.assertIn(fragment, config)
        for fragment in (
            "rawValue >= KEYPAD_A_ADC_MIN",
            "rawValue <= KEYPAD_A_ADC_MAX",
            "candidateSinceMs_",
            "stablePressed_",
            "update.clicked = true",
        ):
            self.assertIn(fragment, source)
        self.assertIn("buttonA_.update(snapshot_.keypadRaw, nowMs)", hardware)
        self.assertIn("enqueueLocalEvent(LocalEventType::ButtonA", hardware)
        self.assertIn("toggleMode(protocolState.targetMode)", main)

    def test_button_and_knob_changes_emit_local_events_not_command_acks(self) -> None:
        source = (
            self.read_required(PROTOCOL_SOURCE)
            if PROTOCOL_SOURCE.is_file()
            else self.read_required(MAIN)
        )
        emit_start = source.index("emitLocalEvent")
        emit_end = source.index("\n}", emit_start) + 2
        emit_block = source[emit_start:emit_end]
        for fragment in (
            'doc["type"] = "event"',
            'doc["event"] = "button"',
            'doc["key"] = "A"',
            'doc["action"] = "toggle_mode"',
            'doc["event"] = "threshold_changed"',
            'doc["source"] = "knob"',
        ):
            self.assertIn(fragment, emit_block)
        self.assertNotIn("ack", emit_block)

    def test_hello_frame_contains_the_frozen_identity_contract(self) -> None:
        source = self.read_required(PROTOCOL_SOURCE)
        for fragment in (
            'doc["type"] = "hello"',
            'doc["protocolVersion"] = PROTOCOL_VERSION',
            'doc["project"] = PROJECT_NAME',
            'doc["profileId"] = PROFILE_ID',
            'doc["board"] = BOARD_ID',
            'doc["baud"] = SERIAL_BAUD',
            'doc["rfid"] = RFID_ENABLED',
        ):
            self.assertIn(fragment, source)

    def test_telemetry_frame_contains_score_truth_and_health(self) -> None:
        source = self.read_required(PROTOCOL_SOURCE)
        for fragment in (
            'doc["type"] = "telemetry"',
            'doc["seq"]',
            'doc["uptimeMs"]',
            'doc["mode"]',
            'doc["solarTerm"]',
            'doc["sensors"]',
            'doc["thresholds"]',
            'doc["actuators"]',
            'doc["display"]',
            'doc["alerts"]',
            'doc["health"]',
            'doc["lastAppliedCommandId"]',
            'sensors["airQualityEqPpm"]',
            'health["mq2"] = snapshot.mq2Ready ? "ready" : "warming"',
            "TELEMETRY_INTERVAL_MS",
        ):
            self.assertIn(fragment, source)
        self.assertIn("nullptr", source)

    def test_command_validation_idempotency_and_safety_errors_are_explicit(self) -> None:
        source = self.read_required(PROTOCOL_CORE_SOURCE)
        header = self.read_required(PROTOCOL_CORE_HEADER)
        for fragment in (
            '"missing_id"',
            '"unsupported_origin"',
            '"invalid_mode"',
            '"invalid_solar_term"',
            '"out_of_range"',
            '"unsupported_command"',
            '"safety_lock"',
            "findCachedAck",
            "cacheAck",
            "lastAppliedCommandId",
        ):
            self.assertIn(fragment, source)
        self.assertIn("class CommandProcessor", header)
        self.assertIn("ManualActuatorOverrides", header)

    def test_serial_transport_emits_only_complete_json_lines(self) -> None:
        source = self.read_required(PROTOCOL_SOURCE)
        main = self.read_required(MAIN)
        self.assertIn("serializeJson(doc, serial)", source)
        self.assertIn("serial.println()", source)
        self.assertIn("protocol.poll(", main)
        self.assertIn("protocol.emitTelemetry(", main)
        self.assertNotIn("Serial.print(", main)
        self.assertNotIn("Serial.println(", main)


if __name__ == "__main__":
    unittest.main()
