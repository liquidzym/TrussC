# TrussC on Headless Linux (Console Mode)

Guide for running TrussC on Linux systems without a desktop environment — Raspberry Pi Lite, Orange Pi, Arch ARM, and other single-board computers (SBCs) or headless servers.

## Overview

TrussC apps fall into two categories on headless systems:

| App Type | Display Server Needed? | Example |
|---|---|---|
| **Headless** (no window) | No | `noWindowExample`, MCP servers, network tools |
| **GUI** (window + graphics) | Yes | Graphics examples, interactive installations |

Headless apps work out of the box. GUI apps need a display session — see [Section 3](#3-running-gui-apps-with---session) below.

---

## 1. Build Dependencies

Install the minimum build dependencies using the included script:

```bash
cd TrussC
chmod +x tools/install_dependencies_linux.sh
./tools/install_dependencies_linux.sh
```

This installs compilers, CMake, and the development libraries needed to build TrussC. It does **not** install any display server packages — those are only needed if you want to run GUI apps.

---

## 2. Building and Running Headless Apps

Headless apps (no window, no graphics) work without any display server:

```bash
# Build trusscli
cd tools
./build_linux.sh

# Build a headless example
trusscli build -p examples/windowing/noWindowModeExample

# Run directly — no display needed
./examples/windowing/noWindowModeExample/bin/noWindowModeExample
```

Use cases:
- **MCP server mode** (`TRUSSC_MCP=1`) — AI-driven automation without a screen
- **Network tools** — OSC receivers, TCP/UDP servers
- **Batch processing** — image/video processing, audio generation

---

## 3. Running GUI Apps with `--session`

To run GUI apps on a headless system, `trusscli run --session <backend>` launches a minimal display session just for your app — no full desktop environment needed.

### Option A: X11

The simplest option. Works well and avoids Wayland-specific issues (e.g., surface throttling when backgrounded).

**Install X11:**
```bash
# Debian / Raspbian / Ubuntu
sudo apt install xserver-xorg xinit

# Arch
sudo pacman -S xorg-server xorg-xinit
```

**Run:**
```bash
trusscli run --session x11
```

### Option B: labwc (Wayland)

A lightweight Wayland compositor. Useful for installations that need Wayland-specific features.

**Install labwc:**
```bash
# Debian / Raspbian
sudo apt install labwc

# Arch
sudo pacman -S labwc
```

**Permissions** — your user needs access to input and video devices:
```bash
sudo usermod -aG video,input,render $USER
# Log out and back in for changes to take effect
```

You may also need polkit:
```bash
sudo apt install policykit-1    # Debian / Raspbian
sudo pacman -S polkit            # Arch
```

**Run:**
```bash
trusscli run --session labwc
```

### Which to choose?

| | X11 | labwc (Wayland) |
|---|---|---|
| **Setup** | Simple | Requires group permissions + polkit |
| **Compatibility** | Mature, widely tested | Newer, some edge cases |
| **AV sync** | Stable | May throttle backgrounded surfaces ([#29](https://github.com/TrussC-org/TrussC/issues/29)) |
| **Recommended for** | Most use cases | Wayland-specific projects |

**Recommendation: start with X11.** Switch to labwc if you need Wayland features.

---

## 4. Auto-start at Boot

For installations and kiosk setups, you can auto-start your app on boot using a systemd service:

```bash
sudo tee /etc/systemd/system/trussc-app.service << 'EOF'
[Unit]
Description=TrussC App
After=network.target

[Service]
Type=simple
User=your_username
WorkingDirectory=/path/to/your/project
ExecStart=/usr/local/bin/trusscli run --session x11
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl enable trussc-app
sudo systemctl start trussc-app
```

Replace `/path/to/your/project` with your project directory and `your_username` with your user.

---

## 5. SBC-Specific Tips

### Raspberry Pi (Raspbian Lite)

- **Recommended OS**: Raspberry Pi OS Lite (64-bit)
- **GPU**: VideoCore (Pi 4) or VideoCore VII (Pi 5) — TrussC uses OpenGL ES 3.0
- **HW video decode**: VAAPI / V4L2M2M / DRM supported (auto-detected by TrussC)
- **Boot to CLI**: `sudo raspi-config` → System Options → Boot / Auto Login → Console
- **Remote access**: SSH for development, physical display for final output

### Orange Pi / Other Allwinner SBCs

- Install ARM-compatible GPU drivers (Mali, PowerVR)
- `install_dependencies_linux.sh` should cover build deps on Debian-based distros
- Use `--session x11` for display; Wayland support varies by GPU driver

### Arch Linux ARM

- Use `pacman` for dependencies:
  ```bash
  sudo pacman -S cmake gcc mesa libx11 libxrandr libxinerama libxcursor libxi gtk3
  ```
- X11 is generally more stable than Wayland on ARM Arch
- AUR may have additional GPU-specific packages

---

## 6. Troubleshooting

### `trusscli run --session x11` fails with "Server is already active"

An X server is already running (desktop session or previous crashed session):
```bash
# Check for existing X processes
ps aux | grep -i xorg

# Remove stale lock if X is not actually running
sudo rm /tmp/.X0-lock
```

### `trusscli run --session labwc` fails with "Could not take control of session"

Seat permission issue:
```bash
# Add yourself to required groups
sudo usermod -aG video,input,render $USER

# Install polkit if not present
sudo apt install policykit-1

# Log out and back in
```

### Build fails with missing OpenGL headers

```bash
# Debian / Raspbian
sudo apt install libgles2-mesa-dev libegl1-mesa-dev

# Arch
sudo pacman -S mesa
```

### App runs but no display output (labwc, no physical monitor)

This is expected — labwc creates a Wayland session without a physical display. The app runs in the background. Connect a monitor to see the output, or use MCP mode (`TRUSSC_MCP=1`) for headless interaction.

---

## See Also

- [GET_STARTED.md](GET_STARTED.md) — General getting started guide
- [BUILD_SYSTEM.md](BUILD_SYSTEM.md) — Build system details and hot reload
- [AI_AUTOMATION.md](AI_AUTOMATION.md) — MCP mode for headless AI interaction
