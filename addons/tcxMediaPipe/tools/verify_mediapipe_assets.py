#!/usr/bin/env python3
"""Verify offline MediaPipe Web Tasks assets."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path


REQUIRED_WASM = {
    "vision_wasm_internal.js",
    "vision_wasm_internal.wasm",
    "vision_wasm_module_internal.js",
    "vision_wasm_module_internal.wasm",
    "vision_wasm_nosimd_internal.js",
    "vision_wasm_nosimd_internal.wasm",
}

REQUIRED_MODELS = {
    "hand_landmarker.task",
    "pose_landmarker_full.task",
    "face_landmarker.task",
    "gesture_recognizer.task",
}


def addon_root() -> Path:
    return Path(__file__).resolve().parents[1]


def web_root() -> Path:
    return addon_root() / "web"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"Missing {label}: {path}")
    if path.stat().st_size <= 0:
        raise RuntimeError(f"Empty {label}: {path}")


def main() -> int:
    root = web_root()
    require_file(root / "dist" / "index.html", "web build output")
    for filename in REQUIRED_WASM:
        require_file(root / "wasm" / filename, "MediaPipe wasm file")
    for filename in REQUIRED_MODELS:
        require_file(root / "models" / filename, "MediaPipe model")

    manifest_path = root / "models" / "manifest.json"
    require_file(manifest_path, "MediaPipe manifest")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    for entry in manifest.get("wasmFiles", []):
        path = root / "wasm" / entry["file"]
        require_file(path, "manifest wasm file")
        if entry["bytes"] != path.stat().st_size:
            raise RuntimeError(f"Byte count mismatch for {path}")
        if entry["sha256"] != sha256(path):
            raise RuntimeError(f"sha256 mismatch for {path}")

    for entry in manifest.get("models", []):
        path = root / "models" / entry["file"]
        require_file(path, "manifest model file")
        if entry["bytes"] != path.stat().st_size:
            raise RuntimeError(f"Byte count mismatch for {path}")
        if entry["sha256"] != sha256(path):
            raise RuntimeError(f"sha256 mismatch for {path}")

    print("MediaPipe offline assets verified")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"verify_mediapipe_assets.py failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
