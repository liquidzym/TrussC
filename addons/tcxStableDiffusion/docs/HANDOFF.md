# tcxStableDiffusion Handoff

## Current State

This addon is in the first implementation pass, with the core structure, scripts, CMake integration, C++ API, worker queue, Windows CUDA native build, pure C++ persistent `sd-server` backend, process-isolated CLI fallback, first Ideogram4 prompt composer, result sidecar JSON, and first Chinese ImGui example in place. Ideogram4 starter models are present and real smoke generations completed through the TrussC example.

## Important Paths

- Addon root: `G:/TrussC/addons/tcxStableDiffusion`
- Native source: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/source`
- Native install: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current`
- Native CLI: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current/bin/sd-cli.exe`
- Native server: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current/bin/sd-server.exe`
- First example: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic`
- FLUX.2-klein starter: `G:/TrussC/addons/tcxStableDiffusion/examples/flux2-klein-basic`
- Z-Image starter: `G:/TrussC/addons/tcxStableDiffusion/examples/z-image-basic`
- Shared example models: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/data/models/<model-id>`
- Node package: `G:/TrussC/addons/tcxStableDiffusion/node`
- First smoke output: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/outputs/ideogram4_job_1.png`
- First smoke sidecar: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/outputs/ideogram4_job_1.json`
- Docs:
  - `docs/ISSUES.md`
  - `docs/PROGRESS.md`
  - `docs/HANDOFF.md`
  - `docs/API_USAGE.md`

## Key Commands

List model profiles:

```powershell
python tools\setup_sd.py list-models
```

Build native runtime for Windows CUDA:

```powershell
python tools\setup_sd.py build-native --profile windows-cuda
```

Download first example model assets:

```powershell
python tools\setup_sd.py download-model --model ideogram4-q4_0
```

Verify local setup:

```powershell
python tools\verify_sd.py
```

Validate or summarize a generation sidecar:

```powershell
python tools\tcxsd_sidecar.py validate examples\ideogram4-basic\outputs\ideogram4_job_1.json --require-success-image
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\ideogram4_job_1.json --json
```

Run a JSON job file through the script/Node-adjacent runner:

```powershell
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\flux2-klein-basic\jobs\flux2_klein_product_job.json
python tools\tcxsd_job.py run examples\z-image-basic\jobs\z_image_turbo_wide_job.json
```

Run the pure Node package tests:

```powershell
cd node
npm test
```

Run tests:

```powershell
python -m unittest discover -s tests
```

Build the first example:

```powershell
cd examples\ideogram4-basic
G:\TrussC\tools\bin\trusscli.exe build
```

Build the first example as Release:

```powershell
cd examples\ideogram4-basic
G:\TrussC\tools\bin\trusscli.exe build --release
```

For Ninja/single-config presets, current `trusscli` reconfigures the preset with `-DCMAKE_BUILD_TYPE=Release` before building when the cache is still Debug.

Run smoke mode:

```powershell
$env:TCXSD_SMOKE='1'
$env:TCXSD_SMOKE_PROMPT='A compact clean product poster for tcxStableDiffusion, sharp typography, elegant studio lighting'
$env:TCXSD_SMOKE_WIDTH='512'
$env:TCXSD_SMOKE_HEIGHT='512'
$env:TCXSD_SMOKE_STEPS='4'
$env:TCXSD_SMOKE_SEED='45'
$env:TCXSD_SMOKE_LOW_VRAM='1'
.\bin\ideogram4-basic.exe
```

## Upstream Facts Used

- `stable-diffusion.cpp` currently supports separate runtime and parameter backend assignments such as `cuda0`, `metal`, and `cpu`.
- Windows profile must enable only CUDA among accelerator backends:
  - `SD_CUDA=ON`
  - `SD_METAL=OFF`
  - `SD_VULKAN=OFF`
  - `SD_OPENCL=OFF`
  - `SD_HIPBLAS=OFF`
  - `SD_SYCL=OFF`
  - `SD_MUSA=OFF`
- Upstream `generate_image()` returns heap-allocated `sd_image_t*`; each image `data` and the image array must be freed with `free()`.
- Upstream progress callback is global and non-cancellable.
- BFL's FLUX.2 VAE URL is gated without a Hugging Face token in this environment; the default registry uses the public `nvidia/PiD` mirror `checkpoints/flux2_ae.safetensors`.
- Upstream `sd-cli` and managed `sd-server` completed Ideogram4 generation while direct in-process `generate_image()` in the TrussC/D3D process hung after `decode_first_stage completed`.
- `trusscli build --release` must set `CMAKE_BUILD_TYPE=Release` during configure for Ninja/single-config presets; this is now handled in `G:/TrussC/tools/src/main.cpp`.

## Architecture Notes

