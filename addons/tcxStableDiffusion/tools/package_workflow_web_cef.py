from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import sys
import time
from typing import Iterable, Sequence


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
TRUSS_ROOT = ADDON_ROOT.parents[1]
EXAMPLE_ROOT = ADDON_ROOT / "examples" / "workflow-web-cef"
DIST_ROOT = ADDON_ROOT / "dist"
PACKAGE_ROOT = DIST_ROOT / "workflow-web-cef-windows-x64"


def ensure_within(path: pathlib.Path, parent: pathlib.Path) -> pathlib.Path:
    resolved = path.resolve()
    root = parent.resolve()
    if resolved != root and root not in resolved.parents:
        raise RuntimeError(f"Refusing to write outside {root}: {resolved}")
    return resolved


def reset_package_dir(path: pathlib.Path) -> None:
    target = ensure_within(path, DIST_ROOT)
    if target.exists():
        try:
            remove_tree_with_retries(target)
        except RuntimeError as exc:
            stale = locked_package_dir(target)
            try:
                target.rename(stale)
            except OSError as rename_error:
                print(
                    "Could not remove or rename the existing package; reusing it in place. "
                    f"Reason: {rename_error}",
                    file=sys.stderr,
                )
                prepare_existing_package_dir(target)
                return
            print(f"Renamed locked previous package to: {stale}", file=sys.stderr)
    target.mkdir(parents=True, exist_ok=True)


def prepare_existing_package_dir(path: pathlib.Path) -> None:
    for rel in ("workflow-web-cef", "runtime/node-package"):
        target = ensure_within(path / rel, path)
        if target.exists():
            remove_tree_with_retries(target)
    for rel in ("data/workflows/outputs", "data/workflows/tmp", "data/workflows/cache", "data/workflows/logs"):
        target = ensure_within(path / rel, path)
        if target.exists():
            clear_directory(target)


def clear_directory(path: pathlib.Path) -> None:
    for child in path.iterdir():
        if child.is_dir():
            remove_tree_with_retries(child)
        else:
            remove_file_with_retries(child)


def remove_file_with_retries(path: pathlib.Path, attempts: int = 8, delay_seconds: float = 0.5) -> None:
    last_error: OSError | None = None
    for _ in range(attempts):
        try:
            path.unlink()
            return
        except PermissionError as exc:
            last_error = exc
            time.sleep(delay_seconds)
    raise RuntimeError(f"Could not remove locked file: {getattr(last_error, 'filename', path)}") from last_error


def locked_package_dir(path: pathlib.Path) -> pathlib.Path:
    stamp = time.strftime("%Y%m%d-%H%M%S")
    candidate = path.with_name(f"{path.name}.locked-{stamp}")
    counter = 1
    while candidate.exists():
        candidate = path.with_name(f"{path.name}.locked-{stamp}-{counter}")
        counter += 1
    return candidate


def remove_tree_with_retries(path: pathlib.Path, attempts: int = 8, delay_seconds: float = 0.5) -> None:
    last_error: OSError | None = None
    for _ in range(attempts):
        try:
            shutil.rmtree(path)
            return
        except PermissionError as exc:
            last_error = exc
            time.sleep(delay_seconds)
    raise RuntimeError(
        "Could not remove the existing workflow-web-cef package directory. "
        "Close workflow-web-cef.exe, sd-server.exe, and any Node worker using the package, then retry. "
        f"Locked path: {getattr(last_error, 'filename', path)}"
    ) from last_error


