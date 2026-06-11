# tcxStableDiffusion API Usage

## Minimal Image Generation

```cpp
#include <tcxStableDiffusion.h>

tcx::StableDiffusion sd;

void setup() {
    auto settings = tcx::sd::RuntimeSettings::windowsCuda();
    bool ok = sd.setupIdeogram4Async("bin/data/models/ideogram4-q4_0", settings);
    if (!ok) {
        tc::logError("sd") << sd.lastError();
    }
}

void update() {
    sd.update();

    tcx::StableDiffusionImage result;
    while (sd.pollResult(result)) {
        if (result.hasImage()) {
            result.saveWithMetadata("outputs/latest.png");
        }
    }
}

void generate() {
    if (!sd.isReady()) {
        return;
    }

    sd.createImage("A calm architectural studio, clean light, high detail")
        .size(1024, 1024)
        .steps(8)
        .cfg(1.0f)
        .run();
}
```

On Windows CUDA, `RuntimeSettings::windowsCuda()` leaves `executionMode` as `Auto`. `Auto` selects the pure C++ persistent `sd-server.exe` backend when it is available, then falls back to the isolated `sd-cli` process backend. The high-level API stays the same and normal C++ apps do not need Python at runtime.

## Ideogram4 Prompt Composer

Use `IdeogramPrompt` when you want a designer-friendly API that emits the structured JSON prompt style used by upstream Ideogram4 examples:

```cpp
auto prompt = tcx::IdeogramPrompt::poster(
        "A clean futuristic product poster for tcxStableDiffusion, a local AI image addon")
    .text("tcxStableDiffusion")
    .styleDescription("premium technical product poster, refined typography, elegant studio lighting")
    .compositionDescription("upright poster layout with a clear title zone and balanced interface details")
    .palette({"#F7F4EC", "#111111", "#2F80ED", "#27AE60", "#FFFFFF"});

sd.createImage(prompt)
    .size(1024, 1024)
    .steps(12)
    .cfg(1.0f)
    .run();
```

Lower-level request construction is also available:

```cpp
auto request = tcx::StableDiffusionRequest::fromIdeogram4(prompt)
    .size(1024, 1024)
    .stepsCount(12);

sd.submit(request);
```

The composer returns plain strings under the hood, so it stays compatible with `PersistentServer`, `CliProcess`, `InProcess`, and future Node-facing tooling.

## Model Initialization

The main example keeps all priority models in one shared bin data folder:

```text
examples/ideogram4-basic/bin/data/models/
  ideogram4-q4_0/
    ideogram4-Q4_0.gguf
    ideogram4_uncond-Q4_0.gguf
    Qwen3VL-8B-Instruct-Q4_K_M.gguf
    flux2_ae.safetensors
  flux2-klein-4b-q4_0/
    flux-2-klein-4b-Q4_0.gguf
    Qwen3-4B-Q4_K_M.gguf
    flux2_ae.safetensors
  z-image-turbo-q3_k/
    z_image_turbo-Q3_K.gguf
    Qwen3-4B-Instruct-2507-Q4_K_M.gguf
    z_image_ae.safetensors
```

Use the helper script:

```powershell
python tools\setup_sd.py download-model --model ideogram4-q4_0
```

If download fails 3 times, the script prints exact manual URLs and the target directory.

Other built-in starter profiles:

```cpp
sd.setupFlux2KleinAsync("bin/data/models/flux2-klein-4b-q4_0", tcx::sd::RuntimeSettings::windowsCuda());
sd.setupZImageTurboAsync("bin/data/models/z-image-turbo-q3_k", tcx::sd::RuntimeSettings::windowsCuda());
```

Download their assets with:

```powershell
python tools\setup_sd.py download-model --model flux2-klein-4b-q4_0
python tools\setup_sd.py download-model --model z-image-turbo-q3_k
```

Their starter JSON jobs live in:

```text
examples/flux2-klein-basic/jobs/flux2_klein_product_job.json
examples/z-image-basic/jobs/z_image_turbo_wide_job.json
```

In this workspace all three priority models have been downloaded into the shared bin data folder, verified, and smoke-tested through `persistent_server`.

## Multi-Model Example

`examples/ideogram4-basic` is the main multi-model workbench despite the historical folder name. The Chinese GUI exposes:

- model selector for Ideogram4, FLUX.2-klein, and Z-Image Turbo,
- per-model default prompt, negative prompt, size, steps, CFG, and seed,
- async model initialization,
- persistent server generation,
- per-model output folders under `outputs/<model-id>`.

The GUI loads a CJK-capable ImGui font at startup so Chinese labels render correctly on Windows and macOS. If labels display as `????`, first check whether one of the configured CJK system fonts is available before changing source-file encodings. The current workbench uses labels above full-width controls and a modern dark neutral theme with blue/teal accents.

## Building The Example

Debug build:

```powershell
cd examples\ideogram4-basic
G:\TrussC\tools\bin\trusscli.exe build
```

Release build:

```powershell
cd examples\ideogram4-basic
G:\TrussC\tools\bin\trusscli.exe build --release
```

On Windows, the generated preset currently uses Ninja. `trusscli build --release` reconfigures single-config presets with `CMAKE_BUILD_TYPE=Release` before building, then still passes `--config Release` for compatibility with multi-config generators.

## Runtime Profiles

Windows CUDA default for RTX 4090:

```cpp
auto settings = tcx::sd::RuntimeSettings::windowsCuda();
settings.backendAssignment = "cuda0";
settings.paramsBackendAssignment = "cuda0";
settings.diffusionFlashAttention = true;
```

Safer Ideogram4 profile, matching the upstream `--offload-to-cpu` workflow:

```cpp
auto settings = tcx::sd::RuntimeSettings::lowVramCuda();
```

macOS profile:

```cpp
auto settings = tcx::sd::RuntimeSettings::macMetal();
```

## Execution Modes

```cpp
auto settings = tcx::sd::RuntimeSettings::windowsCuda();
settings.executionMode = tcx::sd::ExecutionMode::Auto;       // Windows CUDA prefers persistent sd-server, then CLI fallback.
settings.executionMode = tcx::sd::ExecutionMode::PersistentServer; // Force managed sd-server.exe.
settings.executionMode = tcx::sd::ExecutionMode::CliProcess; // Force process isolation.
settings.executionMode = tcx::sd::ExecutionMode::InProcess;  // Force direct stable-diffusion.cpp API.
```

Optional persistent server controls:

```cpp
settings.serverExecutable = "path/to/sd-server.exe";
settings.serverHost = "127.0.0.1";
settings.serverPort = 1234;
settings.serverStartupTimeoutSeconds = 120;
settings.serverPollIntervalMs = 500;
settings.serverReuseExisting = false;
settings.keepServerRunning = false;
settings.loraModelDirectory = "loras";
settings.hiresUpscalersDirectory = "upscalers";
```

Optional CLI controls:

```cpp
settings.cliExecutable = "path/to/sd-cli.exe";
settings.cliWorkDir = "path/to/runtime/bin";
settings.outputDirectory = "outputs/native";
settings.processTimeoutSeconds = 300;
```

For `PersistentServer`, `ImageResult::outputPath` is the PNG decoded from `/sdcpp/v1/img_gen`, and `result.metadata["server_log"]` points to the managed server log when the addon launched the server. For `CliProcess`, `ImageResult::outputPath` is the PNG written by `sd-cli`, and `result.metadata["cli_log"]` points to the captured upstream log.

## Image Inputs, ControlNet, And LoRA

The persistent server backend supports image inputs through upstream's native HTTP API:

```cpp
sd.createImage("Keep the composition, redesign as a polished product poster")
    .imageToImage("inputs/sketch.png", 0.55f)
    .mask("inputs/mask.png")
    .control("inputs/pose_or_depth.png", 0.8f)
    .lora("loras/product-style.safetensors", 0.7f)
    .size(1024, 1024)
    .steps(12)
    .run();
```

`CliProcess` and direct `InProcess` still reject these fields until their image-loading paths are implemented. Use `PersistentServer` for these extended workflows on Windows.

For LoRA, upstream `sd-server` scans `settings.loraModelDirectory`; pass `.lora(...)` paths relative to that folder, or pass absolute paths that live under that folder so the addon can convert them to server-relative names.

## Result Metadata Sidecars

Use `saveWithMetadata()` when you want a PNG and a replay/debug JSON sidecar next to it:

```cpp
tcx::StableDiffusionImage result;
while (sd.pollResult(result)) {
    if (result.ok && result.hasImage()) {
        result.saveWithMetadata("outputs/poster.png");
        // Writes:
        // outputs/poster.png
        // outputs/poster.json
    } else {
        result.saveMetadata("outputs/poster_failed.json");
    }
}
```

The sidecar includes job id, state, error text, duration, image dimensions, saved image path, native output path, prompt, seed, steps, CFG, execution mode, backend settings, model paths, and the `sd-cli` log path when using `CliProcess`.

Inspect a sidecar from scripts or Node-adjacent tooling:

```powershell
python tools\tcxsd_sidecar.py validate examples\ideogram4-basic\outputs\ideogram4_job_1.json --require-success-image
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\ideogram4_job_1.json --json
```

## JSON Job Runner

For Node or automation workflows, start with a JSON job file and let `tools/tcxsd_job.py` call the bundled runtime:

```powershell
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\ideogram4_poster_job.json
```

The runner writes:

```text
examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.png
examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.json
examples/ideogram4-basic/outputs/jobs/ideogram4_poster_job.log
```

The JSON request can contain either a plain `prompt` string or a structured `prompt_json` object. Relative paths are resolved from the job file location, so the same job can be launched from C++, PowerShell, Node, CI, or another working directory.

Set `runtime.execution_mode` to `persistent_server` in JSON jobs to use the same persistent backend as C++ `Auto`.

## Node Package

The initial Node-facing package lives in `node/`. It does not call Python; it resolves the shared `bin/data/models/<model-id>` layout, starts `sd-server.exe`, submits `/sdcpp/v1/img_gen`, polls status, and writes PNG output.

```powershell
cd node
npm test
node .\bin\tcxsd-node.mjs --job ..\examples\z-image-basic\jobs\z_image_turbo_wide_job.json
```

Programmatic use:

```js
import { runTextToImage } from "@trussc/tcx-stable-diffusion";

await runTextToImage({
  model: "flux2-klein-4b-q4_0",
  prompt: "A polished local AI image tool interface",
  output: "outputs/node/flux.png",
  backend: "cuda0",
  paramsBackend: "cpu",
  offloadToCpu: true
});
```

## Explicit Model Paths

```cpp
tcx::sd::ModelPaths paths;
paths.diffusionModel = "bin/data/models/ideogram4-q4_0/ideogram4-Q4_0.gguf";
paths.unconditionalDiffusionModel = "bin/data/models/ideogram4-q4_0/ideogram4_uncond-Q4_0.gguf";
paths.llm = "bin/data/models/ideogram4-q4_0/Qwen3VL-8B-Instruct-Q4_K_M.gguf";
paths.vae = "bin/data/models/ideogram4-q4_0/flux2_ae.safetensors";

sd.setup(paths, tcx::sd::RuntimeSettings::windowsCuda());
```

## Progress And Results

Callbacks are delivered from `sd.update()`, so they are safe for UI state.

```cpp
sd.onProgress([](const tcx::sd::Progress& p) {
    tc::logNotice("sd") << p.step << "/" << p.totalSteps;
});

sd.onResult([](const tcx::StableDiffusionImage& result) {
    if (!result.ok) {
        tc::logError("sd") << result.error;
    }
});
```

Keep calling `pollResult()` to take ownership of generated `Pixels`.

## Main Thread Rule

The addon worker returns CPU `tc::Pixels`. Convert to `tc::Image` or `tc::Texture` only on the main thread:

```cpp
tc::Image image;
image.allocate(result.pixels.getWidth(), result.pixels.getHeight(), result.pixels.getChannels());
std::memcpy(image.getPixelsData(), result.pixels.getData(), result.pixels.getTotalBytes());
image.setDirty();
image.update();
```

## Future API Surface

These fields are now part of the public request shape. `PersistentServer` wires the image and LoRA fields first; the lower-level direct/CLI paths remain future work:

- `ImageRequest::initImage` for image-to-image
- `ImageRequest::maskImage` for inpainting
- `ImageRequest::controlImage` and `controlStrength` for ControlNet
- `ImageRequest::loras` for LoRA stacks
- `ModelPaths::audioVae` for audio/video model families
- `ModelPaths::highNoiseDiffusionModel` for two-stage/video pipelines
