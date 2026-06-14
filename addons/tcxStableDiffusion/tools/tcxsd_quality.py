from __future__ import annotations

import pathlib
from typing import Any, Dict, List, Mapping

import tcxsd_job


PLACEHOLDER_MARKERS = ("TODO", "TBD", "{subject}", "{prompt}", "{{", "}}", "__")


def _metadata(sidecar: Mapping[str, Any]) -> Mapping[str, Any]:
    metadata = sidecar.get("metadata", {}) or {}
    return metadata if isinstance(metadata, Mapping) else {}


def _as_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _add_unique(items: List[str], value: str) -> None:
    if value not in items:
        items.append(value)


def assess_sidecar(sidecar: Mapping[str, Any]) -> Dict[str, Any]:
    errors: List[str] = []
    warnings: List[str] = []
    checks: Dict[str, Any] = {}
    metadata = _metadata(sidecar)

    prompt = str(metadata.get("prompt", ""))
    if any(marker in prompt for marker in PLACEHOLDER_MARKERS):
        _add_unique(warnings, "PLACEHOLDER_PROMPT")

    expected_width = _as_int(metadata.get("width"), 0)
    expected_height = _as_int(metadata.get("height"), 0)
    actual_width = _as_int(sidecar.get("image_width"), 0)
    actual_height = _as_int(sidecar.get("image_height"), 0)
    if expected_width and expected_height and actual_width and actual_height:
        checks["expected_size"] = [expected_width, expected_height]
        checks["actual_size"] = [actual_width, actual_height]
        if expected_width != actual_width or expected_height != actual_height:
            _add_unique(errors, "SIZE_MISMATCH")

    output = sidecar.get("saved_image_path") or sidecar.get("native_output_path")
    if output:
        output_path = pathlib.Path(str(output))
        if not output_path.exists():
            _add_unique(errors, "OUTPUT_MISSING")
        else:
            size = output_path.stat().st_size
            checks["output_bytes"] = size
            png_size = tcxsd_job.read_png_size(output_path)
            checks["png_size"] = list(png_size) if png_size else None
            if not png_size:
                _add_unique(errors, "INVALID_IMAGE")
            if size < 1024:
                _add_unique(errors, "PLACEHOLDER_OR_BLANK_IMAGE")

    visible_text = metadata.get("visible_text") or metadata.get("expected_text")
    text_status = str(metadata.get("text_check_status", "")).lower()
    text_verified = str(metadata.get("text_verified", "")).lower()
    if visible_text and text_status in {"failed", "fail", "mismatch"}:
        _add_unique(errors, "TEXT_FAILED")
    elif visible_text and text_verified not in {"1", "true", "yes", "pass", "passed"}:
        _add_unique(warnings, "TEXT_NOT_VERIFIED")

    ok = not errors and bool(sidecar.get("ok", True))
    return {
        "ok": ok,
        "error_codes": errors,
        "warning_codes": warnings,
        "checks": checks,
    }
