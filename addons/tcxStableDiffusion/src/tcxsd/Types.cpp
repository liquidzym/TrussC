#include "tcxsd/Types.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace tcx::sd {

namespace {

std::string escapeJson(const std::string& text) {
    std::ostringstream out;
    for (unsigned char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u"
                        << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c)
                        << std::nouppercase << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
                break;
        }
    }
    return out.str();
}

void appendJsonStringField(std::ostream& out, bool& first, const std::string& key, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (!first) {
        out << ",";
    }
    out << "\"" << escapeJson(key) << "\":\"" << escapeJson(value) << "\"";
    first = false;
}

void appendJsonStringArrayField(std::ostream& out, bool& first, const std::string& key, const std::vector<std::string>& values) {
    if (values.empty()) {
        return;
    }
    if (!first) {
        out << ",";
    }
    out << "\"" << escapeJson(key) << "\":[";
    bool firstValue = true;
    for (const auto& value : values) {
        if (value.empty()) {
            continue;
        }
        if (!firstValue) {
            out << ",";
        }
        out << "\"" << escapeJson(value) << "\"";
        firstValue = false;
    }
    out << "]";
    first = false;
}

void appendJsonRawField(std::ostream& out, bool& first, const std::string& key, const std::string& value) {
    if (!first) {
        out << ",";
    }
    out << "\"" << escapeJson(key) << "\":" << value;
    first = false;
}

void appendJsonBoolField(std::ostream& out, bool& first, const std::string& key, bool value) {
    appendJsonRawField(out, first, key, value ? "true" : "false");
}

void appendJsonNumberField(std::ostream& out, bool& first, const std::string& key, double value) {
    std::ostringstream valueOut;
    valueOut << std::fixed << std::setprecision(3) << value;
    appendJsonRawField(out, first, key, valueOut.str());
}

std::string nowIsoLocal() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

fs::path defaultMetadataPathForImage(fs::path imagePath) {
    imagePath.replace_extension(".json");
    return imagePath;
}

std::string defaultStyle(IdeogramPromptKind kind) {
    switch (kind) {
        case IdeogramPromptKind::Poster:
            return "premium editorial poster design, crisp layout, refined typography, high-end visual direction";
        case IdeogramPromptKind::Product:
            return "clean product advertising, precise industrial design language, polished commercial photography";
        case IdeogramPromptKind::Typography:
            return "typography-led graphic design, readable lettering, strong hierarchy, precise spacing";
        case IdeogramPromptKind::Logo:
            return "minimal identity design, balanced mark, clean vector-like edges, professional branding";
        case IdeogramPromptKind::Illustration:
            return "high-detail illustration, coherent shape language, polished color design, expressive composition";
        case IdeogramPromptKind::General:
            return "detailed image generation prompt, coherent visual structure, polished rendering";
    }
    return "polished image generation prompt";
}

std::string defaultComposition(IdeogramPromptKind kind) {
    switch (kind) {
        case IdeogramPromptKind::Poster:
            return "upright poster layout with clear foreground, middle ground, and background; readable horizontal text; balanced margins";
        case IdeogramPromptKind::Product:
            return "centered hero product composition with generous negative space and clean supporting details";
        case IdeogramPromptKind::Typography:
            return "typographic composition with the main text as the focal point, horizontal baseline, and controlled spacing";
        case IdeogramPromptKind::Logo:
            return "centered logo presentation on a simple background with strong silhouette and no extra marks";
        case IdeogramPromptKind::Illustration:
            return "clear illustrative scene composition with readable subject relationships and controlled visual density";
        case IdeogramPromptKind::General:
            return "well-composed image with clear subject priority and no clutter";
    }
    return "well-composed image with clear subject priority";
}

std::vector<std::string> defaultPalette(IdeogramPromptKind kind) {
    switch (kind) {
        case IdeogramPromptKind::Poster:
            return {"#F4F1EA", "#111111", "#3B82F6", "#E2B714", "#FFFFFF"};
        case IdeogramPromptKind::Product:
            return {"#F7F7F2", "#151515", "#B9C2C9", "#2F80ED", "#FFFFFF"};
        case IdeogramPromptKind::Typography:
            return {"#FFFFFF", "#111111", "#E63946", "#457B9D"};
        case IdeogramPromptKind::Logo:
            return {"#FFFFFF", "#0F172A", "#22C55E"};
        case IdeogramPromptKind::Illustration:
            return {"#F8FAFC", "#1F2937", "#F59E0B", "#10B981", "#6366F1"};
        case IdeogramPromptKind::General:
            return {"#FFFFFF", "#111111", "#8AB4F8", "#F2C94C"};
    }
    return {};
}

std::vector<IdeogramPromptElement> defaultElements(const IdeogramPrompt& prompt) {
    std::vector<IdeogramPromptElement> elements = prompt.elements;
    if (!prompt.subject.empty()) {
        elements.push_back({"obj", "Main subject: " + prompt.subject + ". Keep it visually dominant, coherent, and not duplicated."});
    }
    if (!prompt.visibleText.empty()) {
        std::string textRule = "Print the exact text \"" + prompt.visibleText + "\"";
        if (prompt.uprightText) {
            textRule += " horizontally, upright, and readable from left to right";
        }
        if (prompt.preserveText) {
            textRule += ". The spelling must be exact, with no missing, extra, mirrored, rotated, or distorted characters";
        }
        elements.push_back({"text", textRule + "."});
    }
    if (elements.empty()) {
        elements.push_back({"obj", "A clear primary subject with coherent shape, material, color, and spatial relationships."});
    }
    return elements;
}

} // namespace

ModelPaths ModelPaths::ideogram4Example(const fs::path& modelDir) {
    ModelPaths paths;
    paths.diffusionModel = modelDir / "ideogram4-Q4_0.gguf";
    paths.unconditionalDiffusionModel = modelDir / "ideogram4_uncond-Q4_0.gguf";
    paths.llm = modelDir / "Qwen3VL-8B-Instruct-Q4_K_M.gguf";
    paths.vae = modelDir / "flux2_ae.safetensors";
    return paths;
}

