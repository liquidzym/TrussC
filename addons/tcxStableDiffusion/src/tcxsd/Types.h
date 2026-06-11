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
    fs::path outputDirectory;
    int processTimeoutSeconds = 0;

    static RuntimeSettings windowsCuda();
    static RuntimeSettings lowVramCuda();
    static RuntimeSettings macMetal();
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
    float controlStrength = 1.0f;
    std::vector<Lora> loras;
    std::map<std::string, std::string> metadata;

    static ImageRequest fromPrompt(std::string promptText);
    static ImageRequest fromIdeogram4(const IdeogramPrompt& promptSpec);

    ImageRequest& size(int w, int h);
    ImageRequest& square(int side);
    ImageRequest& stepsCount(int value);
    ImageRequest& seedValue(std::int64_t value);
    ImageRequest& cfg(float value);
    ImageRequest& negative(std::string text);
    ImageRequest& ideogram4(const IdeogramPrompt& promptSpec);
    ImageRequest& draft();
    ImageRequest& balanced();
    ImageRequest& final();
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
const char* toString(Sampler sampler);
const char* toString(IdeogramPromptKind kind);

} // namespace tcx::sd
