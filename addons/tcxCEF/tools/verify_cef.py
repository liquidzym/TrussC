#!/usr/bin/env python3
"""Verify tcxCEF generated CEF setup files."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def addon_root() -> Path:
    return Path(__file__).resolve().parents[1]


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"Missing {label}: {path}")
    if path.stat().st_size <= 0:
        raise RuntimeError(f"Empty {label}: {path}")


def require_dir(path: Path, label: str) -> None:
    if not path.is_dir():
        raise RuntimeError(f"Missing {label}: {path}")


def main() -> int:
    current = addon_root() / "libs" / "cef" / "current"
    manifest_path = current / "build_manifest.json"
    paths_path = current / "cef_paths.cmake"
    require_file(manifest_path, "CEF build manifest")
    require_file(paths_path, "CEF CMake paths")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require_dir(Path(manifest["source_dir"]), "CEF source directory")
    require_dir(Path(manifest["include_dir"]) / "include", "CEF include directory")
    require_dir(Path(manifest["release_dir"]), "CEF release directory")
    require_dir(Path(manifest["resource_dir"]), "CEF resource directory")
    require_file(Path(manifest["wrapper_library"]), "libcef_dll_wrapper")

    platform = manifest["platform"]
    if platform.startswith("macos"):
        require_dir(Path(manifest["cef_framework_path"]), "CEF macOS framework")
    else:
        require_file(Path(manifest["libcef_library"]), "libcef library")

    for file_name in manifest.get("runtime_files", []):
        require_file(Path(file_name), "CEF runtime file")
    for file_name in manifest.get("resource_files", []):
        require_file(Path(file_name), "CEF resource file")

    print("tcxCEF setup verified")
    print(f"  platform: {platform}")
    print(f"  version:  {manifest['version']}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"verify_cef.py failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
