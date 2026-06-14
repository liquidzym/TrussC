# Raspberry Pi Deployment Notes

These notes capture the issues found while bringing up the reference `mayaRFID` app on a Raspberry Pi, adapted for the `tcxNFC/examples/mayaRFID` addon example.

## Project Layout

Use this layout on the Pi when deploying the generated bundle:

```text
/home/showlabpi4/Desktop/mayaRFID
/home/showlabpi4/Desktop/mayaRFID/vendor/tcxNFC
```

The source bundle includes `vendor/tcxNFC`, so this example can build outside the TrussC checkout while still using the new addon implementation. Inside the main TrussC repo, the example uses the parent `addons/tcxNFC` source directly.

## First-Time Pi Steps

Create a minimal source bundle on the Mac instead of copying the whole development repository:

```bash
./scripts/make_pi_source_bundle.sh
```

Copy `dist/mayaRFID-pi-source` to the Pi as `~/Desktop/mayaRFID`. The bundle intentionally excludes `.git`, `ref`, task books, local build folders, and runtime databases, but keeps these deployment docs and scripts.

Install Linux build dependencies and optional TrussC GUI dependencies:

```bash
cd ~/Desktop/mayaRFID
./scripts/pi_install_deps.sh
```

Open `~/Desktop/mayaRFID` in VSCode, select the `linux` preset, and build. The committed Linux build preset uses one build job for Raspberry Pi stability.

Prepare runtime files:

```bash
./scripts/pi_prepare_runtime.sh
```

Start the GUI:

```bash
./scripts/pi_run_gui.sh
```

The GUI script detects the current desktop session (`wayland-0` or `DISPLAY=:0`) and runs:

```bash
bin/mayaRFID --mode gui
```

`mayaRFID_cli` remains the console-only diagnostic binary. The `mayaRFID --mode gui` path in this addon example uses the same activation runtime as headless mode; it is a deployment placeholder for a future TrussC windowed dashboard.

After an accidental full repository copy, prune the Pi workspace:

```bash
./scripts/pi_prune_workspace.sh
```

This removes development-only files while keeping source, scripts, VSCode settings, `bin/`, `build-linux/`, and runtime config/data.

## Runtime Config Path

The runtime config lives here:

```text
bin/data/config/pi01.example.yaml
```

Do not edit only `config/pi01.example.yaml` on the Pi after deployment; that file is the source template. Runtime changes such as `rfid.reader_source_host` should go into `bin/data/config/pi01.example.yaml`.

The JSONL log resolves under the same `bin/data` tree even when the program is launched by absolute path from another shell working directory. `worldtree_rfid.db` is kept as a compatibility placeholder for scripts that came from the reference app.

```text
bin/data/worldtree_rfid.db
bin/data/logs/events.jsonl
```

## Reader Network

The BKS-710I reader is `192.168.1.100:502`. If Wi-Fi also uses `192.168.1.0/24`, do not configure the Pi Ethernet port with a gateway/router address. Use a host route instead, for example:

```bash
sudo nmcli con mod netplan-eth0 \
  ipv4.method manual \
  ipv4.addresses 192.168.1.10/32 \
  ipv4.routes "192.168.1.100/32" \
  ipv4.never-default yes
sudo nmcli con up netplan-eth0
ip route get 192.168.1.100
```

Then set the runtime config:

```yaml
rfid:
  reader_host: "192.168.1.100"
  reader_source_host: "192.168.1.10"
  reader_port: 502
```

Expected checks:

```bash
./bin/mayaRFID_cli config-check
./bin/mayaRFID_cli reader-ping
./bin/mayaRFID_cli read-uid
```

## TrussC `sokol-shdc` Architecture Cache

Problem seen during Pi GUI build:

```text
/home/showlabpi4/Desktop/TrussC/core/tools/sokol-shdc/sokol-shdc: Exec format error
```

Root cause: `core/tools/sokol-shdc/sokol-shdc` was a copied macOS x86_64 executable. TrussC CMake only downloads `sokol-shdc` when the file is missing, so an incompatible cached file prevents the Pi from building shaders.

Fix:

```bash
file ~/Desktop/TrussC/core/tools/sokol-shdc/sokol-shdc
mv ~/Desktop/TrussC/core/tools/sokol-shdc/sokol-shdc \
   ~/Desktop/TrussC/core/tools/sokol-shdc/sokol-shdc.incompatible.bak
cmake --preset linux
```

`scripts/pi_install_deps.sh` now checks this automatically on Linux and renames incompatible cached tools so CMake can download the correct `linux_arm64` binary.

## GUI Session

On the tested Pi, the desktop session was `labwc` with:

```text
DISPLAY=:0
XDG_RUNTIME_DIR=/run/user/1000
WAYLAND_DISPLAY=wayland-0
```

If launching over SSH, those variables may not exist in the SSH environment. Use:

```bash
./scripts/pi_run_gui.sh
```

The script fills them from the active desktop session. It exits with a clear error if no desktop display socket is available.

## Verified Smoke Test

The verified Pi flow was:

```bash
cmake --preset linux
cmake --build --preset linux
./bin/mayaRFID_cli config-check
./bin/mayaRFID_cli storage-check
./bin/mayaRFID_cli reader-ping
./bin/mayaRFID_cli read-uid
./bin/mayaRFID_cli read-ndef
./bin/mayaRFID --mode headless --loop-count 1
./scripts/pi_run_gui.sh
```

Expected hardware results with a wristband on the reader:

```text
uid: 09:44:00:04:08:58:62
url: https://www.baidu.com/
verification: verified
```

The addon example should print the UID, URL, write strategy, and verification level. A richer TrussC dashboard can be layered on top of the same `tcx::nfc::ActivationRuntime` path later.
