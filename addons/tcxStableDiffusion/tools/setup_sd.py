from __future__ import annotations

import argparse
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from typing import List, Sequence

import tcxsd_models


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
NATIVE_ROOT = ADDON_ROOT / "libs" / "stable-diffusion"
SOURCE_DIR = NATIVE_ROOT / "source"
BUILD_ROOT = NATIVE_ROOT / "build"
CURRENT_DIR = NATIVE_ROOT / "current"
SDCPP_REPOSITORY = "https://github.com/leejet/stable-diffusion.cpp.git"


def default_profile() -> str:
    system = platform.system().lower()
    if system == "windows":
        return "windows-cuda"
    if system == "darwin":
        return "macos-metal"
    return "cpu-dev"


def run(command: Sequence[str], cwd: pathlib.Path | None = None) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=str(cwd) if cwd else None, check=True)


def run_with_env(command: Sequence[str], cwd: pathlib.Path | None = None, env: dict[str, str] | None = None) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=str(cwd) if cwd else None, env=env, check=True)


def git_commit(directory: pathlib.Path) -> str:
    try:
        completed = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(directory),
            check=True,
            capture_output=True,
            text=True,
        )
        return completed.stdout.strip()
    except Exception:
        return "unknown"


def ensure_source(force: bool = False) -> None:
    if force and SOURCE_DIR.exists():
        shutil.rmtree(SOURCE_DIR)

    if SOURCE_DIR.exists():
        print(f"stable-diffusion.cpp source already exists: {SOURCE_DIR}")
        return

    SOURCE_DIR.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--depth", "1", "--recursive", SDCPP_REPOSITORY, str(SOURCE_DIR)])


def cmake_build_type() -> str:
    return "Release"


