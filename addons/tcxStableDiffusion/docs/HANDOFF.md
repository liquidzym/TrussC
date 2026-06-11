# tcxStableDiffusion Handoff

## Current State

This addon is in the first implementation pass, with the core structure, scripts, CMake integration, C++ API, worker queue, Windows CUDA native build, process-isolated CLI backend, first Ideogram4 prompt composer, result sidecar JSON, and first Chinese ImGui example in place. Ideogram4 starter models are present and real smoke generations completed through the TrussC example.

## Important Paths

- Addon root: `G:/TrussC/addons/tcxStableDiffusion`
- Native source: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/source`
- Native install: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current`
- Native CLI: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current/bin/sd-cli.exe`
- First example: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic`
- First example models: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/models`
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
- Upstream `sd-cli` completed Ideogram4 generation while direct in-process `generate_image()` in the TrussC/D3D process hung after `decode_first_stage completed`.
- `trusscli build --release` must set `CMAKE_BUILD_TYPE=Release` during configure for Ninja/single-config presets; this is now handled in `G:/TrussC/tools/src/main.cpp`.

## Architecture Notes

- `tcx::sd::NativeRuntime` is the only layer that includes `stable-diffusion.h`.
- `tcx::sd::StableDiffusion` owns the worker queue and exposes designer-friendly calls.
- `tcx::sd::IdeogramPrompt` builds upstream-style Ideogram4 JSON prompts while still returning a plain `std::string` for backend compatibility.
- `ImageResult::saveWithMetadata(...)` writes a PNG plus JSON sidecar. `saveMetadata(...)` can also write failure sidecars.
- `tools/tcxsd_sidecar.py` is the first script/Node-adjacent reader for sidecar validation and summaries.
- `tools/tcxsd_job.py` is the first script/Node-adjacent generation entry. It reads JSON, resolves paths relative to the job file, calls `sd-cli`, and writes PNG/JSON/log outputs.
- `RuntimeSettings::executionMode` controls `Auto`, `InProcess`, and `CliProcess`.
- Windows CUDA `Auto` prefers the bundled `sd-cli.exe` process backend when present.
- Worker threads only produce CPU `Pixels`; examples upload to GPU textures on the main thread.
- `ModelPaths` already reserves fields for video/audio/controlnet/photo maker additions.
- `ImageRequest` already reserves fields for img2img, inpainting, ControlNet, LoRA, and metadata.

## Next Best Step

Add a persistent backend option (`sd-server`, local worker process, or fixed in-process direct API) so Windows CUDA can keep large models loaded across jobs. In parallel, tune the Ideogram4 prompt composer defaults for text fidelity and add named prompt profiles for poster, product, typography, and logo workflows.

## Latest Verified State

- Native build: `windows-cuda @ 19bdfe22d255d5b4dff39d449318b9bc5ea2317f`
- Native install: `libs/stable-diffusion/current`, including `sd-cli.exe`
- Model check: `python tools/verify_sd.py`
- Example build: `examples/ideogram4-basic` builds with `G:/TrussC/tools/bin/trusscli.exe build`
- Release build: `G:/TrussC/tools/bin/trusscli.exe build --release`, verified from a Debug cache to a Release cache
- Smoke run: `TCXSD_SMOKE=1 ... ./bin/ideogram4-basic.exe`, exit code 0
- Composed smoke run: `TCXSD_SMOKE=1 TCXSD_SMOKE_COMPOSE=1 ... ./bin/ideogram4-basic.exe`, exit code 0
- Smoke output: `examples/ideogram4-basic/outputs/ideogram4_job_1.png`, latest composed output 201036 bytes
- Smoke sidecar: `examples/ideogram4-basic/outputs/ideogram4_job_1.json`, parsed successfully with `ok=true`, `execution_mode=cli_process`, `backend=cuda`, and `duration_seconds=8.986`
- Sidecar tool: `python tools/tcxsd_sidecar.py validate ... --require-success-image`, exit code 0
- JSON job runner: `python tools/tcxsd_job.py run examples/ideogram4-basic/jobs/ideogram4_poster_job.json`, exit code 0
  - Output: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.png`
  - Sidecar: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.json`
  - Sidecar summary: `ok=true`, `execution_mode=cli_process`, `backend=cuda0`, `duration_seconds=8.73`
- Python compile: `python -m py_compile tools/setup_sd.py tools/tcxsd_models.py tools/verify_sd.py tools/tcxsd_sidecar.py tools/tcxsd_job.py`
- Tests: `python -m unittest discover -s tests`, 17 tests passing

## Known Caveat

Direct in-process Windows CUDA generation still hangs after `decode_first_stage completed` in the TrussC/D3D process. Windows CUDA `Auto` should keep using the isolated `sd-cli` process backend until that upstream/direct-API interaction is understood. Prompt composer output is functional but still needs quality tuning before it is treated as a polished default.
