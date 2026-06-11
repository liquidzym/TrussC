# tcxStableDiffusion API Usage

## Minimal Image Generation

```cpp
#include <tcxStableDiffusion.h>

tcx::StableDiffusion sd;

void setup() {
    auto settings = tcx::sd::RuntimeSettings::windowsCuda();
    bool ok = sd.setupIdeogram4("models", settings);
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
    sd.createImage("A calm architectural studio, clean light, high detail")
        .size(1024, 1024)
        .steps(8)
        .cfg(1.0f)
        .run();
}
```

On Windows CUDA, `RuntimeSettings::windowsCuda()` leaves `executionMode` as `Auto`. `Auto` selects the isolated `sd-cli` process backend when `sd-cli.exe` is available, then loads the generated PNG back into `tc::Pixels`. The high-level API stays the same.

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

The composer returns plain strings under the hood, so it stays compatible with `CliProcess`, `InProcess`, and future persistent or Node-facing backends.

## Model Initialization

First example model layout:

```text
examples/ideogram4-basic/models/
  ideogram4-Q4_0.gguf
  ideogram4_uncond-Q4_0.gguf
  Qwen3VL-8B-Instruct-Q4_K_M.gguf
  flux2_ae.safetensors
```

Use the helper script:

```powershell
python tools\setup_sd.py download-model --model ideogram4-q4_0
```

If download fails 3 times, the script prints exact manual URLs and the target directory.

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
settings.executionMode = tcx::sd::ExecutionMode::Auto;       // Windows CUDA uses sd-cli if present.
settings.executionMode = tcx::sd::ExecutionMode::CliProcess; // Force process isolation.
settings.executionMode = tcx::sd::ExecutionMode::InProcess;  // Force direct stable-diffusion.cpp API.
```

Optional CLI controls:

```cpp
settings.cliExecutable = "path/to/sd-cli.exe";
settings.cliWorkDir = "path/to/runtime/bin";
settings.outputDirectory = "outputs/native";
settings.processTimeoutSeconds = 300;
```

For `CliProcess`, `ImageResult::outputPath` is the PNG written by `sd-cli`, and `result.metadata["cli_log"]` points to the captured upstream log.

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

For Node or automation workflows, start with a JSON job file and let `tools/tcxsd_job.py` call the bundled `sd-cli`:

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

## Explicit Model Paths

```cpp
tcx::sd::ModelPaths paths;
paths.diffusionModel = "models/ideogram4-Q4_0.gguf";
paths.unconditionalDiffusionModel = "models/ideogram4_uncond-Q4_0.gguf";
paths.llm = "models/Qwen3VL-8B-Instruct-Q4_K_M.gguf";
paths.vae = "models/flux2_ae.safetensors";

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

The first implementation is txt2img-oriented. These fields are already reserved for upcoming passes:

- `ImageRequest::initImage` for image-to-image
- `ImageRequest::maskImage` for inpainting
- `ImageRequest::controlImage` and `controlStrength` for ControlNet
- `ImageRequest::loras` for LoRA stacks
- `ModelPaths::audioVae` for audio/video model families
- `ModelPaths::highNoiseDiffusionModel` for two-stage/video pipelines
