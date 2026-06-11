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
  sd15-controlnet-canny/
    v1-5-pruned-emaonly.safetensors
    control_v11p_sd15_canny_fp16.safetensors
```

Use the helper script:

```powershell
python tools\setup_sd.py download-model --model ideogram4-q4_0
python tools\setup_sd.py download-model --model sd15-controlnet-canny
```

If download fails 3 times, the script prints exact manual URLs and the target directory. Python is only used here for setup and verification; published C++ and Node runtimes consume the native binaries and model files directly.

Other built-in starter profiles:

```cpp
sd.setupFlux2KleinAsync("bin/data/models/flux2-klein-4b-q4_0", tcx::sd::RuntimeSettings::windowsCuda());
sd.setupZImageTurboAsync("bin/data/models/z-image-turbo-q3_k", tcx::sd::RuntimeSettings::windowsCuda());
```

The SD 1.5 ControlNet Canny assets are shared with the Node CLI and JSON job examples. The C++ workbench currently returns a structured `BACKEND_UNSUPPORTED` setup error for that profile instead of entering the native Windows ControlNet startup path that was observed to access-violate during GUI smoke testing.

Download their assets with:

```powershell
python tools\setup_sd.py download-model --model flux2-klein-4b-q4_0
python tools\setup_sd.py download-model --model z-image-turbo-q3_k
python tools\setup_sd.py download-model --model sd15-controlnet-canny
```

Their starter JSON jobs live in:

```text
examples/flux2-klein-basic/jobs/flux2_klein_product_job.json
examples/z-image-basic/jobs/z_image_turbo_wide_job.json
```

In this workspace the priority text models have been downloaded into the shared bin data folder, verified, and smoke-tested through `persistent_server`. The SD 1.5 ControlNet Canny profile is the real ControlNet backend-parity profile and uses its own local assets under the same model root.

## Multi-Model Example

`examples/ideogram4-basic` is the main multi-model workflow workbench despite the historical folder name. The Chinese GUI exposes:

- model selector for Ideogram4, FLUX.2-klein, Z-Image Turbo, and SD 1.5 ControlNet Canny asset routing,
- workflow selector for text-to-image, image-to-image, inpaint, ControlNet Canny, LoRA stack, refine, and upscale,
- per-model default prompt, negative prompt, size, steps, CFG, and seed,
- explicit project/output/temp/cache roots through `GenerationProject`,
- async model initialization,
- persistent server generation for the C++ text-model profiles,
- per-model output folders under `outputs/<model-id>`.

The GUI loads a CJK-capable ImGui font at startup so Chinese labels render correctly on Windows and macOS. If labels display as `????`, first check whether one of the configured CJK system fonts is available before changing source-file encodings. The current workbench uses labels above full-width controls and a dark charcoal, warm gray, and earthy-gold theme.

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

Model profiles now carry their own generation defaults and runtime presets:

```cpp
auto profile = tcx::sd::ModelProfile::ideogram4();
auto request = profile.request(tcx::sd::Quality::Balanced);
auto lowVram = profile.runtime(tcx::sd::RuntimePreset::LowVram);
auto fullSpeed4090 = profile.runtime(tcx::sd::RuntimePreset::Rtx4090FullSpeed);
```

The built-in profiles are:

- `ideogram4-q4_0`: draft 512x512/8 steps/CFG 7, balanced 1024x1024/20 steps/CFG 7, final 1024x1024/28 steps/CFG 7.
- `flux2-klein-4b-q4_0`: draft 512x512/4 steps/CFG 1, balanced 768x768/6 steps/CFG 1, final 1024x1024/8 steps/CFG 1.
- `z-image-turbo-q3_k`: draft 768x512/4 steps/CFG 1, balanced 1024x512/8 steps/CFG 1, final 1280x768/12 steps/CFG 1.
- `sd15-controlnet-canny`: draft 512x512/12 steps/CFG 7.5, balanced 512x512/20 steps/CFG 7.5, final 768x768/28 steps/CFG 7.5. This profile defaults to `persistent_server` so C++ and Node keep the SD 1.5 base and ControlNet model loaded across jobs.

Legacy helpers remain available:

```cpp
auto cudaSettings = tcx::sd::RuntimeSettings::windowsCuda();
auto lowVramSettings = tcx::sd::RuntimeSettings::lowVramCuda();
auto metalSettings = tcx::sd::RuntimeSettings::macMetal();
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
settings.tempDirectory = "outputs/tmp";
settings.cacheDirectory = "outputs/cache";
settings.processTimeoutSeconds = 300;
```

For `PersistentServer`, `ImageResult::outputPath` is the PNG decoded from `/sdcpp/v1/img_gen`, and `result.metadata["server_log"]` points to the managed server log when the addon launched the server. For `CliProcess`, `ImageResult::outputPath` is the PNG written by `sd-cli`, and `result.metadata["cli_log"]` points to the captured upstream log.

## Image Inputs, ControlNet, LoRA, Refine, And Upscale

Use typed factories when the task mode matters to routing, sidecars, and capability checks:

```cpp
auto control = tcx::StableDiffusionRequest::controlNet(
        "golden product workstation following the canny guide",
        "inputs/control_canny.png",
        0.85f)
    .size(1024, 1024)
    .stepsCount(12);

