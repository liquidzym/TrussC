#!/usr/bin/env python3
"""Download a CEF binary distribution and build libcef_dll_wrapper.

This script intentionally does not build Chromium or CEF from source. It uses
the standalone CEF binary distributions and compiles only the C++ wrapper
library required by CEF applications.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import ssl
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path


BASE_URL = "https://cef-builds.spotifycdn.com/"
DEFAULT_CONFIG = "Release"


def addon_root() -> Path:
    return Path(__file__).resolve().parents[1]


def detect_platform() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "darwin":
        return "macosarm64" if machine in {"arm64", "aarch64"} else "macosx64"
    if system == "windows":
        if machine not in {"amd64", "x86_64"}:
            raise RuntimeError(f"Unsupported Windows CEF architecture: {machine}")
        return "windows64"
    if system == "linux":
        return "linuxarm64" if machine in {"arm64", "aarch64"} else "linux64"
    raise RuntimeError(f"Unsupported CEF host platform: {platform.system()} {platform.machine()}")


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required tool not found on PATH: {name}")


def read_url(url: str) -> str:
    with urllib.request.urlopen(url, timeout=60, context=tls_context()) as response:
        return response.read().decode("utf-8", errors="replace")


def tls_context() -> ssl.SSLContext:
    paths = ssl.get_default_verify_paths()
    if paths.cafile and Path(paths.cafile).exists():
        return ssl.create_default_context()
    try:
        import certifi  # type: ignore
    except Exception:
        return ssl.create_default_context()
    return ssl.create_default_context(cafile=certifi.where())


def find_latest_archive(binary_platform: str, version: str | None) -> str:
    try:
        listing = json.loads(read_url(BASE_URL + "index.json"))
        platform_info = listing[binary_platform]
        versions = platform_info["versions"]
        for item in versions:
            if version and item.get("cef_version") != version:
                continue
            for file_info in item.get("files", []):
                if file_info.get("type") == "standard":
                    return file_info["name"]
        if version:
            raise RuntimeError(f"CEF version {version} for {binary_platform} was not found in index.json")
        raise RuntimeError(f"No standard CEF archive found for platform {binary_platform}")
    except Exception as json_error:
        index = read_url(BASE_URL + "index.html")
        pattern = re.compile(rf"cef_binary_([^\"<>]+_{re.escape(binary_platform)}\.tar\.bz2)")
        matches = pattern.findall(index)
        if not matches:
            raise RuntimeError(f"No CEF binary archive found for platform {binary_platform}: {json_error}") from json_error

        filenames = [f"cef_binary_{match}" for match in matches]
        if version:
            expected_suffix = f"{version}_{binary_platform}.tar.bz2"
            for filename in filenames:
                if filename.endswith(expected_suffix):
                    return filename
            raise RuntimeError(f"CEF version {version} for {binary_platform} was not found in {BASE_URL}index.html")

        return filenames[0]


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    tmp = destination.with_suffix(destination.suffix + ".tmp")
    with urllib.request.urlopen(url, timeout=120, context=tls_context()) as response, tmp.open("wb") as out:
        shutil.copyfileobj(response, out)
    tmp.replace(destination)


def extract_archive(archive: Path, destination: Path, force: bool) -> Path:
    destination.mkdir(parents=True, exist_ok=True)
    existing = sorted(destination.glob("cef_binary_*"))
    if existing and not force:
        return existing[0]

    if force:
        for item in existing:
            shutil.rmtree(item)

    with tarfile.open(archive, "r:bz2") as tar:
        tar.extractall(destination)

    extracted = sorted(destination.glob("cef_binary_*"))
    if not extracted:
        raise RuntimeError(f"Archive did not contain a cef_binary_* directory: {archive}")
    return extracted[0]


def wrapper_build_env(binary_platform: str, base_env: dict[str, str] | None = None) -> dict[str, str]:
    env = dict(base_env or os.environ)
    if not binary_platform.startswith("windows"):
        return env

    cl_flags = env.get("CL", "")
    parts = cl_flags.split()
    if not any(part.lower() == "/utf-8" for part in parts):
        parts.append("/utf-8")
    env["CL"] = " ".join(parts).strip()
    return env


def run(command: list[str], cwd: Path, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, env=env, check=True)


def build_wrapper(binary_platform: str, source_dir: Path, build_dir: Path, config: str) -> None:
    require_tool("cmake")
    build_dir.mkdir(parents=True, exist_ok=True)
    env = wrapper_build_env(binary_platform)
    run(["cmake", "-S", str(source_dir), "-B", str(build_dir), f"-DCMAKE_BUILD_TYPE={config}"], cwd=source_dir, env=env)
    run(["cmake", "--build", str(build_dir), "--target", "libcef_dll_wrapper", "--config", config, "--parallel"], cwd=source_dir, env=env)


def first_existing(patterns: list[str], root: Path, description: str) -> Path:
    for pattern in patterns:
        matches = sorted(root.glob(pattern))
        for match in matches:
            if match.exists():
                return match
    raise RuntimeError(f"Could not find {description} under {root}")


def collect_files(directory: Path, names: set[str] | None = None) -> list[Path]:
    if not directory.exists():
        return []
    files = []
    for path in sorted(directory.iterdir()):
        if path.is_file() and (names is None or path.name in names):
            files.append(path)
    return files


def make_paths(binary_platform: str, version: str, root: Path, source_dir: Path, build_dir: Path) -> dict[str, object]:
    release_dir = source_dir / "Release"
    resource_dir = source_dir / "Resources"
    include_dir = source_dir
    wrapper_library = first_existing(
        ["**/libcef_dll_wrapper.a", "**/libcef_dll_wrapper.lib"],
        build_dir,
        "libcef_dll_wrapper library",
    )

    cef_framework = ""
    libcef_library = ""
    if binary_platform.startswith("macos"):
        cef_framework_path = release_dir / "Chromium Embedded Framework.framework"
        if not cef_framework_path.exists():
            raise RuntimeError(f"Missing CEF framework: {cef_framework_path}")
        cef_framework = str(cef_framework_path)
        resource_dir = cef_framework_path / "Resources"
        if not resource_dir.exists():
            raise RuntimeError(f"Missing CEF framework resources: {resource_dir}")
    elif binary_platform.startswith("windows"):
        libcef = release_dir / "libcef.lib"
        if not libcef.exists():
            raise RuntimeError(f"Missing libcef import library: {libcef}")
        libcef_library = str(libcef)
    else:
        libcef = release_dir / "libcef.so"
        if not libcef.exists():
            raise RuntimeError(f"Missing libcef shared library: {libcef}")
        libcef_library = str(libcef)

    runtime_names = {
        "chrome_elf.dll",
        "d3dcompiler_47.dll",
        "dxcompiler.dll",
        "dxil.dll",
        "libcef.dll",
        "libcef.so",
        "libEGL.dll",
        "libGLESv2.dll",
        "v8_context_snapshot.bin",
        "vk_swiftshader.dll",
        "vk_swiftshader_icd.json",
        "vulkan-1.dll",
    }
    resource_names = {
        "chrome_100_percent.pak",
        "chrome_200_percent.pak",
        "resources.pak",
        "icudtl.dat",
        "snapshot_blob.bin",
        "v8_context_snapshot.bin",
    }

    runtime_files = [str(path) for path in collect_files(release_dir, runtime_names)]
    resource_files = [str(path) for path in collect_files(resource_dir, resource_names)]
    resource_files.extend(str(path) for path in sorted(resource_dir.glob("*.pak")))
    resource_files = sorted(set(resource_files))

    return {
        "platform": binary_platform,
        "version": version,
        "root": str(root),
        "source_dir": str(source_dir),
        "include_dir": str(include_dir),
        "release_dir": str(release_dir),
        "resource_dir": str(resource_dir),
        "libcef_library": libcef_library,
        "cef_framework_path": cef_framework,
        "wrapper_library": str(wrapper_library),
        "runtime_files": runtime_files,
        "resource_files": resource_files,
    }


def cmake_list(paths: list[str]) -> str:
    return "".join(f'    "{path}"\n' for path in paths)


def write_outputs(paths: dict[str, object], archive_url: str, current_dir: Path) -> None:
    current_dir.mkdir(parents=True, exist_ok=True)
    template = (addon_root() / "tools" / "tcxcef_paths_template.cmake").read_text(encoding="utf-8")
    replacements = {
        "TCXCEF_PLATFORM": str(paths["platform"]),
        "TCXCEF_VERSION": str(paths["version"]),
        "TCXCEF_ROOT": str(paths["root"]),
        "TCXCEF_SOURCE_DIR": str(paths["source_dir"]),
        "TCXCEF_INCLUDE_DIR": str(paths["include_dir"]),
        "TCXCEF_RELEASE_DIR": str(paths["release_dir"]),
        "TCXCEF_RESOURCE_DIR": str(paths["resource_dir"]),
        "TCXCEF_LIBCEF_LIBRARY": str(paths["libcef_library"]),
        "TCXCEF_CEF_FRAMEWORK_PATH": str(paths["cef_framework_path"]),
        "TCXCEF_WRAPPER_LIBRARY": str(paths["wrapper_library"]),
        "TCXCEF_RUNTIME_FILES": cmake_list(paths["runtime_files"]),  # type: ignore[arg-type]
        "TCXCEF_RESOURCE_FILES": cmake_list(paths["resource_files"]),  # type: ignore[arg-type]
    }
    for key, value in replacements.items():
        template = template.replace(f"@{key}@", value)
    (current_dir / "cef_paths.cmake").write_text(template, encoding="utf-8")

    manifest = dict(paths)
    manifest["archive_url"] = archive_url
    manifest["generated_by"] = "tcxCEF/tools/setup_cef.py"
    (current_dir / "build_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Setup CEF for tcxCEF")
    parser.add_argument("--version", default=os.environ.get("TCXCEF_CEF_VERSION"), help="CEF binary version without platform suffix")
    parser.add_argument("--platform", default=os.environ.get("TCXCEF_PLATFORM"), help="CEF binary platform override")
    parser.add_argument("--config", default=os.environ.get("TCXCEF_CONFIG", DEFAULT_CONFIG), help="CMake build config")
    parser.add_argument("--force", action="store_true", help="Re-extract archive and rebuild paths")
    parser.add_argument("--dry-run", action="store_true", help="Resolve the CEF archive URL without downloading or building")
    args = parser.parse_args()

    binary_platform = args.platform or detect_platform()
    archive_name = find_latest_archive(binary_platform, args.version)
    version = archive_name.removeprefix("cef_binary_").removesuffix(f"_{binary_platform}.tar.bz2")
    archive_url = BASE_URL + archive_name

    root = addon_root() / "libs" / "cef" / binary_platform / version
    archive = root / "download" / archive_name
    source_parent = root / "source"
    build_dir = root / f"wrapper-build-{args.config}"

    if args.dry_run:
        print("CEF setup dry run")
        print(f"  platform: {binary_platform}")
        print(f"  version:  {version}")
        print(f"  archive:  {archive_url}")
        return 0

    if not archive.exists() or args.force:
        print(f"Downloading {archive_url}")
        download(archive_url, archive)
    else:
        print(f"Using existing archive: {archive}")

    source_dir = extract_archive(archive, source_parent, args.force)
    build_wrapper(binary_platform, source_dir, build_dir, args.config)
    paths = make_paths(binary_platform, version, root, source_dir, build_dir)
    write_outputs(paths, archive_url, addon_root() / "libs" / "cef" / "current")

    print("CEF setup complete")
    print(f"  platform: {binary_platform}")
    print(f"  version:  {version}")
    print(f"  paths:    {addon_root() / 'libs' / 'cef' / 'current' / 'cef_paths.cmake'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"setup_cef.py failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
