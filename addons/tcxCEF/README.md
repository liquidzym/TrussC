# tcxCEF

`tcxCEF` is an independent TrussC addon that owns Chromium Embedded Framework setup and browser bridge utilities.

It provides:

- `tcxCEF::Browser`
- `tcxCEF::LocalAssetServer`
- `tcxCEF::WebSocketBridge`
- `tcxcef_copy_runtime_files(<target>)`

`tcxCEF` downloads CEF binary distributions and builds only `libcef_dll_wrapper`. It does not build Chromium or CEF from source.

## Setup

Run from the TrussC repo root:

```bash
python addons/tcxCEF/tools/setup_cef.py --config Release
python addons/tcxCEF/tools/verify_cef.py
```

On Windows, `setup_cef.py` adds `/utf-8` to the MSVC `CL` environment while
building `libcef_dll_wrapper`. This prevents CEF UTF-8 headers from triggering
warning C4819 under non-UTF-8 system code pages; the CEF wrapper project treats
warnings as errors.

The setup script writes:

```txt
addons/tcxCEF/libs/cef/current/cef_paths.cmake
addons/tcxCEF/libs/cef/current/build_manifest.json
```

These generated files are intentionally local build artifacts.

## Cross-platform CEF

The setup script detects and supports:

- macOS x64 / arm64
- Windows x64
- Linux x64 / arm64

Override detection when needed:

```bash
TCXCEF_PLATFORM=linux64 python addons/tcxCEF/tools/setup_cef.py --config Release
TCXCEF_CEF_VERSION=148.0.10+g7ee53f5+chromium-148.0.7778.218 python addons/tcxCEF/tools/setup_cef.py
```

## CMake Runtime Copy

App CMake files should call `tcxcef_copy_runtime_files()` after `trussc_app()`:

```cmake
trussc_app()
tcxcef_copy_runtime_files(my-target)
```

Do not duplicate CEF runtime copy logic in dependent addons.

On macOS, `tcxcef_copy_runtime_files()` also ad-hoc signs the app bundle by default
after copying the CEF framework, helpers, and resources. This is required for local
privacy services such as camera capture to get a stable TCC identity. Disable it only
when another signing step runs later:

```bash
cmake -DTCXCEF_ADHOC_SIGN_APP_BUNDLES=OFF ...
```

## Hidden Window Capture

`BrowserSettings::keepRunningWhenHidden` is an opt-in flag for camera or sensor
bridges that must keep producing data while the native CEF window is minimized or
occluded. The default remains `false` so existing addons keep Chromium's normal
background throttling behavior.

When enabled before CEF initialization, `tcxCEF` appends Chromium switches that
disable background timer throttling and renderer backgrounding. Use this only for
runtime capture windows, not for ordinary embedded pages that should save power
when hidden.
