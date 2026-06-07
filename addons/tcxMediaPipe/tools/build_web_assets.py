#!/usr/bin/env python3
"""Build tcxMediaPipe Vite assets and verify the offline bundle."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


def addon_root() -> Path:
    return Path(__file__).resolve().parents[1]


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required tool not found on PATH: {name}")


def run(command: list[str], cwd: Path) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def main() -> int:
    require_tool("npm")
    root = addon_root()
    web = root / "web"
    run(["npm", "install"], cwd=web)
    run(["npm", "run", "build"], cwd=web)
    run([sys.executable, str(root / "tools" / "verify_mediapipe_assets.py")], cwd=root)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"build_web_assets.py failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