ModelPaths ModelPaths::flux2KleinExample(const fs::path& modelDir) {
    ModelPaths paths;
    paths.diffusionModel = modelDir / "flux-2-klein-4b-Q4_0.gguf";
    paths.llm = modelDir / "Qwen3-4B-Q4_K_M.gguf";
    paths.vae = modelDir / "flux2_ae.safetensors";
    return paths;
}

ModelPaths ModelPaths::zImageTurboExample(const fs::path& modelDir) {
    ModelPaths paths;
    paths.diffusionModel = modelDir / "z_image_turbo-Q3_K.gguf";
    paths.llm = modelDir / "Qwen3-4B-Instruct-2507-Q4_K_M.gguf";
    paths.vae = modelDir / "z_image_ae.safetensors";
    return paths;
}

ModelPaths ModelPaths::sd15ControlNetCannyExample(const fs::path& modelDir) {
    ModelPaths paths;
    paths.model = modelDir / "v1-5-pruned-emaonly.safetensors";
    paths.controlNet = modelDir / "control_v11p_sd15_canny_fp16.safetensors";
    return paths;
}

bool ModelPaths::hasImagePipeline() const {
    return !model.empty() || !diffusionModel.empty();
}

RuntimeSettings RuntimeSettings::windowsCuda() {
    RuntimeSettings settings;
    settings.backend = Backend::Cuda;
    settings.executionMode = ExecutionMode::Auto;
    settings.backendAssignment = "cuda0";
    settings.paramsBackendAssignment = "cuda0";
    settings.offloadParamsToCpu = false;
    settings.keepTextEncoderOnCpu = false;
    settings.keepVaeOnCpu = false;
    settings.diffusionFlashAttention = true;
    settings.diffusionConvDirect = false;
    settings.vaeConvDirect = false;
    settings.keepModelLoaded = true;
    settings.serverHost = "127.0.0.1";
    settings.serverPort = 1234;
    settings.serverStartupTimeoutSeconds = 120;
    settings.serverPollIntervalMs = 500;
    return settings;
}

RuntimeSettings RuntimeSettings::lowVramCuda() {
    RuntimeSettings settings = RuntimeSettings::windowsCuda();
    settings.offloadParamsToCpu = true;
    settings.backendAssignment = "cuda0,te=cpu";
    settings.paramsBackendAssignment = "cpu";
    settings.keepTextEncoderOnCpu = true;
    settings.keepVaeOnCpu = false;
    settings.maxVramGiB = 8.0f;
    settings.streamLayers = true;
    return settings;
}

RuntimeSettings RuntimeSettings::macMetal() {
    RuntimeSettings settings;
    settings.backend = Backend::Metal;
    settings.backendAssignment = "metal";
    settings.paramsBackendAssignment = "metal";
    settings.diffusionFlashAttention = true;
    return settings;
}

QualityDefaults ModelProfile::defaults(Quality quality) const {
    switch (quality) {
        case Quality::Draft: return draft;
        case Quality::Balanced: return balanced;
        case Quality::Final: return final;
    }
    return balanced;
}

RuntimeSettings ModelProfile::runtime(RuntimePreset preset) const {
    RuntimeSettings settings = RuntimeSettings::windowsCuda();
    settings.executionMode = ExecutionMode::PersistentServer;

    if (id == "ideogram4-q4_0") {
        if (preset == RuntimePreset::Rtx4090FullSpeed) {
            settings.backendAssignment = "cuda0";
            settings.paramsBackendAssignment = "cuda0";
            settings.offloadParamsToCpu = false;
            settings.keepTextEncoderOnCpu = false;
            settings.maxVramGiB = 0.0f;
            settings.streamLayers = false;
        } else {
            settings.backendAssignment = "cuda0,te=cpu";
            settings.paramsBackendAssignment = "cpu";
            settings.offloadParamsToCpu = true;
            settings.keepTextEncoderOnCpu = true;
            settings.maxVramGiB = 8.0f;
            settings.streamLayers = true;
        }
        return settings;
    }

    if (id == "flux2-klein-4b-q4_0") {
        if (preset == RuntimePreset::LowVram) {
            settings.backendAssignment = "cuda0,te=cpu";
            settings.paramsBackendAssignment = "cpu";
            settings.offloadParamsToCpu = true;
            settings.keepTextEncoderOnCpu = true;
            settings.maxVramGiB = 6.0f;
            settings.streamLayers = true;
        } else if (preset == RuntimePreset::Rtx4090FullSpeed) {
            settings.backendAssignment = "cuda0";
            settings.paramsBackendAssignment = "cuda0";
            settings.offloadParamsToCpu = false;
            settings.maxVramGiB = 0.0f;
            settings.streamLayers = false;
        } else {
            settings.backendAssignment = "cuda0";
            settings.paramsBackendAssignment = "cpu";
            settings.offloadParamsToCpu = true;
            settings.maxVramGiB = 0.0f;
            settings.streamLayers = false;
        }
        return settings;
    }

    if (id == "z-image-turbo-q3_k") {
        if (preset == RuntimePreset::Rtx4090FullSpeed) {
            settings.backendAssignment = "cuda0";
            settings.paramsBackendAssignment = "cuda0";
            settings.offloadParamsToCpu = false;
            settings.maxVramGiB = 0.0f;
            settings.streamLayers = false;
        } else {
            settings.backendAssignment = "cuda0,te=cpu";
            settings.paramsBackendAssignment = "cpu";
            settings.offloadParamsToCpu = true;
            settings.keepTextEncoderOnCpu = true;
            settings.maxVramGiB = preset == RuntimePreset::LowVram ? 6.0f : 8.0f;
            settings.streamLayers = true;
        }
        return settings;
    }

    if (id == "sd15-controlnet-canny") {
        settings.executionMode = ExecutionMode::PersistentServer;
        settings.backendAssignment = "cuda0";
        settings.paramsBackendAssignment = preset == RuntimePreset::Rtx4090FullSpeed ? "cuda0" : "cpu";
        settings.offloadParamsToCpu = preset != RuntimePreset::Rtx4090FullSpeed;
        settings.keepControlNetOnCpu = preset == RuntimePreset::LowVram;
        settings.maxVramGiB = preset == RuntimePreset::LowVram ? 6.0f : 0.0f;
        settings.streamLayers = preset == RuntimePreset::LowVram;
        settings.diffusionFlashAttention = true;
        return settings;
    }

    return settings;
}

