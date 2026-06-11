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
  - Loads Ideogram4 from `examples/ideogram4-basic/models`.
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
  - Example output size: 201036 bytes
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
- Direct in-process `generate_image()` reached `decode_first_stage completed` and then did not return in the TrussC/D3D app process. The process-isolated CLI backend is now the Windows default mitigation.
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
