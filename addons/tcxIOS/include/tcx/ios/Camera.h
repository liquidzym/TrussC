#pragma once

#include "Types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace trussc {
class Pixels;
class Texture;
} // namespace trussc

namespace tcx::ios {

enum class CameraPixelFormat {
    BGRA8
};

enum class CameraDevicePosition {
    Unspecified,
    Back,
    Front,
    External
};

enum class CameraOrientation {
    Unspecified,
    Portrait,
    PortraitUpsideDown,
    LandscapeLeft,
    LandscapeRight
};

struct CameraConfig {
    int width = 1280;
    int height = 720;
    int framesPerSecond = 30;
    CameraPixelFormat pixelFormat = CameraPixelFormat::BGRA8;
    CameraDevicePosition position = CameraDevicePosition::Back;
    CameraOrientation orientation = CameraOrientation::Unspecified;
    bool mirrored = false;
    int ringBufferCapacity = 3;
};

struct CameraFormat {
    int width = 0;
    int height = 0;
    int minFramesPerSecond = 0;
    int maxFramesPerSecond = 0;
    CameraPixelFormat pixelFormat = CameraPixelFormat::BGRA8;
};

struct CameraDeviceInfo {
    std::string identifier;
    std::string name;
    CameraDevicePosition position = CameraDevicePosition::Unspecified;
    std::vector<CameraFormat> formats;
};

struct CameraFrame {
    std::uint64_t frameId = 0;
    std::uint64_t droppedFrameCount = 0;
    int width = 0;
    int height = 0;
    int bytesPerRow = 0;
    CameraPixelFormat pixelFormat = CameraPixelFormat::BGRA8;
    double timestampSeconds = 0.0;
    std::vector<std::uint8_t> data;
};

struct CameraFrameView {
    std::uint64_t frameId = 0;
    std::uint64_t droppedFrameCount = 0;
    int width = 0;
    int height = 0;
    int bytesPerRow = 0;
    CameraPixelFormat pixelFormat = CameraPixelFormat::BGRA8;
    double timestampSeconds = 0.0;
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::shared_ptr<const std::vector<std::uint8_t>> storage;

    bool valid() const { return data != nullptr && size > 0; }
};

class Camera {
public:
    std::vector<CameraDeviceInfo> availableDevices() const;
    void start(const CameraConfig& config, Completion<void> done);
    void stop();
    bool isRunning() const;
    bool latestFrame(CameraFrame& out) const;
    bool latestFrameView(CameraFrameView& out) const;
    bool copyLatestFrameToPixels(trussc::Pixels& dst) const;
    bool uploadLatestFrameToTexture(trussc::Texture& dst) const;
};

Camera& camera();

bool copyCameraFrameToPixels(const CameraFrame& frame, trussc::Pixels& dst);

std::string toString(CameraDevicePosition position);
std::string toString(CameraOrientation orientation);

} // namespace tcx::ios