ImageRequest ModelProfile::request(Quality quality) const {
    ImageRequest request;
    const QualityDefaults values = defaults(quality);
    request.width = values.width;
    request.height = values.height;
    request.steps = values.steps;
    request.cfgScale = values.cfgScale;
    request.sampler = values.sampler;
    request.quality = quality;
    request.metadata["model"] = id;
    request.metadata["model_family"] = family;
    return request;
}

ModelPaths ModelProfile::paths(const fs::path& modelDir) const {
    if (id == "ideogram4-q4_0") {
        return ModelPaths::ideogram4Example(modelDir);
    }
    if (id == "flux2-klein-4b-q4_0") {
        return ModelPaths::flux2KleinExample(modelDir);
    }
    if (id == "z-image-turbo-q3_k") {
        return ModelPaths::zImageTurboExample(modelDir);
    }
    if (id == "sd15-controlnet-canny") {
        return ModelPaths::sd15ControlNetCannyExample(modelDir);
    }
    return {};
}

ModelProfile ModelProfile::ideogram4() {
    return {
        "ideogram4-q4_0",
        "Ideogram4",
        {512, 512, 8, 7.0f, Sampler::Euler},
        {1024, 1024, 20, 7.0f, Sampler::Euler},
        {1024, 1024, 28, 7.0f, Sampler::Euler},
    };
}

ModelProfile ModelProfile::flux2Klein() {
    return {
        "flux2-klein-4b-q4_0",
        "FLUX.2-klein",
        {512, 512, 4, 1.0f, Sampler::Euler},
        {768, 768, 6, 1.0f, Sampler::Euler},
        {1024, 1024, 8, 1.0f, Sampler::Euler},
    };
}

ModelProfile ModelProfile::zImageTurbo() {
    return {
        "z-image-turbo-q3_k",
        "Z-Image",
        {768, 512, 4, 1.0f, Sampler::Euler},
        {1024, 512, 8, 1.0f, Sampler::Euler},
        {1280, 768, 12, 1.0f, Sampler::Euler},
    };
}

ModelProfile ModelProfile::sd15ControlNetCanny() {
    return {
        "sd15-controlnet-canny",
        "SD 1.5 ControlNet Canny",
        {512, 512, 12, 7.5f, Sampler::Euler},
        {512, 512, 20, 7.5f, Sampler::Euler},
        {768, 768, 28, 7.5f, Sampler::Euler},
    };
}

ModelProfile ModelProfile::byId(const std::string& modelId) {
    if (modelId == "ideogram4-q4_0" || modelId == "ideogram4") {
        return ideogram4();
    }
    if (modelId == "flux2-klein-4b-q4_0" || modelId == "flux2-klein" || modelId == "flux2") {
        return flux2Klein();
    }
    if (modelId == "z-image-turbo-q3_k" || modelId == "z-image" || modelId == "zimage") {
        return zImageTurbo();
    }
    if (modelId == "sd15-controlnet-canny" || modelId == "controlnet-canny" || modelId == "sd15-controlnet") {
        return sd15ControlNetCanny();
    }
    return ideogram4();
}

StorageRoots StorageRoots::fromRuntime(const RuntimeSettings& settings) {
    StorageRoots roots;
    roots.outputRoot = settings.outputDirectory;
    if (roots.outputRoot.empty()) {
        roots.outputRoot = "tcxStableDiffusionOutputs";
    }
    roots.tempRoot = settings.tempDirectory.empty() ? roots.outputRoot / "tmp" : settings.tempDirectory;
    roots.cacheRoot = settings.cacheDirectory.empty() ? roots.outputRoot / "cache" : settings.cacheDirectory;
    return roots;
}

CleanupResult cleanupRuntimeStorage(const CleanupOptions& options) {
    CleanupResult result;
    const auto now = fs::file_time_type::clock::now();
    const auto age = std::chrono::seconds(std::max(0, options.olderThanSeconds));
    const bool ignoreAge = options.olderThanSeconds <= 0;
    const std::vector<std::pair<fs::path, std::vector<std::string>>> groups = {
        {options.roots.outputRoot, {".json", ".log"}},
        {options.roots.tempRoot, {".json", ".log", ".tmp", ".part", ".png"}},
        {options.roots.cacheRoot, {".tmp", ".part"}},
    };

    for (const auto& [root, extensions] : groups) {
        if (root.empty()) {
            continue;
        }
        std::error_code ec;
        if (!fs::exists(root, ec)) {
            continue;
        }
        for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
            if (ec || !it->is_regular_file(ec)) {
                continue;
            }
            const fs::path path = it->path();
            const std::string ext = path.extension().string();
            if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) {
                continue;
            }
            const auto modified = fs::last_write_time(path, ec);
            if (ec || (!ignoreAge && modified + age > now)) {
                continue;
            }
            result.removed.push_back(path);
            if (!options.dryRun) {
                fs::remove(path, ec);
                if (ec) {
                    result.errors.push_back(path.string() + ": " + ec.message());
                    ec.clear();
                }
            }
        }
        if (ec) {
            result.errors.push_back(root.string() + ": " + ec.message());
        }
    }
    return result;
}

