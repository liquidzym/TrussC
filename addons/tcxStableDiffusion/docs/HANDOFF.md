# tcxStableDiffusion Handoff

## Current State

This addon now has the core C++ runtime, Python tooling, and Node package surfaces in place. Windows CUDA uses the managed pure C++ `sd-server` backend by default when available, with `sd-cli` as process-isolated fallback. The current API includes per-model profiles, runtime presets, sidecar JSON, explicit output/temp/cache roots, cleanup helpers, structured error codes, prompt packs, quality checks, and a Chinese multi-model ImGui workbench.

## 2026-06-11 Immediate Handoff Addendum

This section is the current resume point for a new Codex thread.

Current local pass changes:

- `src/tcxsd/NativeRuntime.cpp`
  - Default native output/log directory no longer falls back to the Windows system temp directory.
  - If `RuntimeSettings::outputDirectory` is empty, fallback is now `<current-working-directory>/tcxStableDiffusionOutputs`.
- `tools/setup_sd.py`
  - Temporary `vcvars` batch files now use `G:/TrussC/addons/tcxStableDiffusion/.codex-logs/tmp`, not the system temp directory.
  - Default model downloads and verification now target `examples/ideogram4-basic/bin/data/models/<model-id>`.
- `src/tcxsd/Types.cpp`
  - `RuntimeSettings::lowVramCuda()` now uses `backendAssignment = "cuda0,te=cpu"`, `paramsBackendAssignment = "cpu"`, `streamLayers = true`, and `maxVramGiB = 8.0`.
  - This avoids Qwen/TE runtime offload spikes on busy RTX 4090 Windows desktops.
- `examples/ideogram4-basic/src/tcApp.cpp`
  - Model folders are now loaded from `examples/ideogram4-basic/bin/data/models/<model-id>`.
  - Ideogram4 default prompt is now an upstream-verified 1024 poster prompt.
  - Ideogram4 defaults are now `1024x1024`, `steps=20`, `cfg=7.0`, `seed=42`.
  - The workbench still exposes the Ideogram4 composer, but raw verified prompt is the default starter.
  - Safety-placeholder image detection now reports a diagnostic instead of repeatedly changing seed.
- `examples/ideogram4-basic/jobs/ideogram4_poster_job.json`
  - `model_dir` now points into `../bin/data/models/ideogram4-q4_0`.
  - Updated to the same verified Ideogram4 prompt and low-VRAM streaming runtime.
- `examples/flux2-klein-basic/jobs/flux2_klein_product_job.json`
  - `model_dir` now points into `../../ideogram4-basic/bin/data/models/flux2-klein-4b-q4_0`.
- `examples/z-image-basic/jobs/z_image_turbo_wide_job.json`
  - `model_dir` now points into `../../ideogram4-basic/bin/data/models/z-image-turbo-q3_k`.
- `node/src/index.mjs`
  - Default `modelRoot` now points into the example `bin/data/models` folder.

C-drive / temp status:

- Earlier historical outputs did write native images and server logs to `C:/Users/Admin/AppData/Local/Temp/tcxStableDiffusion` when `outputDirectory` was not set.
- The current example sets `settings.outputDirectory` under `examples/ideogram4-basic/outputs/<model-id>/native`, so latest GUI smoke outputs are on `G:`.
- Future tests should also set process temp variables to the ignored local temp folder:

```powershell
$tmp = 'G:\TrussC\addons\tcxStableDiffusion\.codex-logs\tmp'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$env:TEMP = $tmp
$env:TMP = $tmp
$env:TMPDIR = $tmp
```

Latest model-diagnosis conclusion:

- The `Image blocked by safety filter` frame is not produced by tcxStableDiffusion code and is not logged by `sd-server` as a moderation block.
- Direct `sd-cli.exe` can also generate that frame for some custom Ideogram4 prompts, so it is model output, not an addon/UI keyword mutation.
- Upstream Ideogram4 verified prompt works with the same local assets and seed class, so the assets and CUDA path are usable.
- FLUX.2-klein and Z-Image do not reproduce the safety-placeholder issue in the current multi-model workbench smoke tests. This points to Ideogram4 prompt/model behavior, not a common code-path failure.
- Ideogram4 text spelling is still imperfect even on a good image; that remains prompt/model tuning, not backend failure.

Latest smoke evidence:

- Ideogram4 GUI smoke:
  - `TCXSD_SMOKE=1 TCXSD_SMOKE_MODEL=ideogram4 TCXSD_SMOKE_LOW_VRAM=1`
  - Output: `examples/ideogram4-basic/outputs/ideogram4-q4_0/ideogram4-q4_0_job_1.png`
  - Sidecar: `examples/ideogram4-basic/outputs/ideogram4-q4_0/ideogram4-q4_0_job_1.json`
  - Summary: `ok=true`, `1024x1024`, `steps=20`, `cfg_scale=7`, `seed=42`, `backend_assignment=cuda0,te=cpu`, `stream_layers=true`, `duration_seconds=115.952`.
- FLUX.2-klein GUI smoke:
  - `TCXSD_SMOKE=1 TCXSD_SMOKE_MODEL=flux2-klein TCXSD_SMOKE_LOW_VRAM=1 TCXSD_SMOKE_WIDTH=512 TCXSD_SMOKE_HEIGHT=512 TCXSD_SMOKE_STEPS=4 TCXSD_SMOKE_SEED=2048`
  - Output: `examples/ideogram4-basic/outputs/flux2-klein-4b-q4_0/flux2-klein-4b-q4_0_job_1.png`
  - Sidecar: `examples/ideogram4-basic/outputs/flux2-klein-4b-q4_0/flux2-klein-4b-q4_0_job_1.json`
  - Summary: `ok=true`, `512x512`, `steps=4`, `seed=2048`, no safety-placeholder image, output/log paths on `G:`, `duration_seconds=9.556`.
