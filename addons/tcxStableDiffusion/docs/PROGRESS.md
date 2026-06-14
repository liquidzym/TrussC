# tcxStableDiffusion Progress

## 2026-06-10

### Completed

- Created the first addon skeleton under `addons/tcxStableDiffusion`.
- Added shared TrussC addon metadata and CMake integration.
- Added conditional native runtime integration:
  - Missing native runtime does not break addon compilation.
  - Installed runtime is discovered from `libs/stable-diffusion/current/tcxStableDiffusionPaths.cmake`.
  - Runtime DLL/dylib/so files are registered with `tc_addon_bundle_file` when available.
- Added `tools/tcxsd_models.py` model registry and tests.
- Added `tools/setup_sd.py`:
  - Lists model profiles.
  - Downloads model assets into the matching example directory.
  - Stops after 3 failed attempts and prints exact manual download URLs.
  - Clones/builds `stable-diffusion.cpp` once and writes a build manifest.
- Added `tools/verify_sd.py` for native/model setup troubleshooting.
- Added C++ API foundation:
  - `tcx::sd::ModelPaths`
  - `tcx::sd::RuntimeSettings`
  - `tcx::sd::ImageRequest`
  - `tcx::sd::ImageResult`
  - `tcx::sd::StableDiffusion`
  - `tcx::StableDiffusion` high-level alias
- Added async generation architecture:
  - Worker thread handles native calls.
  - Main thread drains progress/result messages with `update()`.
  - GPU texture upload stays in the example main thread.
- Added first example at `examples/ideogram4-basic`.
  - Uses `tcxImGui`.
  - Chinese GUI.
  - Historical note: this first pass loaded Ideogram4 from `examples/ideogram4-basic/models`; current layout uses `examples/ideogram4-basic/bin/data/models/<model-id>`.
  - Separates model initialization from image generation.

### Verified

- `python -m unittest discover -s tests`
- `python tools/setup_sd.py list-models`
- `python tools/verify_sd.py --allow-missing`
- `cmake -S "G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic" -B "G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/build"`
- `cmake --build "G:/TrussC/addons/tcxStableDiffusion/examples/ideogram4-basic/build" --config Debug --parallel`

### Not Yet Verified

- Real `stable-diffusion.cpp` CUDA build.
- Real Ideogram4 generation on RTX 4090.
- Model download resilience against interrupted multi-GB downloads.

### Next Implementation Pass

- Build the example without native runtime and fix compile errors.
- Run `tools/setup_sd.py build-native --profile windows-cuda` on the RTX 4090 machine.
- Download or manually place Ideogram4 assets.
- Run a first 1024x1024 prompt and capture timing/VRAM notes.
- Add image-to-image, inpainting, and ControlNet input loading.
- Add stronger result metadata sidecar JSON.
- Add Node-facing command or IPC layer after C++ image generation is proven.

## 2026-06-11

### Completed

- Built `stable-diffusion.cpp` for the Windows CUDA profile on the RTX 4090 machine.
- Confirmed the native install at `libs/stable-diffusion/current` contains:
  - `include/stable-diffusion.h`
  - `lib/stable-diffusion.lib`
  - `bin/stable-diffusion.dll`
  - `bin/sd-cli.exe`
  - `tcxStableDiffusionPaths.cmake`
  - `build_manifest.json`
- Confirmed the Windows CUDA manifest enables only CUDA among accelerator backends:
  - `SD_CUDA=ON`
  - `SD_METAL=OFF`
  - `SD_VULKAN=OFF`
  - `SD_OPENCL=OFF`
  - `SD_HIPBLAS=OFF`
  - `SD_SYCL=OFF`
  - `SD_MUSA=OFF`
- Improved the native build helper for Windows:
  - Detects Visual Studio `vcvars64.bat`.
  - Uses Ninja when CUDA MSBuild integration is unavailable.
  - Passes the detected `nvcc` path explicitly.
