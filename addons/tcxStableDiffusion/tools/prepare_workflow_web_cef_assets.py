from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import urllib.request
import zipfile
import zlib
from typing import Callable, Iterable, Sequence

import setup_sd
import tcxsd_models


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
TRUSS_ROOT = ADDON_ROOT.parents[1]
EXAMPLE_ROOT = ADDON_ROOT / "examples" / "workflow-web-cef"
EXAMPLE_BIN = EXAMPLE_ROOT / "bin"
DATA_ROOT = EXAMPLE_BIN / "data"
RUNTIME_ROOT = EXAMPLE_ROOT / "runtime"
NODE_RUNTIME = RUNTIME_ROOT / "node"
NODE_VERSION = "20.11.1"
NODE_ZIP_URL = f"https://nodejs.org/dist/v{NODE_VERSION}/node-v{NODE_VERSION}-win-x64.zip"
CACHE_ROOT = ADDON_ROOT / ".codex-logs" / "cache"


def run(command: Sequence[str], cwd: pathlib.Path) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=str(cwd), check=True)


def ensure_within(path: pathlib.Path, parent: pathlib.Path) -> pathlib.Path:
    resolved = path.resolve()
    root = parent.resolve()
    if resolved != root and root not in resolved.parents:
        raise RuntimeError(f"Refusing to write outside {root}: {resolved}")
    return resolved


def copy_or_link_file(source: pathlib.Path, target: pathlib.Path) -> None:
    source = source.resolve()
    target = ensure_within(target, EXAMPLE_ROOT)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and target.stat().st_size == source.stat().st_size:
        return
    try:
        os.link(source, target)
    except OSError:
        shutil.copy2(source, target)


def copy_tree(source: pathlib.Path, target: pathlib.Path) -> None:
    source = source.resolve()
    target = ensure_within(target, EXAMPLE_ROOT)
    if not source.exists():
        raise FileNotFoundError(source)
    for path in source.rglob("*"):
        relative = path.relative_to(source)
        destination = target / relative
        if path.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
        elif path.is_file():
            copy_or_link_file(path, destination)


def stage_cef() -> None:
    manifest = TRUSS_ROOT / "addons" / "tcxCEF" / "libs" / "cef" / "current" / "build_manifest.json"
    if not manifest.exists():
        run([sys.executable, "addons/tcxCEF/tools/setup_cef.py", "--config", "Release"], TRUSS_ROOT)
    run([sys.executable, "addons/tcxCEF/tools/verify_cef.py"], TRUSS_ROOT)


def stage_node_runtime() -> None:
    node_exe = NODE_RUNTIME / ("node.exe" if os.name == "nt" else "bin/node")
    if node_exe.exists():
        print(f"Node runtime already staged: {node_exe}")
        return

    if os.name != "nt":
        found = shutil.which("node")
        if not found:
            raise RuntimeError("No local node runtime found for this platform.")
        NODE_RUNTIME.mkdir(parents=True, exist_ok=True)
        copy_or_link_file(pathlib.Path(found), NODE_RUNTIME / "bin" / "node")
        return

    CACHE_ROOT.mkdir(parents=True, exist_ok=True)
    archive = CACHE_ROOT / f"node-v{NODE_VERSION}-win-x64.zip"
    if not archive.exists():
        print(f"Downloading portable Node runtime: {NODE_ZIP_URL}")
        with urllib.request.urlopen(NODE_ZIP_URL, timeout=120) as response:
            with archive.open("wb") as handle:
                shutil.copyfileobj(response, handle, length=1024 * 1024)

    extract_root = CACHE_ROOT / f"node-v{NODE_VERSION}-win-x64"
    if not (extract_root / "node.exe").exists():
        with zipfile.ZipFile(archive) as handle:
            handle.extractall(CACHE_ROOT)

    copy_tree(extract_root, NODE_RUNTIME)


def stage_native_sd() -> None:
    current = ADDON_ROOT / "libs" / "stable-diffusion" / "current"
    if not current.exists():
        run([sys.executable, "tools/setup_sd.py", "build-native"], ADDON_ROOT)
    files = setup_sd.runtime_files(current)
    if not files:
        raise RuntimeError(f"No stable-diffusion runtime files found under {current}")
    EXAMPLE_BIN.mkdir(parents=True, exist_ok=True)
    for source in files:
        copy_or_link_file(source, EXAMPLE_BIN / source.name)


def stage_models() -> None:
    registry = tcxsd_models.load_model_registry()
    source_root = ADDON_ROOT / "examples" / "ideogram4-basic" / "bin" / "data" / "models"
    target_root = DATA_ROOT / "models"
    for model in registry.all():
        source_dir = source_root / model.id
        target_dir = target_root / model.id
        target_dir.mkdir(parents=True, exist_ok=True)
        missing = []
        for asset in model.files:
            source = source_dir / asset.filename
            target = target_dir / asset.filename
            if source.exists() and source.stat().st_size > 0:
                copy_or_link_file(source, target)
            elif not (target.exists() and target.stat().st_size > 0):
                missing.append(asset.filename)
        if missing:
            print(f"Downloading missing model assets for {model.id}: {', '.join(missing)}")
            tcxsd_models.download_model(model, target_dir, max_attempts=3)


