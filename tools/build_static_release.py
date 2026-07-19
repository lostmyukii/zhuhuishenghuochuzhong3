#!/usr/bin/env python3
"""Build the deployable static Dashboard directory without publishing it."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

from static_release import PROFILE_ID, build_release


def current_dashboard_commit(repository: Path) -> str:
    completed = subprocess.run(
        ["git", "log", "-1", "--format=%H", "--", "dashboard"],
        cwd=repository,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return completed.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="构建纯静态 HTTPS/Web Serial 部署目录，不上线")
    parser.add_argument("--repository", type=Path, default=Path.cwd())
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, default=Path("dist") / PROFILE_ID)
    args = parser.parse_args()
    repository = args.repository.resolve()
    output = args.output if args.output.is_absolute() else repository / args.output
    manifest = build_release(
        repository / "dashboard",
        output,
        version=args.version,
        source_commit=current_dashboard_commit(repository),
    )
    print(
        json.dumps(
            {
                "status": "release-built-not-deployed",
                "output": str(output),
                "releaseVersion": manifest["releaseVersion"],
                "sourceCommit": manifest["sourceCommit"],
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
