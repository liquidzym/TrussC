#!/usr/bin/env python3
"""Generate and check tcxIOS app configuration fragments."""

from __future__ import annotations

import argparse
import json
import plistlib
import sys
from pathlib import Path


PERMISSIONS = {
    "camera": ("NSCameraUsageDescription", "Capture camera frames for the TrussC app."),
    "microphone": ("NSMicrophoneUsageDescription", "Use microphone input for the TrussC app."),
    "photo-library-read": ("NSPhotoLibraryUsageDescription", "Import images and media from the photo library."),
    "photo-library-add": ("NSPhotoLibraryAddUsageDescription", "Save exported media to the photo library."),
    "motion": ("NSMotionUsageDescription", "Read device motion for interactive controls."),
    "location-when-in-use": ("NSLocationWhenInUseUsageDescription", "Use location while the app is open."),
    "location-always": ("NSLocationAlwaysAndWhenInUseUsageDescription", "Use location for allowed background workflows."),
    "bluetooth": ("NSBluetoothAlwaysUsageDescription", "Connect to nearby Bluetooth devices."),
    "contacts": ("NSContactsUsageDescription", "Read contacts selected by the user."),
}

BACKGROUND_MODES = {
    "background-tasks": ["fetch", "processing"],
    "background-downloads": ["remote-notification"],
    "bluetooth": ["bluetooth-central"],
}

ENTITLEMENTS = {
    "background-downloads": ["com.apple.developer.networking.background"],
    "multipeer": ["com.apple.developer.networking.multicast"],
}

PRIVACY_BY_FEATURE = {
    "camera": [("NSPrivacyCollectedDataTypePhotosorVideos", "Camera capture")],
    "photo-library": [("NSPrivacyCollectedDataTypePhotosorVideos", "Photo library access")],
    "location": [("NSPrivacyCollectedDataTypePreciseLocation", "Location-enabled app functionality")],
    "contacts": [("NSPrivacyCollectedDataTypeOtherUserContactInfo", "User-selected contacts")],
    "arkit": [("NSPrivacyCollectedDataTypeOtherDiagnosticData", "AR session diagnostics")],
}

PRIVACY_BY_PERMISSION = {
    "camera": [("NSPrivacyCollectedDataTypePhotosorVideos", "Camera capture")],
    "photo-library-read": [("NSPrivacyCollectedDataTypePhotosorVideos", "Photo library access")],
    "photo-library-add": [("NSPrivacyCollectedDataTypePhotosorVideos", "Photo library access")],
    "location-when-in-use": [("NSPrivacyCollectedDataTypePreciseLocation", "Location permission")],
    "location-always": [("NSPrivacyCollectedDataTypePreciseLocation", "Location permission")],
    "contacts": [("NSPrivacyCollectedDataTypeOtherUserContactInfo", "Contacts permission")],
}


def split_csv(value: str) -> list[str]:
    if not value:
        return []
    return [part.strip() for part in value.split(",") if part.strip()]


def unique(values: list[str]) -> list[str]:
    out: list[str] = []
    for value in values:
        if value not in out:
            out.append(value)
    return out


def validate_names(names: list[str], known: set[str], label: str) -> None:
    unknown = [name for name in names if name not in known]
    if unknown:
        joined = ", ".join(unknown)
        raise SystemExit(f"Unknown {label}: {joined}")


def collect_privacy_entries(features: list[str], permissions: list[str]) -> list[dict[str, object]]:
    merged: dict[str, list[str]] = {}
    for name in features:
        for category, reason in PRIVACY_BY_FEATURE.get(name, []):
            merged.setdefault(category, [])
            if reason not in merged[category]:
                merged[category].append(reason)
    for name in permissions:
        for category, reason in PRIVACY_BY_PERMISSION.get(name, []):
            merged.setdefault(category, [])
            if reason not in merged[category]:
                merged[category].append(reason)
    return [
        {
            "category": category,
            "reasons": reasons,
        }
        for category, reasons in merged.items()
    ]


def build_requirements(args: argparse.Namespace) -> dict[str, object]:
    permissions = split_csv(args.permissions)
    features = split_csv(args.features)
    background_task_ids = unique(split_csv(args.background_task_id))

    validate_names(permissions, set(PERMISSIONS), "permissions")
    known_features = set(BACKGROUND_MODES) | set(ENTITLEMENTS) | set(PRIVACY_BY_FEATURE) | {
        "notifications",
        "external-display",
        "storekit",
        "motion",
        "microphone",
        "vision",
    }
    validate_names(features, known_features, "features")

    usage_descriptions = {
        key: purpose
        for key, purpose in (PERMISSIONS[name] for name in permissions)
    }
    background_modes = unique([
        mode
        for feature in features
        for mode in BACKGROUND_MODES.get(feature, [])
    ])
    entitlement_keys = unique([
        key
        for feature in features
        for key in ENTITLEMENTS.get(feature, [])
    ])
    privacy_entries = collect_privacy_entries(features, permissions)

    return {
        "permissions": permissions,
        "features": features,
        "infoPlistUsageDescriptions": usage_descriptions,
        "backgroundModes": background_modes,
        "entitlements": entitlement_keys,
        "backgroundTaskIdentifiers": background_task_ids,
        "privacyManifestEntries": privacy_entries,
    }


