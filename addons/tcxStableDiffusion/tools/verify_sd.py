from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import List, Sequence

import tcxsd_models
import setup_sd


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
CURRENT_DIR = ADDON_ROOT / "libs" / "stable-diffusion" / "current"


def _ok(message: str) -> None:
    print(f"[OK] {message}")


def _warn(message: str) -> None:
    print(f"[WARN] {message}")


def _fail(message: str, failures: List[str]) -> None:
    failures.append(message)
    print(f"[FAIL] {message}")


def check_native(failures: List[str], allow_missing: bool = False) -> None:
    manifest = CURRENT_DIR / "build_manifest.json"
    paths = CURRENT_DIR / "tcxStableDiffusionPaths.cmake"
    header = CURRENT_DIR / "include" / "stable-diffusion.h"
    profile = "unknown"

    if not CURRENT_DIR.exists():
        message = f"Native runtime is missing: {CURRENT_DIR}"
        (_warn if allow_missing else lambda m: _fail(m, failures))(message)
        return

    if manifest.exists():
        data = json.loads(manifest.read_text(encoding="utf-8"))
        profile = data.get("profile", "unknown")
        _ok(f"native profile: {profile} @ {data.get('source_commit', 'unknown')}")
    else:
        _fail(f"Missing native manifest: {manifest}", failures)

    if paths.exists():
        _ok(f"CMake path file exists: {paths}")
    else:
        _fail(f"Missing CMake path file: {paths}", failures)

    if header.exists():
        _ok(f"stable-diffusion.h exists: {header}")
    else:
        _fail(f"Missing stable-diffusion.h: {header}", failures)

    library = setup_sd.find_library_file(CURRENT_DIR)
    if library:
        _ok(f"native library exists: {library}")
    else:
        _fail(f"Missing stable-diffusion native library under {CURRENT_DIR}", failures)

    cli = setup_sd.find_cli_file(CURRENT_DIR)
    server = setup_sd.find_server_file(CURRENT_DIR)
    if profile == "windows-cuda":
        if cli:
            _ok(f"sd-cli process backend exists: {cli}")
        else:
            _fail(f"Missing sd-cli process backend under {CURRENT_DIR}", failures)
        if server:
            _ok(f"sd-server persistent backend exists: {server}")
        else:
            _fail(f"Missing sd-server persistent backend under {CURRENT_DIR}", failures)
    elif cli:
        _ok(f"optional sd-cli process backend exists: {cli}")
    if profile != "windows-cuda" and server:
        _ok(f"optional sd-server persistent backend exists: {server}")


def check_model(model_id: str, target: str | None, failures: List[str], allow_missing: bool) -> None:
    registry = tcxsd_models.load_model_registry()
    model = registry.model(model_id)
    target_dir = setup_sd.model_target_dir(model, target)

    missing = []
    for asset in model.files:
        path = target_dir / asset.filename
        if path.exists() and path.stat().st_size > 0:
            _ok(f"{asset.role}: {path}")
        else:
            missing.append(asset)

    if missing:
        message = f"Model '{model_id}' is missing {len(missing)} file(s) in {target_dir}"
        if allow_missing:
            _warn(message)
        else:
            _fail(message, failures)
        for line in tcxsd_models.manual_download_lines(model, target_dir):
            print(line)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Verify tcxStableDiffusion local setup")
    parser.add_argument("--model", default="ideogram4-q4_0")
    parser.add_argument("--model-target")
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument("--skip-model", action="store_true")
    parser.add_argument("--allow-missing", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    failures: List[str] = []

    if not args.skip_native:
        check_native(failures, allow_missing=args.allow_missing)
    if not args.skip_model:
        check_model(args.model, args.model_target, failures, args.allow_missing)

    if failures:
        print("")
        print("Verification failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("")
    print("Verification completed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
