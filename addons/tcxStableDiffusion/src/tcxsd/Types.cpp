#include "tcxsd/Types.h"

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

bool ModelPaths::hasImagePipeline() const {
    return !model.empty() || !diffusionModel.empty();
}

RuntimeSettings RuntimeSettings::windowsCuda() {
    RuntimeSettings settings;
    settings.backend = Backend::Cuda;
    settings.backendAssignment = "cuda0";
    settings.paramsBackendAssignment = "cuda0";
    settings.offloadParamsToCpu = false;
    settings.keepTextEncoderOnCpu = false;
    settings.keepVaeOnCpu = false;
    settings.diffusionFlashAttention = true;
    settings.diffusionConvDirect = false;
    settings.vaeConvDirect = false;
    return settings;
}

RuntimeSettings RuntimeSettings::lowVramCuda() {
    RuntimeSettings settings = RuntimeSettings::windowsCuda();
    settings.offloadParamsToCpu = true;
    settings.paramsBackendAssignment = "cpu";
    settings.keepTextEncoderOnCpu = false;
    settings.keepVaeOnCpu = false;
    settings.maxVramGiB = 0.0f;
    settings.streamLayers = false;
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
    return request;
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
    if (!hasImage()) {
        return false;
    }
    return pixels.save(path);
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