- FLUX.2-klein bin-data relocation smoke:
  - `TCXSD_SMOKE=1 TCXSD_SMOKE_MODEL=flux2-klein TCXSD_SMOKE_LOW_VRAM=1 TCXSD_SMOKE_WIDTH=512 TCXSD_SMOKE_HEIGHT=512 TCXSD_SMOKE_STEPS=1 TCXSD_SMOKE_SEED=777`
  - Model dir: `examples/ideogram4-basic/bin/data/models/flux2-klein-4b-q4_0`
  - Summary: `ok=true`, `512x512`, `steps=1`, `seed=777`, `execution_mode=persistent_server`, `duration_seconds=9.063`.
- Z-Image GUI smoke:
  - `TCXSD_SMOKE=1 TCXSD_SMOKE_MODEL=z-image TCXSD_SMOKE_LOW_VRAM=1`
  - Output: `examples/ideogram4-basic/outputs/z-image-turbo-q3_k/z-image-turbo-q3_k_job_1.png`
  - Sidecar: `examples/ideogram4-basic/outputs/z-image-turbo-q3_k/z-image-turbo-q3_k_job_1.json`
  - Summary: `ok=true`, `1024x512`, `steps=8`, `seed=4096`, no safety-placeholder image, output/log paths on `G:`, `duration_seconds=10.067`.

This pass has already been verified with `trusscli build --release`, full Python/Node tests, model path validation, and a short FLUX.2-klein GUI smoke from the relocated `bin/data/models` folder.

Next continuation focus: curate real quality examples for each model/preset, add focused img2img/ControlNet/LoRA examples, and continue upstream diagnostics for the direct in-process CUDA hang.

## Persistent Encapsulation Roadmap

High-priority encapsulation work to keep iterating:

- Model profiles:
  - Keep tuning per-model runtime defaults based on real smoke and final-quality outputs.
  - Existing presets cover draft, balanced, final, low-VRAM, and dedicated 4090 full-speed.
  - Store model capability metadata: text encoder type, native resolution, recommended size, step range, CFG/guidance range, VAE format, prompt style, and known caveats.
- Runtime and storage:
  - Keep output/temp/cache roots explicit in `RuntimeSettings`, Node, and Python job tooling.
  - Expand cleanup policy only with user-visible controls; avoid deleting final output PNGs from output roots.
  - Add GPU memory policy knobs: `te=cpu`, `vae=cpu`, `stream_layers`, `max_vram`, persistent model reuse, and server reuse.
  - Detect CUDA OOM and return structured remediation hints instead of generic failed generation text.
- Prompt and quality:
  - Keep Ideogram4 verified templates separate from general high-level prompt builders.
  - Add prompt packs for poster, product, typography, logo, UI mockup, photo, and illustration.
  - Add a result quality classifier for placeholder images, blank images, severe text failure, and wrong aspect output.
  - Record prompt profile/version in every sidecar for reproducibility.
- Node-facing package:
  - Continue hardening the stable public API around progress events and examples.
  - Keep Node pure process/server based; do not depend on Python for normal runtime.
  - TypeScript declarations, cancellation, sidecars, server reuse, storage roots, and prompt packs are in place.
- C++ API:
  - Add strongly typed request structs for text-to-image, image-to-image, inpainting, ControlNet, LoRA, upscale, and future video/audio.
  - Keep designer-friendly high-level helpers, but preserve low-level escape hatches for professional tuning.
  - Add structured error codes and retry policies at the addon layer.
- Examples:
  - Keep all example model folders under `examples/ideogram4-basic/bin/data/models/<model-id>`.
  - Turn the current workbench into the main multi-model example, then add smaller focused examples for CLI/job, Node, batch, img2img, ControlNet, and LoRA.
  - Keep GUI labels Chinese and use a modern restrained theme.
- Future media:
  - Reserve architecture for video generation, audio generation, ControlNet, reference images, upscalers, and prompt/asset caching without breaking current API names.

## Important Paths

- Addon root: `G:/TrussC/addons/tcxStableDiffusion`
- Native source: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/source`
- Native install: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current`
- Native CLI: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current/bin/sd-cli.exe`
- Native server: `G:/TrussC/addons/tcxStableDiffusion/libs/stable-diffusion/current/bin/sd-server.exe`
- First example: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic`
- FLUX.2-klein starter: `G:/TrussC/addons/tcxStableDiffusion/examples/flux2-klein-basic`
- Z-Image starter: `G:/TrussC/addons/tcxStableDiffusion/examples/z-image-basic`
- Shared example models: `G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/bin/data/models/<model-id>`
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

Tune final prompt quality for each model and harden the Node package sidecar/cancellation story. The three priority model assets are already centralized under the main example bin data folder and smoke-verified. For GUI polish, the next useful step is a small responsive pass for very short window heights, but the current 1280x900 workbench layout is visually verified.

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
- Tests: `python -m unittest discover -s tests`, 22 tests passing

## Known Caveat

Direct in-process Windows CUDA generation still hangs after `decode_first_stage completed` in the TrussC/D3D process. Windows CUDA `Auto` should keep using the managed `sd-server` backend until that upstream/direct-API interaction is understood. Persistent server cancellation is best-effort for already-generating jobs because upstream currently does not interrupt active generation through the HTTP cancel endpoint. Prompt composer output is functional but still needs quality tuning before it is treated as a polished default.
