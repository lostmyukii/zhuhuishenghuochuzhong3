import json
import tempfile
import unittest
from pathlib import Path

from tools.g1_mock_acceptance import build_g1_run, write_g1_evidence


class G1MockAcceptanceTest(unittest.TestCase):
    def test_full_run_covers_every_required_g1_scene(self) -> None:
        run = build_g1_run()

        self.assertEqual("mock-passed", run["status"])
        self.assertTrue(all(check["passed"] for check in run["checks"]), run["checks"])
        self.assertEqual(
            {
                "identity",
                "auto_sleep",
                "knob_cross_threshold",
                "xiaoshu_dahan",
                "water_trigger_recovery",
                "command_ack_telemetry",
                "offline_boundary",
                "mock_source",
                "telemetry_sequence",
            },
            {check["id"] for check in run["checks"]},
        )

        telemetry = [frame for frame in run["frames"] if frame["type"] == "telemetry"]
        self.assertEqual(list(range(1, len(telemetry) + 1)), [frame["seq"] for frame in telemetry])
        self.assertTrue(all(frame.get("source") == "mock" for frame in run["frames"]))

        self.assertTrue(any(frame["mode"] == "Sleep" for frame in telemetry))
        self.assertTrue(any(frame["thresholds"]["temperatureC"] == 18 for frame in telemetry))
        self.assertTrue(any(frame["thresholds"]["temperatureC"] == 35 for frame in telemetry))

        xiaoshu = next(frame for frame in telemetry if frame["solarTerm"] == "小暑")
        dahan = next(frame for frame in telemetry if frame["solarTerm"] == "大寒")
        self.assertEqual((80, 30), (xiaoshu["actuators"]["curtainClosePercent"], xiaoshu["thresholds"]["lightRelative"]))
        self.assertEqual((20, 45), (dahan["actuators"]["curtainClosePercent"], dahan["thresholds"]["lightRelative"]))

        water = next(frame for frame in telemetry if frame["alerts"] == ["water"])
        self.assertEqual(100, water["actuators"]["fanPercent"])
        self.assertEqual("red", water["actuators"]["rgb"])
        self.assertTrue(water["actuators"]["buzzer"])
        self.assertFalse(water["actuators"]["relay"])
        self.assertEqual([], telemetry[-1]["alerts"])
        self.assertEqual(35, telemetry[-1]["actuators"]["fanPercent"])

        self.assertEqual(4, len(run["commands"]))
        self.assertEqual(
            ["cmd-g1-sleep", "cmd-g1-xiaoshu", "cmd-g1-dahan", "cmd-g1-water-sleep"],
            [command["id"] for command in run["commands"]],
        )

    def test_evidence_writer_keeps_raw_streams_and_integrity_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result = write_g1_evidence(
                Path(temporary_directory),
                date_stamp="20260720",
                generated_at="2026-07-20T09:00:00+08:00",
            )

            frame_path = result["framePath"]
            command_path = result["commandPath"]
            summary_path = result["summaryPath"]
            self.assertTrue(frame_path.exists())
            self.assertTrue(command_path.exists())
            self.assertTrue(summary_path.exists())

            frame_lines = frame_path.read_text(encoding="utf-8").splitlines()
            command_lines = command_path.read_text(encoding="utf-8").splitlines()
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(summary["frameCount"], len(frame_lines))
            self.assertEqual(summary["commandCount"], len(command_lines))
            self.assertEqual("mock-passed", summary["status"])
            self.assertEqual("2026-07-20T09:00:00+08:00", summary["generatedAt"])
            self.assertEqual(64, len(summary["sha256"]["frames"]))
            self.assertEqual(64, len(summary["sha256"]["commands"]))
            self.assertTrue(all(json.loads(line)["source"] == "mock" for line in frame_lines))
            self.assertTrue(all(json.loads(line)["origin"] == "test" for line in command_lines))


if __name__ == "__main__":
    unittest.main()