- Improved model download robustness:
  - Resumes `.part` files with HTTP `Range`.
  - Restarts cleanly when a server rejects a resume range.
  - Keeps the 3-attempt stop rule and prints exact manual URLs.
- Switched the default FLUX.2 VAE asset to a public mirror:
  - `nvidia/PiD/checkpoints/flux2_ae.safetensors`
  - This avoids the gated BFL FLUX.2-dev VAE URL in this environment.
- Rebuilt the `ideogram4-basic` TrussC example against the native runtime.
- Adjusted the example to resolve its model/output folder from `__FILE__`, so it does not depend on fragile generated CMake definitions.
- Downloaded and verified the first Ideogram4 starter assets:
  - `ideogram4-Q4_0.gguf`
  - `ideogram4_uncond-Q4_0.gguf`
  - `Qwen3VL-8B-Instruct-Q4_K_M.gguf`
  - `flux2_ae.safetensors`
- Added a Windows CUDA `sd-cli` process backend:
  - `RuntimeSettings::executionMode` supports `Auto`, `InProcess`, and `CliProcess`.
  - Windows CUDA `Auto` uses `sd-cli.exe` when available.
  - `sd-cli.exe` and `stable-diffusion.dll` are both bundled into the example `bin` folder.
  - CLI output is loaded back into `trussc::Pixels`, keeping GPU texture work on the app main thread.
- Added smoke mode for `examples/ideogram4-basic` through `TCXSD_SMOKE`.
- Set the Ideogram4 example to default to the low-VRAM/offload profile, matching the upstream CLI command pattern.
- Fixed `trusscli build --release` for single-config CMake presets such as Ninja:
  - `trusscli` now detects non-Release `CMAKE_BUILD_TYPE`.
  - It re-runs the configure preset with `-DCMAKE_BUILD_TYPE=Release` before building.
  - Multi-config generators still use the existing `--config Release` build path.
- Added the first Ideogram4 prompt composer pass:
  - `tcx::sd::IdeogramPrompt` / `tcx::IdeogramPrompt`
  - `ImageRequest::fromIdeogram4(...)`
  - `ImageRequest::ideogram4(...)`
  - `StableDiffusion::createImage(const IdeogramPrompt&)`
  - JSON output matching upstream Ideogram4-style `high_level_description`, `style_description`, and `compositional_deconstruction` fields.
- Updated `examples/ideogram4-basic` with a Chinese prompt-template panel:
  - subject,
  - visible text,
  - style,
  - comma-separated palette,
  - `Apply Template` flow into the raw prompt preview.
- Added result sidecar metadata support:
  - `ImageResult::durationSeconds`
  - `ImageResult::saveMetadata(...)`
  - `ImageResult::saveWithMetadata(...)`
  - request/runtime/model/backend fields in result metadata
  - JSON sidecar saving in `examples/ideogram4-basic`
  - failed jobs can save `_failed.json` for debugging.
- Added `tools/tcxsd_sidecar.py`:
  - `validate` checks required sidecar fields.
  - `summary` emits compact text or JSON for batch/Node-adjacent consumers.
- Added sidecar tool unit tests.
- Added `tools/tcxsd_job.py`:
  - reads JSON job files,
  - resolves paths relative to the job file,
  - validates model/runtime/output inputs,
  - builds the `sd-cli` argument list from the model registry,
  - runs local CUDA generation through the CLI process backend,
  - writes PNG, log, and sidecar JSON artifacts.
- Added first tracked JSON job example at `examples/ideogram4-basic/jobs/ideogram4_poster_job.json`.
- Added job runner unit tests using a fake `sd-cli` process and fake PNG output.
- Hardened the job runner after code audit:
  - string booleans such as `"true"` and `"false"` are parsed explicitly,
  - invalid numeric fields report validation errors,
  - explicit `cli` paths must exist,
  - model assets must exist and be non-empty.
- Hardened sidecar JSON writing by escaping metadata keys as well as values.

### Verified

