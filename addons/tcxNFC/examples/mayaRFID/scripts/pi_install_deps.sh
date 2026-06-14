#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This script is intended for Raspberry Pi OS / Linux." >&2
  exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
  echo "apt-get was not found. Install CMake, build tools, sqlite3, and libsqlite3-dev manually." >&2
  exit 1
fi

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  sqlite3 \
  libsqlite3-dev

trussc_root="${MAYARFID_TRUSSC_ROOT:-}"
if [[ -z "$trussc_root" ]]; then
  for candidate in "$repo_root/../../../TrussC" "$HOME/Desktop/TrussC"; do
    if [[ -d "$candidate/core" ]]; then
      trussc_root="$(cd "$candidate" && pwd)"
      break
    fi
  done
fi

if [[ -n "$trussc_root" && -x "$trussc_root/tools/install_dependencies_linux.sh" ]]; then
  "$trussc_root/tools/install_dependencies_linux.sh" -y
else
  echo "TrussC dependency installer was not found." >&2
  echo "Set MAYARFID_TRUSSC_ROOT=/path/to/TrussC and rerun this script if GUI build dependencies are missing." >&2
fi

if [[ -n "$trussc_root" ]]; then
  shdc="$trussc_root/core/tools/sokol-shdc/sokol-shdc"
  if [[ -f "$shdc" ]]; then
    shdc_info="$(file "$shdc")"
    machine="$(uname -m)"
    if [[ "$machine" == "aarch64" || "$machine" == "arm64" ]]; then
      if [[ "$shdc_info" != *"ELF"* || "$shdc_info" != *"aarch64"* ]]; then
        backup="$shdc.incompatible.$(date +%Y%m%d%H%M%S).bak"
        mv "$shdc" "$backup"
        echo "Moved incompatible sokol-shdc cache to $backup"
      fi
    fi
  fi
fi

echo "Raspberry Pi build dependencies are installed."
