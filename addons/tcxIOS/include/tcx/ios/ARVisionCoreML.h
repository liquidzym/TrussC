#pragma once

#include "Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tcx::ios {

struct ARSessionConfig {
    bool worldTracking = true;
    bool planeDetection = false;
};

struct ARFrameInfo {
    double timestampSeconds = 0.0;
    int cameraImageWidth = 0;
    int cameraImageHeight = 0;
};

class ARKitBridge {
public:
    bool isWorldTrackingSupported() const;
    void start(const ARSessionConfig& config, Completion<void> done);
    void stop();
    bool latestFrame(ARFrameInfo& out) const;
};

struct VisionRectangle {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float confidence = 0.0f;
};

class VisionBridge {
public:
    void detectRectangles(const std::filesystem::path& imagePath,
                          Completion<std::vector<VisionRectangle>> done);
};

struct CoreMLModelInfo {
    std::filesystem::path path;
    bool loadable = false;
};

class CoreMLBridge {
public:
    Result<CoreMLModelInfo> inspectModel(const std::filesystem::path& compiledModelPath) const;
};

ARKitBridge& arKit();
VisionBridge& vision();
CoreMLBridge& coreML();

} // namespace tcx::ios
