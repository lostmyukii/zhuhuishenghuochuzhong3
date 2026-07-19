#!/usr/bin/env python3
"""Deterministic JSONL mock for the junior solar-home N16R8 profile.

Every emitted frame carries ``source=mock``.  This program is a software
acceptance aid and never represents real-board evidence.
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
import time
from collections import OrderedDict
from typing import Any, Iterable


PROTOCOL_VERSION = 1
PROJECT = "smartlife-junior"
PROFILE_ID = "smartlife-junior-solar-home-v1"
BOARD_ID = "n16r8_esp32s3"
SERIAL_BAUD = 115200
TELEMETRY_INTERVAL_MS = 1000
OFFLINE_AFTER_MS = 3500
SAFETY_CLEAR_MS = 3000
INTRUSION_CLEAR_MS = 5000
MQ2_ALERT_EQ_PPM = 600
MQ2_CLEAR_EQ_PPM = 550
TEMPERATURE_HYSTERESIS_C = 0.5
ACK_CACHE_SIZE = 16
COMMAND_ID_MAX_LENGTH = 48


SOLAR_TERMS = OrderedDict(
    (
        ("立春", (26, 20, 35)),
        ("雨水", (26, 20, 35)),
        ("惊蛰", (26, 50, 35)),
        ("春分", (26, 50, 35)),
        ("清明", (26, 50, 35)),
        ("谷雨", (26, 50, 35)),
        ("立夏", (27, 80, 35)),
        ("小满", (27, 80, 35)),
        ("芒种", (27, 80, 35)),
        ("夏至", (27, 80, 30)),
        ("小暑", (26, 80, 30)),
        ("大暑", (26, 80, 30)),
        ("立秋", (27, 80, 35)),
        ("处暑", (27, 80, 35)),
        ("白露", (26, 50, 35)),
        ("秋分", (26, 50, 35)),
        ("寒露", (25, 20, 40)),
        ("霜降", (25, 20, 40)),
        ("立冬", (24, 20, 45)),
        ("小雪", (24, 20, 45)),
        ("大雪", (24, 20, 45)),
        ("冬至", (24, 20, 45)),
        ("小寒", (24, 20, 45)),
        ("大寒", (24, 20, 45)),
    )
)


def is_frame_fresh(last_frame_ms: int | None, now_ms: int, stale_ms: int = OFFLINE_AFTER_MS) -> bool:
    """Match the dashboard rule: a frame is fresh through the exact boundary."""
    return last_frame_ms is not None and now_ms - last_frame_ms <= stale_ms


def _mock_frame(frame_type: str, **fields: Any) -> dict[str, Any]:
    return {"type": frame_type, "source": "mock", **fields}


class MockSmartLifeBoard:
    """Pure, deterministic state machine matching the current firmware protocol."""

    def __init__(self) -> None:
        self.now_ms = 0
        self.sequence = 0
        self.telemetry_enabled = True
        self.mode = "Auto"
        self.solar_term = "立春"
        self.guard_armed = False
        self.buzzer_enabled = True
        self.temperature_threshold_c = 27
        self.auto_fan_high = False
        self.last_applied_command_id: str | None = None
        self.applied_command_count = 0
        self._ack_cache: OrderedDict[str, dict[str, Any]] = OrderedDict()
        self._manual: dict[str, Any] = {}
        self._latched = {code: False for code in ("mq2", "flame", "water", "intrusion")}
        self._safe_since: dict[str, int | None] = {code: None for code in self._latched}
        self._last_alerts: list[str] = []
        self.sensors: dict[str, Any] = {
            "temperatureC": 26.0,
            "humidityRh": 55.0,
            "airQualityEqPpm": 180,
            "soundRelative": 18,
            "presence": True,
            "lightRelative": 25,
            "water": False,
            "flame": False,
            "keypadRaw": 4095,
            "knobRaw": 2167,
        }

    def startup_frames(self) -> list[dict[str, Any]]:
        return [self.emit_hello(), self.emit_telemetry()]

    def emit_hello(self) -> dict[str, Any]:
        return _mock_frame(
            "hello",
            protocolVersion=PROTOCOL_VERSION,
            project=PROJECT,
            profileId=PROFILE_ID,
            board=BOARD_ID,
            baud=SERIAL_BAUD,
            rfid=False,
            health={"oled": "ready", "buzzerEnabled": self.buzzer_enabled},
        )

    def emit_telemetry(self) -> dict[str, Any]:
        outputs, alerts = self._evaluate()
        self.sequence += 1
        solar_default, curtain_percent, light_threshold = SOLAR_TERMS[self.solar_term]
        threshold = self.temperature_threshold_c
        display_lines = [
            self.mode,
            f"T:{self.sensors['temperatureC']:.1f} c",
            f"Q:{self.sensors['airQualityEqPpm']} ppm",
            f"N:{self.sensors['soundRelative']} H:{1 if self.sensors['presence'] else 0}",
        ]
        return _mock_frame(
            "telemetry",
            protocolVersion=PROTOCOL_VERSION,
            seq=self.sequence,
            uptimeMs=self.now_ms,
            mode=self.mode,
            solarTerm=self.solar_term,
            sensors=copy.deepcopy(self.sensors),
            thresholds={
                "temperatureC": threshold,
                "solarDefaultTemperatureC": solar_default,
                "lightRelative": light_threshold,
                "mq2EqPpm": MQ2_ALERT_EQ_PPM,
            },
            actuators=outputs,
            display={"page": "score", "lines": display_lines},
            alerts=alerts,
            health={
                "dht": "ok",
                "mq2": "ready",
                "knob": "ok",
                "oled": "ready",
                "buzzerEnabled": self.buzzer_enabled,
                "relayOutputOnly": True,
                "evidence": "mock-only",
            },
            lastAppliedCommandId=self.last_applied_command_id,
        )

    def advance(self, milliseconds: int = TELEMETRY_INTERVAL_MS) -> list[dict[str, Any]]:
        if milliseconds < 0:
            raise ValueError("milliseconds must be non-negative")
        self.now_ms += milliseconds
        _, alerts = self._evaluate()
        if not self.telemetry_enabled:
            return []
        frames: list[dict[str, Any]] = []
        if alerts != self._last_alerts:
            self._last_alerts = list(alerts)
            frames.append(_mock_frame("event", event="alert_changed", active=alerts))
        frames.append(self.emit_telemetry())
        return frames

    def stop_telemetry(self) -> None:
        self.telemetry_enabled = False

    def resume_telemetry(self) -> None:
        self.telemetry_enabled = True

    def press_button_a(self) -> dict[str, Any]:
        self.mode = "Sleep" if self.mode == "Auto" else "Auto"
        self._manual.clear()
        self._evaluate()
        return _mock_frame(
            "event",
            event="button",
            key="A",
            action="toggle_mode",
            mode=self.mode,
        )

    def turn_knob(self, raw_value: int) -> dict[str, Any]:
        raw = max(0, min(4095, int(raw_value)))
        self.sensors["knobRaw"] = raw
        self.temperature_threshold_c = 18 + (raw * 17 + 2047) // 4095
        self._evaluate()
        return _mock_frame(
            "event",
            event="threshold_changed",
            eventSource="knob",
            knobRaw=raw,
            temperatureThresholdC=self.temperature_threshold_c,
        )

    def set_water(self, detected: bool) -> dict[str, Any]:
        self.sensors["water"] = bool(detected)
        return self._alert_change_event()

    def set_flame(self, detected: bool) -> dict[str, Any]:
        self.sensors["flame"] = bool(detected)
        return self._alert_change_event()

    def set_air_quality(self, eq_ppm: int) -> dict[str, Any]:
        self.sensors["airQualityEqPpm"] = max(0, min(1000, int(eq_ppm)))
        return self._alert_change_event()

    def set_presence(self, detected: bool) -> dict[str, Any]:
        self.sensors["presence"] = bool(detected)
        return self._alert_change_event()

    def set_solar_term(self, name: str) -> dict[str, Any]:
        if name not in SOLAR_TERMS:
            raise ValueError(f"unknown solar term: {name}")
        self.solar_term = name
        default_temperature, curtain_percent, light_threshold = SOLAR_TERMS[name]
        self._evaluate()
        return _mock_frame(
            "event",
            event="solar_term_changed",
            solarTerm=name,
            profile={
                "defaultTemperatureC": default_temperature,
                "curtainClosePercent": curtain_percent,
                "lightRelative": light_threshold,
            },
        )

    def handle_json_line(self, line: str) -> dict[str, Any]:
        try:
            command = json.loads(line)
        except json.JSONDecodeError:
            return self._error_ack("", "invalid_json", "命令不是有效 JSON", cache=False)
        if not isinstance(command, dict):
            return self._error_ack("", "invalid_json", "命令必须是 JSON 对象", cache=False)
        return self.handle_command(command)

    def handle_command(self, command: dict[str, Any]) -> dict[str, Any]:
        command_id = command.get("id")
        if not isinstance(command_id, str) or not command_id:
            return self._error_ack("", "missing_id", "命令缺少非空 id", cache=False)
        if len(command_id) > COMMAND_ID_MAX_LENGTH:
            return self._error_ack(command_id, "out_of_range", "命令 id 过长")
        if command_id in self._ack_cache:
            return copy.deepcopy(self._ack_cache[command_id])
        if command.get("type") != "command":
            return self._error_ack(command_id, "unsupported_command", "type 必须是 command")
        if command.get("origin") not in ("dashboard", "voice", "test"):
            return self._error_ack(command_id, "unsupported_origin", "origin 不在白名单")

        action_keys = [key for key in ("mode", "set", "actuator") if key in command]
        if len(action_keys) != 1:
            return self._error_ack(command_id, "unsupported_command", "每条命令只能包含一个动作")

        action = action_keys[0]
        if action == "mode":
            requested_mode = command["mode"]
            if requested_mode not in ("Auto", "Sleep"):
                return self._error_ack(command_id, "invalid_mode", "mode 只接受 Auto 或 Sleep")
            safety_active = bool(self._evaluate()[1])
            self.mode = requested_mode
            self._manual.clear()
            fields: dict[str, Any] = {"applied": {"mode": requested_mode}}
            if safety_active:
                fields["deferredBy"] = "safety"
            return self._success_ack(command_id, **fields)

        if action == "set":
            setting = command["set"]
            if not isinstance(setting, dict) or len(setting) != 1:
                return self._error_ack(command_id, "unsupported_command", "set 每次只允许一个字段")
            key, value = next(iter(setting.items()))
            if key == "solarTerm":
                if value not in SOLAR_TERMS:
                    return self._error_ack(command_id, "invalid_solar_term", "solarTerm 必须是准确节气名称")
                self.solar_term = value
            elif key == "guardArmed":
                if not isinstance(value, bool):
                    return self._error_ack(command_id, "unsupported_command", "guardArmed 必须是布尔值")
                self.guard_armed = value
            elif key == "buzzerEnabled":
                if not isinstance(value, bool):
                    return self._error_ack(command_id, "unsupported_command", "buzzerEnabled 必须是布尔值")
                self.buzzer_enabled = value
            else:
                return self._error_ack(command_id, "unsupported_command", "未知 set 字段")
            return self._success_ack(command_id, applied={key: value})

        actuator = command["actuator"]
        if not isinstance(actuator, dict) or len(actuator) != 1:
            return self._error_ack(command_id, "unsupported_command", "actuator 每次只允许一个字段")
        key, value = next(iter(actuator.items()))
        error = self._validate_actuator(key, value)
        if error is not None:
            return self._error_ack(command_id, *error)
        if self._evaluate()[1]:
            return self._error_ack(command_id, "safety_lock", "安全告警期间不能覆盖执行器")
        self._manual[key] = value
        return self._success_ack(command_id, applied={"actuator": {key: value}})

    def _validate_actuator(self, key: str, value: Any) -> tuple[str, str] | None:
        if key in ("fanPercent", "curtainClosePercent"):
            if isinstance(value, bool) or not isinstance(value, int):
                return "unsupported_command", f"{key} 必须是整数"
            if not 0 <= value <= 100:
                return "out_of_range", f"{key} 范围为 0 到 100"
            return None
        if key in ("relay", "buzzer"):
            if not isinstance(value, bool):
                return "unsupported_command", f"{key} 必须是布尔值"
            return None
        if key == "rgb":
            if value not in ("off", "yellow", "red"):
                return "unsupported_command", "rgb 只接受 off/yellow/red"
            return None
        return "unsupported_command", "未知 actuator 字段"

    def _success_ack(self, command_id: str, **fields: Any) -> dict[str, Any]:
        self.last_applied_command_id = command_id
        self.applied_command_count += 1
        ack = _mock_frame("ack", id=command_id, ok=True, **fields)
        self._cache_ack(command_id, ack)
        return copy.deepcopy(ack)

    def _error_ack(
        self,
        command_id: str,
        error: str,
        message: str,
        *,
        cache: bool = True,
    ) -> dict[str, Any]:
        ack = _mock_frame("ack", id=command_id, ok=False, error=error, message=message)
        if cache and command_id:
            self._cache_ack(command_id, ack)
        return copy.deepcopy(ack)

    def _cache_ack(self, command_id: str, ack: dict[str, Any]) -> None:
        self._ack_cache[command_id] = copy.deepcopy(ack)
        self._ack_cache.move_to_end(command_id)
        while len(self._ack_cache) > ACK_CACHE_SIZE:
            self._ack_cache.popitem(last=False)

    def _alert_change_event(self) -> dict[str, Any]:
        _, alerts = self._evaluate()
        self._last_alerts = list(alerts)
        return _mock_frame("event", event="alert_changed", active=alerts)

    def _update_latch(
        self,
        code: str,
        trigger: bool,
        safe_condition: bool,
        clear_ms: int,
    ) -> None:
        if trigger:
            self._latched[code] = True
            self._safe_since[code] = None
            return
        if not self._latched[code]:
            self._safe_since[code] = None
            return
        if not safe_condition:
            self._safe_since[code] = None
            return
        if self._safe_since[code] is None:
            self._safe_since[code] = self.now_ms
            return
        if self.now_ms - int(self._safe_since[code]) >= clear_ms:
            self._latched[code] = False
            self._safe_since[code] = None

    def _evaluate(self) -> tuple[dict[str, Any], list[str]]:
        air_quality = int(self.sensors["airQualityEqPpm"])
        self._update_latch("mq2", air_quality >= MQ2_ALERT_EQ_PPM, air_quality <= MQ2_CLEAR_EQ_PPM, SAFETY_CLEAR_MS)
        self._update_latch("flame", bool(self.sensors["flame"]), not self.sensors["flame"], SAFETY_CLEAR_MS)
        self._update_latch("water", bool(self.sensors["water"]), not self.sensors["water"], SAFETY_CLEAR_MS)
        if self.guard_armed:
            self._update_latch(
                "intrusion",
                bool(self.sensors["presence"]),
                not self.sensors["presence"],
                INTRUSION_CLEAR_MS,
            )
        else:
            self._latched["intrusion"] = False
            self._safe_since["intrusion"] = None

        alerts = [code for code in ("mq2", "flame", "water", "intrusion") if self._latched[code]]
        _, curtain_percent, light_threshold = SOLAR_TERMS[self.solar_term]
        if self.mode == "Sleep":
            outputs: dict[str, Any] = {
                "fanPercent": 35,
                "curtainClosePercent": curtain_percent,
                "curtainControlEnabled": False,
                "relay": False,
                "buzzer": False,
                "rgb": "off",
            }
        else:
            temperature = float(self.sensors["temperatureC"])
            if temperature >= self.temperature_threshold_c + TEMPERATURE_HYSTERESIS_C:
                self.auto_fan_high = True
            elif temperature <= self.temperature_threshold_c - TEMPERATURE_HYSTERESIS_C:
                self.auto_fan_high = False
            outputs = {
                "fanPercent": 100 if self.auto_fan_high else 0,
                "curtainClosePercent": curtain_percent,
                "curtainControlEnabled": True,
                "relay": bool(self.sensors["presence"]) and int(self.sensors["lightRelative"]) < light_threshold,
                "buzzer": False,
                "rgb": "yellow",
            }

        if alerts:
            if any(code in alerts for code in ("mq2", "flame", "water")):
                outputs["fanPercent"] = 100
                outputs["relay"] = False
            outputs["buzzer"] = self.buzzer_enabled
            outputs["rgb"] = "red"
        else:
            for key, value in self._manual.items():
                outputs[key] = value
        return outputs, alerts


def _json_line(frame: dict[str, Any]) -> str:
    return json.dumps(frame, ensure_ascii=False, separators=(",", ":"))


def _emit(frames: Iterable[dict[str, Any]], realtime_delay: float = 0.0) -> None:
    for frame in frames:
        print(_json_line(frame), flush=True)
        if realtime_delay > 0:
            time.sleep(realtime_delay)


def run_scenario(name: str, ticks: int = 6, realtime: bool = False) -> None:
    board = MockSmartLifeBoard()
    delay = TELEMETRY_INTERVAL_MS / 1000 if realtime else 0.0
    _emit(board.startup_frames(), delay)
    if name == "water-demo":
        _emit((board.set_water(True), board.emit_telemetry()), delay)
        _emit(board.advance(1000), delay)
        _emit((board.set_water(False),), delay)
        for _ in range(max(3, ticks - 2)):
            _emit(board.advance(1000), delay)
        return
    if name == "controls-demo":
        for event in (
            board.press_button_a(),
            board.turn_knob(4095),
            board.set_solar_term("小暑"),
            board.set_solar_term("大寒"),
        ):
            _emit((event, board.emit_telemetry()), delay)
        return
    for _ in range(max(0, ticks)):
        _emit(board.advance(1000), delay)


def serve() -> None:
    board = MockSmartLifeBoard()
    _emit(board.startup_frames())
    for line in sys.stdin:
        if not line.strip():
            continue
        ack = board.handle_json_line(line)
        _emit((ack, board.emit_telemetry()))


def main() -> int:
    parser = argparse.ArgumentParser(description="确定性 N16R8 智慧生活模拟主板（仅模拟证据）")
    parser.add_argument("--scenario", choices=("steady", "water-demo", "controls-demo"), default="steady")
    parser.add_argument("--ticks", type=int, default=6, help="有限场景的遥测步数")
    parser.add_argument("--realtime", action="store_true", help="按 1 秒真实间隔输出")
    parser.add_argument("--serve", action="store_true", help="从 stdin 接收逐行 JSON 命令")
    args = parser.parse_args()
    if args.serve:
        serve()
    else:
        run_scenario(args.scenario, ticks=args.ticks, realtime=args.realtime)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
