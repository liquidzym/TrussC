from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any, Dict, List, Mapping, Sequence


REQUIRED_TOP_LEVEL = (
    "job_id",
    "ok",
    "state",
    "duration_seconds",
    "metadata",
)

REQUIRED_SUCCESS_TOP_LEVEL = (
    "saved_image_path",
    "image_width",
    "image_height",
)

REQUIRED_METADATA = (
    "prompt",
    "steps",
    "seed",
    "execution_mode",
)


class SidecarError(ValueError):
    pass


def load_sidecar(path: pathlib.Path | str) -> Dict[str, Any]:
    sidecar_path = pathlib.Path(path)
    try:
        data = json.loads(sidecar_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise SidecarError(f"Invalid JSON in sidecar: {sidecar_path}: {exc}") from exc
    except OSError as exc:
        raise SidecarError(f"Could not read sidecar: {sidecar_path}: {exc}") from exc

    if not isinstance(data, dict):
        raise SidecarError(f"Sidecar root must be a JSON object: {sidecar_path}")
    return data


def validate_sidecar(data: Mapping[str, Any], require_success_image: bool = False) -> List[str]:
    failures: List[str] = []

    for key in REQUIRED_TOP_LEVEL:
        if key not in data:
            failures.append(f"missing top-level field: {key}")

    metadata = data.get("metadata")
    if not isinstance(metadata, Mapping):
        failures.append("metadata must be an object")
        metadata = {}

    for key in REQUIRED_METADATA:
        if key not in metadata:
            failures.append(f"missing metadata field: {key}")

    if data.get("ok") is True or require_success_image:
        for key in REQUIRED_SUCCESS_TOP_LEVEL:
            if key not in data:
                failures.append(f"missing success field: {key}")

    if "duration_seconds" in data and not isinstance(data["duration_seconds"], (int, float)):
        failures.append("duration_seconds must be numeric")

    if "ok" in data and not isinstance(data["ok"], bool):
        failures.append("ok must be boolean")

    return failures


def summarize_sidecar(data: Mapping[str, Any]) -> Dict[str, Any]:
    metadata = data.get("metadata")
    if not isinstance(metadata, Mapping):
        metadata = {}

    return {
        "job_id": data.get("job_id"),
        "ok": data.get("ok"),
        "state": data.get("state"),
        "error": data.get("error"),
        "duration_seconds": data.get("duration_seconds"),
        "saved_image_path": data.get("saved_image_path"),
        "native_output_path": data.get("native_output_path"),
        "image_width": data.get("image_width"),
        "image_height": data.get("image_height"),
        "prompt_profile": metadata.get("prompt_profile"),
        "prompt_kind": metadata.get("prompt_kind"),
        "model": metadata.get("model"),
        "execution_mode": metadata.get("execution_mode"),
        "backend": metadata.get("backend"),
        "steps": metadata.get("steps"),
        "seed": metadata.get("seed"),
        "cfg_scale": metadata.get("cfg_scale"),
        "cli_log": metadata.get("cli_log"),
    }


def print_text_summary(summary: Mapping[str, Any]) -> None:
    for key, value in summary.items():
        if value is None or value == "":
            continue
        print(f"{key}: {value}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Inspect tcxStableDiffusion result sidecar JSON")
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate = subparsers.add_parser("validate", help="Validate sidecar schema")
    validate.add_argument("path", type=pathlib.Path)
    validate.add_argument("--require-success-image", action="store_true")

    summary = subparsers.add_parser("summary", help="Print a compact sidecar summary")
    summary.add_argument("path", type=pathlib.Path)
    summary.add_argument("--json", action="store_true", help="Emit summary as JSON")

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        data = load_sidecar(args.path)
    except SidecarError as exc:
        print(f"[FAIL] {exc}")
        return 1

    if args.command == "validate":
        failures = validate_sidecar(data, require_success_image=args.require_success_image)
        if failures:
            print("[FAIL] invalid sidecar")
            for failure in failures:
                print(f"- {failure}")
            return 1
        print("[OK] sidecar is valid")
        return 0

    if args.command == "summary":
        summary = summarize_sidecar(data)
        if args.json:
            print(json.dumps(summary, ensure_ascii=False, indent=2))
        else:
            print_text_summary(summary)
        return 0

    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
