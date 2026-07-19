#!/usr/bin/env python3
"""Build deterministic G1 mock evidence without touching a serial port.

The output keeps board frames and outbound commands as separate raw JSONL
streams. Every board frame is marked ``source=mock`` and may only be stored in
``docs/evidence/mock``.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from datetime import datetime
from pathlib import Path
from typing import Any
from zoneinfo import ZoneInfo

if __package__:
    from tools.n16r8_mock_board import (
        PROFILE_ID,
        MockSmartLifeBoard,
        is_frame_fresh,
    )
else:
    from n16r8_mock_board import (  # type: ignore[no-redef]
        PROFILE_ID,
        MockSmartLifeBoard,
        is_frame_fresh,
    )


SHANGHAI = ZoneInfo("Asia/Shanghai")


def _check(check_id: str, passed: bool, evidence: dict[str, Any]) -> dict[str, Any]:
    return {"id": check_id, "passed": bool(passed), "evidence": evidence}


def build_g1_run() -> dict[str, Any]:
    """Return one complete deterministic G1 scenario and its assertions."""
    board = MockSmartLifeBoard()
    frames: list[dict[str, Any]] = []
    commands: list[dict[str, Any]] = []

    def emit(*items: dict[str, Any]) -> None:
        frames.extend(copy.deepcopy(items))

    def apply_command(command: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
        commands.append(copy.deepcopy(command))
        ack = board.handle_command(command)
        telemetry = board.emit_telemetry()
        emit(ack, telemetry)
        return ack, telemetry

    hello, auto_baseline = board.startup_frames()
    emit(hello, auto_baseline)

    sleep_ack, sleep_telemetry = apply_command(
        {"type": "command", "id": "cmd-g1-sleep", "origin": "test", "mode": "Sleep"}
    )
    button_auto = board.press_button_a()
    auto_after_button = board.emit_telemetry()
    emit(button_auto, auto_after_button)

    board.sensors["temperatureC"] = 28.0
    knob_low = board.turn_knob(0)
    knob_low_telemetry = board.emit_telemetry()
    knob_high = board.turn_knob(4095)
    knob_high_telemetry = board.emit_telemetry()
    emit(knob_low, knob_low_telemetry, knob_high, knob_high_telemetry)

    xiaoshu_ack, xiaoshu_telemetry = apply_command(
        {
            "type": "command",
            "id": "cmd-g1-xiaoshu",
            "origin": "test",
            "set": {"solarTerm": "小暑"},
        }
    )
    dahan_ack, dahan_telemetry = apply_command(
        {
            "type": "command",
            "id": "cmd-g1-dahan",
            "origin": "test",
            "set": {"solarTerm": "大寒"},
        }
    )

    water_sleep_ack, water_sleep_telemetry = apply_command(
        {
            "type": "command",
            "id": "cmd-g1-water-sleep",
            "origin": "test",
            "mode": "Sleep",
        }
    )
    water_trigger = board.set_water(True)
    water_alarm = board.emit_telemetry()
    emit(water_trigger, water_alarm)
    water_safe_input = board.set_water(False)
    emit(water_safe_input)
    for _ in range(3):
        emit(*board.advance(1000))
    water_recovered = next(
        frame
        for frame in reversed(frames)
        if frame.get("type") == "telemetry"
    )

    last_frame_ms = board.now_ms
    board.stop_telemetry()
    stopped_output = board.advance(1000)
    offline_boundary = {
        "lastFrameMs": last_frame_ms,
        "freshAt3500": is_frame_fresh(last_frame_ms, last_frame_ms + 3500),
        "freshAt3501": is_frame_fresh(last_frame_ms, last_frame_ms + 3501),
        "framesAfterStop": len(stopped_output),
    }

    telemetry = [frame for frame in frames if frame.get("type") == "telemetry"]
    command_ids = [command["id"] for command in commands]
    ack_by_id = {
        frame.get("id"): frame
        for frame in frames
        if frame.get("type") == "ack" and frame.get("id")
    }
    telemetry_command_ids = {
        frame.get("lastAppliedCommandId")
        for frame in telemetry
        if frame.get("lastAppliedCommandId")
    }

    checks = [
        _check(
            "identity",
            hello.get("profileId") == PROFILE_ID and hello.get("source") == "mock",
            {"profileId": hello.get("profileId"), "source": hello.get("source")},
        ),
        _check(
            "auto_sleep",
            auto_baseline["mode"] == "Auto"
            and sleep_ack.get("ok") is True
            and sleep_telemetry["mode"] == "Sleep"
            and button_auto.get("mode") == "Auto"
            and auto_after_button["mode"] == "Auto",
            {
                "baseline": auto_baseline["mode"],
                "commandMode": sleep_telemetry["mode"],
                "buttonMode": auto_after_button["mode"],
            },
        ),
        _check(
            "knob_cross_threshold",
            knob_low.get("temperatureThresholdC") == 18
            and knob_low_telemetry["actuators"]["fanPercent"] == 100
            and knob_high.get("temperatureThresholdC") == 35
            and knob_high_telemetry["actuators"]["fanPercent"] == 0,
            {
                "temperatureC": 28.0,
                "low": {
                    "raw": knob_low.get("knobRaw"),
                    "thresholdC": knob_low.get("temperatureThresholdC"),
                    "fanPercent": knob_low_telemetry["actuators"]["fanPercent"],
                },
                "high": {
                    "raw": knob_high.get("knobRaw"),
                    "thresholdC": knob_high.get("temperatureThresholdC"),
                    "fanPercent": knob_high_telemetry["actuators"]["fanPercent"],
                },
            },
        ),
        _check(
            "xiaoshu_dahan",
            xiaoshu_ack.get("ok") is True
            and xiaoshu_telemetry["actuators"]["curtainClosePercent"] == 80
            and xiaoshu_telemetry["thresholds"]["lightRelative"] == 30
            and dahan_ack.get("ok") is True
            and dahan_telemetry["actuators"]["curtainClosePercent"] == 20
            and dahan_telemetry["thresholds"]["lightRelative"] == 45,
            {
                "小暑": {"curtain": 80, "lightThreshold": 30},
                "大寒": {"curtain": 20, "lightThreshold": 45},
            },
        ),
        _check(
            "water_trigger_recovery",
            water_sleep_ack.get("ok") is True
            and water_sleep_telemetry["mode"] == "Sleep"
            and water_alarm["alerts"] == ["water"]
            and water_alarm["actuators"]["fanPercent"] == 100
            and water_alarm["actuators"]["relay"] is False
            and water_alarm["actuators"]["buzzer"] is True
            and water_alarm["actuators"]["rgb"] == "red"
            and water_recovered["alerts"] == []
            and water_recovered["actuators"]["fanPercent"] == 35,
            {
                "alarmSeq": water_alarm["seq"],
                "recoverySeq": water_recovered["seq"],
                "alarmOutputs": water_alarm["actuators"],
                "recoveryOutputs": water_recovered["actuators"],
            },
        ),
        _check(
            "command_ack_telemetry",
            all(ack_by_id.get(command_id, {}).get("ok") is True for command_id in command_ids)
            and set(command_ids).issubset(telemetry_command_ids),
            {"commandIds": command_ids, "telemetryCommandIds": sorted(telemetry_command_ids)},
        ),
        _check(
            "offline_boundary",
            offline_boundary["freshAt3500"] is True
            and offline_boundary["freshAt3501"] is False
            and offline_boundary["framesAfterStop"] == 0,
            offline_boundary,
        ),
        _check(
            "mock_source",
            bool(frames) and all(frame.get("source") == "mock" for frame in frames),
            {"frameCount": len(frames), "nonMockCount": sum(frame.get("source") != "mock" for frame in frames)},
        ),
        _check(
            "telemetry_sequence",
            [frame["seq"] for frame in telemetry] == list(range(1, len(telemetry) + 1)),
            {"sequences": [frame["seq"] for frame in telemetry]},
        ),
    ]

    return {
        "profileId": PROFILE_ID,
        "evidence": "mock-only",
        "status": "mock-passed" if all(check["passed"] for check in checks) else "failed",
        "frames": frames,
        "commands": commands,
        "checks": checks,
        "offlineBoundary": offline_boundary,
    }


def _jsonl_bytes(items: list[dict[str, Any]]) -> bytes:
    text = "".join(
        json.dumps(item, ensure_ascii=False, separators=(",", ":")) + "\n"
        for item in items
    )
    return text.encode("utf-8")


def write_g1_evidence(
    output_directory: Path,
    *,
    date_stamp: str | None = None,
    generated_at: str | None = None,
) -> dict[str, Path]:
    run = build_g1_run()
    if run["status"] != "mock-passed":
        failed = [check["id"] for check in run["checks"] if not check["passed"]]
        raise RuntimeError(f"G1 acceptance failed: {', '.join(failed)}")

    now = datetime.now(SHANGHAI)
    date_stamp = date_stamp or now.strftime("%Y%m%d")
    generated_at = generated_at or now.isoformat(timespec="seconds")
    output_directory = Path(output_directory)
    output_directory.mkdir(parents=True, exist_ok=True)
    frame_path = output_directory / f"task10-g1-frames-{date_stamp}.jsonl"
    command_path = output_directory / f"task10-g1-commands-{date_stamp}.jsonl"
    summary_path = output_directory / f"task10-g1-summary-{date_stamp}.json"

    frame_bytes = _jsonl_bytes(run["frames"])
    command_bytes = _jsonl_bytes(run["commands"])
    frame_path.write_bytes(frame_bytes)
    command_path.write_bytes(command_bytes)
    summary = {
        "profileId": run["profileId"],
        "evidence": run["evidence"],
        "status": run["status"],
        "generatedAt": generated_at,
        "frameCount": len(run["frames"]),
        "commandCount": len(run["commands"]),
        "telemetrySequences": [
            frame["seq"] for frame in run["frames"] if frame.get("type") == "telemetry"
        ],
        "offlineBoundary": run["offlineBoundary"],
        "checks": run["checks"],
        "sha256": {
            "frames": hashlib.sha256(frame_bytes).hexdigest(),
            "commands": hashlib.sha256(command_bytes).hexdigest(),
        },
    }
    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return {
        "framePath": frame_path,
        "commandPath": command_path,
        "summaryPath": summary_path,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="生成任务十 G1 确定性模拟验收证据")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("docs/evidence/mock"),
    )
    parser.add_argument("--date-stamp", default=None)
    args = parser.parse_args()
    paths = write_g1_evidence(args.output_dir, date_stamp=args.date_stamp)
    print(
        json.dumps(
            {"status": "mock-passed", **{key: str(value) for key, value in paths.items()}},
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
