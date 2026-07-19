#!/usr/bin/env python3
"""Capture raw serial JSONL plus a separate timestamp integrity sidecar."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Callable, Iterable
from zoneinfo import ZoneInfo


SHANGHAI = ZoneInfo("Asia/Shanghai")


def current_timestamp() -> str:
    return datetime.now(SHANGHAI).isoformat(timespec="milliseconds")


def capture_lines(
    lines: Iterable[str],
    output_path: Path,
    *,
    timestamp_factory: Callable[[], str] = current_timestamp,
) -> Path:
    """Write each raw line byte-for-byte and timestamp it without wrapping it."""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sidecar_path = output_path.with_name(output_path.name + ".timestamps.jsonl")
    with output_path.open("w", encoding="utf-8", newline="") as raw_output, sidecar_path.open(
        "w", encoding="utf-8", newline="\n"
    ) as timestamp_output:
        for line_number, raw_line in enumerate(lines, start=1):
            raw_output.write(raw_line)
            raw_bytes = raw_line.encode("utf-8")
            timestamp_output.write(
                json.dumps(
                    {
                        "lineNumber": line_number,
                        "capturedAt": timestamp_factory(),
                        "byteLength": len(raw_bytes),
                        "sha256": hashlib.sha256(raw_bytes).hexdigest(),
                    },
                    ensure_ascii=False,
                    separators=(",", ":"),
                )
                + "\n"
            )
    return sidecar_path


def default_output_path() -> Path:
    stamp = datetime.now(SHANGHAI).strftime("%Y%m%d-%H%M%S")
    return Path("docs/evidence/mock") / f"serial-capture-{stamp}.jsonl"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="从 stdin 保存原始 JSONL；时间戳写入独立旁车文件，不修改帧内容"
    )
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()
    output = args.output or default_output_path()
    sidecar = capture_lines(sys.stdin, output)
    print(f"raw={output}", file=sys.stderr)
    print(f"timestamps={sidecar}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