- `tcx::sd::NativeRuntime` is the only layer that includes `stable-diffusion.h`.
- `tcx::sd::StableDiffusion` owns the worker queue and exposes designer-friendly calls.
- `tcx::sd::IdeogramPrompt` builds upstream-style Ideogram4 JSON prompts while still returning a plain `std::string` for backend compatibility.
- `ImageResult::saveWithMetadata(...)` writes a PNG plus JSON sidecar. `saveMetadata(...)` can also write failure sidecars.
- `tools/tcxsd_sidecar.py` is the first script/Node-adjacent reader for sidecar validation and summaries.
- `tools/tcxsd_job.py` is the first script/Node-adjacent generation entry. It reads JSON, resolves paths relative to the job file, calls either `sd-cli` or persistent `sd-server`, and writes PNG/JSON/log outputs.
- `tools/tcxsd_server.py` provides optional script-side helpers for `/sdcpp/v1/img_gen`; it is not required by normal C++ runtime use.
- `node/` is the first formal Node-facing package. It starts `sd-server.exe` directly and does not call Python.
- `RuntimeSettings::executionMode` controls `Auto`, `PersistentServer`, `CliProcess`, and `InProcess`.
- Windows CUDA `Auto` prefers the bundled `sd-server.exe` persistent backend when present, then falls back to `sd-cli.exe`.
- Worker threads only produce CPU `Pixels`; examples upload to GPU textures on the main thread.
- `ModelPaths` already reserves fields for video/audio/controlnet/photo maker additions.
- `ImageRequest` exposes chain helpers for img2img, inpainting masks, ControlNet images, LoRA stacks, and metadata. `PersistentServer` wires those image/LoRA fields first.
- `examples/ideogram4-basic` is now the main multi-model GUI workbench despite the historical folder name.
- The workbench loads a CJK-capable ImGui font at startup. If Chinese labels ever regress to `????`, check the CJK font candidate list in `examples/ideogram4-basic/src/tcApp.cpp` before changing source encodings.

## Next Best Step

Tune final prompt quality for each model and harden the Node package sidecar/cancellation story. The three priority model assets are already centralized under the main example data folder and smoke-verified. For GUI polish, the next useful step is a small responsive pass for very short window heights, but the current 1280x900 workbench layout is visually verified.

## Latest Verified State

- Native build: `windows-cuda @ 19bdfe22d255d5b4dff39d449318b9bc5ea2317f`
- Native install: `libs/stable-diffusion/current`, including `sd-cli.exe` and `sd-server.exe`
- Model check: `python tools/verify_sd.py`
- Example build: `examples/ideogram4-basic` builds with `G:/TrussC/tools/bin/trusscli.exe build`
- Release build: `G:/TrussC/tools/bin/trusscli.exe build --release`, verified from a Debug cache to a Release cache
- Smoke run: `TCXSD_SMOKE=1 ... ./bin/ideogram4-basic.exe`, exit code 0
- Composed smoke run: `TCXSD_SMOKE=1 TCXSD_SMOKE_COMPOSE=1 ... ./bin/ideogram4-basic.exe`, exit code 0
- Smoke output: `examples/ideogram4-basic/outputs/ideogram4_job_1.png`, latest composed output 112127 bytes
- Smoke sidecar: `examples/ideogram4-basic/outputs/ideogram4_job_1.json`, parsed successfully with `ok=true`, `execution_mode=persistent_server`, `backend=cuda`, `seed=9011`, and `duration_seconds=6.030`
- Sidecar tool: `python tools/tcxsd_sidecar.py validate ... --require-success-image`, exit code 0
- JSON job runner: `python tools/tcxsd_job.py run examples/ideogram4-basic/jobs/ideogram4_poster_job.json`, exit code 0
  - Output: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.png`
  - Sidecar: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.json`
  - Sidecar summary: `ok=true`, `execution_mode=cli_process`, `backend=cuda0`, `duration_seconds=8.73`
- FLUX.2-klein model check: `python tools/verify_sd.py --model flux2-klein-4b-q4_0`, exit code 0
- FLUX.2-klein JSON job: `python tools/tcxsd_job.py run examples/flux2-klein-basic/jobs/flux2_klein_product_job.json`, exit code 0
  - Sidecar summary: `ok=true`, `execution_mode=persistent_server`, `backend=cuda0`, `image_width=512`, `image_height=512`, `duration_seconds=2.529`
- Z-Image model check: `python tools/verify_sd.py --model z-image-turbo-q3_k`, exit code 0
- Z-Image JSON job: `python tools/tcxsd_job.py run examples/z-image-basic/jobs/z_image_turbo_wide_job.json`, exit code 0
  - Sidecar summary: `ok=true`, `execution_mode=persistent_server`, `backend=cuda0`, `image_width=1024`, `image_height=512`, `duration_seconds=4.040`
- Node package tests: `cd node; npm test`, 4 tests passing
- Node package smoke: `node .\bin\tcxsd-node.mjs --job ..\examples\flux2-klein-basic\jobs\flux2_klein_product_job.json`, exit code 0
- GUI Chinese/theme fix: `examples/ideogram4-basic` loads Microsoft YaHei/Noto/PingFang-style CJK fonts into ImGui, uses a modern dark blue/teal theme, and places Chinese labels above full-width controls.
- GUI visual check: `examples/ideogram4-basic/outputs/ui_cjk_check.png`, confirmed Chinese labels render normally instead of `????`.
- GUI release rebuild: `G:/TrussC/tools/bin/trusscli.exe build --release`, exit code 0.
- GUI short smoke: `TCXSD_SMOKE=1 TCXSD_SMOKE_MODEL=flux2-klein TCXSD_SMOKE_WIDTH=512 TCXSD_SMOKE_HEIGHT=512 TCXSD_SMOKE_STEPS=1 TCXSD_SMOKE_SEED=777 ./bin/ideogram4-basic.exe`, exit code 0.
- Python compile: `python -m py_compile tools/setup_sd.py tools/tcxsd_models.py tools/verify_sd.py tools/tcxsd_sidecar.py tools/tcxsd_job.py tools/tcxsd_server.py`
- Tests: `python -m unittest discover -s tests`, 20 tests passing

## Known Caveat

Direct in-process Windows CUDA generation still hangs after `decode_first_stage completed` in the TrussC/D3D process. Windows CUDA `Auto` should keep using the managed `sd-server` backend until that upstream/direct-API interaction is understood. Persistent server cancellation is best-effort for already-generating jobs because upstream currently does not interrupt active generation through the HTTP cancel endpoint. Prompt composer output is functional but still needs quality tuning before it is treated as a polished default.
