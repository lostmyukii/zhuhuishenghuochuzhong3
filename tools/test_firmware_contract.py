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


if __name__ == "__main__":
    unittest.main()