- `python -m unittest discover -s tests`
- `python tools/setup_sd.py list-models`
- `python tools/verify_sd.py`
- `G:/TrussC/tools/bin/trusscli.exe update`
- `G:/TrussC/tools/bin/trusscli.exe build`
- `cmake --preset windows -DCMAKE_BUILD_TYPE=Debug`
- `G:/TrussC/tools/bin/trusscli.exe build --release`
  - Printed: `[configure] Switching single-config preset 'windows' to CMAKE_BUILD_TYPE=Release (was Debug)`
  - `CMakeCache.txt`: `CMAKE_BUILD_TYPE:STRING=Release`
  - `compile_commands.json`: `/O2 /Ob2 /DNDEBUG -MD`
- `TCXSD_SMOKE=1 ... ./bin/ideogram4-basic.exe`
  - Exit code: 0
  - CLI temp output: `C:/Users/Admin/AppData/Local/Temp/tcxStableDiffusion/tcxsd_job_1_1781147930824.png`
  - Example output: `examples/ideogram4-basic/outputs/ideogram4_job_1.png`
  - Example output size: 82286 bytes before the composed-prompt smoke overwrite
- `TCXSD_SMOKE=1 TCXSD_SMOKE_COMPOSE=1 ... ./bin/ideogram4-basic.exe`
  - Exit code: 0
  - CLI temp output: `C:/Users/Admin/AppData/Local/Temp/tcxStableDiffusion/tcxsd_job_1_1781149594771.png`
  - Example output: `examples/ideogram4-basic/outputs/ideogram4_job_1.png`
  - Historical CLI example output size before the persistent-server overwrite: 201036 bytes
  - Example sidecar: `examples/ideogram4-basic/outputs/ideogram4_job_1.json`
  - Sidecar parse check confirmed `ok=true`, `state=complete`, `prompt_profile=ideogram4`, `execution_mode=cli_process`, `backend=cuda`, `image_width=512`, `image_height=512`, and `duration_seconds=8.986`.
- `python tools/tcxsd_sidecar.py validate examples/ideogram4-basic/outputs/ideogram4_job_1.json --require-success-image`
- `python tools/tcxsd_sidecar.py summary examples/ideogram4-basic/outputs/ideogram4_job_1.json`
- `python tools/tcxsd_job.py validate examples/ideogram4-basic/jobs/ideogram4_poster_job.json`
- `python tools/tcxsd_job.py run examples/ideogram4-basic/jobs/ideogram4_poster_job.json`
  - Exit code: 0
  - Output: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.png`
  - Sidecar: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.json`
  - Log: `examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.log`
  - Sidecar summary confirmed `ok=true`, `execution_mode=cli_process`, `backend=cuda0`, `image_width=512`, `image_height=512`, and `duration_seconds=8.73`.
- `python -m py_compile tools/setup_sd.py tools/tcxsd_models.py tools/verify_sd.py tools/tcxsd_sidecar.py tools/tcxsd_job.py`

### Findings

- Upstream `sd-cli` completed the same Ideogram4 generation path successfully.
- Direct in-process `generate_image()` reached `decode_first_stage completed` and then did not return in the TrussC/D3D app process. This pass initially used the process-isolated CLI backend as mitigation; the later persistent `sd-server` backend is now the Windows CUDA `Auto` preference.
- The previous Release issue was a TrussC CLI single-config preset problem, not a tcxStableDiffusion addon problem.
- The prompt composer path is functional, but the 3-step 512 smoke output is not a final quality target. Text fidelity and default prompt wording still need dedicated tuning.
- The sidecar JSON path is now usable for replay/debug tooling and future Node/IPC consumers.
- `tcxsd_sidecar.py` provides the first stable non-C++ consumer path for result artifacts.
- `tcxsd_job.py` provides a real Node-adjacent generation path before the persistent worker/IPC backend is designed.
- Code audit found and fixed JSON-key escaping plus job-runner input validation issues before commit.

### Notes

