import json
import tempfile
import unittest
from pathlib import Path

from tools.capture_serial_evidence import capture_lines
from tools.n16r8_mock_board import MockSmartLifeBoard, is_frame_fresh


class MockSmartLifeBoardTest(unittest.TestCase):
    def setUp(self) -> None:
        self.board = MockSmartLifeBoard()

    def assert_mock_frame(self, frame: dict) -> None:
        self.assertEqual("mock", frame.get("source"), frame)

    def test_startup_emits_identity_hello_and_incrementing_telemetry(self) -> None:
        hello, first = self.board.startup_frames()
        second = self.board.advance(1000)[-1]

        self.assertEqual("hello", hello["type"])
        self.assertEqual(1, hello["protocolVersion"])
        self.assertEqual("smartlife-junior", hello["project"])
        self.assertEqual("smartlife-junior-solar-home-v1", hello["profileId"])
        self.assertEqual("n16r8_esp32s3", hello["board"])
        self.assertEqual(115200, hello["baud"])
        self.assertFalse(hello["rfid"])
        self.assert_mock_frame(hello)

        self.assertEqual("telemetry", first["type"])
        self.assertEqual(first["seq"] + 1, second["seq"])
        self.assertEqual(1000, second["uptimeMs"])
        self.assert_mock_frame(first)
        self.assert_mock_frame(second)

    def test_command_ack_is_visible_in_next_telemetry(self) -> None:
        ack = self.board.handle_command(
            {
                "type": "command",
                "id": "cmd-mode-sleep",
                "origin": "dashboard",
                "mode": "Sleep",
            }
        )
        telemetry = self.board.emit_telemetry()

        self.assertTrue(ack["ok"])
        self.assertEqual({"mode": "Sleep"}, ack["applied"])
        self.assertEqual("cmd-mode-sleep", telemetry["lastAppliedCommandId"])
        self.assertEqual("Sleep", telemetry["mode"])
        self.assert_mock_frame(ack)
        self.assert_mock_frame(telemetry)

    def test_local_a_key_knob_water_and_solar_term_events(self) -> None:
        button = self.board.press_button_a()
        knob = self.board.turn_knob(4095)
        water = self.board.set_water(True)
        hot_term = self.board.set_solar_term("小暑")
        hot_telemetry = self.board.emit_telemetry()
        cold_term = self.board.set_solar_term("大寒")
        cold_telemetry = self.board.emit_telemetry()

        self.assertEqual(
            {"event": "button", "key": "A", "action": "toggle_mode", "mode": "Sleep"},
            {key: button[key] for key in ("event", "key", "action", "mode")},
        )
        self.assertEqual("threshold_changed", knob["event"])
        self.assertEqual("knob", knob["eventSource"])
        self.assertEqual(4095, knob["knobRaw"])
        self.assertEqual(35, knob["temperatureThresholdC"])
        self.assertEqual("alert_changed", water["event"])
        self.assertIn("water", water["active"])
        self.assertEqual("solar_term_changed", hot_term["event"])
        self.assertEqual("小暑", hot_term["solarTerm"])
        self.assertEqual(80, hot_telemetry["actuators"]["curtainClosePercent"])
        self.assertEqual(30, hot_telemetry["thresholds"]["lightRelative"])
        self.assertEqual("solar_term_changed", cold_term["event"])
        self.assertEqual("大寒", cold_term["solarTerm"])
        self.assertEqual(20, cold_telemetry["actuators"]["curtainClosePercent"])
        self.assertEqual(45, cold_telemetry["thresholds"]["lightRelative"])
        for frame in (button, knob, water, hot_term, hot_telemetry, cold_term, cold_telemetry):
            self.assert_mock_frame(frame)

    def test_water_priority_overrides_sleep_and_clears_after_three_seconds(self) -> None:
        self.board.press_button_a()
        self.board.set_water(True)
        alarm = self.board.emit_telemetry()
        self.assertEqual("Sleep", alarm["mode"])
        self.assertEqual(["water"], alarm["alerts"])
        self.assertEqual(100, alarm["actuators"]["fanPercent"])
        self.assertFalse(alarm["actuators"]["relay"])
        self.assertTrue(alarm["actuators"]["buzzer"])
        self.assertEqual("red", alarm["actuators"]["rgb"])

        self.board.set_water(False)
        before_clear = self.board.advance(2999)[-1]
        after_clear = self.board.advance(1)[-1]
        self.assertEqual(["water"], before_clear["alerts"])
        self.assertEqual([], after_clear["alerts"])
        self.assertEqual(35, after_clear["actuators"]["fanPercent"])
        self.assertFalse(after_clear["actuators"]["buzzer"])

    def test_duplicate_command_returns_cached_ack_without_repeating_action(self) -> None:
        command = {
            "type": "command",
            "id": "cmd-relay-on",
            "origin": "test",
            "actuator": {"relay": True},
        }
        first = self.board.handle_command(command)
        count_after_first = self.board.applied_command_count
        duplicate = self.board.handle_command(command)

        self.assertEqual(first, duplicate)
        self.assertEqual(count_after_first, self.board.applied_command_count)
        self.assertEqual(1, self.board.applied_command_count)

    def test_safety_lock_rejects_manual_actuator_but_defers_mode(self) -> None:
        self.board.set_water(True)
        actuator_ack = self.board.handle_command(
            {
                "type": "command",
                "id": "cmd-unsafe-fan",
                "origin": "test",
                "actuator": {"fanPercent": 0},
            }
        )
        mode_ack = self.board.handle_command(
            {
                "type": "command",
                "id": "cmd-safe-mode",
                "origin": "test",
                "mode": "Sleep",
            }
        )

        self.assertFalse(actuator_ack["ok"])
        self.assertEqual("safety_lock", actuator_ack["error"])
        self.assertTrue(mode_ack["ok"])
        self.assertEqual("safety", mode_ack["deferredBy"])

    def test_stopped_telemetry_supports_3500ms_offline_boundary(self) -> None:
        last_frame_ms = self.board.now_ms
        self.board.stop_telemetry()
        self.assertEqual([], self.board.advance(1000))
        self.assertTrue(is_frame_fresh(last_frame_ms, last_frame_ms + 3500))
        self.assertFalse(is_frame_fresh(last_frame_ms, last_frame_ms + 3501))

    def test_capture_keeps_raw_jsonl_unchanged_and_writes_timestamp_sidecar(self) -> None:
        raw_lines = [
            '{"type":"hello","source":"mock"}\n',
            '{"type":"telemetry","seq":1,"source":"mock"}\n',
        ]
        timestamps = iter(("2026-07-19T10:00:00.000+08:00", "2026-07-19T10:00:01.000+08:00"))
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "serial.jsonl"
            sidecar = capture_lines(raw_lines, output, timestamp_factory=lambda: next(timestamps))

            self.assertEqual("".join(raw_lines), output.read_text(encoding="utf-8"))
            entries = [json.loads(line) for line in sidecar.read_text(encoding="utf-8").splitlines()]
            self.assertEqual(2, len(entries))
            self.assertEqual([1, 2], [entry["lineNumber"] for entry in entries])
            self.assertEqual(
                ["2026-07-19T10:00:00.000+08:00", "2026-07-19T10:00:01.000+08:00"],
                [entry["capturedAt"] for entry in entries],
            )
            self.assertTrue(all("frame" not in entry for entry in entries))


if __name__ == "__main__":
    unittest.main()
