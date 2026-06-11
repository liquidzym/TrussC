# tcxStableDiffusion

Stable Diffusion local AI generation addon for TrussC.

The addon targets Windows and macOS only. Windows is the primary platform and uses a CUDA-only `stable-diffusion.cpp` build profile by default. On Windows CUDA, `RuntimeSettings::windowsCuda()` uses a pure C++ persistent `sd-server.exe` backend in `Auto` mode when available, so models stay loaded across jobs without putting the CUDA generation pipeline inside the TrussC/D3D app process. `sd-cli.exe` remains the fallback and explicit process-isolation backend.

## First Setup

From this addon directory:

```powershell
python tools\setup_sd.py build-native --profile windows-cuda
python tools\setup_sd.py download-model --model ideogram4-q4_0
python tools\setup_sd.py download-model --model sd15-controlnet-canny
python tools\verify_sd.py
```

The Python commands above are setup and verification tooling. Published C++ and
Node runtime paths consume the native binaries and files under `bin/data`
directly; they do not call Python.

If model download fails 3 times, the setup script prints exact manual download URLs. Place downloaded files in:

```text
examples/ideogram4-basic/bin/data/models/<model-id>
```

## First Example

```text
examples/ideogram4-basic
```

The example uses `tcxImGui` and a Chinese GUI. It is now the main multi-model
workflow workbench: Ideogram4, FLUX.2-klein, and Z-Image Turbo run in the C++
GUI, while the SD 1.5 ControlNet Canny assets and inputs are shared with the
tracked Node/JSON ControlNet examples under `bin/data/models/<model-id>`.

The Windows example bundles both:

- `stable-diffusion.dll`
- `sd-cli.exe`
- `sd-server.exe`

## Core API

```cpp
tcx::StableDiffusion sd;
sd.setupIdeogram4Async("bin/data/models/ideogram4-q4_0", tcx::sd::RuntimeSettings::windowsCuda());

sd.createImage("A precise product render, studio lighting")
    .size(1024, 1024)
    .steps(8)
    .cfg(1.0f)
    .run();
```

Call `sd.update()` in the app loop, wait for `sd.isReady()`, then drain results with `sd.pollResult(result)`.

Advanced users can force `settings.executionMode = tcx::sd::ExecutionMode::PersistentServer`, `CliProcess`, or `InProcess`. Normal C++ app usage does not require Python; Python scripts are setup, verification, and optional Node-adjacent tooling only.

## Script And Node Jobs

Use the tracked JSON job file as the first automation/Node integration surface:

```powershell
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\sd15_controlnet_canny_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\sd15_controlnet_canny_job.json
python tools\tcxsd_job.py run examples\flux2-klein-basic\jobs\flux2_klein_product_job.json
python tools\tcxsd_job.py run examples\z-image-basic\jobs\z_image_turbo_wide_job.json
```

Generation writes a PNG, backend log (`sd-server` or `sd-cli`), and JSON sidecar. Inspect sidecars with:

```powershell
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\jobs\ideogram4_poster_job.json --json
```

The first pure Node package lives in `node/` and talks directly to `sd-server.exe` without Python:

```powershell
cd node
npm test
node .\bin\tcxsd-node.mjs --job ..\examples\flux2-klein-basic\jobs\flux2_klein_product_job.json
node .\bin\tcxsd-node.mjs --prompt "一张中文海报，文字写着本地生图" --quality draft --runtime-preset lowVram --sidecar .\outputs\cn.json
node .\bin\tcxsd-node.mjs --model sd15-controlnet-canny --mode controlNet --control-image ..\examples\ideogram4-basic\inputs\control_canny.png --prompt "golden product workstation following the canny guide" --quality draft --runtime-preset lowVram
```

The Node package exports TypeScript declarations, structured `TcxSdError` errors with remediation hints, sidecar writing, task cancellation through `/cancel`, reusable `TcxSdServerSession`, `GenerationSession`/project/artifact/batch/variant workflow objects, storage cleanup helpers, prompt packs, and quality checks.

For script workflows:

```powershell
python tools\tcxsd_storage.py roots --output-root .\outputs --temp-root .\tmp --cache-root .\cache
python tools\tcxsd_storage.py cleanup --output-root .\outputs --temp-root .\tmp --cache-root .\cache --older-than-seconds 86400
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\jobs\ideogram4_poster_job.json --json
```

Detailed docs:

- `docs/API_USAGE.md`
- `docs/ISSUES.md`
- `docs/PROGRESS.md`
- `docs/HANDOFF.md`