- The initial Visual Studio CMake generator failed with `No CUDA toolset found`; the current helper avoids that path by using Ninja inside a Visual Studio developer environment.
- `trusscli update` rewrites example `CMakeLists.txt`, so durable example behavior should live in source code, addon metadata, or future TrussC template support.
- The current smoke outputs are runtime verification artifacts, not curated Ideogram4 quality benchmarks.

### 2026-06-11 Continued Implementation

- Added `sd-server.exe` to the Windows CUDA native build/install path:
  - build target: `sd-server`
  - path file: `TCXSD_NATIVE_SERVER`
  - bundled runtime files: `sd-cli.exe`, `sd-server.exe`, `stable-diffusion.dll`
- Added pure C++ persistent server execution:
  - `ExecutionMode::PersistentServer`
  - Windows CUDA `Auto` prefers managed `sd-server.exe` when available.
  - C++ launches the server process, health-checks `/sdcpp/v1/capabilities`, submits `/sdcpp/v1/img_gen`, polls jobs, decodes base64 PNG data, and returns `tc::Pixels`.
  - No Python is required for normal TrussC/C++ runtime use.
- Added async model setup:
  - `setupAsync(...)`
  - `setupIdeogram4Async(...)`
  - `setupFlux2KleinAsync(...)`
  - `setupZImageTurboAsync(...)`
  - `isSettingUp()`
- Updated `examples/ideogram4-basic` to initialize models asynchronously and submit smoke jobs only after `isReady()`.
- Added high-level request helpers:
  - `imageToImage(path, strength)`
  - `mask(path)`
  - `control(path, weight)`
  - `lora(path, weight)`
- Wired those image/LoRA fields through the persistent server backend.
- Added C++ model path helpers for:
  - FLUX.2-klein
  - Z-Image Turbo
- Added starter example folders/jobs:
  - `examples/flux2-klein-basic/jobs/flux2_klein_product_job.json`
  - `examples/z-image-basic/jobs/z_image_turbo_wide_job.json`
- Added optional script/Node-adjacent persistent server tooling:
  - `tools/tcxsd_server.py`
  - `runtime.execution_mode = persistent_server` support in `tools/tcxsd_job.py`
- Added unit coverage for setup/server packaging and server request body mapping.

### 2026-06-11 Continued Verification

- `python -m unittest discover -s tests`
  - 20 tests passing.
- `python -m py_compile tools/setup_sd.py tools/tcxsd_models.py tools/verify_sd.py tools/tcxsd_sidecar.py tools/tcxsd_job.py tools/tcxsd_server.py`
- `python tools/setup_sd.py build-native --profile windows-cuda`
  - Confirmed `sd-server.exe` exists in `libs/stable-diffusion/current/bin`.
- `G:/TrussC/tools/bin/trusscli.exe build --release`
  - Confirmed example bundles `sd-cli.exe`, `sd-server.exe`, and `stable-diffusion.dll`.
- Pure C++ persistent server smoke:
  - Command: `TCXSD_SMOKE=1 TCXSD_SMOKE_COMPOSE=1 TCXSD_SMOKE_WIDTH=512 TCXSD_SMOKE_HEIGHT=512 TCXSD_SMOKE_STEPS=3 TCXSD_SMOKE_SEED=9011 trusscli run --release`
  - Exit code: 0
  - Sidecar: `examples/ideogram4-basic/outputs/ideogram4_job_1.json`
  - Summary: `ok=true`, `execution_mode=persistent_server`, `backend=cuda`, `image_width=512`, `image_height=512`, `seed=9011`, `duration_seconds=6.030`.
  - Server log reached both `decode_first_stage completed` and `generate_image completed`, confirming the managed server path avoids the direct in-process post-decode hang.
- Priority starter model downloads:
  - `python tools/setup_sd.py download-model --model flux2-klein-4b-q4_0`
  - `python tools/setup_sd.py download-model --model z-image-turbo-q3_k`
  - Both completed without manual-download fallback.