def info_plist_fragment(requirements: dict[str, object]) -> dict[str, object]:
    fragment: dict[str, object] = {}
    fragment.update(requirements["infoPlistUsageDescriptions"])
    background_modes = requirements["backgroundModes"]
    if background_modes:
        fragment["UIBackgroundModes"] = background_modes
    background_task_ids = requirements["backgroundTaskIdentifiers"]
    if background_task_ids:
        fragment["BGTaskSchedulerPermittedIdentifiers"] = background_task_ids
    return fragment


def background_modes_fragment(requirements: dict[str, object]) -> dict[str, object]:
    background_modes = requirements["backgroundModes"]
    return {"UIBackgroundModes": background_modes} if background_modes else {}


def entitlements_fragment(requirements: dict[str, object]) -> dict[str, object]:
    return {key: True for key in requirements["entitlements"]}


def privacy_manifest(requirements: dict[str, object]) -> dict[str, object]:
    entries = []
    for item in requirements["privacyManifestEntries"]:
        entries.append({
            "NSPrivacyCollectedDataType": item["category"],
            "NSPrivacyCollectedDataTypeLinked": False,
            "NSPrivacyCollectedDataTypeTracking": False,
            "NSPrivacyCollectedDataTypePurposes": item["reasons"],
        })
    return {"NSPrivacyCollectedDataTypes": entries}


def load_plist(path: Path) -> dict[str, object]:
    with path.open("rb") as handle:
        value = plistlib.load(handle)
    if not isinstance(value, dict):
        raise SystemExit(f"{path} is not a plist dictionary")
    return value


def check_info_plist(path: Path, requirements: dict[str, object]) -> list[str]:
    plist = load_plist(path)
    missing: list[str] = []
    for key in requirements["infoPlistUsageDescriptions"]:
        value = plist.get(key)
        if not isinstance(value, str) or not value.strip():
            missing.append(key)

    present_modes = plist.get("UIBackgroundModes", [])
    if not isinstance(present_modes, list):
        present_modes = []
    for mode in requirements["backgroundModes"]:
        if mode not in present_modes:
            missing.append(f"UIBackgroundModes:{mode}")

    present_task_ids = plist.get("BGTaskSchedulerPermittedIdentifiers", [])
    if not isinstance(present_task_ids, list):
        present_task_ids = []
    for task_id in requirements["backgroundTaskIdentifiers"]:
        if task_id not in present_task_ids:
            missing.append(f"BGTaskSchedulerPermittedIdentifiers:{task_id}")
    return missing


def check_entitlements(path: Path, requirements: dict[str, object]) -> list[str]:
    plist = load_plist(path)
    return [
        key
        for key in requirements["entitlements"]
        if key not in plist
    ]


def write_plist(path: Path, value: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as handle:
        plistlib.dump(value, handle, sort_keys=False)


def write_json(path: Path, value: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--permissions", default="", help="Comma-separated permission names.")
    parser.add_argument("--features", default="", help="Comma-separated feature names.")
    parser.add_argument("--background-task-id", default="", help="Comma-separated BGTask identifiers.")
    parser.add_argument("--output-requirements", type=Path)
    parser.add_argument("--output-info-plist", type=Path)
    parser.add_argument("--output-background-modes", type=Path)
    parser.add_argument("--output-entitlements", type=Path)
    parser.add_argument("--output-privacy", type=Path)
    parser.add_argument("--check-info-plist", type=Path)
    parser.add_argument("--check-entitlements", type=Path)
    args = parser.parse_args()

    requirements = build_requirements(args)

    if args.output_requirements:
        write_json(args.output_requirements, requirements)
    else:
        print(json.dumps(requirements, indent=2, sort_keys=False))

    if args.output_info_plist:
        write_plist(args.output_info_plist, info_plist_fragment(requirements))
    if args.output_background_modes:
        write_plist(args.output_background_modes, background_modes_fragment(requirements))
    if args.output_entitlements:
        write_plist(args.output_entitlements, entitlements_fragment(requirements))
    if args.output_privacy:
        write_plist(args.output_privacy, privacy_manifest(requirements))

    missing: list[str] = []
    if args.check_info_plist:
        missing.extend(f"Info.plist:{item}" for item in check_info_plist(args.check_info_plist, requirements))
    if args.check_entitlements:
        missing.extend(f"Entitlements:{item}" for item in check_entitlements(args.check_entitlements, requirements))

    if missing:
        for item in missing:
            print(f"missing {item}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
