# tcxMediaPipe

`tcxMediaPipe` runs MediaPipe Web Tasks Vision inside TrussC through the independent `tcxCEF` addon.

It does not use Python MediaPipe, p5.js, ml5.js, CDN assets, cloud inference, or runtime model downloads.

## Dependency Boundary

`tcxMediaPipe` depends on `tcxCEF`.

`tcxMediaPipe` never:

- includes raw CEF headers
- links `libcef` directly
- downloads or builds CEF
- copies CEF runtime files directly

CEF setup is owned by:

```bash
python addons/tcxCEF/tools/setup_cef.py --config Release
```

MediaPipe JS/WASM/model setup is owned by:

```bash
python addons/tcxMediaPipe/tools/fetch_mediapipe_assets.py
python addons/tcxMediaPipe/tools/build_web_assets.py
python addons/tcxMediaPipe/tools/verify_mediapipe_assets.py
```

The distributable addon contains the offline runtime directories:

```txt
web/dist/
web/wasm/
web/models/
```

## Offline Runtime

The first asset fetch needs network access. After setup, runtime requests stay local:

```txt
http://127.0.0.1:<assetPort>/
ws://127.0.0.1:<bridgePort>/bridge
```

The web runtime loads:

```txt
/wasm
/models/hand_landmarker.task
/models/pose_landmarker_full.task
/models/face_landmarker.task
/models/gesture_recognizer.task
```

Check for accidental remote runtime URLs:

```bash
rg -n "fetch\\(['\\\"]https?://|importScripts\\(['\\\"]https?://|src=['\\\"]https?://|href=['\\\"]https?://" addons/tcxMediaPipe/web/dist addons/tcxMediaPipe/web/src addons/tcxMediaPipe/web/index.html
```

Disconnect the network after asset setup and run the examples. DevTools Network should not show CDN, npm, Google model storage, or other remote requests.

## Examples

Examples live under `examples/`:

- `examples/example-hand`
- `examples/example-pose`
- `examples/example-face`
- `examples/example-holistic`
- `examples/example-gesture`

Each example uses:

```txt
tcxTls
tcxWebSocket
tcxCEF
tcxMediaPipe
```

Build after both CEF and MediaPipe assets are setup:

```bash
(cd addons/tcxMediaPipe/examples/example-hand && cmake --preset macos)
trusscli build -p addons/tcxMediaPipe/examples/example-hand

(cd addons/tcxMediaPipe/examples/example-pose && cmake --preset macos)
trusscli build -p addons/tcxMediaPipe/examples/example-pose

(cd addons/tcxMediaPipe/examples/example-face && cmake --preset macos)
trusscli build -p addons/tcxMediaPipe/examples/example-face

(cd addons/tcxMediaPipe/examples/example-holistic && cmake --preset macos)
trusscli build -p addons/tcxMediaPipe/examples/example-holistic

(cd addons/tcxMediaPipe/examples/example-gesture && cmake --preset macos)
trusscli build -p addons/tcxMediaPipe/examples/example-gesture
```

Do not run `trusscli update` on these examples unless you re-apply the
`tcxcef_copy_runtime_files(...)` and `tcxmediapipe_copy_web_assets(...)` lines in
the generated `CMakeLists.txt`; those lines are required for CEF, offline web
assets, and macOS signing.

The examples draw hand, pose, face, combined hand+pose+face, and gesture results. They display active delegate, source FPS, inference FPS, average inference time, frame age, and bridge latency.

By default the examples request a 640x480 camera stream for the CEF preview. Hand,
pose, and gesture resize inference frames to 480x360. Face and holistic use
320x240, single-face defaults, and a 24 FPS cap because the face landmarker is the
heaviest path and otherwise tends to build visible lag. Set
`Settings::processingWidth` and `Settings::processingHeight` to `0` to run
inference at the captured camera size.

When `Settings::mirror` is true, the CEF preview and the normalized 2D landmarks
sent back to C++ are mirrored together, so TrussC-side drawing matches the camera
window.

### Multi-target Detection

Multi-person detection is enabled by default. The C++ `Settings` object is the
external configuration surface and is serialized into the web runtime config:

```cpp
tcx::mediapipe::Settings settings;
settings.enableHand = true;
settings.enablePose = true;
settings.enableFace = true;
settings.enableGesture = true;
settings.multiPerson = true;
settings.maxHands = 4;
settings.maxPoses = 2;
settings.maxFaces = 1;
settings.maxGestures = 4;
settings.outputFaceBlendshapes = false;
settings.outputFaceTransformationMatrix = false;
```

The same values can be loaded from an optional JSON file:

```cpp
tcx::mediapipe::Settings settings;
settings.configPath = "addons/tcxMediaPipe/config/default.json";
```

Set `settings.multiPerson = false` to force every task back to one target. Raising
the max counts increases work per frame, especially for pose and face; lower the
counts or `Settings::processingWidth/processingHeight` if FPS drops.

Hand and gesture results are hand-based arrays. Pose and face results are
multi-target arrays. The addon does not assign stable person IDs across hand,
pose, and face tasks; applications that need person-level grouping should add a
separate association layer.

### macOS Camera Permission

The macOS examples are ad-hoc signed during the CMake build after CEF and
tcxMediaPipe resources are copied into the app bundle. This gives macOS TCC a
stable app identity for camera permission.

If a rebuilt example shows a green camera indicator but no video frames, reset the
example permission once, then launch it again and allow Camera access:

```bash
tccutil reset Camera com.trussc.example-hand
tccutil reset Camera com.trussc.example-pose
tccutil reset Camera com.trussc.example-face
tccutil reset Camera com.trussc.example-holistic
tccutil reset Camera com.trussc.example-gesture
```

Do not run multiple examples at once while testing camera input.

## First-version Scope

Implemented first:

- WebCamera input
- HandLandmarker
- PoseLandmarker
- FaceLandmarker
- GestureRecognizer
- GPU delegate with CPU fallback
- local WASM and local `.task` model loading
- WebSocket result bridge
- C++ result parser

Deferred:

- ExternalFrame input
- ObjectDetector
- offscreen CEF texture overlay
- GPU texture zero-copy
