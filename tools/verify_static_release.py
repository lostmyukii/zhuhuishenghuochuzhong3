#!/usr/bin/env python3
"""Verify all files and hashes in a built static Dashboard release."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from static_release import PROFILE_ID, verify_release


def main() -> int:
    parser = argparse.ArgumentParser(description="核验部署目录的文件、版本引用和 SHA-256")
    parser.add_argument("--release", type=Path, default=Path("dist") / PROFILE_ID)
    args = parser.parse_args()
    manifest = verify_release(args.release.resolve())
    print(
        json.dumps(
            {
                "status": "release-verified-not-deployed",
                "releaseVersion": manifest["releaseVersion"],
                "sourceCommit": manifest["sourceCommit"],
                "assetCount": len(manifest["assets"]),
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