- Priority starter model verification:
  - `python tools/verify_sd.py --model flux2-klein-4b-q4_0`
  - `python tools/verify_sd.py --model z-image-turbo-q3_k`
- FLUX.2-klein persistent-server JSON smoke:
  - `python tools/tcxsd_job.py run examples/flux2-klein-basic/jobs/flux2_klein_product_job.json`
  - Summary: `ok=true`, `execution_mode=persistent_server`, `backend=cuda0`, `image_width=512`, `image_height=512`, `duration_seconds=2.529`.
- Z-Image Turbo persistent-server JSON smoke:
  - `python tools/tcxsd_job.py run examples/z-image-basic/jobs/z_image_turbo_wide_job.json`
  - Summary: `ok=true`, `execution_mode=persistent_server`, `backend=cuda0`, `image_width=1024`, `image_height=512`, `duration_seconds=4.040`.

### 2026-06-11 Multi-Model And Node Pass

- Centralized model storage under the main example:
  - `examples/ideogram4-basic/bin/data/models/ideogram4-q4_0`
  - `examples/ideogram4-basic/bin/data/models/flux2-klein-4b-q4_0`
  - `examples/ideogram4-basic/bin/data/models/z-image-turbo-q3_k`
- Updated `tools/setup_sd.py` so default downloads and verification use `bin/data/models/<model-id>`.
- Updated the main GUI example into a multi-model workbench:
  - model selector for Ideogram4, FLUX.2-klein, and Z-Image Turbo,
  - model-specific defaults for prompt, negative prompt, size, steps, CFG, and seed,
  - output folders under `outputs/<model-id>`,
  - smoke model selection through `TCXSD_SMOKE_MODEL`.
- Added direct in-process diagnostic breadcrumbs around upstream `generate_image()`.
- Added `docs/DIRECT_INPROCESS_DEBUG.md` with the current root-cause evidence and safe diagnostic rules.
- Added the first formal Node-facing package:
  - `node/src/index.mjs`
  - `node/bin/tcxsd-node.mjs`
  - `node/test/model-paths.test.mjs`
  - Uses `sd-server.exe` directly and does not call Python.
- Verified `npm test` with 4 passing Node tests.
- Verified `node .\bin\tcxsd-node.mjs --job ..\examples\flux2-klein-basic\jobs\flux2_klein_product_job.json`, exit code 0.

### 2026-06-11 GUI Chinese And Theme Pass

- Fixed the main ImGui workbench Chinese rendering issue.
  - Root cause: the window title used system text rendering and displayed Chinese correctly, but Dear ImGui was still using its default ASCII-oriented font, so CJK labels rendered as question marks.
  - The example now loads a CJK-capable UI font during startup before the first ImGui frame.
  - Windows candidates include Microsoft YaHei, Noto Sans SC, SimHei, SimSun, and DengXian.
  - macOS candidates include PingFang, Heiti, Hiragino Sans GB, and Songti.
- Modernized the multi-model workbench appearance:
  - wider control panel,
  - cleaner dark neutral theme,
  - blue/teal/green accent states,
  - rounded controls with restrained borders,
  - Chinese labels placed above full-width controls to avoid label clipping.
- Verified the fix visually with a captured app screenshot:
  - `examples/ideogram4-basic/outputs/ui_cjk_check.png`
  - Chinese labels render normally instead of `????`.
- Rebuilt and smoke-tested the example after the UI changes.

### 2026-06-11 GUI Verification

- `G:/TrussC/tools/bin/trusscli.exe build --release`
  - Exit code: 0.
  - Only the existing C++20 `std::atomic_* shared_ptr` deprecation warnings appeared from TrussC event internals.
- Short FLUX.2-klein app smoke:
  - `TCXSD_SMOKE=1 TCXSD_SMOKE_MODEL=flux2-klein TCXSD_SMOKE_WIDTH=512 TCXSD_SMOKE_HEIGHT=512 TCXSD_SMOKE_STEPS=1 TCXSD_SMOKE_SEED=777 ./bin/ideogram4-basic.exe`
  - Exit code: 0.
