#pragma once

#include <TrussC.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace tcx::sd {

using JobId = std::uint64_t;
namespace fs = std::filesystem;

enum class Backend {
    Auto,
    Cuda,
    Metal,
    Cpu,
};

enum class ExecutionMode {
    Auto,
    InProcess,
    CliProcess,
    PersistentServer,
};

enum class JobState {
    Queued,
    LoadingModel,
    Running,
    Complete,
    Failed,
    Cancelled,
};

enum class Quality {
    Draft,
    Balanced,
    Final,
};

enum class RuntimePreset {
    Default,
    LowVram,
    Rtx4090FullSpeed,
};

enum class RequestMode {
    TextToImage,
    ImageToImage,
    Inpaint,
    ControlNet,
    LoraStack,
    Refine,
    Upscale,
};

enum class CanvasPreset {
    SquarePreview,
    MobilePoster,
    WideHero,
    DesktopWallpaper,
    AppIcon,
};

enum class StylePreset {
    CommercialPoster,
    CleanProductShot,
    WideScene,
    GameAsset,
    UiMockup,
};

enum class TextRenderingPreset {
    None,
    ChinesePoster,
    EnglishTitle,
    BrandWordmark,
};

enum class Sampler {
    Euler,
    EulerA,
    Dpmpp2M,
    DdimTrailing,
    Auto,
};

enum class IdeogramPromptKind {
    General,
    Poster,
    Product,
    Typography,
    Logo,
    Illustration,
};

struct ModelPaths {
    fs::path model;
    fs::path diffusionModel;
    fs::path highNoiseDiffusionModel;
    fs::path unconditionalDiffusionModel;
    fs::path clipL;
    fs::path clipG;
    fs::path clipVision;
    fs::path t5xxl;
    fs::path llm;
    fs::path llmVision;
    fs::path vae;
    fs::path audioVae;
    fs::path controlNet;
    fs::path photoMaker;

    static ModelPaths ideogram4Example(const fs::path& modelDir = "models");
    static ModelPaths flux2KleinExample(const fs::path& modelDir = "models");
    static ModelPaths zImageTurboExample(const fs::path& modelDir = "models");
    static ModelPaths sd15ControlNetCannyExample(const fs::path& modelDir = "models");
    bool hasImagePipeline() const;
};

struct RuntimeSettings {
    Backend backend = Backend::Auto;
    ExecutionMode executionMode = ExecutionMode::Auto;
    int cpuThreads = 0;
    bool keepModelLoaded = true;
    bool mmap = true;
    bool flashAttention = false;
    bool diffusionFlashAttention = true;
    bool diffusionConvDirect = false;
    bool vaeConvDirect = false;
    bool offloadParamsToCpu = false;
    bool keepTextEncoderOnCpu = false;
    bool keepVaeOnCpu = false;
    bool keepControlNetOnCpu = false;
    float maxVramGiB = 0.0f;
    bool streamLayers = false;
    std::string backendAssignment;
    std::string paramsBackendAssignment;
    fs::path cliExecutable;
    fs::path cliWorkDir;
    fs::path serverExecutable;
    fs::path serverWorkDir;
    fs::path loraModelDirectory;
    fs::path hiresUpscalersDirectory;
    std::string serverHost = "127.0.0.1";
    int serverPort = 1234;
    int serverStartupTimeoutSeconds = 120;
    int serverPollIntervalMs = 500;
    bool serverReuseExisting = false;
    bool keepServerRunning = false;
    fs::path outputDirectory;
    fs::path tempDirectory;
    fs::path cacheDirectory;
    int processTimeoutSeconds = 0;

    static RuntimeSettings windowsCuda();
    static RuntimeSettings lowVramCuda();
    static RuntimeSettings macMetal();
};

struct QualityDefaults {
    int width = 1024;
    int height = 1024;
    int steps = 8;
    float cfgScale = 1.0f;
    Sampler sampler = Sampler::Euler;
};

struct ImageRequest;
struct ImageResult;

struct ModelProfile {
    std::string id;
    std::string family;
    QualityDefaults draft;
    QualityDefaults balanced;
    QualityDefaults final;

    QualityDefaults defaults(Quality quality = Quality::Balanced) const;
    RuntimeSettings runtime(RuntimePreset preset = RuntimePreset::Default) const;
    ImageRequest request(Quality quality = Quality::Balanced) const;
    ModelPaths paths(const fs::path& modelDir = "models") const;

    static ModelProfile ideogram4();
    static ModelProfile flux2Klein();
    static ModelProfile zImageTurbo();
    static ModelProfile sd15ControlNetCanny();
    static ModelProfile byId(const std::string& modelId);
};

struct StorageRoots {
    fs::path outputRoot;
    fs::path tempRoot;
    fs::path cacheRoot;

