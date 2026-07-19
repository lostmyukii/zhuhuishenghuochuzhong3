"""Build and verify a dependency-free static Dashboard release."""

from __future__ import annotations

import hashlib
import json
import re
import shutil
from pathlib import Path
from typing import Any


PROFILE_ID = "smartlife-junior-solar-home-v1"
ENTRYPOINT = "index.html"
SOURCE_ASSETS = (
    "index.html",
    "style.css",
    "protocol-core.js",
    "alert-core.js",
    "voice-intent-core.js",
    "app.js",
)
RELEASE_ONLY_ASSETS = (".nojekyll",)
VERSIONED_ASSETS = SOURCE_ASSETS[1:]
FORBIDDEN_VISIBLE_TERMS = ("评分", "得分", "满分", "评委")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_version(version: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", version):
        raise ValueError("release version must be 1-64 URL-safe characters")
    return version


def validate_source(source_directory: Path) -> None:
    missing = [name for name in SOURCE_ASSETS if not (source_directory / name).is_file()]
    if missing:
        raise ValueError(f"missing dashboard assets: {', '.join(missing)}")

    html = (source_directory / ENTRYPOINT).read_text(encoding="utf-8")
    app = (source_directory / "app.js").read_text(encoding="utf-8")
    visible_sources = html + "\n" + app
    forbidden = [term for term in FORBIDDEN_VISIBLE_TERMS if term in visible_sources]
    if forbidden:
        raise ValueError(f"visible score-operation terms are forbidden: {', '.join(forbidden)}")
    if "navigator.serial" not in app or "window.isSecureContext" not in app:
        raise ValueError("dashboard must retain Web Serial secure-context checks")
    if "smartlife-junior-solar-home-v1" not in visible_sources:
        raise ValueError("dashboard profile identity is missing")
    if re.search(r"(?:src|href)=[\"']https?://", html, flags=re.IGNORECASE):
        raise ValueError("remote script/style dependencies are not allowed")
    lowered = visible_sources.lower()
    if "wss://" in lowered or "mqtt" in lowered or "serviceworker" in lowered:
        raise ValueError("release must remain static Web Serial without WSS, MQTT, or Service Worker")

    positions = []
    for asset in VERSIONED_ASSETS:
        token = f'src="{asset}"' if asset.endswith(".js") else f'href="{asset}"'
        position = html.find(token)
        if position < 0:
            raise ValueError(f"index does not reference {asset}")
        positions.append(position)
    if positions != sorted(positions):
        raise ValueError("dashboard asset order is not deterministic")


def version_index(html: str, version: str) -> str:
    for asset in VERSIONED_ASSETS:
        attribute = "src" if asset.endswith(".js") else "href"
        old = f'{attribute}="{asset}"'
        new = f'{attribute}="{asset}?v={version}"'
        if html.count(old) != 1:
            raise ValueError(f"expected one unversioned reference for {asset}")
        html = html.replace(old, new)
    return html


def build_release(
    source_directory: Path,
    output_directory: Path,
    *,
    version: str,
    source_commit: str,
) -> dict[str, Any]:
    version = validate_version(version)
    validate_source(source_directory)
    if output_directory.exists() and any(output_directory.iterdir()):
        raise FileExistsError(f"release directory must be absent or empty: {output_directory}")
    output_directory.mkdir(parents=True, exist_ok=True)

    for name in SOURCE_ASSETS:
        source = source_directory / name
        target = output_directory / name
        if name == ENTRYPOINT:
            target.write_text(version_index(source.read_text(encoding="utf-8"), version), encoding="utf-8")
        else:
            shutil.copyfile(source, target)
    (output_directory / ".nojekyll").write_bytes(b"")

    asset_entries = []
    for name in (*SOURCE_ASSETS, *RELEASE_ONLY_ASSETS):
        path = output_directory / name
        asset_entries.append(
            {"path": name, "sizeBytes": path.stat().st_size, "sha256": file_sha256(path)}
        )
    manifest = {
        "schemaVersion": 1,
        "profileId": PROFILE_ID,
        "releaseVersion": version,
        "sourceCommit": source_commit,
        "entrypoint": ENTRYPOINT,
        "runtime": "static-https-web-serial",
        "deploymentBoundary": "no-cloud-command-relay-no-mqtt-no-score-ui",
        "assets": asset_entries,
    }
    (output_directory / "asset-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    verify_release(output_directory)
    return manifest


def verify_release(output_directory: Path) -> dict[str, Any]:
    manifest_path = output_directory / "asset-manifest.json"
    if not manifest_path.is_file():
        raise ValueError("asset-manifest.json is missing")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("profileId") != PROFILE_ID:
        raise ValueError("release profile identity mismatch")
    version = validate_version(manifest.get("releaseVersion", ""))
    if manifest.get("entrypoint") != ENTRYPOINT:
        raise ValueError("release entrypoint mismatch")
    if manifest.get("runtime") != "static-https-web-serial":
        raise ValueError("release runtime mismatch")

    entries = manifest.get("assets")
    if not isinstance(entries, list):
        raise ValueError("manifest assets must be a list")
    expected_names = set((*SOURCE_ASSETS, *RELEASE_ONLY_ASSETS))
    recorded_names = {entry.get("path") for entry in entries if isinstance(entry, dict)}
    if recorded_names != expected_names or len(entries) != len(expected_names):
        raise ValueError("manifest asset list mismatch")

    for entry in entries:
        path = output_directory / entry["path"]
        if not path.is_file():
            raise ValueError(f"release asset missing: {entry['path']}")
        if path.stat().st_size != entry.get("sizeBytes"):
            raise ValueError(f"release asset size mismatch: {entry['path']}")
        if file_sha256(path) != entry.get("sha256"):
            raise ValueError(f"release asset hash mismatch: {entry['path']}")

    actual_files = {
        str(path.relative_to(output_directory))
        for path in output_directory.rglob("*")
        if path.is_file()
    }
    if actual_files != expected_names | {"asset-manifest.json"}:
        raise ValueError("release directory contains untracked or missing files")

    html = (output_directory / ENTRYPOINT).read_text(encoding="utf-8")
    for asset in VERSIONED_ASSETS:
        attribute = "src" if asset.endswith(".js") else "href"
        if f'{attribute}="{asset}?v={version}"' not in html:
            raise ValueError(f"versioned reference missing: {asset}")
    if any(term in html for term in FORBIDDEN_VISIBLE_TERMS):
        raise ValueError("release entrypoint contains score-operation wording")
    if re.search(r"(?:src|href)=[\"']https?://", html, flags=re.IGNORECASE):
        raise ValueError("release entrypoint contains remote dependencies")
    return manifest
