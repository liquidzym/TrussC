#pragma once

#include "tcxMediaPipeResultParser.h"
#include "tcxMediaPipeSettings.h"

#include "tc/events/tcEventListener.h"
#include "tcxCEF.h"

#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <cstdint>

namespace tcx::mediapipe {

class MediaPipe {
public:
    MediaPipe();
    ~MediaPipe();

    bool setup(const Settings& settings);
    void update();
    void shutdown();

    void enableHandTracking(bool enabled);
    void enablePoseTracking(bool enabled);
    void enableFaceTracking(bool enabled);
    void enableGestureRecognition(bool enabled);

    void setDelegate(Delegate delegate);
    void setInputMode(InputMode mode);
    void setMaxFPS(int fps);
    void setInputResolution(int width, int height);
    void setProcessingResolution(int width, int height);
    void setMirror(bool mirror);
    void setMultiPerson(bool enabled);
    void setMaxHands(int count);
    void setMaxPoses(int count);
    void setMaxFaces(int count);
    void setMaxGestures(int count);
    void setDetectionLimits(int hands, int poses, int faces, int gestures);

    std::optional<HandResult> getHandResult() const;
    std::optional<PoseResult> getPoseResult() const;
    std::optional<FaceResult> getFaceResult() const;
    std::optional<GestureResult> getGestureResult() const;

    bool hasNewHandResult() const;
    bool hasNewPoseResult() const;
    bool hasNewFaceResult() const;
    bool hasNewGestureResult() const;

    void sendFrame(std::span<const std::uint8_t> rgba, int width, int height, double timestampMs);

    float getSourceFPS() const;
    float getInferenceFPS() const;
    float getAverageInferenceTimeMs() const;
    float getBridgeLatencyMs() const;

    std::string activeDelegate() const;
    std::string runtimeStage() const;
    std::string runtimeDetail() const;
    RuntimeGpuInfo gpuInfo() const;
    bool isReady() const;
    bool isFallbackToCPU() const;
    std::string lastError() const;

private:
    std::filesystem::path defaultWebRoot() const;
    void sendConfig();
    void handleBridgeMessage(tcxCEF::WebSocketBridgeMessage& message);
    std::string makeConfigJson() const;

    mutable std::mutex mutex_;
    Settings settings_;
    RuntimeStatus runtimeStatus_;
    std::optional<HandResult> handResult_;
    std::optional<PoseResult> poseResult_;
    std::optional<FaceResult> faceResult_;
    std::optional<GestureResult> gestureResult_;
    bool newHandResult_ = false;
    bool newPoseResult_ = false;
    bool newFaceResult_ = false;
    bool newGestureResult_ = false;
    float sourceFPS_ = 0.0f;
    float inferenceFPS_ = 0.0f;
    float averageInferenceTimeMs_ = 0.0f;
    float bridgeLatencyMs_ = 0.0f;
    std::string lastError_;

    tcxCEF::LocalAssetServer assetServer_;
    tcxCEF::WebSocketBridge bridge_;
    tcxCEF::Browser browser_;
    tc::EventListener bridgeMessageListener_;
    tc::EventListener bridgeConnectListener_;
};

} // namespace tcx::mediapipe