    static StorageRoots fromRuntime(const RuntimeSettings& settings);
};

struct CleanupOptions {
    StorageRoots roots;
    int olderThanSeconds = 24 * 60 * 60;
    bool dryRun = true;
};

struct CleanupResult {
    std::vector<fs::path> removed;
    std::vector<std::string> errors;
};

struct BackendCapabilities {
    bool textToImage = true;
    bool imageToImage = true;
    bool inpaint = true;
    bool controlNet = false;
    bool loraStack = false;
    bool refine = true;
    bool upscale = true;

    bool supports(const ImageRequest& request) const;
    std::string unsupportedReason(const ImageRequest& request, ExecutionMode mode) const;

    static BackendCapabilities forRuntime(const ModelPaths& paths, const RuntimeSettings& settings, ExecutionMode mode);
};

struct CanvasDefaults {
    int width = 1024;
    int height = 1024;
    std::string label;

    static CanvasDefaults fromPreset(CanvasPreset preset);
};

struct PromptPack {
    std::string prompt;
    std::string negativePrompt;
    std::map<std::string, std::string> metadata;

    static PromptPack poster(std::string subject, std::string visibleText = {}, TextRenderingPreset textPreset = TextRenderingPreset::None);
    static PromptPack productShot(std::string subject);
    static PromptPack wideScene(std::string subject);
    static PromptPack gameAsset(std::string subject);
    static PromptPack uiMockup(std::string subject);
};

struct GenerationArtifact {
    std::string id;
    fs::path imagePath;
    fs::path sidecarPath;
    fs::path parentSidecarPath;
    std::map<std::string, std::string> metadata;

    static GenerationArtifact fromResult(const ImageResult& result, fs::path sidecarPath = {});
};

struct GenerationProject {
    fs::path root;
    fs::path outputRoot;
    fs::path tempRoot;
    fs::path cacheRoot;
    fs::path logRoot;
    fs::path inputRoot;

    static GenerationProject at(const fs::path& root, std::string name = "tcxsd-project");
    RuntimeSettings apply(RuntimeSettings settings) const;
    fs::path outputPath(std::string label, std::string extension = ".png") const;
    fs::path sidecarPath(std::string label) const;
    GenerationArtifact artifact(std::string label) const;
};

struct Lora {
    fs::path path;
    float weight = 1.0f;
};

struct IdeogramPromptElement {
    std::string type = "obj";
    std::string description;
};

struct IdeogramPrompt {
    IdeogramPromptKind kind = IdeogramPromptKind::Poster;
    std::string subject;
    std::string visibleText;
    std::string style;
    std::string composition;
    std::string background;
    std::string lighting;
    std::string medium;
    std::string mood;
    std::vector<std::string> colorPalette;
    std::vector<IdeogramPromptElement> elements;
    bool preserveText = true;
    bool uprightText = true;

    static IdeogramPrompt general(std::string subjectText);
    static IdeogramPrompt poster(std::string subjectText);
    static IdeogramPrompt product(std::string subjectText);
    static IdeogramPrompt typography(std::string subjectText, std::string visibleText);
    static IdeogramPrompt logo(std::string subjectText, std::string visibleText = {});
    static IdeogramPrompt illustration(std::string subjectText);

    IdeogramPrompt& text(std::string value);
    IdeogramPrompt& styleDescription(std::string value);
    IdeogramPrompt& compositionDescription(std::string value);
    IdeogramPrompt& backgroundDescription(std::string value);
    IdeogramPrompt& lightingDescription(std::string value);
    IdeogramPrompt& mediumDescription(std::string value);
    IdeogramPrompt& moodDescription(std::string value);
    IdeogramPrompt& palette(std::initializer_list<std::string> colors);
    IdeogramPrompt& palette(std::vector<std::string> colors);
    IdeogramPrompt& element(std::string type, std::string description);
    IdeogramPrompt& preserveExactText(bool enabled = true);
    IdeogramPrompt& keepTextUpright(bool enabled = true);

    std::string build() const;
    std::string negative() const;
};

struct ImageRequest {
    RequestMode mode = RequestMode::TextToImage;
    std::string prompt;
    std::string negativePrompt;
    int width = 1024;
    int height = 1024;
    int steps = 8;
    float cfgScale = 1.0f;
    float strength = 0.75f;
    std::int64_t seed = -1;
    int batchCount = 1;
    Quality quality = Quality::Balanced;
    Sampler sampler = Sampler::Euler;
    fs::path initImage;
    fs::path maskImage;
    fs::path controlImage;
    fs::path refineSourceImage;
    float controlStrength = 1.0f;
    float upscaleFactor = 1.0f;
    std::vector<Lora> loras;
    std::map<std::string, std::string> metadata;

