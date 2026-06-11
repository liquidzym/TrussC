# tcxStableDiffusion Issues

## Open Issues

### P1 - Direct in-process Windows CUDA generation hangs after VAE decode

In the TrussC/D3D example process, direct `generate_image()` reached:

```text
decode_first_stage completed
```

and then did not return a final `sd_image_t*`. The same model path completed through upstream `sd-cli`. Windows CUDA `Auto` now uses the `sd-cli` process backend by default, while `ExecutionMode::InProcess` remains available for controlled experiments and future upstream/debug work.

### P1 - CLI process backend reloads models per job

The Windows default is robust, but each `sd-cli` job is a fresh process and therefore reloads Ideogram4 model assets. This is acceptable for the first stable Windows path, but the performance roadmap should add one of:

- a persistent local worker process,
- a managed `sd-server` backend,
- or a fixed in-process direct API path once the post-decode hang is understood.

### P1 - Upstream progress callback is global

`stable-diffusion.cpp` exposes global progress callbacks. The current addon serializes generation through one worker/runtime instance, which is correct for the first version. If we support multiple simultaneous in-process runtimes later, callback routing must be redesigned carefully.

### P1 - Cancellation differs by execution backend

The in-process upstream progress callback does not return a cancellation decision. `cancel()` marks the job cancelled before/after native generation, but cannot reliably interrupt a running diffusion pass mid-step. The Windows CLI process backend can terminate the child process, but that is a coarse process-level cancellation.

### P1 - Model loading needs async/persistent variants

In-process `setup()` loads the native context synchronously. The CLI process backend moves loading into generation and returns from setup quickly, but pays per-job loading cost. The next pass should add explicit async setup/progress semantics and a persistent backend option.

### P2 - Input image APIs are reserved but not wired

`ImageRequest` already has `initImage`, `maskImage`, and `controlImage` fields, but native image loading into `sd_image_t` is intentionally deferred. The first stable native pass is txt2img only.

### P2 - Node mode is architectural, not implemented

The addon now has clean C++ service boundaries, a process-isolated backend option, model registry tooling, setup manifests, result sidecars, and a JSON job runner. A full Node-facing runtime interface still needs one of:

- TrussC Node wrapper around `StableDiffusion`
- local HTTP/WebSocket service
- command/IPC bridge that calls the same model registry and setup manifest

The current bridge is `tools/tcxsd_job.py`, which can already run a JSON job file and emit PNG/JSON/log artifacts. The remaining Node task is a persistent API shape, not first-use generation.

### P2 - Ideogram4 prompt quality needs calibration

The first `IdeogramPrompt` composer pass is wired into the API and example, and the composed smoke path generates an image. The low-step 512 smoke output is valid but does not yet prove final text fidelity or visual quality. Continue tuning:

- default JSON structure and element wording,
- text-preservation wording,
- recommended step/count/CFG presets,
- and example prompt profiles for poster, product, typography, and logo workflows.

## Resolved Issues

### Resolved - Ideogram4 model assets are present

The first example now has these files in `examples/ideogram4-basic/models` and `python tools/verify_sd.py` confirms them:

- `ideogram4-Q4_0.gguf`
- `ideogram4_uncond-Q4_0.gguf`
- `Qwen3VL-8B-Instruct-Q4_K_M.gguf`
- `flux2_ae.safetensors`

### Resolved - Windows CUDA native runtime build

`tools/setup_sd.py build-native --profile windows-cuda` completed in this workspace. The install exists at `libs/stable-diffusion/current` and `tools/verify_sd.py` confirms the generated CMake paths, header, native library, `stable-diffusion.dll`, and `sd-cli.exe`.

The build manifest confirms the required accelerator policy:

- `SD_CUDA=ON`
- `SD_METAL=OFF`
- `SD_VULKAN=OFF`
- `SD_OPENCL=OFF`
- `SD_HIPBLAS=OFF`
- `SD_SYCL=OFF`
- `SD_MUSA=OFF`

The first example has been rebuilt through `trusscli build` and verified through smoke mode.

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

Failed jobs can write `_failed.json` sidecars. The sidecar contains job state, error text, duration, image dimensions, saved image path, native output path, prompt/request fields, runtime/backend settings, model paths, and `sd-cli` log path. This gives future batch, Node, IPC, and debugging tools a stable artifact to read without scraping UI text or temp logs.

`tools/tcxsd_sidecar.py` now validates and summarizes these sidecars from scripts:

```powershell
python tools\tcxsd_sidecar.py validate examples\ideogram4-basic\outputs\ideogram4_job_1.json --require-success-image
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\ideogram4_job_1.json --json
```

### Resolved - JSON job runner for script and Node-adjacent workflows

Added `tools/tcxsd_job.py` and a tracked example job:

```text
examples/ideogram4-basic/jobs/ideogram4_poster_job.json
```

The runner supports:

- `validate` for job schema/path checks,
- `args` for inspecting the exact `sd-cli` command,
- `run` for producing PNG, log, and JSON sidecar artifacts.

Relative paths are resolved from the job file location, which keeps Node, PowerShell, CI, and manual calls consistent. This is the first stable external generation contract while the persistent worker/IPC design remains open.
