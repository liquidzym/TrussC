#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This script is intended for Raspberry Pi OS / Linux." >&2
  exit 1
fi

rm -rf -- \
  .git \
  .gitignore \
  .superpowers \
  build \
  build-macos \
  ref \
  "世界树_RFID_TrussC_WS2812_Codex任务书.md"

find . -name .DS_Store -type f -delete

echo "Pruned non-runtime Pi workspace files."
echo "Kept source, CMake files, VSCode settings, docs, scripts, bin/, build-linux/, config/, tools/, and vendor/tcxNFC."