    static ImageRequest fromPrompt(std::string promptText);
    static ImageRequest fromIdeogram4(const IdeogramPrompt& promptSpec);
    static ImageRequest textToImage(std::string promptText);
    static ImageRequest imageToImage(std::string promptText, fs::path imagePath, float denoiseStrength = 0.75f);
    static ImageRequest inpaint(std::string promptText, fs::path imagePath, fs::path maskPath, float denoiseStrength = 0.75f);
    static ImageRequest controlNet(std::string promptText, fs::path controlImagePath, float weight = 1.0f);
    static ImageRequest loraStack(std::string promptText, std::vector<Lora> stack);
    static ImageRequest refine(std::string promptText, fs::path sourceImagePath, float denoiseStrength = 0.35f);
    static ImageRequest upscale(std::string promptText, fs::path sourceImagePath, float scale = 2.0f);

    ImageRequest& size(int w, int h);
    ImageRequest& square(int side);
    ImageRequest& canvas(CanvasPreset preset);
    ImageRequest& style(StylePreset preset);
    ImageRequest& promptPack(const PromptPack& pack);
    ImageRequest& modeValue(RequestMode value);
    ImageRequest& stepsCount(int value);
    ImageRequest& seedValue(std::int64_t value);
    ImageRequest& cfg(float value);
    ImageRequest& negative(std::string text);
    ImageRequest& imageToImage(fs::path imagePath, float denoiseStrength = 0.75f);
    ImageRequest& mask(fs::path maskPath);
    ImageRequest& control(fs::path imagePath, float weight = 1.0f);
    ImageRequest& lora(fs::path loraPath, float weight = 1.0f);
    ImageRequest& refineSource(fs::path imagePath, float denoiseStrength = 0.35f);
    ImageRequest& upscaleSource(fs::path imagePath, float scale = 2.0f);
    ImageRequest& ideogram4(const IdeogramPrompt& promptSpec);
    ImageRequest& draft();
    ImageRequest& balanced();
    ImageRequest& final();
};

struct BatchJob {
    std::string label;
    std::vector<ImageRequest> requests;

    BatchJob& add(ImageRequest request);
    static BatchJob seedSweep(ImageRequest base, std::vector<std::int64_t> seeds);
};

struct VariantJob {
    GenerationArtifact source;
    ImageRequest request;

    static VariantJob fromArtifact(const GenerationArtifact& artifact, std::string prompt, float strength = 0.55f);
};

struct GenerationSession {
    std::string id;
    ModelProfile profile;
    fs::path modelDirectory;
    RuntimePreset runtimePreset = RuntimePreset::Default;
    RuntimeSettings settings;
    ModelPaths paths;
    GenerationProject project;
    BackendCapabilities capabilities;

    static GenerationSession forProfile(
        ModelProfile profile,
        const fs::path& modelDir,
        RuntimePreset preset = RuntimePreset::Default,
        GenerationProject project = {});
    static GenerationSession forModelId(
        const std::string& modelId,
        const fs::path& modelDir,
        RuntimePreset preset = RuntimePreset::Default,
        GenerationProject project = {});

    RuntimeSettings appliedSettings() const;
    ImageRequest request(Quality quality = Quality::Balanced) const;
    GenerationArtifact artifact(std::string label) const;
    bool supports(const ImageRequest& request) const;
    std::string unsupportedReason(const ImageRequest& request) const;
};

struct Progress {
    JobId jobId = 0;
    JobState state = JobState::Queued;
    int step = 0;
    int totalSteps = 0;
    float seconds = 0.0f;
    std::string message;
};

struct ImageResult {
    JobId jobId = 0;
    bool ok = false;
    JobState state = JobState::Failed;
    std::string error;
    double durationSeconds = 0.0;
    trussc::Pixels pixels;
    fs::path outputPath;
    std::map<std::string, std::string> metadata;

    bool hasImage() const;
    bool save(const fs::path& path) const;
    bool saveMetadata(const fs::path& path, const fs::path& savedImagePath = {}) const;
    bool saveWithMetadata(const fs::path& imagePath, const fs::path& metadataPath = {}) const;
};

using ProgressCallback = std::function<void(const Progress&)>;
using ResultCallback = std::function<void(const ImageResult&)>;

const char* toString(Backend backend);
const char* toString(ExecutionMode mode);
const char* toString(JobState state);
const char* toString(Quality quality);
const char* toString(RuntimePreset preset);
const char* toString(RequestMode mode);
const char* toString(CanvasPreset preset);
const char* toString(StylePreset preset);
const char* toString(TextRenderingPreset preset);
const char* toString(Sampler sampler);
const char* toString(IdeogramPromptKind kind);

CleanupResult cleanupRuntimeStorage(const CleanupOptions& options);

} // namespace tcx::sd