bool BackendCapabilities::supports(const ImageRequest& request) const {
    if (request.mode == RequestMode::TextToImage && !textToImage) return false;
    if (request.mode == RequestMode::ImageToImage && !imageToImage) return false;
    if (request.mode == RequestMode::Inpaint && !inpaint) return false;
    if (request.mode == RequestMode::ControlNet && !controlNet) return false;
    if (request.mode == RequestMode::LoraStack && !loraStack) return false;
    if (request.mode == RequestMode::Refine && !refine) return false;
    if (request.mode == RequestMode::Upscale && !upscale) return false;
    if (!request.controlImage.empty() && !controlNet) return false;
    if (!request.maskImage.empty() && !inpaint) return false;
    if (!request.initImage.empty() && !imageToImage) return false;
    if (!request.loras.empty() && !loraStack) return false;
    return true;
}

std::string BackendCapabilities::unsupportedReason(const ImageRequest& request, ExecutionMode mode) const {
    if (supports(request)) {
        return {};
    }
    std::string feature = toString(request.mode);
    if (!request.controlImage.empty() && !controlNet) feature = "control_net";
    if (!request.maskImage.empty() && !inpaint) feature = "inpaint";
    if (!request.initImage.empty() && !imageToImage) feature = "image_to_image";
    if (!request.loras.empty() && !loraStack) feature = "lora_stack";
    return "BACKEND_UNSUPPORTED: " + feature + " is not supported by " + toString(mode) + " for this runtime/model. Use a backend and model profile that advertises the capability.";
}

BackendCapabilities BackendCapabilities::forRuntime(const ModelPaths& paths, const RuntimeSettings& settings, ExecutionMode mode) {
    BackendCapabilities caps;
    const bool hasControlNet = !paths.controlNet.empty();
    if (mode == ExecutionMode::InProcess) {
        caps.imageToImage = false;
        caps.inpaint = false;
        caps.controlNet = false;
        caps.loraStack = false;
        caps.refine = false;
        caps.upscale = false;
        return caps;
    }
    if (mode == ExecutionMode::CliProcess) {
        caps.controlNet = hasControlNet;
        caps.loraStack = false;
        caps.refine = true;
        caps.upscale = true;
        return caps;
    }
    if (mode == ExecutionMode::PersistentServer) {
        caps.controlNet = hasControlNet;
        caps.loraStack = !settings.loraModelDirectory.empty();
        return caps;
    }
    caps.controlNet = hasControlNet;
    caps.loraStack = !settings.loraModelDirectory.empty();
    return caps;
}

CanvasDefaults CanvasDefaults::fromPreset(CanvasPreset preset) {
    switch (preset) {
        case CanvasPreset::SquarePreview: return {512, 512, "square_preview"};
        case CanvasPreset::MobilePoster: return {768, 1344, "mobile_poster"};
        case CanvasPreset::WideHero: return {1280, 720, "wide_hero"};
        case CanvasPreset::DesktopWallpaper: return {1536, 864, "desktop_wallpaper"};
        case CanvasPreset::AppIcon: return {1024, 1024, "app_icon"};
    }
    return {1024, 1024, "square_preview"};
}

PromptPack PromptPack::poster(std::string subject, std::string visibleText, TextRenderingPreset textPreset) {
    PromptPack pack;
    pack.prompt = subject.empty() ? "A refined poster design" : std::move(subject);
    pack.prompt += ", premium editorial composition, clear hierarchy, polished lighting, elegant layout";
    pack.negativePrompt = "low quality, blurry, cluttered, watermark, signature";
    pack.metadata["prompt_pack"] = "poster";
    if (!visibleText.empty()) {
        pack.prompt += ". Include exact readable text \"" + visibleText + "\".";
        pack.negativePrompt += ", misspelled text, unreadable text, mirrored text, cropped text";
        pack.metadata["visible_text"] = visibleText;
        pack.metadata["text_rendering_preset"] = toString(textPreset);
    }
    return pack;
}

PromptPack PromptPack::productShot(std::string subject) {
    PromptPack pack;
    pack.prompt = (subject.empty() ? "A product" : std::move(subject)) + ", clean product photography, controlled studio lighting, crisp materials, commercial catalog quality";
    pack.negativePrompt = "low quality, clutter, noisy text, watermark, signature";
    pack.metadata["prompt_pack"] = "product_shot";
    return pack;
}

PromptPack PromptPack::wideScene(std::string subject) {
    PromptPack pack;
    pack.prompt = (subject.empty() ? "A wide cinematic scene" : std::move(subject)) + ", cinematic wide composition, readable subject hierarchy, refined color, atmospheric but clear";
    pack.negativePrompt = "low quality, blurry, distorted perspective, clutter, watermark, signature";
    pack.metadata["prompt_pack"] = "wide_scene";
    return pack;
}

PromptPack PromptPack::gameAsset(std::string subject) {
    PromptPack pack;
    pack.prompt = (subject.empty() ? "A game asset" : std::move(subject)) + ", isolated readable silhouette, production-ready game art, clean shape language, consistent material detail";
    pack.negativePrompt = "low quality, blurry, extra limbs, messy silhouette, watermark, signature";
    pack.metadata["prompt_pack"] = "game_asset";
    return pack;
}

PromptPack PromptPack::uiMockup(std::string subject) {
    PromptPack pack;
    pack.prompt = (subject.empty() ? "A software UI mockup" : std::move(subject)) + ", professional interface mockup, organized panels, readable layout, restrained visual design";
    pack.negativePrompt = "low quality, unreadable text, clutter, distorted UI, watermark, signature";
    pack.metadata["prompt_pack"] = "ui_mockup";
    return pack;
}

GenerationArtifact GenerationArtifact::fromResult(const ImageResult& result, fs::path sidecarPath) {
    GenerationArtifact artifact;
    artifact.id = "job_" + std::to_string(result.jobId);
    artifact.imagePath = result.outputPath;
    artifact.sidecarPath = std::move(sidecarPath);
    artifact.metadata = result.metadata;
    return artifact;
}

GenerationProject GenerationProject::at(const fs::path& rootPath, std::string name) {
    GenerationProject project;
    project.root = rootPath / std::move(name);
    project.outputRoot = project.root / "outputs";
    project.tempRoot = project.root / "tmp";
    project.cacheRoot = project.root / "cache";
    project.logRoot = project.root / "logs";
    project.inputRoot = project.root / "inputs";
    return project;
}

