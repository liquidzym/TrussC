#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This script is intended for Raspberry Pi OS / Linux." >&2
  exit 1
fi

app_path="bin/mayaRFID_gui"
if [[ ! -x "$app_path" ]]; then
    echo "Missing $app_path." >&2
    echo "Build the linux preset with MAYARFID_BUILD_GUI=ON first, then run this script again." >&2
  exit 1
fi

if [[ "${MAYARFID_SKIP_PREPARE:-0}" != "1" ]]; then
  ./scripts/pi_prepare_runtime.sh >/dev/null
fi

uid="$(id -u)"
runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$uid}"
display="${DISPLAY:-}"
wayland_display="${WAYLAND_DISPLAY:-}"

if [[ -z "$wayland_display" && -S "$runtime_dir/wayland-0" ]]; then
  wayland_display="wayland-0"
fi

if [[ -z "$display" && -S /tmp/.X11-unix/X0 ]]; then
  display=":0"
fi

if [[ -z "$wayland_display" && -z "$display" ]]; then
  echo "No GUI display session was found." >&2
  echo "Log in to the Pi desktop first, or start a TrussC session, then run this script again." >&2
  exit 2
fi

echo "Starting mayaRFID GUI"
echo "  XDG_RUNTIME_DIR=$runtime_dir"
if [[ -n "$wayland_display" ]]; then
  echo "  WAYLAND_DISPLAY=$wayland_display"
fi
if [[ -n "$display" ]]; then
  echo "  DISPLAY=$display"
fi

env_args=("XDG_RUNTIME_DIR=$runtime_dir")
if [[ -n "$wayland_display" ]]; then
  env_args+=("WAYLAND_DISPLAY=$wayland_display")
fi
if [[ -n "$display" ]]; then
  env_args+=("DISPLAY=$display")
fi

exec env "${env_args[@]}" "./$app_path" "$@"
