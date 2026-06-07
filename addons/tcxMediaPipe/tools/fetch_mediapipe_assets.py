#!/usr/bin/env python3
"""Fetch MediaPipe Web Tasks assets for offline tcxMediaPipe runtime."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import ssl
import subprocess
import sys
import urllib.request
from pathlib import Path


DEFAULT_TASKS_VISION_VERSION = "0.10.35"
MODEL_SOURCES = {
    "hand_landmarker.task": {
        "name": "hand",
        "url": "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task",
    },
    "pose_landmarker_full.task": {
        "name": "pose",
        "url": "https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_full/float16/latest/pose_landmarker_full.task",
    },
    "face_landmarker.task": {
        "name": "face",
        "url": "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task",
    },
    "gesture_recognizer.task": {
        "name": "gesture",
        "url": "https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/latest/gesture_recognizer.task",
    },
}


def addon_root() -> Path:
    return Path(__file__).resolve().parents[1]


def web_root() -> Path:
    return addon_root() / "web"


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required tool not found on PATH: {name}")


def run(command: list[str], cwd: Path) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tls_context() -> ssl.SSLContext:
    paths = ssl.get_default_verify_paths()
    if paths.cafile and Path(paths.cafile).exists():
        return ssl.create_default_context()
    try:
        import certifi  # type: ignore
    except Exception:
        return ssl.create_default_context()
    return ssl.create_default_context(cafile=certifi.where())


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    tmp = destination.with_suffix(destination.suffix + ".tmp")
    with urllib.request.urlopen(url, timeout=120, context=tls_context()) as response, tmp.open("wb") as out:
        shutil.copyfileobj(response, out)
    tmp.replace(destination)
    if destination.stat().st_size <= 0:
        raise RuntimeError(f"Downloaded empty file: {destination}")


def copy_wasm_files() -> list[dict[str, object]]:
    src = web_root() / "node_modules" / "@mediapipe" / "tasks-vision" / "wasm"
    dst = web_root() / "wasm"
    if not src.is_dir():
        raise RuntimeError(f"MediaPipe wasm directory missing after npm install: {src}")

    dst.mkdir(parents=True, exist_ok=True)
    for item in dst.iterdir():
        if item.name != ".gitkeep":
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

    entries: list[dict[str, object]] = []
    for item in sorted(src.iterdir()):
        if not item.is_file():
            continue
        target = dst / item.name
        shutil.copy2(item, target)
        entries.append({
            "file": item.name,
            "sha256": sha256(target),
            "bytes": target.stat().st_size,
        })
    return entries


def fetch_models() -> list[dict[str, object]]:
    models_dir = web_root() / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, object]] = []
    for filename, meta in MODEL_SOURCES.items():
        target = models_dir / filename
        print(f"Downloading {filename}")
        download(meta["url"], target)
        entries.append({
            "name": meta["name"],
            "file": filename,
            "sourceUrl": meta["url"],
            "sha256": sha256(target),
            "bytes": target.stat().st_size,
        })
    return entries


def update_package_version(version: str) -> None:
    package_path = web_root() / "package.json"
    package = json.loads(package_path.read_text(encoding="utf-8"))
    package.setdefault("dependencies", {})["@mediapipe/tasks-vision"] = version
    package_path.write_text(json.dumps(package, indent=2) + "\n", encoding="utf-8")


def write_manifest(version: str, wasm_files: list[dict[str, object]], models: list[dict[str, object]]) -> None:
    manifest = {
        "tasksVisionVersion": version,
        "wasmFiles": wasm_files,
        "models": models,
    }
    (web_root() / "models" / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    version = os.environ.get("MEDIAPIPE_TASKS_VISION_VERSION", DEFAULT_TASKS_VISION_VERSION)
    require_tool("node")
    require_tool("npm")
    update_package_version(version)
    run(["npm", "install"], cwd=web_root())
    wasm_files = copy_wasm_files()
    models = fetch_models()
    write_manifest(version, wasm_files, models)
    print("MediaPipe offline assets fetched")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"fetch_mediapipe_assets.py failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