sd.submit(control);

auto variant = tcx::StableDiffusionRequest::imageToImage(
        "Keep the composition, redesign as a polished product poster",
        "inputs/source.png",
        0.55f)
    .mask("inputs/mask.png")
    .lora("product-style.safetensors", 0.7f);
```

`PersistentServer` supports image inputs, inpaint masks, ControlNet images, per-request LoRA stacks, refine, and upscale request modes through upstream's native HTTP API. `CliProcess` supports image-to-image, mask/inpaint, ControlNet, refine, and upscale through upstream `sd-cli` flags. Direct `InProcess` still returns a structured `BACKEND_UNSUPPORTED` error for image inputs, ControlNet, LoRA, refine, and upscale.

For LoRA, upstream `sd-server` scans `settings.loraModelDirectory`; pass `.lora(...)` paths relative to that folder, or pass absolute paths that live under that folder so the addon can convert them to server-relative names.

The main example includes real local inputs:

```text
examples/ideogram4-basic/inputs/source.png
examples/ideogram4-basic/inputs/mask.png
examples/ideogram4-basic/inputs/control_canny.png
examples/ideogram4-basic/jobs/sd15_controlnet_canny_job.json
```

ControlNet assets live in:

```text
examples/ideogram4-basic/bin/data/models/sd15-controlnet-canny/
  v1-5-pruned-emaonly.safetensors
  control_v11p_sd15_canny_fp16.safetensors
```

## Workflow Objects

Use `GenerationProject` when a tool needs explicit lifecycle roots instead of ad hoc output paths:

```cpp
auto project = tcx::sd::GenerationProject::at("outputs", "poster-batch");
auto settings = project.apply(tcx::sd::ModelProfile::ideogram4().runtime(tcx::sd::RuntimePreset::LowVram));

sd.setupIdeogram4Async("bin/data/models/ideogram4-q4_0", settings);

auto artifact = project.artifact("gold-poster");
sd.submit(tcx::StableDiffusionRequest::textToImage("一张中文海报，文字写着本地生图")
    .output(artifact.imagePath)
    .metadata("project", project.name)
    .metadata("sidecar", artifact.sidecarPath.string()));
```

`GenerationSession` binds a model profile, runtime preset, explicit project roots, resolved model paths, and backend capabilities without starting the runtime itself. `GenerationArtifact` records the image path, sidecar path, parent artifact, source inputs, metadata, and quality report. `BatchJob::seedSweep(...)` and `VariantJob::fromArtifact(...)` create concrete request sets for batch and variant workflows; the Node package exposes the same concepts as `GenerationSession`, `GenerationProject`, `BatchJob`, `createVariantJob(...)`, and `runBatchJob(...)`.

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
Failures also include stable error metadata such as `error_code` and `remediation_hint`/`remediation_hints` for common cases like CUDA OOM, missing model files, server startup failures, backend unsupported requests, cancellation that cannot interrupt an active generation, timeouts, and missing output images.

Inspect a sidecar from scripts or Node-adjacent tooling:

```powershell
python tools\tcxsd_sidecar.py validate examples\ideogram4-basic\outputs\ideogram4_job_1.json --require-success-image
python tools\tcxsd_sidecar.py summary examples\ideogram4-basic\outputs\ideogram4_job_1.json --json
```

Storage cleanup for batch/script workflows:

```powershell
python tools\tcxsd_storage.py roots --output-root .\outputs --temp-root .\tmp --cache-root .\cache
python tools\tcxsd_storage.py cleanup --output-root .\outputs --temp-root .\tmp --cache-root .\cache --older-than-seconds 86400
```

Quality checks are available from Python and Node. They flag placeholder prompts, invalid/tiny output files, size mismatches, and text-check failures or missing verification:

```python
import tcxsd_quality
report = tcxsd_quality.assess_sidecar(sidecar_json)
```

The prompt pack helpers preserve Chinese prompt text as UTF-8:

```python
import tcxsd_prompts
packed = tcxsd_prompts.ideogram4_poster("一张展示本地 AI 生图工作流的中文海报", "本地生图", language="zh")
```

## Node Package

```js
import {
  TcxSdError,
  createBatchJob,
  createControlNetRequest,
  createGenerationSession,
  createGenerationProject,
  createServerSession,
  runBatchJob,
  promptPacks,
  runTextToImage
} from "@trussc/tcx-stable-diffusion";

