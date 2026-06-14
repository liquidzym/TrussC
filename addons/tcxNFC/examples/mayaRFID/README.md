# mayaRFID

This example is the `mayaRFID` app flow rebuilt on top of the new `tcxNFC` addon. It keeps the practical Raspberry Pi deployment assets from the reference project, while NFC/RFID protocol work now goes through:

- `tcx::nfc::NdefUriBuilder`
- `tcx::nfc::TcpSocketTransport`
- `tcx::nfc::Bks710iReader`
- `tcx::nfc::ActivationRuntime`
- `tcx::nfc::PluginHost`

## Build

```bash
cmake --preset macos
cmake --build --preset macos
```

The binaries are written to `bin/`:

```bash
./bin/mayaRFID_cli config-check
./bin/mayaRFID_cli storage-check
./bin/mayaRFID_cli build-ndef --url https://www.baidu.com/
./bin/mayaRFID_cli mock-once
./bin/mayaRFID --mode mock --once
```

The GUI is built as its own TrussC target instead of a `--mode gui` branch inside
the headless executable:

```bash
./bin/mayaRFID_gui.app/Contents/MacOS/mayaRFID_gui   # macOS
./bin/mayaRFID_gui                                   # Linux / Raspberry Pi
```

The example source is split by responsibility:

- `src/common/` handles config, option parsing, and shared formatting.
- `src/runtime/` owns activation, mock reader flow, and JSONL plugin dispatch.
- `src/cli/` contains diagnostics and hardware command handlers.
- `src/gui/` contains the TrussC window app and calls the same runtime API.

## Hardware Commands

With a BKS-710I reader at `192.168.1.100:502`:

```bash
./bin/mayaRFID_cli reader-ping
./bin/mayaRFID_cli read-uid
./bin/mayaRFID_cli write-url --url https://www.baidu.com/
./bin/mayaRFID_cli read-ndef --start-page 4 --end-page 39
./bin/mayaRFID --mode headless --once
```

If Wi-Fi shares the `192.168.1.0/24` subnet, set `rfid.reader_source_host` in `bin/data/config/pi01.example.yaml` to the local Ethernet IP, for example `192.168.1.10`.

## Raspberry Pi

The reference Pi scripts are included under `scripts/` and adapted for this example:

```bash
./scripts/make_pi_source_bundle.sh
./scripts/pi_install_deps.sh
./scripts/pi_prepare_runtime.sh
./scripts/pi_run_gui.sh
```

Known Pi setup issues and fixes are copied into `docs/RASPBERRY_PI_DEPLOYMENT.md`.
