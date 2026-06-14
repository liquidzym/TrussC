#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

runtime_dir="${MAYARFID_RUNTIME_DIR:-bin/data}"
config_dir="$runtime_dir/config"
logs_dir="$runtime_dir/logs"
config_source="${MAYARFID_CONFIG_SOURCE:-config/pi01.example.yaml}"
config_dest="$config_dir/pi01.example.yaml"
cli_path="bin/mayaRFID_cli"
app_path="bin/mayaRFID"

if [[ ! -x "$cli_path" || ! -x "$app_path" ]]; then
  echo "Missing bin/mayaRFID or bin/mayaRFID_cli." >&2
  echo "Build the linux preset in VSCode first, then run this script again." >&2
  exit 1
fi

mkdir -p "$config_dir" "$logs_dir"

if [[ ! -f "$config_dest" ]]; then
  cp "$config_source" "$config_dest"
  echo "Created runtime config: $config_dest"
else
  echo "Keeping existing runtime config: $config_dest"
fi

"./$cli_path" config-check
"./$cli_path" storage-check

cat <<EOF
Runtime is prepared.

Next checks:
  ./bin/mayaRFID_cli reader-ping
  ./bin/mayaRFID_cli read-uid
  ./bin/mayaRFID --mode headless --once
EOF
