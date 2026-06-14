# tcxStableDiffusion Issues

## Open Issues

### P1 - Direct in-process Windows CUDA generation hangs after VAE decode

In the TrussC/D3D example process, direct `generate_image()` reached:

```text
decode_first_stage completed
```

and then did not return a final `sd_image_t*`. The same model path completed through upstream `sd-cli` and through the new managed `sd-server` backend. Windows CUDA `Auto` now prefers the pure C++ persistent `sd-server.exe` backend when available, then falls back to `sd-cli`. `ExecutionMode::InProcess` remains available only for controlled experiments and future upstream/debug work.

Current investigation details live in `docs/DIRECT_INPROCESS_DEBUG.md`. The direct path now emits progress breadcrumbs before and after the upstream `generate_image()` call so isolated diagnostics can prove whether the function returns.

### P1 - Upstream progress callback is global

`stable-diffusion.cpp` exposes global progress callbacks. The current addon serializes generation through one worker/runtime instance, which is correct for the first version. If we support multiple simultaneous in-process runtimes later, callback routing must be redesigned carefully.

### P1 - Cancellation differs by execution backend

The in-process upstream progress callback does not return a cancellation decision. `cancel()` marks the job cancelled before/after native generation, but cannot reliably interrupt a running diffusion pass mid-step. The Windows CLI process backend can terminate the child process, but that is a coarse process-level cancellation.

### P1 - Persistent server cancellation is best-effort while generating

The persistent server backend can cancel queued jobs through `/sdcpp/v1/jobs/{id}/cancel`, but upstream currently reports active generation as non-interruptible. The addon returns a cancelled result when the user asks to stop, but the server may keep the active native job busy until upstream completes it.

### P2 - Direct InProcess image-input APIs remain backend-dependent

`PersistentServer` wires `imageToImage`, `mask`, `control`, and per-request `lora` through upstream's `/sdcpp/v1/img_gen` API. `CliProcess` now maps init image, mask, ControlNet image, strength, and control strength to upstream `sd-cli` flags. Direct `InProcess` still returns structured `BACKEND_UNSUPPORTED` errors for image inputs and per-request LoRA.

### P2 - Ideogram4 prompt quality needs calibration

The first `IdeogramPrompt` composer pass is wired into the API and example, and the composed smoke path generates an image. The low-step 512 smoke output is valid but does not yet prove final text fidelity or visual quality. Continue tuning:

- default JSON structure and element wording,
- text-preservation wording,
- recommended step/count/CFG presets,
- and example prompt profiles for poster, product, typography, and logo workflows.

## Resolved Issues

### Resolved - ImGui Chinese labels rendered as question marks

The main Windows title bar rendered Chinese correctly, but the ImGui panel showed Chinese labels as `????`. Root cause: Dear ImGui was using its default font, which does not include CJK glyphs. This was not a source-file encoding problem.

`examples/ideogram4-basic` now loads a CJK-capable ImGui font at startup, before the first frame. Windows candidates include Microsoft YaHei, Noto Sans SC, SimHei, SimSun, and DengXian; macOS candidates include PingFang, Heiti, Hiragino Sans GB, and Songti.

The workbench UI was also modernized with a wider panel, full-width controls, labels above fields, and a neutral dark theme with blue/teal accents. Visual verification screenshot:

```text
examples/ideogram4-basic/outputs/ui_cjk_check.png
```

### Resolved - Ideogram4 model assets are present

The first example now has these files in `examples/ideogram4-basic/bin/data/models/ideogram4-q4_0` and `python tools/verify_sd.py` confirms them:

- `ideogram4-Q4_0.gguf`
- `ideogram4_uncond-Q4_0.gguf`
- `Qwen3VL-8B-Instruct-Q4_K_M.gguf`
- `flux2_ae.safetensors`

### Resolved - Windows CUDA native runtime build

`tools/setup_sd.py build-native --profile windows-cuda` completed in this workspace. The install exists at `libs/stable-diffusion/current` and `tools/verify_sd.py` confirms the generated CMake paths, header, native library, `stable-diffusion.dll`, `sd-cli.exe`, and `sd-server.exe`.

The build manifest confirms the required accelerator policy:

- `SD_CUDA=ON`
- `SD_METAL=OFF`
- `SD_VULKAN=OFF`
- `SD_OPENCL=OFF`
- `SD_HIPBLAS=OFF`
- `SD_SYCL=OFF`
- `SD_MUSA=OFF`

The first example has been rebuilt through `trusscli build` and verified through smoke mode.

### Resolved - Pure C++ persistent server backend

Windows CUDA `Auto` now prefers a managed `sd-server.exe` backend. The C++ addon launches the C++ server process, waits for `/sdcpp/v1/capabilities`, submits `/sdcpp/v1/img_gen` jobs through WinHTTP, polls status, decodes returned base64 PNG data with WinCrypt, writes the native PNG, and returns `tc::Pixels` to the caller. Normal TrussC apps do not need Python at runtime.

The latest C++ smoke run completed with `execution_mode=persistent_server`. Its server log reached:

```text
decode_first_stage completed
generate_image completed
```

which confirms the managed process path avoids the direct TrussC/D3D in-process post-decode hang.

### Resolved - Async setup and persistent loading

Added `StableDiffusion::setupAsync(...)`, `setupIdeogram4Async(...)`, `setupFlux2KleinAsync(...)`, `setupZImageTurboAsync(...)`, and `isSettingUp()`. The first example now initializes models asynchronously and submits smoke jobs only after `isReady()` becomes true.

### Resolved - Persistent image-input and LoRA request wiring

Added designer-friendly request helpers:

```cpp
sd.createImage("Redesign this sketch as a poster")
    .imageToImage("inputs/sketch.png", 0.55f)
    .mask("inputs/mask.png")
    .control("inputs/control.png", 0.8f)
    .lora("loras/style.safetensors", 0.7f)
    .run();
```

These fields are wired through the persistent server backend first.

### Resolved - Shared priority model assets and jobs

FLUX.2-klein and Z-Image Turbo starter assets were downloaded successfully without hitting the 3-failure manual-download rule:

```text
examples/ideogram4-basic/bin/data/models/flux2-klein-4b-q4_0
examples/ideogram4-basic/bin/data/models/z-image-turbo-q3_k
```

Both starter JSON jobs validate and complete through `runtime.execution_mode = persistent_server`:

```text
examples/flux2-klein-basic/jobs/flux2_klein_product_job.json
examples/z-image-basic/jobs/z_image_turbo_wide_job.json
```

The current outputs are smoke/architecture checks, not curated quality presets.

### Resolved - Multi-model GUI example

The main `examples/ideogram4-basic` GUI is now a multi-model workbench. It selects between Ideogram4, FLUX.2-klein, and Z-Image Turbo, loading each from:

```text
examples/ideogram4-basic/bin/data/models/<model-id>
```

Each profile has model-specific defaults for prompt, negative prompt, size, steps, CFG, seed, and output folder.

### Resolved - Initial Node-facing package

Added `node/` with an ESM package and CLI:

```text
node/src/index.mjs
node/bin/tcxsd-node.mjs
node/test/model-paths.test.mjs
```

### Resolved - Node package production hardening

The Node package now includes:

- per-model quality and runtime profiles,
- TypeScript declarations,
- structured `TcxSdError` errors with remediation hints,
- sidecar parity for success and failure paths,
- `/cancel` support,
- reusable `TcxSdServerSession`,
- explicit storage roots and cleanup helpers,
- prompt packs and quality checks,
- Chinese prompt round-trip coverage.

It uses the shared data-model layout and direct `sd-server.exe` HTTP calls. It does not require Python.

### Resolved - TrussC release build command for Ninja single-config presets

Root cause: the example uses the single-config `Ninja` CMake generator. Passing `--config Release` to `cmake --build --preset windows` is meaningful for multi-config generators, but does not switch `CMAKE_BUILD_TYPE` for Ninja after the configure step has already written a Debug cache.

`G:/TrussC/tools/src/main.cpp` now detects `trusscli build --release` on single-config presets, reads the current cache build type, and runs:

```powershell
cmake --preset windows -DCMAKE_BUILD_TYPE=Release
```

before the build when the cache is not already Release.

Verified by intentionally switching the example cache back to Debug, then running `G:/TrussC/tools/bin/trusscli.exe build --release`. The command printed:

```text
[configure] Switching single-config preset 'windows' to CMAKE_BUILD_TYPE=Release (was Debug)
```

The resulting cache reports `CMAKE_BUILD_TYPE:STRING=Release`, `compile_commands.json` includes `/O2 /Ob2 /DNDEBUG -MD`, and the Release smoke run produced `examples/ideogram4-basic/outputs/ideogram4_job_1.png`.

### Resolved - First Ideogram4 prompt composer pass

Added `tcx::sd::IdeogramPrompt` and `tcx::IdeogramPrompt` as a designer-friendly builder for Ideogram4 JSON prompts. It supports high-level prompt kinds, exact visible text, style/composition/background/lighting/medium/mood descriptions, palettes, and typed elements.

High-level usage is now:

```cpp
auto prompt = tcx::IdeogramPrompt::poster("A clean product poster for tcxStableDiffusion")
    .text("tcxStableDiffusion")
    .styleDescription("premium technical product poster, refined typography")
    .palette({"#F7F4EC", "#111111", "#2F80ED"});

sd.createImage(prompt).size(1024, 1024).steps(12).run();
```

The first example GUI now exposes Chinese fields for the template subject, visible text, style, and palette. `TCXSD_SMOKE_COMPOSE=1` verifies the composed prompt path.

### Resolved - Result sidecar metadata JSON

Added `ImageResult::saveMetadata(...)` and `ImageResult::saveWithMetadata(...)`. Successful example runs now write:

```text
examples/ideogram4-basic/outputs/ideogram4_job_1.png
examples/ideogram4-basic/outputs/ideogram4_job_1.json
```

Failed jobs can write `_failed.json` sidecars. The sidecar contains job state, error text, duration, image dimensions, saved image path, native output path, prompt/request fields, runtime/backend settings, model paths, `sd-cli` log paths for CLI mode, and `sd-server` URL/log metadata for persistent mode. This gives future batch, Node, IPC, and debugging tools a stable artifact to read without scraping UI text or temp logs.

`tools/tcxsd_sidecar.py` now validates and summarizes these sidecars from scripts:

```powershell
python tools\tcxsd_sidecar.py validate examples\ideogram4-basic\outputs\ideogram4_job_1.json --require-success-image
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\ideogram4_job_1.json --json
```

### Resolved - JSON job runner for script and Node-adjacent workflows

Added `tools/tcxsd_job.py`, `tools/tcxsd_server.py`, and tracked example jobs:

```text
examples/ideogram4-basic/jobs/ideogram4_poster_job.json
examples/flux2-klein-basic/jobs/flux2_klein_product_job.json
examples/z-image-basic/jobs/z_image_turbo_wide_job.json
```

The runner supports:

- `validate` for job schema/path checks,
- `args` for inspecting the exact `sd-cli` command,
- `run` for producing PNG, log, and JSON sidecar artifacts,
- `runtime.execution_mode = persistent_server` for the same server-backed flow used by pure C++.

Relative paths are resolved from the job file location, which keeps Node, PowerShell, CI, and manual calls consistent. This remains a stable script contract alongside the first formal Node package in `node/`.