- Visual UI check:
  - launched `bin/ideogram4-basic.exe`,
  - captured the window to `examples/ideogram4-basic/outputs/ui_cjk_check.png`,
  - confirmed the UI shows correct Chinese labels and the updated modern theme.

## 2026-06-12 Profile, Node, Storage, And Quality Pass

### Completed

- Added per-model profile data for Ideogram4, FLUX.2-klein, and Z-Image Turbo:
  - draft/balanced/final quality defaults,
  - default/low-VRAM/RTX 4090 full-speed runtime presets,
  - C++ `ModelProfile`, Python model registry helpers, and Node `modelProfiles`.
- Productionized the Node package:
  - TypeScript declarations,
  - structured `TcxSdError` errors with remediation hints,
  - sidecar parity,
  - `/cancel` support,
  - reusable `TcxSdServerSession`,
  - storage roots and cleanup helpers,
  - prompt packs and quality checks.
- Added explicit runtime/storage lifecycle controls:
  - C++ `outputDirectory`, `tempDirectory`, `cacheDirectory`, `StorageRoots`, and `cleanupRuntimeStorage`,
  - Python `tools/tcxsd_storage.py`,
  - Node `resolveStorageRoots()` and `cleanupStorage()`.
- Unified backend handling for extended image requests:
  - `PersistentServer` supports init/mask/control/LoRA,
  - `CliProcess` supports init/mask/control through `sd-cli`,
  - LoRA outside `PersistentServer` and direct `InProcess` image inputs return structured `BACKEND_UNSUPPORTED` errors.
- Added prompt quality and result quality checks:
  - placeholder prompt detection,
  - tiny/invalid image detection,
  - size mismatch detection,
  - text verification warning/failure hooks,
  - Chinese Ideogram4 prompt pack round-trip coverage.
- Added stable error codes and remediation hints for CUDA OOM, missing model assets, server startup failures, unsupported backend fields, cancellation limits, timeouts, and missing outputs.

### Verified

- `python -m unittest discover -s tests -p 'test_*.py'`
- `npm test` in `node/`
- `python -m py_compile tools/setup_sd.py tools/tcxsd_models.py tools/verify_sd.py tools/tcxsd_sidecar.py tools/tcxsd_job.py tools/tcxsd_server.py tools/tcxsd_errors.py tools/tcxsd_storage.py tools/tcxsd_prompts.py tools/tcxsd_quality.py`
- `node --check src/index.mjs`
- `node --check bin/tcxsd-node.mjs`
- `G:/TrussC/tools/bin/trusscli.exe build` in `examples/ideogram4-basic`

## 2026-06-12 Runtime Workflow Encapsulation Pass

### Completed

- Added the non-MVP workflow execution checklist in `docs/ROADMAP_RUNTIME_WORKFLOWS.md`.
- Added typed C++ request modes and factories for text-to-image, image-to-image, inpaint, ControlNet, LoRA stack, refine, and upscale.
- Added backend capability checks so unsupported combinations return `BACKEND_UNSUPPORTED` with remediation hints instead of silently dropping fields.
- Added sidecar metadata for request mode, image inputs, mask/control/refine paths, LoRA count, strengths, upscale factor, and backend capability checks.
- Added `GenerationSession`, `GenerationProject`, `GenerationArtifact`, `BatchJob`, and `VariantJob` C++/Node workflow objects.
- Added user-intent presets: prompt packs, canvas/style/text presets, and model routing helpers.
- Added a real SD 1.5 ControlNet Canny profile across C++, Node, and setup tooling.
- Updated the main Chinese ImGui workbench with workflow modes, explicit project roots, example image paths, and a dark charcoal/warm gray/earthy-gold theme.
- Added real example inputs and a tracked `sd15_controlnet_canny_job.json`.
- Added C++ `ImagePreprocess` helpers for Canny-style ControlNet guide
  generation and center inpaint mask generation, plus workbench buttons and
  smoke-mode switches for both paths.
