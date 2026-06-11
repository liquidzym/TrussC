#pragma once

#include "tcxsd/NativeRuntime.h"
#include "tcxsd/Types.h"

#include <memory>
#include <mutex>
#include <vector>

namespace tcx::sd {

class StableDiffusion;

class ImageJobBuilder {
public:
    ImageJobBuilder(StableDiffusion& generator, ImageRequest request);

    ImageJobBuilder& size(int width, int height);
    ImageJobBuilder& square(int side);
    ImageJobBuilder& steps(int value);
    ImageJobBuilder& seed(std::int64_t value);
    ImageJobBuilder& cfg(float value);
    ImageJobBuilder& negative(std::string text);
    ImageJobBuilder& draft();
    ImageJobBuilder& balanced();
    ImageJobBuilder& final();
    ImageJobBuilder& metadata(std::string key, std::string value);

    JobId run();
    const ImageRequest& request() const;

private:
    StableDiffusion* generator_ = nullptr;
    ImageRequest request_;
};

class StableDiffusion {
public:
    StableDiffusion();
    ~StableDiffusion();

    StableDiffusion(const StableDiffusion&) = delete;
    StableDiffusion& operator=(const StableDiffusion&) = delete;

    static bool nativeAvailable();
    static std::string nativeSystemInfo();

    bool setup(const ModelPaths& paths, const RuntimeSettings& settings = RuntimeSettings());
    bool setupIdeogram4(const fs::path& modelDir = "models", const RuntimeSettings& settings = RuntimeSettings::windowsCuda());
    void shutdown();

    bool isReady() const;
    bool isRunning() const;
    std::string lastError() const;
    Progress progress() const;

    ImageJobBuilder createImage(std::string prompt);
    ImageJobBuilder createImage(const IdeogramPrompt& promptSpec);
    JobId submit(ImageRequest request);
    void cancel();

    void update();
    bool pollResult(ImageResult& result);
    bool hasResult() const;

    void onProgress(ProgressCallback callback);
    void onResult(ResultCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx::sd