def write_png(path: pathlib.Path, width: int, height: int, pixel: Callable[[int, int], tuple[int, int, int, int]]) -> None:
    path = ensure_within(path, EXAMPLE_ROOT)
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(pixel(x, y))
    payload = bytes(raw)

    def chunk(name: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data) & 0xFFFFFFFF)

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(payload, 9))
        + chunk(b"IEND", b"")
    )


def stage_inputs() -> None:
    control = DATA_ROOT / "inputs" / "control" / "canny-guide.png"
    source = DATA_ROOT / "inputs" / "inpaint" / "source.png"
    mask = DATA_ROOT / "inputs" / "inpaint" / "mask.png"

    write_png(
        control,
        512,
        512,
        lambda x, y: (230, 198, 112, 255)
        if x in {80, 256, 432} or y in {80, 256, 432} or abs((x - 256) * (x - 256) + (y - 256) * (y - 256) - 16000) < 700
        else (13, 11, 8, 255),
    )
    write_png(
        source,
        512,
        512,
        lambda x, y: (
            26 + (x * 24 // 512),
            21 + (y * 20 // 512),
            16 + ((x + y) * 14 // 1024),
            255,
        )
        if not (188 <= x <= 324 and 188 <= y <= 324)
        else (185, 144, 58, 255),
    )
    write_png(
        mask,
        512,
        512,
        lambda x, y: (255, 255, 255, 255) if 188 <= x <= 324 and 188 <= y <= 324 else (0, 0, 0, 255),
    )


def verify_assets() -> None:
    required = [
        NODE_RUNTIME / ("node.exe" if os.name == "nt" else "bin/node"),
        EXAMPLE_BIN / ("sd-server.exe" if os.name == "nt" else "sd-server"),
        EXAMPLE_BIN / ("sd-cli.exe" if os.name == "nt" else "sd-cli"),
        EXAMPLE_BIN / ("stable-diffusion.dll" if os.name == "nt" else "libstable-diffusion.so"),
        DATA_ROOT / "inputs" / "control" / "canny-guide.png",
        DATA_ROOT / "inputs" / "inpaint" / "source.png",
        DATA_ROOT / "inputs" / "inpaint" / "mask.png",
    ]
    missing = [str(path) for path in required if not path.exists()]
    registry = tcxsd_models.load_model_registry()
    for model in registry.all():
        for asset in model.files:
            path = DATA_ROOT / "models" / model.id / asset.filename
            if not (path.exists() and path.stat().st_size > 0):
                missing.append(str(path))
    if missing:
        raise RuntimeError("Missing workflow-web-cef asset(s):\n- " + "\n- ".join(missing))
    print("workflow-web-cef assets verified")


def write_asset_manifest() -> None:
    manifest = {
        "node_version": NODE_VERSION,
        "node_runtime": str(NODE_RUNTIME),
        "data_root": str(DATA_ROOT),
        "native_runtime": str(EXAMPLE_BIN),
        "models": [model.id for model in tcxsd_models.load_model_registry().all()],
        "inputs": [
            "bin/data/inputs/control/canny-guide.png",
            "bin/data/inputs/inpaint/source.png",
            "bin/data/inputs/inpaint/mask.png",
        ],
    }
    (DATA_ROOT / "workflow-web-cef-assets.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def selected(args: argparse.Namespace) -> Iterable[str]:
    if args.all:
        return ("cef", "node_runtime", "native_sd", "models", "inputs")
    steps = []
    if args.cef:
        steps.append("cef")
    if args.node_runtime:
        steps.append("node_runtime")
    if args.native_sd:
        steps.append("native_sd")
    if args.models:
        steps.append("models")
    if args.inputs:
        steps.append("inputs")
    return steps


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Stage developer assets for examples/workflow-web-cef.")
    parser.add_argument("--cef", action="store_true", help="Verify or prepare the tcxCEF Windows runtime.")
    parser.add_argument("--node-runtime", action="store_true", help="Stage a portable Node runtime.")
    parser.add_argument("--native-sd", action="store_true", help="Stage stable-diffusion.cpp runtime binaries.")
    parser.add_argument("--models", action="store_true", help="Stage model assets under bin/data/models.")
    parser.add_argument("--inputs", action="store_true", help="Stage deterministic ControlNet and inpaint input PNGs.")
    parser.add_argument("--all", action="store_true", help="Stage CEF, portable Node, native sd runtime, models, and inputs.")
    parser.add_argument("--verify-only", action="store_true", help="Only verify already staged assets.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.verify_only:
        verify_assets()
        return 0

    steps = list(selected(args))
    if not steps:
        parser.error("Select one or more staging steps, or use --all.")

    for step in steps:
        {
            "cef": stage_cef,
            "node_runtime": stage_node_runtime,
            "native_sd": stage_native_sd,
            "models": stage_models,
            "inputs": stage_inputs,
        }[step]()
    write_asset_manifest()
    verify_assets()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