RuntimeSettings GenerationProject::apply(RuntimeSettings settings) const {
    settings.outputDirectory = outputRoot;
    settings.tempDirectory = tempRoot;
    settings.cacheDirectory = cacheRoot;
    return settings;
}

fs::path GenerationProject::outputPath(std::string label, std::string extension) const {
    if (extension.empty() || extension.front() != '.') {
        extension = "." + extension;
    }
    return outputRoot / (std::move(label) + extension);
}

fs::path GenerationProject::sidecarPath(std::string label) const {
    return outputRoot / (std::move(label) + ".json");
}

GenerationArtifact GenerationProject::artifact(std::string label) const {
    GenerationArtifact artifact;
    artifact.id = label;
    artifact.imagePath = outputPath(label);
    artifact.sidecarPath = sidecarPath(std::move(label));
    return artifact;
}

BatchJob& BatchJob::add(ImageRequest request) {
    requests.push_back(std::move(request));
    return *this;
}

BatchJob BatchJob::seedSweep(ImageRequest base, std::vector<std::int64_t> seeds) {
    BatchJob job;
    job.label = "seed_sweep";
    for (std::int64_t seed : seeds) {
        ImageRequest request = base;
        request.seedValue(seed);
        request.metadata["batch_kind"] = "seed_sweep";
        request.metadata["batch_seed"] = std::to_string(seed);
        job.add(std::move(request));
    }
    return job;
}

VariantJob VariantJob::fromArtifact(const GenerationArtifact& artifact, std::string prompt, float strength) {
    VariantJob job;
    job.source = artifact;
    job.request = ImageRequest::imageToImage(std::move(prompt), artifact.imagePath, strength);
    job.request.metadata["variant_source"] = artifact.id;
    if (!artifact.sidecarPath.empty()) {
        job.request.metadata["parent_sidecar_path"] = artifact.sidecarPath.string();
    }
    return job;
}

GenerationSession GenerationSession::forProfile(
    ModelProfile sessionProfile,
    const fs::path& modelDir,
    RuntimePreset preset,
    GenerationProject sessionProject) {
    GenerationSession session;
    session.id = sessionProfile.id;
    session.profile = std::move(sessionProfile);
    session.modelDirectory = modelDir;
    session.runtimePreset = preset;
    if (sessionProject.root.empty()) {
        sessionProject = GenerationProject::at("outputs", session.profile.id);
    }
    session.project = std::move(sessionProject);
    session.paths = session.profile.paths(modelDir);
    session.settings = session.project.apply(session.profile.runtime(preset));
    const ExecutionMode capabilityMode = session.settings.executionMode == ExecutionMode::Auto
        ? ExecutionMode::PersistentServer
        : session.settings.executionMode;
    session.capabilities = BackendCapabilities::forRuntime(session.paths, session.settings, capabilityMode);
    return session;
}

GenerationSession GenerationSession::forModelId(
    const std::string& modelId,
    const fs::path& modelDir,
    RuntimePreset preset,
    GenerationProject project) {
    return forProfile(ModelProfile::byId(modelId), modelDir, preset, std::move(project));
}

RuntimeSettings GenerationSession::appliedSettings() const {
    return project.apply(settings);
}

ImageRequest GenerationSession::request(Quality requestQuality) const {
    ImageRequest imageRequest = profile.request(requestQuality);
    imageRequest.metadata["generation_session"] = id;
    imageRequest.metadata["model_profile"] = profile.id;
    imageRequest.metadata["runtime_preset"] = toString(runtimePreset);
    if (!project.root.empty()) {
        imageRequest.metadata["project_root"] = project.root.string();
    }
    return imageRequest;
}

GenerationArtifact GenerationSession::artifact(std::string label) const {
    return project.artifact(std::move(label));
}

bool GenerationSession::supports(const ImageRequest& imageRequest) const {
    return capabilities.supports(imageRequest);
}

std::string GenerationSession::unsupportedReason(const ImageRequest& imageRequest) const {
    const ExecutionMode capabilityMode = settings.executionMode == ExecutionMode::Auto
        ? ExecutionMode::PersistentServer
        : settings.executionMode;
    return capabilities.unsupportedReason(imageRequest, capabilityMode);
}

IdeogramPrompt IdeogramPrompt::general(std::string subjectText) {
    IdeogramPrompt prompt;
    prompt.kind = IdeogramPromptKind::General;
    prompt.subject = std::move(subjectText);
    return prompt;
}

IdeogramPrompt IdeogramPrompt::poster(std::string subjectText) {
    IdeogramPrompt prompt;
    prompt.kind = IdeogramPromptKind::Poster;
    prompt.subject = std::move(subjectText);
    return prompt;
}

IdeogramPrompt IdeogramPrompt::product(std::string subjectText) {
    IdeogramPrompt prompt;
    prompt.kind = IdeogramPromptKind::Product;
    prompt.subject = std::move(subjectText);
    return prompt;
}

IdeogramPrompt IdeogramPrompt::typography(std::string subjectText, std::string visibleText) {
    IdeogramPrompt prompt;
    prompt.kind = IdeogramPromptKind::Typography;
    prompt.subject = std::move(subjectText);
    prompt.visibleText = std::move(visibleText);
    return prompt;
}

IdeogramPrompt IdeogramPrompt::logo(std::string subjectText, std::string visibleText) {
    IdeogramPrompt prompt;
    prompt.kind = IdeogramPromptKind::Logo;
    prompt.subject = std::move(subjectText);
    prompt.visibleText = std::move(visibleText);
    return prompt;
}

IdeogramPrompt IdeogramPrompt::illustration(std::string subjectText) {
    IdeogramPrompt prompt;
    prompt.kind = IdeogramPromptKind::Illustration;
    prompt.subject = std::move(subjectText);
    return prompt;
}

