# tcxStableDiffusion

Stable Diffusion local AI generation addon for TrussC.

The addon targets Windows and macOS only. Windows is the primary platform and uses a CUDA-only `stable-diffusion.cpp` build profile by default. On Windows CUDA, `RuntimeSettings::windowsCuda()` uses a pure C++ persistent `sd-server.exe` backend in `Auto` mode when available, so models stay loaded across jobs without putting the CUDA generation pipeline inside the TrussC/D3D app process. `sd-cli.exe` remains the fallback and explicit process-isolation backend.

## First Setup

From this addon directory:

```powershell
python tools\setup_sd.py build-native --profile windows-cuda
python tools\setup_sd.py download-model --model ideogram4-q4_0
python tools\verify_sd.py
```

If model download fails 3 times, the setup script prints exact manual download URLs. Place downloaded files in:

```text
examples/ideogram4-basic/models
```

## First Example

```text
examples/ideogram4-basic
```

The example uses `tcxImGui` and a Chinese GUI. It initializes Ideogram4 only when the user clicks the initialization button, then generates images asynchronously.

The Windows example bundles both:

- `stable-diffusion.dll`
- `sd-cli.exe`
- `sd-server.exe`

## Core API

```cpp
tcx::StableDiffusion sd;
sd.setupIdeogram4Async("models", tcx::sd::RuntimeSettings::windowsCuda());

sd.createImage("A precise product render, studio lighting")
    .size(1024, 1024)
    .steps(8)
    .cfg(1.0f)
    .run();
```

Call `sd.update()` in the app loop, wait for `sd.isReady()`, then drain results with `sd.pollResult(result)`.

Advanced users can force `settings.executionMode = tcx::sd::ExecutionMode::PersistentServer`, `CliProcess`, or `InProcess`. Normal C++ app usage does not require Python; Python scripts are setup, verification, and optional Node-adjacent tooling only.

## Script And Node-Adjacent Jobs

Use the tracked JSON job file as the first automation/Node integration surface:

```powershell
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\flux2-klein-basic\jobs\flux2_klein_product_job.json
python tools\tcxsd_job.py run examples\z-image-basic\jobs\z_image_turbo_wide_job.json
```

Generation writes a PNG, backend log (`sd-server` or `sd-cli`), and JSON sidecar. Inspect sidecars with:

```powershell
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\jobs\ideogram4_poster_job.json --json
```

Detailed docs:

- `docs/API_USAGE.md`
- `docs/ISSUES.md`
- `docs/PROGRESS.md`
- `docs/HANDOFF.md`