def copy_file(source: pathlib.Path, target: pathlib.Path) -> None:
    if not source.exists():
        raise FileNotFoundError(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def copy_tree(source: pathlib.Path, target: pathlib.Path, *, skip_same_files: bool = False) -> None:
    if not source.exists():
        raise FileNotFoundError(source)
    if skip_same_files:
        copy_tree_skipping_same_files(source, target)
        return
    shutil.copytree(source, target, dirs_exist_ok=True)


def copy_tree_skipping_same_files(source: pathlib.Path, target: pathlib.Path) -> None:
    for item in source.rglob("*"):
        relative = item.relative_to(source)
        destination = target / relative
        if item.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() and destination.stat().st_size == item.stat().st_size:
            continue
        copy_file(item, destination)


def find_executable() -> pathlib.Path:
    candidates = [
        EXAMPLE_ROOT / "bin" / "workflow-web-cef.exe",
        EXAMPLE_ROOT / "build" / "bin" / "workflow-web-cef.exe",
    ]
    candidates.extend(EXAMPLE_ROOT.glob("**/workflow-web-cef.exe"))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("workflow-web-cef.exe")


def cef_manifest() -> dict:
    manifest = TRUSS_ROOT / "addons" / "tcxCEF" / "libs" / "cef" / "current" / "build_manifest.json"
    if not manifest.exists():
        raise FileNotFoundError(manifest)
    return json.loads(manifest.read_text(encoding="utf-8"))


def copy_cef_runtime(package_root: pathlib.Path) -> None:
    manifest = cef_manifest()
    for key in ("runtime_files", "resource_files"):
        for value in manifest.get(key, []):
            source = pathlib.Path(value)
            copy_file(source, package_root / source.name)
    locales = pathlib.Path(manifest["resource_dir"]) / "locales"
    if locales.exists():
        copy_tree(locales, package_root / "locales")


def copy_native_runtime(package_root: pathlib.Path) -> None:
    for name in ("sd-server.exe", "sd-cli.exe", "stable-diffusion.dll"):
        copy_file(EXAMPLE_ROOT / "bin" / name, package_root / name)


def ensure_data_dirs(package_root: pathlib.Path) -> None:
    for name in ("outputs", "tmp", "cache", "logs"):
        (package_root / "data" / "workflows" / name).mkdir(parents=True, exist_ok=True)


def copy_data_assets(package_root: pathlib.Path) -> None:
    data_source = EXAMPLE_ROOT / "bin" / "data"
    copy_tree(data_source / "models", package_root / "data" / "models", skip_same_files=True)
    copy_tree(data_source / "inputs", package_root / "data" / "inputs")
    readme = data_source / "README.md"
    if readme.exists():
        copy_file(readme, package_root / "data" / "README.md")
    ensure_data_dirs(package_root)


def verify_layout(package_root: pathlib.Path) -> None:
    required = [
        "workflow-web-cef.exe",
        "libcef.dll",
        "chrome_elf.dll",
        "icudtl.dat",
        "workflow-web-cef/web/dist/index.html",
        "workflow-web-cef/worker/dist/worker.mjs",
        "workflow-web-cef/workflows/text-to-image.zh.json",
        "runtime/node/node.exe",
        "runtime/node-package/src/index.mjs",
        "sd-server.exe",
        "sd-cli.exe",
        "stable-diffusion.dll",
        "data/models",
        "data/inputs",
        "data/workflows/outputs",
        "data/workflows/tmp",
        "data/workflows/cache",
        "data/workflows/logs",
    ]
    missing = [entry for entry in required if not (package_root / entry).exists()]
    if missing:
        raise RuntimeError("Package layout is incomplete:\n- " + "\n- ".join(missing))


def package_example(package_root: pathlib.Path = PACKAGE_ROOT) -> pathlib.Path:
    reset_package_dir(package_root)
    copy_file(find_executable(), package_root / "workflow-web-cef.exe")
    copy_cef_runtime(package_root)
    copy_native_runtime(package_root)
    copy_tree(EXAMPLE_ROOT / "web" / "dist", package_root / "workflow-web-cef" / "web" / "dist")
    copy_tree(EXAMPLE_ROOT / "worker" / "dist", package_root / "workflow-web-cef" / "worker" / "dist")
    copy_tree(EXAMPLE_ROOT / "workflows", package_root / "workflow-web-cef" / "workflows")
    copy_tree(EXAMPLE_ROOT / "runtime" / "node", package_root / "runtime" / "node")
    copy_tree(ADDON_ROOT / "node", package_root / "runtime" / "node-package")
    copy_data_assets(package_root)
    verify_layout(package_root)
    (package_root / "workflow-web-cef-package.json").write_text(
        json.dumps(
            {
                "name": "workflow-web-cef-windows-x64",
                "entry": "workflow-web-cef.exe",
                "contains": [
                    "workflow-web-cef.exe",
                    "CEF runtime",
                    "workflow-web-cef/web/dist",
                    "workflow-web-cef/worker/dist",
                    "workflow-web-cef/workflows",
                    "runtime/node/node.exe",
                    "native sd-server runtime",
                    "data/models",
                    "data/inputs",
                ],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    return package_root


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Package examples/workflow-web-cef for direct Windows launch.")
    parser.add_argument("--output", type=pathlib.Path, default=PACKAGE_ROOT)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    output = package_example(args.output)
    print(f"workflow-web-cef package ready: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
