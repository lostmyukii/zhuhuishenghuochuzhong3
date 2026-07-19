#!/usr/bin/env python3
"""Create a read-only G2 preflight record without uploading firmware."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from datetime import datetime
from pathlib import Path
from typing import Any
from zoneinfo import ZoneInfo


SHANGHAI = ZoneInfo("Asia/Shanghai")
PORT_PATTERNS = (
    "cu.usbserial-*",
    "cu.wchusbserial-*",
    "cu.SLAB_USBtoUART*",
    "cu.usbmodem*",
)
EXPECTED_PINS = {
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
MANUAL_CHECKS = (
    ("gpio17-voltage", "万用表确认 GPIO17 信号最大值不超过 3.3V"),
    ("mq2-divider", "万用表确认 MQ2 AO 分压后最大值不超过 3.3V"),
    ("common-ground", "确认风扇、舵机、继电器外部电源与 N16R8 共地"),
    ("relay-no-load", "继电器空载确认触发极性且上电不误吸合"),
    ("servo-detached", "舵机脱开机械连杆确认方向与安全端点"),
    ("wiring-photos", "拍摄电源、模块丝印和固定 GPIO 线号照片"),
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def enumerate_ports(device_directory: Path = Path("/dev")) -> list[str]:
    candidates: set[str] = set()
    for pattern in PORT_PATTERNS:
        candidates.update(str(path) for path in device_directory.glob(pattern))
    return sorted(candidates)


def read_pin_contract(config_path: Path) -> dict[str, int]:
    text = config_path.read_text(encoding="utf-8")
    values: dict[str, int] = {}
    for name, value in re.findall(
        r"constexpr\s+uint8_t\s+(PIN_[A-Z0-9_]+)\s*=\s*(\d+)\s*;", text
    ):
        values[name] = int(value)
    return values


def build_report(
    repository: Path,
    *,
    device_directory: Path = Path("/dev"),
    now: datetime | None = None,
) -> dict[str, Any]:
    now = now or datetime.now(SHANGHAI)
    firmware = repository / "firmware/.pio/build/n16r8_esp32s3/firmware.bin"
    config = repository / "firmware/include/smartlife_config.h"
    if not firmware.is_file():
        raise FileNotFoundError(f"compiled firmware not found: {firmware}")
    if not config.is_file():
        raise FileNotFoundError(f"GPIO contract not found: {config}")

    actual_pins = read_pin_contract(config)
    pin_checks = {
        name: {"expected": expected, "actual": actual_pins.get(name), "match": actual_pins.get(name) == expected}
        for name, expected in EXPECTED_PINS.items()
    }
    ports = enumerate_ports(device_directory)
    return {
        "profileId": "smartlife-junior-solar-home-v1",
        "evidence": "read-only-preflight",
        "generatedAt": now.isoformat(timespec="seconds"),
        "status": "hardware-preflight-pending",
        "uploadAuthorized": False,
        "uploadPerformed": False,
        "serialPort": {
            "patterns": list(PORT_PATTERNS),
            "candidates": ports,
            "confirmed": len(ports) == 1,
            "note": "单一候选也只表示系统枚举成功，不表示已经授权上传。",
        },
        "firmware": {
            "path": str(firmware.relative_to(repository)),
            "sizeBytes": firmware.stat().st_size,
            "sha256": sha256(firmware),
        },
        "gpioContract": {
            "path": str(config.relative_to(repository)),
            "allMatch": all(check["match"] for check in pin_checks.values()),
            "checks": pin_checks,
        },
        "manualChecks": [
            {"id": check_id, "description": description, "status": "pending"}
            for check_id, description in MANUAL_CHECKS
        ],
        "decision": "禁止上传：人工电气项未实测；如无串口候选，还需先解决 USB 数据连接。",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 N16R8 G2 只读预检证据，不烧录")
    parser.add_argument("--repository", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device-directory", type=Path, default=Path("/dev"))
    args = parser.parse_args()
    repository = args.repository.resolve()
    report = build_report(repository, device_directory=args.device_directory)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "output": str(args.output)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