IdeogramPrompt& IdeogramPrompt::text(std::string value) {
    visibleText = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::styleDescription(std::string value) {
    style = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::compositionDescription(std::string value) {
    composition = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::backgroundDescription(std::string value) {
    background = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::lightingDescription(std::string value) {
    lighting = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::mediumDescription(std::string value) {
    medium = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::moodDescription(std::string value) {
    mood = std::move(value);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::palette(std::initializer_list<std::string> colors) {
    colorPalette.assign(colors.begin(), colors.end());
    return *this;
}

IdeogramPrompt& IdeogramPrompt::palette(std::vector<std::string> colors) {
    colorPalette = std::move(colors);
    return *this;
}

IdeogramPrompt& IdeogramPrompt::element(std::string type, std::string description) {
    elements.push_back({std::move(type), std::move(description)});
    return *this;
}

IdeogramPrompt& IdeogramPrompt::preserveExactText(bool enabled) {
    preserveText = enabled;
    return *this;
}

IdeogramPrompt& IdeogramPrompt::keepTextUpright(bool enabled) {
    uprightText = enabled;
    return *this;
}

std::string IdeogramPrompt::build() const {
    const std::string styleValue = style.empty() ? defaultStyle(kind) : style;
    const std::string compositionValue = composition.empty() ? defaultComposition(kind) : composition;
    const std::string backgroundValue = background.empty()
        ? "clean, intentional background that supports the subject without clutter"
        : background;
    const std::string lightingValue = lighting.empty()
        ? "clear, controlled lighting with enough contrast to separate every important shape"
        : lighting;
    const std::string mediumValue = medium.empty()
        ? "local AI image generation with polished commercial-art direction"
        : medium;
    const std::vector<std::string> paletteValue = colorPalette.empty() ? defaultPalette(kind) : colorPalette;

    std::string highLevel = subject.empty() ? "A polished generated image" : subject;
    if (!visibleText.empty()) {
        highLevel += ". Include the exact readable text \"" + visibleText + "\".";
    }
    if (!mood.empty()) {
        highLevel += " The overall mood is " + mood + ".";
    }

    std::ostringstream out;
    out << "{\"high_level_description\":\"" << escapeJson(highLevel) << "\",";

    out << "\"style_description\":{";
    bool firstStyle = true;
    appendJsonStringField(out, firstStyle, "aesthetics", styleValue);
    appendJsonStringField(out, firstStyle, "lighting", lightingValue);
    appendJsonStringField(out, firstStyle, "medium", mediumValue);
    appendJsonStringArrayField(out, firstStyle, "color_palette", paletteValue);
    out << "},";

    out << "\"compositional_deconstruction\":{";
    bool firstComp = true;
    appendJsonStringField(out, firstComp, "canvas", "upright image canvas; do not rotate the image or any text");
    appendJsonStringField(out, firstComp, "background", backgroundValue);
    appendJsonStringField(out, firstComp, "layout", compositionValue);
    if (!firstComp) {
        out << ",";
    }
    out << "\"elements\":[";
    bool firstElement = true;
    for (const auto& item : defaultElements(*this)) {
        if (item.description.empty()) {
            continue;
        }
        if (!firstElement) {
            out << ",";
        }
        out << "{\"type\":\"" << escapeJson(item.type.empty() ? "obj" : item.type)
            << "\",\"desc\":\"" << escapeJson(item.description) << "\"}";
        firstElement = false;
    }
    out << "]";
    out << "}}";
    return out.str();
}

std::string IdeogramPrompt::negative() const {
    std::string text = "low quality, blurry, cluttered layout, distorted anatomy, duplicate subjects, watermark, signature";
    if (!visibleText.empty()) {
        text += ", misspelled text, unreadable text, cropped text, mirrored text, rotated text, extra words";
    }
    return text;
}

ImageRequest ImageRequest::fromPrompt(std::string promptText) {
    ImageRequest request;
    request.prompt = std::move(promptText);
    request.metadata["request_mode"] = toString(request.mode);
    return request;
}

ImageRequest ImageRequest::fromIdeogram4(const IdeogramPrompt& promptSpec) {
    ImageRequest request;
    request.prompt = promptSpec.build();
    request.negativePrompt = promptSpec.negative();
    request.metadata["prompt_profile"] = "ideogram4";
    request.metadata["prompt_kind"] = toString(promptSpec.kind);
    if (!promptSpec.visibleText.empty()) {
        request.metadata["visible_text"] = promptSpec.visibleText;
    }
    request.metadata["request_mode"] = toString(request.mode);
    return request;
}

ImageRequest ImageRequest::textToImage(std::string promptText) {
    return fromPrompt(std::move(promptText)).modeValue(RequestMode::TextToImage);
}

ImageRequest ImageRequest::imageToImage(std::string promptText, fs::path imagePath, float denoiseStrength) {
    return fromPrompt(std::move(promptText))
        .modeValue(RequestMode::ImageToImage)
        .imageToImage(std::move(imagePath), denoiseStrength);
}

ImageRequest ImageRequest::inpaint(std::string promptText, fs::path imagePath, fs::path maskPath, float denoiseStrength) {
    return fromPrompt(std::move(promptText))
        .modeValue(RequestMode::Inpaint)
        .imageToImage(std::move(imagePath), denoiseStrength)
        .mask(std::move(maskPath));
}

ImageRequest ImageRequest::controlNet(std::string promptText, fs::path controlImagePath, float weight) {
    return fromPrompt(std::move(promptText))
        .modeValue(RequestMode::ControlNet)
        .control(std::move(controlImagePath), weight);
}

ImageRequest ImageRequest::loraStack(std::string promptText, std::vector<Lora> stack) {
    ImageRequest request = fromPrompt(std::move(promptText)).modeValue(RequestMode::LoraStack);
    request.loras = std::move(stack);
    request.metadata["lora_count"] = std::to_string(request.loras.size());
    return request;
}

ImageRequest ImageRequest::refine(std::string promptText, fs::path sourceImagePath, float denoiseStrength) {
    return fromPrompt(std::move(promptText))
        .modeValue(RequestMode::Refine)
        .refineSource(std::move(sourceImagePath), denoiseStrength);
}

ImageRequest ImageRequest::upscale(std::string promptText, fs::path sourceImagePath, float scale) {
    return fromPrompt(std::move(promptText))
        .modeValue(RequestMode::Upscale)
        .upscaleSource(std::move(sourceImagePath), scale);
}

ImageRequest& ImageRequest::size(int w, int h) {
    width = w;
    height = h;
    return *this;
}

ImageRequest& ImageRequest::square(int side) {
    width = side;
    height = side;
    return *this;
}

ImageRequest& ImageRequest::canvas(CanvasPreset preset) {
    const CanvasDefaults values = CanvasDefaults::fromPreset(preset);
    width = values.width;
    height = values.height;
    metadata["canvas_preset"] = values.label.empty() ? toString(preset) : values.label;
    return *this;
}

ImageRequest& ImageRequest::style(StylePreset preset) {
    metadata["style_preset"] = toString(preset);
    switch (preset) {
        case StylePreset::CommercialPoster:
            return promptPack(PromptPack::poster(prompt));
        case StylePreset::CleanProductShot:
            return promptPack(PromptPack::productShot(prompt));
        case StylePreset::WideScene:
            return promptPack(PromptPack::wideScene(prompt));
        case StylePreset::GameAsset:
            return promptPack(PromptPack::gameAsset(prompt));
        case StylePreset::UiMockup:
            return promptPack(PromptPack::uiMockup(prompt));
    }
    return *this;
}

ImageRequest& ImageRequest::promptPack(const PromptPack& pack) {
    prompt = pack.prompt;
    if (negativePrompt.empty()) {
        negativePrompt = pack.negativePrompt;
    }
    for (const auto& [key, value] : pack.metadata) {
        metadata[key] = value;
    }
    return *this;
}

ImageRequest& ImageRequest::modeValue(RequestMode value) {
    mode = value;
    metadata["request_mode"] = toString(value);
    return *this;
}

ImageRequest& ImageRequest::stepsCount(int value) {
    steps = value;
    return *this;
}

ImageRequest& ImageRequest::seedValue(std::int64_t value) {
    seed = value;
    return *this;
}

ImageRequest& ImageRequest::cfg(float value) {
    cfgScale = value;
    return *this;
}

ImageRequest& ImageRequest::negative(std::string text) {
    negativePrompt = std::move(text);
    return *this;
}

ImageRequest& ImageRequest::imageToImage(fs::path imagePath, float denoiseStrength) {
    if (mode == RequestMode::TextToImage) {
        modeValue(RequestMode::ImageToImage);
    }
    initImage = std::move(imagePath);
    strength = denoiseStrength;
    metadata["init_image"] = initImage.string();
    metadata["strength"] = std::to_string(strength);
    return *this;
}

ImageRequest& ImageRequest::mask(fs::path maskPath) {
    modeValue(RequestMode::Inpaint);
    maskImage = std::move(maskPath);
    metadata["mask_image"] = maskImage.string();
    return *this;
}

ImageRequest& ImageRequest::control(fs::path imagePath, float weight) {
    modeValue(RequestMode::ControlNet);
    controlImage = std::move(imagePath);
    controlStrength = weight;
    metadata["control_image"] = controlImage.string();
    metadata["control_strength"] = std::to_string(controlStrength);
    return *this;
}

ImageRequest& ImageRequest::lora(fs::path loraPath, float weight) {
    if (mode == RequestMode::TextToImage) {
        modeValue(RequestMode::LoraStack);
    }
    loras.push_back({std::move(loraPath), weight});
    metadata["lora_count"] = std::to_string(loras.size());
    metadata["lora_stack"] = "true";
    return *this;
}

ImageRequest& ImageRequest::refineSource(fs::path imagePath, float denoiseStrength) {
    modeValue(RequestMode::Refine);
    refineSourceImage = imagePath;
    initImage = std::move(imagePath);
    strength = denoiseStrength;
    metadata["refine_source_image"] = refineSourceImage.string();
    metadata["init_image"] = initImage.string();
    metadata["strength"] = std::to_string(strength);
    return *this;
}

ImageRequest& ImageRequest::upscaleSource(fs::path imagePath, float scale) {
    modeValue(RequestMode::Upscale);
    refineSourceImage = imagePath;
    initImage = std::move(imagePath);
    strength = 0.25f;
    upscaleFactor = scale;
    metadata["upscale_source_image"] = refineSourceImage.string();
    metadata["init_image"] = initImage.string();
    metadata["upscale_factor"] = std::to_string(upscaleFactor);
    metadata["upscale_method"] = "img2img_refine";
    return *this;
}

ImageRequest& ImageRequest::ideogram4(const IdeogramPrompt& promptSpec) {
    prompt = promptSpec.build();
    if (negativePrompt.empty()) {
        negativePrompt = promptSpec.negative();
    }
    metadata["prompt_profile"] = "ideogram4";
    metadata["prompt_kind"] = toString(promptSpec.kind);
    if (!promptSpec.visibleText.empty()) {
        metadata["visible_text"] = promptSpec.visibleText;
    }
    return *this;
}

ImageRequest& ImageRequest::draft() {
    quality = Quality::Draft;
    if (steps <= 0 || steps > 6) {
        steps = 4;
    }
    return *this;
}

ImageRequest& ImageRequest::balanced() {
    quality = Quality::Balanced;
    if (steps <= 0) {
        steps = 8;
    }
    return *this;
}

ImageRequest& ImageRequest::final() {
    quality = Quality::Final;
    if (steps < 16) {
        steps = 20;
    }
    return *this;
}

bool ImageResult::hasImage() const {
    return ok && pixels.isAllocated();
}

bool ImageResult::save(const fs::path& path) const {
    if (hasImage()) {
        return pixels.save(path);
    }
    if (!ok || outputPath.empty() || !fs::exists(outputPath)) {
        return false;
    }
    std::error_code ec;
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }
    fs::copy_file(outputPath, path, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

bool ImageResult::saveMetadata(const fs::path& path, const fs::path& savedImagePath) const {
    std::error_code ec;
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    out << "{";
    bool first = true;
    appendJsonRawField(out, first, "job_id", std::to_string(jobId));
    appendJsonBoolField(out, first, "ok", ok);
    appendJsonStringField(out, first, "state", toString(state));
    appendJsonStringField(out, first, "error", error);
    appendJsonStringField(out, first, "created_at", nowIsoLocal());
    appendJsonNumberField(out, first, "duration_seconds", durationSeconds);
    appendJsonStringField(out, first, "native_output_path", outputPath.empty() ? std::string() : outputPath.string());
    appendJsonStringField(out, first, "saved_image_path", savedImagePath.empty() ? std::string() : savedImagePath.string());

    if (pixels.isAllocated()) {
        appendJsonRawField(out, first, "image_width", std::to_string(pixels.getWidth()));
        appendJsonRawField(out, first, "image_height", std::to_string(pixels.getHeight()));
        appendJsonRawField(out, first, "image_channels", std::to_string(pixels.getChannels()));
        appendJsonRawField(out, first, "image_bytes", std::to_string(pixels.getTotalBytes()));
    } else {
        const auto width = metadata.find("image_width");
        const auto height = metadata.find("image_height");
        if (width != metadata.end()) {
            appendJsonRawField(out, first, "image_width", width->second);
        }
        if (height != metadata.end()) {
            appendJsonRawField(out, first, "image_height", height->second);
        }
    }

    if (!first) {
        out << ",";
    }
    out << "\"metadata\":{";
    bool firstMetadata = true;
    for (const auto& [key, value] : metadata) {
        appendJsonStringField(out, firstMetadata, key, value);
    }
    out << "}";
    first = false;

    out << "}\n";
    return static_cast<bool>(out);
}

bool ImageResult::saveWithMetadata(const fs::path& imagePath, const fs::path& metadataPath) const {
    if (!save(imagePath)) {
        return false;
    }
    const fs::path sidecarPath = metadataPath.empty() ? defaultMetadataPathForImage(imagePath) : metadataPath;
    return saveMetadata(sidecarPath, imagePath);
}

const char* toString(Backend backend) {
    switch (backend) {
        case Backend::Auto: return "auto";
        case Backend::Cuda: return "cuda";
        case Backend::Metal: return "metal";
        case Backend::Cpu: return "cpu";
    }
    return "unknown";
}

const char* toString(ExecutionMode mode) {
    switch (mode) {
        case ExecutionMode::Auto: return "auto";
        case ExecutionMode::InProcess: return "in_process";
        case ExecutionMode::CliProcess: return "cli_process";
        case ExecutionMode::PersistentServer: return "persistent_server";
    }
    return "unknown";
}

const char* toString(JobState state) {
    switch (state) {
        case JobState::Queued: return "queued";
        case JobState::LoadingModel: return "loading_model";
        case JobState::Running: return "running";
        case JobState::Complete: return "complete";
        case JobState::Failed: return "failed";
        case JobState::Cancelled: return "cancelled";
    }
    return "unknown";
}

const char* toString(Quality quality) {
    switch (quality) {
        case Quality::Draft: return "draft";
        case Quality::Balanced: return "balanced";
        case Quality::Final: return "final";
    }
    return "unknown";
}

const char* toString(RuntimePreset preset) {
    switch (preset) {
        case RuntimePreset::Default: return "default";
        case RuntimePreset::LowVram: return "low_vram";
        case RuntimePreset::Rtx4090FullSpeed: return "rtx4090_full_speed";
    }
    return "unknown";
}

const char* toString(RequestMode mode) {
    switch (mode) {
        case RequestMode::TextToImage: return "text_to_image";
        case RequestMode::ImageToImage: return "image_to_image";
        case RequestMode::Inpaint: return "inpaint";
        case RequestMode::ControlNet: return "control_net";
        case RequestMode::LoraStack: return "lora_stack";
        case RequestMode::Refine: return "refine";
        case RequestMode::Upscale: return "upscale";
    }
    return "unknown";
}

const char* toString(CanvasPreset preset) {
    switch (preset) {
        case CanvasPreset::SquarePreview: return "square_preview";
        case CanvasPreset::MobilePoster: return "mobile_poster";
        case CanvasPreset::WideHero: return "wide_hero";
        case CanvasPreset::DesktopWallpaper: return "desktop_wallpaper";
        case CanvasPreset::AppIcon: return "app_icon";
    }
    return "unknown";
}

const char* toString(StylePreset preset) {
    switch (preset) {
        case StylePreset::CommercialPoster: return "commercial_poster";
        case StylePreset::CleanProductShot: return "clean_product_shot";
        case StylePreset::WideScene: return "wide_scene";
        case StylePreset::GameAsset: return "game_asset";
        case StylePreset::UiMockup: return "ui_mockup";
    }
    return "unknown";
}

const char* toString(TextRenderingPreset preset) {
    switch (preset) {
        case TextRenderingPreset::None: return "none";
        case TextRenderingPreset::ChinesePoster: return "chinese_poster";
        case TextRenderingPreset::EnglishTitle: return "english_title";
        case TextRenderingPreset::BrandWordmark: return "brand_wordmark";
    }
    return "unknown";
}

const char* toString(Sampler sampler) {
    switch (sampler) {
        case Sampler::Euler: return "euler";
        case Sampler::EulerA: return "euler_a";
        case Sampler::Dpmpp2M: return "dpmpp2m";
        case Sampler::DdimTrailing: return "ddim_trailing";
        case Sampler::Auto: return "auto";
    }
    return "unknown";
}

const char* toString(IdeogramPromptKind kind) {
    switch (kind) {
        case IdeogramPromptKind::General: return "general";
        case IdeogramPromptKind::Poster: return "poster";
        case IdeogramPromptKind::Product: return "product";
        case IdeogramPromptKind::Typography: return "typography";
        case IdeogramPromptKind::Logo: return "logo";
        case IdeogramPromptKind::Illustration: return "illustration";
    }
    return "unknown";
}

} // namespace tcx::sd
