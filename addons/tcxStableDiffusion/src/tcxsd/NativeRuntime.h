#pragma once

#include "tcxsd/Types.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace tcx::sd {

class NativeRuntime {
public:
    NativeRuntime();
    ~NativeRuntime();

    NativeRuntime(const NativeRuntime&) = delete;
    NativeRuntime& operator=(const NativeRuntime&) = delete;

    static bool available();
    static std::string systemInfo();

    bool setup(const ModelPaths& paths, const RuntimeSettings& settings, std::string* error = nullptr);
    void shutdown();
    bool isLoaded() const;

    ImageResult generateImage(
        JobId jobId,
        const ImageRequest& request,
        const ProgressCallback& progress,
        const std::atomic<bool>& cancelRequested);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx::sd