- Added Node LoRA management helpers: directory scanning, server-relative path
  normalization, CLI `--list-loras`, and TypeScript declarations.
- Kept Python constrained to setup/test/dev tooling; Node and C++ runtime paths consume native binaries and model files directly.
- Guarded the C++ SD 1.5 ControlNet setup helper with a structured `BACKEND_UNSUPPORTED` error after GUI smoke found a Windows native access violation in the C++ ControlNet startup path. The real ControlNet execution examples are Node CLI and JSON job runner paths. `TCXSD_ENABLE_UNSTABLE_CXX_CONTROLNET=1` is available only for native crash diagnostics.

### Pending Verification In This Pass

- Fix the guarded C++ SD 1.5 ControlNet native startup path so the workbench can run that profile directly after the upstream/native crash is isolated.
- Run two full review passes, fix findings, then commit.

### ControlNet Verification

- Downloaded real assets into `examples/ideogram4-basic/bin/data/models/sd15-controlnet-canny`:
  - `v1-5-pruned-emaonly.safetensors` (4,265,146,304 bytes)
  - `control_v11p_sd15_canny_fp16.safetensors` (722,601,100 bytes)
- Verified assets with `python tools/verify_sd.py --model sd15-controlnet-canny`.
- Ran the tracked JSON ControlNet job through `persistent_server`:
  - `examples/ideogram4-basic/outputs/jobs/sd15_controlnet_canny_job.png`
  - `examples/ideogram4-basic/outputs/jobs/sd15_controlnet_canny_job.json`
  - Sidecar summary: `ok=true`, `execution_mode=persistent_server`, `backend=cuda0`, `image_width=512`, `image_height=512`, `duration_seconds=2.557`.
- Ran the pure Node CLI ControlNet smoke:
  - `examples/ideogram4-basic/outputs/node_controlnet_smoke.png`
  - `examples/ideogram4-basic/outputs/node_controlnet_smoke.json`
  - Sidecar metadata includes `request_mode=control_net`, `execution_mode=persistent_server`, and `model=sd15-controlnet-canny`.
- Ran C++ preprocessor smokes without loading a model:
  - `TCXSD_SMOKE_PREPROCESS=control` generated `examples/ideogram4-basic/inputs/control_canny_generated.png` (`512x512`, 10,858 bytes).
  - `TCXSD_SMOKE_PREPROCESS=mask` generated `examples/ideogram4-basic/inputs/mask_generated.png` (`512x512`, 12,066 bytes).

## 2026-06-12 Workbench Visibility Pass

### Completed

- Added a regression test that keeps the main C++ workbench from losing its
  visible model/workflow/preview affordances.
- Updated the Chinese ImGui workbench so model switching shows:
  - capability badges,
  - recommended backend entry (`C++ 原生` vs `Node/JSON`),
  - default size/steps/CFG/seed summary,
  - backend/path notes,
  - structured unsupported-workflow hints.
- Updated workflow switching so the UI shows the current workflow description,
  real backend route, relevant input panel, one-click examples, and matching
  preview tab.
- Added a right-side preview/Sidecar inspector with output, source, mask,
  ControlNet guide, and JSON Sidecar tabs.

### Verification Notes

- Static regression: `python -m unittest discover -s tests -p 'test_workbench_ui.py'`.
- Build: `G:/TrussC/tools/bin/trusscli.exe build` in `examples/ideogram4-basic`.
- Visual fallback: captured
  `examples/ideogram4-basic/outputs/ui_workbench_switching_screen.png` and
  confirmed Chinese labels, dark/earthy-gold theme, model capability section,
  workflow examples, and preview/Sidecar panel are visible. MCP HTTP logging
  reported startup, but Windows did not expose a listening socket during this
  verification run, so the visual check used an OS screenshot fallback.
