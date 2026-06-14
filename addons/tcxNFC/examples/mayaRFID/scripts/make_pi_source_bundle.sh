#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
addon_root="$(cd "$repo_root/../.." && pwd)"
trussc_root="$(cd "$addon_root/../.." && pwd)"
bundle_dir="${1:-$repo_root/dist/mayaRFID-pi-source}"

rm -rf -- "$bundle_dir"
mkdir -p "$bundle_dir"

rsync -a \
  --include '/.vscode/***' \
  --include '/cmake/***' \
  --include '/config/***' \
  --include '/docs/***' \
  --include '/scripts/***' \
  --include '/src/***' \
  --include '/tools/***' \
  --include '/addons.make' \
  --include '/CMakeLists.txt' \
  --include '/CMakePresets.json' \
  --include '/README.md' \
  --exclude '*' \
  "$repo_root/" "$bundle_dir/"

mkdir -p "$bundle_dir/vendor/tcxNFC"
rsync -a \
  --include '/src/***' \
  --include '/docs/***' \
  --include '/CMakeLists.txt' \
  --include '/README.md' \
  --include '/LICENSE' \
  --include '/addon.json' \
  --exclude '*' \
  "$addon_root/" "$bundle_dir/vendor/tcxNFC/"

mkdir -p "$bundle_dir/vendor/TrussC"
rsync -a \
  --include '/core/' \
  --include '/core/CMakeLists.txt' \
  --include '/core/cmake/***' \
  --include '/core/include/***' \
  --include '/core/platform/***' \
  --include '/core/resources/***' \
  --exclude '*' \
  "$trussc_root/" "$bundle_dir/vendor/TrussC/"

find "$bundle_dir" -name .DS_Store -type f -delete

echo "Created Pi source bundle: $bundle_dir"
echo "Copy this bundle to the Pi, then build with the linux preset."
echo "The bundle includes vendor/tcxNFC and vendor/TrussC/core so it can build outside the TrussC repo."
