# tcxCEF and tcxMediaPipe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two independent TrussC addons: `tcxCEF` for cross-platform CEF runtime management and browser bridging, and `tcxMediaPipe` for offline MediaPipe Web Tasks Vision inference through `tcxCEF`.

**Architecture:** `tcxCEF` owns all CEF binary download, wrapper build, runtime file discovery, local HTTP serving, WebSocket bridge, and browser lifecycle APIs. `tcxMediaPipe` depends on `tcxCEF`, serves its local Vite bundle through `tcxCEF::LocalAssetServer`, receives MediaPipe results over `tcxCEF::WebSocketBridge`, and parses JSON into TrussC-side result structs. `tcxMediaPipe` never includes CEF headers, links libcef, or copies CEF runtime files directly.

**Tech Stack:** C++20, TrussC addon CMake, CEF binary distributions, Python 3 deployment scripts, TypeScript/Vite, `@mediapipe/tasks-vision`, Web Workers, nlohmann/json.

---

### Task 1: Repository and addon tracking

**Files:**
- Modify: `/Users/mac/Desktop/TrussC/.gitignore`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/.gitignore`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/.gitignore`

- [ ] Whitelist `addons/tcxCEF` and `addons/tcxMediaPipe` in the root ignore rules.
- [ ] Keep downloaded CEF binaries, CEF build trees, MediaPipe `node_modules`, and process task docs out of tracked output.
- [ ] Confirm `git check-ignore -v` no longer ignores source files under both addons.

### Task 2: `tcxCEF` addon foundation

**Files:**
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/addon.json`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/CMakeLists.txt`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/README.md`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxCEF.h`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxCEF.cpp`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/Browser.h`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/Browser.cpp`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/LocalAssetServer.h`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/LocalAssetServer.cpp`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/WebSocketBridge.h`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/WebSocketBridge.cpp`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/RuntimePaths.h`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/src/tcxcef/RuntimePaths.cpp`

- [ ] Add a static `tcxCEF` target with public include path and `tc::tcxCEF` alias.
- [ ] Add a CEF-present path that includes generated `libs/cef/current/cef_paths.cmake`.
- [ ] Add a CEF-missing path that builds clear stubs instead of silently using CEF.
- [ ] Export only `tcxcef` public C++ APIs; keep raw CEF headers private.

### Task 3: Cross-platform CEF setup

**Files:**
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/tools/setup_cef.py`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/tools/verify_cef.py`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxCEF/tools/tcxcef_paths_template.cmake`

- [ ] Detect host platform as `macosx64`, `macosarm64`, `windows64`, `linux64`, or `linuxarm64`.
- [ ] Download a CEF binary distribution from `https://cef-builds.spotifycdn.com/`.
- [ ] Extract the distribution under `libs/cef/<platform>/<version>/source`.
- [ ] Configure and build `libcef_dll_wrapper` in `libs/cef/<platform>/<version>/wrapper-build-<config>`.
- [ ] Generate `libs/cef/current/cef_paths.cmake` and `build_manifest.json`.
- [ ] Verify required include, library, wrapper, resource, and runtime paths.

### Task 4: `tcxMediaPipe` addon foundation and parser tests

**Files:**
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/addon.json`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/CMakeLists.txt`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/README.md`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/src/*.h`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/src/*.cpp`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/devtests/parser_test.cpp`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/devtests/CMakeLists.txt`

- [ ] Write parser tests for hand, pose, face, malformed JSON, and runtime status.
- [ ] Run parser tests once to confirm missing implementation fails.
- [ ] Implement result structs, settings, result parser, and runtime shell.
- [ ] Re-run parser tests until green.

### Task 5: MediaPipe offline web app and tooling

**Files:**
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/web/*`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/web/src/**/*.ts`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/tools/fetch_mediapipe_assets.py`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/tools/verify_mediapipe_assets.py`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/tools/build_web_assets.py`

- [ ] Pin `@mediapipe/tasks-vision` to `0.10.35` with environment override support.
- [ ] Copy the full `node_modules/@mediapipe/tasks-vision/wasm` directory into `web/wasm`.
- [ ] Download hand, pose full, and face `.task` model bundles into `web/models`.
- [ ] Generate and verify `web/models/manifest.json` with sha256 and byte counts.
- [ ] Use local `/wasm` and `/models/*` paths in the web app; do not use CDN URLs.
- [ ] Build each task through a worker with GPU-first, CPU fallback status reporting.

### Task 6: Examples and validation

**Files:**
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/example-hand/*`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/example-pose/*`
- Create: `/Users/mac/Desktop/TrussC/addons/tcxMediaPipe/example-face/*`

- [ ] Create examples with `addons.make` listing `tcxCEF`, `tcxMediaPipe`, and `tcxWebSocket`.
- [ ] Use `tcxcef_copy_runtime_files(<target>)` from each example CMake path.
- [ ] Run `python addons/tcxCEF/tools/verify_cef.py` and record whether CEF is setup.
- [ ] Run parser tests and Python script syntax checks.
- [ ] Build examples only when CEF and MediaPipe assets are present; otherwise report the exact missing setup commands.