def _candidate_visual_studio_roots() -> List[pathlib.Path]:
    roots: List[pathlib.Path] = []
    vswhere = pathlib.Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        try:
            completed = subprocess.run(
                [
                    str(vswhere),
                    "-latest",
                    "-products",
                    "*",
                    "-requires",
                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                    "-property",
                    "installationPath",
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            path = completed.stdout.strip()
            if path:
                roots.append(pathlib.Path(path))
        except Exception:
            pass

    for drive in ("C:", "D:", "E:"):
        drive_root = pathlib.Path(f"{drive}/")
        roots.extend(drive_root.glob("Program Files/Microsoft Visual Studio/*/*"))
        roots.extend(drive_root.glob("Program Files (x86)/Microsoft Visual Studio/*/*"))

    unique: List[pathlib.Path] = []
    seen = set()
    for root in roots:
        key = str(root).lower()
        if key not in seen and root.exists():
            unique.append(root)
            seen.add(key)
    return unique


def find_ninja() -> pathlib.Path | None:
    found = shutil.which("ninja")
    if found:
        return pathlib.Path(found)

    for root in _candidate_visual_studio_roots():
        candidate = root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja" / "ninja.exe"
        if candidate.exists():
            return candidate
    return None


def find_vcvars64() -> pathlib.Path | None:
    for root in _candidate_visual_studio_roots():
        candidate = root / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if candidate.exists() and _vcvars_works(candidate):
            return candidate
    return None


def _vcvars_works(candidate: pathlib.Path) -> bool:
    try:
        _run_vcvars_batch(candidate, "echo OK")
        return True
    except Exception:
        return False


def find_nvcc() -> pathlib.Path | None:
    found = shutil.which("nvcc")
    if found:
        return pathlib.Path(found)

    cuda_root = pathlib.Path(os.environ.get("CUDA_PATH", ""))
    if cuda_root:
        candidate = cuda_root / "bin" / "nvcc.exe"
        if candidate.exists():
            return candidate

    toolkit_root = pathlib.Path("C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA")
    candidates = sorted(toolkit_root.glob("v*/bin/nvcc.exe"), reverse=True)
    return candidates[0] if candidates else None


def msvc_dev_environment() -> dict[str, str] | None:
    vcvars = find_vcvars64()
    if not vcvars:
        return None

    completed = _run_vcvars_batch(vcvars, "set")
    env = os.environ.copy()
    for line in completed.stdout.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        env[key] = value
    return env


def _run_vcvars_batch(vcvars: pathlib.Path, tail_command: str) -> subprocess.CompletedProcess[str]:
    script = None
    try:
        with tempfile.NamedTemporaryFile("w", suffix=".bat", delete=False, encoding="utf-8") as handle:
            script = pathlib.Path(handle.name)
            handle.write("@echo off\n")
            handle.write(f'call "{vcvars}" >nul\n')
            handle.write("if errorlevel 1 exit /b %errorlevel%\n")
            handle.write(f"{tail_command}\n")
        return subprocess.run(
            ["cmd.exe", "/d", "/c", str(script)],
            check=True,
            capture_output=True,
            text=True,
        )
    finally:
        if script:
            script.unlink(missing_ok=True)


def cmake_configure_command(
    source_dir: pathlib.Path,
    build_dir: pathlib.Path,
    install_dir: pathlib.Path,
    profile: str,
    extra_flags: Sequence[str],
    ninja_path: pathlib.Path | None = None,
    nvcc_path: pathlib.Path | None = None,
) -> List[str]:
    command = [
        "cmake",
        "-S",
        str(source_dir),
        "-B",
        str(build_dir),
    ]

    if profile == "windows-cuda" and ninja_path:
        command.extend([
            "-G",
            "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={ninja_path}",
        ])
        if nvcc_path:
            command.append(f"-DCMAKE_CUDA_COMPILER={nvcc_path}")

    command.extend([
        f"-DCMAKE_INSTALL_PREFIX={install_dir}",
        f"-DCMAKE_BUILD_TYPE={cmake_build_type()}",
        *extra_flags,
    ])
    return command


def install_native(profile: str, force: bool = False) -> None:
    ensure_source(force=force)

    flags = tcxsd_models.stable_diffusion_cmake_flags(profile)
    build_dir = BUILD_ROOT / profile
    install_dir = CURRENT_DIR
    if force and build_dir.exists():
        shutil.rmtree(build_dir)
    if force and install_dir.exists():
        shutil.rmtree(install_dir)

    build_dir.mkdir(parents=True, exist_ok=True)
    install_dir.mkdir(parents=True, exist_ok=True)

    ninja_path = find_ninja() if profile == "windows-cuda" and platform.system().lower() == "windows" else None
    nvcc_path = find_nvcc() if profile == "windows-cuda" else None
    env = msvc_dev_environment() if ninja_path else None

    configure = cmake_configure_command(
        source_dir=SOURCE_DIR,
        build_dir=build_dir,
        install_dir=install_dir,
        profile=profile,
        extra_flags=flags,
        ninja_path=ninja_path,
        nvcc_path=nvcc_path,
    )
    run_with_env(configure, env=env)
    targets = native_build_targets(profile)
    if targets:
        for target in targets:
            run_with_env([
                "cmake",
                "--build",
                str(build_dir),
                "--config",
                cmake_build_type(),
                "--target",
                target,
                "--parallel",
            ], env=env)
    else:
        run_with_env(["cmake", "--build", str(build_dir), "--config", cmake_build_type(), "--parallel"], env=env)
        run_with_env(["cmake", "--install", str(build_dir), "--config", cmake_build_type()], env=env)
    stage_extra_artifacts(build_dir, install_dir)
    write_cmake_paths(profile, install_dir)
    write_manifest(profile, flags, build_dir, install_dir)


def native_build_targets(profile: str) -> List[str]:
    if profile == "windows-cuda":
        return ["stable-diffusion", "sd-cli"]
    return []


def stage_extra_artifacts(build_dir: pathlib.Path, install_dir: pathlib.Path) -> None:
    include_dir = install_dir / "include"
    lib_dir = install_dir / "lib"
    bin_dir = install_dir / "bin"
    include_dir.mkdir(parents=True, exist_ok=True)
    lib_dir.mkdir(parents=True, exist_ok=True)
    bin_dir.mkdir(parents=True, exist_ok=True)

    header = SOURCE_DIR / "include" / "stable-diffusion.h"
    if header.exists():
        shutil.copy2(header, include_dir / header.name)

    for candidate in build_dir.rglob("*"):
        if not candidate.is_file():
            continue
        suffix = candidate.suffix.lower()
        name = candidate.name.lower()
        if suffix in {".dll", ".dylib", ".so"} and "stable-diffusion" in name:
            shutil.copy2(candidate, bin_dir / candidate.name)
        elif suffix in {".lib", ".a"} and "stable-diffusion" in name:
            shutil.copy2(candidate, lib_dir / candidate.name)
        elif candidate.name.lower() in {"sd-cli", "sd-cli.exe"}:
            shutil.copy2(candidate, bin_dir / candidate.name)


def _cmake_path(path: pathlib.Path) -> str:
    return str(path.resolve()).replace("\\", "/")


def find_library_file(install_dir: pathlib.Path) -> pathlib.Path | None:
    candidates = list((install_dir / "lib").glob("stable-diffusion.*"))
    candidates += list((install_dir / "bin").glob("stable-diffusion.*"))
    for suffix in (".lib", ".a", ".dylib", ".so", ".dll"):
        for candidate in candidates:
            if candidate.suffix.lower() == suffix:
                return candidate
    return candidates[0] if candidates else None


def runtime_files(install_dir: pathlib.Path) -> List[pathlib.Path]:
    files: List[pathlib.Path] = []
    for folder in (install_dir / "bin", install_dir / "lib"):
        if not folder.exists():
            continue
        for suffix in ("*.dll", "*.dylib", "*.so"):
            files.extend(folder.glob(suffix))
        for executable in ("sd-cli.exe", "sd-cli"):
            files.extend(folder.glob(executable))
    return sorted(files)


def find_cli_file(install_dir: pathlib.Path) -> pathlib.Path | None:
    candidates = []
    for folder in (install_dir / "bin", install_dir):
        candidates.extend(folder.glob("sd-cli.exe"))
        candidates.extend(folder.glob("sd-cli"))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def write_cmake_paths(profile: str, install_dir: pathlib.Path) -> None:
    include_dir = install_dir / "include"
    library = find_library_file(install_dir)
    cli = find_cli_file(install_dir)
    runtimes = runtime_files(install_dir)
    path_file = install_dir / "tcxStableDiffusionPaths.cmake"

    runtime_lines = "\n".join(f'    "{_cmake_path(path)}"' for path in runtimes)
    path_file.write_text(
        "\n".join(
            [
                "# Generated by tools/setup_sd.py. Do not edit by hand.",
                f'set(TCXSD_NATIVE_PROFILE "{profile}")',
                f'set(TCXSD_NATIVE_ROOT "{_cmake_path(install_dir)}")',
                f'set(TCXSD_NATIVE_INCLUDE_DIR "{_cmake_path(include_dir)}")',
                f'set(TCXSD_NATIVE_LIBRARY "{_cmake_path(library) if library else ""}")',
                f'set(TCXSD_NATIVE_CLI "{_cmake_path(cli) if cli else ""}")',
                "set(TCXSD_NATIVE_RUNTIME_FILES",
                runtime_lines,
                ")",
                "",
            ]
        ),
        encoding="utf-8",
    )


def write_manifest(
    profile: str,
    flags: Sequence[str],
    build_dir: pathlib.Path,
    install_dir: pathlib.Path,
) -> None:
    manifest = {
        "profile": profile,
        "repository": SDCPP_REPOSITORY,
        "source_commit": git_commit(SOURCE_DIR),
        "configured_flags": list(flags),
        "build_dir": str(build_dir),
        "install_dir": str(install_dir),
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "runtime_files": [str(path) for path in runtime_files(install_dir)],
        "library": str(find_library_file(install_dir) or ""),
        "cli": str(find_cli_file(install_dir) or ""),
    }
    (install_dir / "build_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def model_target_dir(model: tcxsd_models.ModelSpec, target: str | None) -> pathlib.Path:
    if target:
        return pathlib.Path(target).resolve()
    return (ADDON_ROOT / "examples" / model.example / "models").resolve()


def list_models() -> None:
    registry = tcxsd_models.load_model_registry()
    for model in registry.all():
        print(f"{model.id} [{model.family}] -> examples/{model.example}/models")
        for asset in model.files:
            print(f"  - {asset.role}: {asset.filename}")


def download_model(model_id: str, target: str | None, attempts: int) -> None:
    registry = tcxsd_models.load_model_registry()
    model = registry.model(model_id)
    target_dir = model_target_dir(model, target)

    print(f"Downloading {model.id} into {target_dir}")
    try:
        files = tcxsd_models.download_model(model, target_dir, max_attempts=attempts)
    except tcxsd_models.DownloadFailure as exc:
        print("")
        print(f"自动下载失败超过 {exc.attempts} 次。请手动下载以下文件：")
        for line in tcxsd_models.manual_download_lines(model, target_dir):
            print(line)
        raise SystemExit(2) from exc

    for path in files:
        print(f"OK: {path}")


def bootstrap(args: argparse.Namespace) -> None:
    if args.build_native:
        install_native(args.profile, force=args.force)
    if args.model:
        download_model(args.model, args.model_target, args.attempts)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="tcxStableDiffusion setup helper")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("list-models", help="List built-in starter model profiles")

    download = sub.add_parser("download-model", help="Download a starter model into an example folder")
    download.add_argument("--model", default="ideogram4-q4_0")
    download.add_argument("--target")
    download.add_argument("--attempts", type=int, default=3)

    build = sub.add_parser("build-native", help="Clone and build stable-diffusion.cpp once")
    build.add_argument("--profile", default=default_profile())
    build.add_argument("--force", action="store_true")

    boot = sub.add_parser("bootstrap", help="Build native runtime and/or download a starter model")
    boot.add_argument("--profile", default=default_profile())
    boot.add_argument("--build-native", action="store_true")
    boot.add_argument("--model", default="ideogram4-q4_0")
    boot.add_argument("--model-target")
    boot.add_argument("--attempts", type=int, default=3)
    boot.add_argument("--force", action="store_true")

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.command == "list-models":
        list_models()
    elif args.command == "download-model":
        download_model(args.model, args.target, args.attempts)
    elif args.command == "build-native":
        install_native(args.profile, force=args.force)
    elif args.command == "bootstrap":
        bootstrap(args)
    else:
        parser.error(f"Unknown command: {args.command}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed with exit code {exc.returncode}: {exc.cmd}", file=sys.stderr)
        raise SystemExit(exc.returncode)
    except KeyboardInterrupt:
        print("Cancelled.", file=sys.stderr)
        raise SystemExit(130)