const packed = promptPacks.ideogram4Poster({
  subject: "一张展示本地 AI 生图工作流的中文海报",
  visibleText: "本地生图",
  language: "zh"
});

const session = await createServerSession({ model: "ideogram4-q4_0", runtimePreset: "lowVram" }).start();
try {
  await session.generate({
    prompt: JSON.stringify(packed.prompt_json),
    negativePrompt: packed.negative_prompt,
    quality: "draft",
    output: "outputs/chinese-poster.png"
  });
} catch (error) {
  if (error instanceof TcxSdError) {
    console.error(error.code, error.remediationHints);
  }
} finally {
  session.close();
}
```

ControlNet and batch workflows use the same Node runtime path:

```js
const project = createGenerationProject({ root: "../examples/ideogram4-basic/outputs", name: "node-controlnet" });

await runTextToImage({
  ...createControlNetRequest({
    model: "sd15-controlnet-canny",
    prompt: "golden product workstation following the canny guide",
    controlImage: "../examples/ideogram4-basic/inputs/control_canny.png",
    controlStrength: 0.85,
    quality: "draft"
  }),
  project,
  output: project.outputPath("controlnet-canny")
});

const batch = createBatchJob({
  baseRequest: {
    model: "flux2-klein-4b-q4_0",
    prompt: "compact local generation UI in dark gold theme",
    quality: "draft"
  }
}).seedSweep([101, 102, 103]);

await runBatchJob(batch, { project });

const sessionContext = createGenerationSession({ model: "sd15-controlnet-canny", runtimePreset: "lowVram", project });
const draftRequest = sessionContext.request("draft", { prompt: "一张中文海报，文字写着本地生图" });
```

## JSON Job Runner

For Node or automation workflows, start with a JSON job file and let `tools/tcxsd_job.py` call the bundled runtime:

```powershell
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py validate examples\ideogram4-basic\jobs\sd15_controlnet_canny_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\ideogram4_poster_job.json
python tools\tcxsd_job.py run examples\ideogram4-basic\jobs\sd15_controlnet_canny_job.json
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
node .\bin\tcxsd-node.mjs --model sd15-controlnet-canny --mode controlNet --control-image ..\examples\ideogram4-basic\inputs\control_canny.png --prompt "golden product workstation following the canny guide" --quality draft --runtime-preset lowVram
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

## Committed Extended Surface

These fields are now part of the public request shape. `PersistentServer` wires image inputs, ControlNet, LoRA, refine, and upscale through the server API. `CliProcess` wires image inputs, masks, ControlNet, refine, and upscale through `sd-cli`. Unsupported backend combinations return `BACKEND_UNSUPPORTED` with remediation hints instead of silently dropping fields:

- `ImageRequest::initImage` for image-to-image
- `ImageRequest::maskImage` for inpainting
- `ImageRequest::controlImage` and `controlStrength` for ControlNet
- `ImageRequest::loras` for LoRA stacks
- `ImageRequest::refineSourceImage` and `upscaleFactor` for refine/upscale

These model path fields remain reserved for broader model families:

- `ModelPaths::audioVae` for audio/video model families
- `ModelPaths::highNoiseDiffusionModel` for two-stage/video pipelines
