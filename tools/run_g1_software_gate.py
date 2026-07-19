#!/usr/bin/env python3
"""Run the complete local G1 software gate and persist auditable logs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import time
from datetime import datetime
from pathlib import Path
from typing import Any
from zoneinfo import ZoneInfo


SHANGHAI = ZoneInfo("Asia/Shanghai")
PLATFORMIO = Path("/Users/yukii/.platformio/penv/bin/pio")


def normalize_log_output(output: str) -> str:
    """Remove terminal-only whitespace while preserving the command evidence."""
    return "\n".join(line.rstrip() for line in output.splitlines()).rstrip() + "\n"


def gate_commands() -> list[tuple[str, list[str]]]:
    return [
        ("python-contract-and-mock", ["python3", "-m", "unittest", "discover", "-s", "tools", "-p", "test_*.py"]),
        ("firmware-native", [str(PLATFORMIO), "test", "-d", "firmware", "-e", "native"]),
        ("firmware-build", [str(PLATFORMIO), "run", "-d", "firmware"]),
        ("node-check-protocol", ["node", "--check", "dashboard/protocol-core.js"]),
        ("node-check-alert", ["node", "--check", "dashboard/alert-core.js"]),
        ("node-check-voice", ["node", "--check", "dashboard/voice-intent-core.js"]),
        ("node-check-app", ["node", "--check", "dashboard/app.js"]),
        ("dashboard-protocol", ["node", "dashboard/tests/protocol-core.test.js"]),
        ("dashboard-alert", ["node", "dashboard/tests/alert-core.test.js"]),
        ("dashboard-voice", ["node", "dashboard/tests/voice-intent-core.test.js"]),
        ("dashboard-contract", ["node", "dashboard/tests/dashboard-contract.test.js"]),
        ("dashboard-mock-replay", ["node", "dashboard/tests/mock-replay.test.js"]),
        ("diff-check", ["git", "diff", "--check"]),
    ]


def run_gate(repository: Path) -> tuple[list[dict[str, Any]], str]:
    results: list[dict[str, Any]] = []
    log_parts: list[str] = []
    environment = os.environ.copy()
    environment["PLATFORMIO_SETTING_ENABLE_TELEMETRY"] = "no"
    for name, command in gate_commands():
        started = time.monotonic()
        completed = subprocess.run(
            command,
            cwd=repository,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        duration_ms = round((time.monotonic() - started) * 1000)
        output = normalize_log_output(completed.stdout or "")
        output_bytes = output.encode("utf-8")
        results.append(
            {
                "name": name,
                "command": command,
                "exitCode": completed.returncode,
                "durationMs": duration_ms,
                "outputSha256": hashlib.sha256(output_bytes).hexdigest(),
            }
        )
        log_parts.extend(
            (
                f"===== {name} =====\n",
                f"command={json.dumps(command, ensure_ascii=False)}\n",
                f"exitCode={completed.returncode}\n",
                f"durationMs={duration_ms}\n",
                output,
                "\n",
            )
        )
    return results, "".join(log_parts).rstrip() + "\n"


def write_gate_evidence(
    repository: Path,
    output_directory: Path,
    *,
    date_stamp: str | None = None,
) -> dict[str, Any]:
    if not PLATFORMIO.is_file():
        raise FileNotFoundError(f"PlatformIO not found: {PLATFORMIO}")
    now = datetime.now(SHANGHAI)
    date_stamp = date_stamp or now.strftime("%Y%m%d")
    output_directory.mkdir(parents=True, exist_ok=True)
    log_path = output_directory / f"task10-g1-test-output-{date_stamp}.txt"
    summary_path = output_directory / f"task10-g1-test-summary-{date_stamp}.json"
    results, log_text = run_gate(repository)
    log_bytes = log_text.encode("utf-8")
    log_path.write_bytes(log_bytes)
    passed = all(result["exitCode"] == 0 for result in results)
    summary = {
        "profileId": "smartlife-junior-solar-home-v1",
        "evidence": "software-and-mock-only",
        "status": "mock-passed" if passed else "failed",
        "generatedAt": now.isoformat(timespec="seconds"),
        "checks": results,
        "logSha256": hashlib.sha256(log_bytes).hexdigest(),
    }
    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return {"passed": passed, "logPath": log_path, "summaryPath": summary_path, "summary": summary}


def main() -> int:
    parser = argparse.ArgumentParser(description="执行任务十 G1 软件模拟闸门并保存完整输出")
    parser.add_argument("--repository", type=Path, default=Path.cwd())
    parser.add_argument("--output-dir", type=Path, default=Path("docs/evidence/mock"))
    parser.add_argument("--date-stamp", default=None)
    args = parser.parse_args()
    result = write_gate_evidence(
        args.repository.resolve(),
        args.output_dir,
        date_stamp=args.date_stamp,
    )
    print(
        json.dumps(
            {
                "status": result["summary"]["status"],
                "logPath": str(result["logPath"]),
                "summaryPath": str(result["summaryPath"]),
            },
            ensure_ascii=False,
        )
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
