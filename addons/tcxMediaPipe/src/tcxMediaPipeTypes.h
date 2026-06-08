#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

namespace tcx::mediapipe {

enum class Delegate {
    GPU,
    CPU
};

enum class InputMode {
    WebCamera,
    ExternalFrame
};

struct Landmark {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float visibility = 0.0f;
    float presence = 0.0f;
};

struct Hand {
    std::string handedness;
    float score = 0.0f;
    std::vector<Landmark> landmarks;
    std::vector<Landmark> worldLandmarks;
};

struct HandResult {
    double timestampMs = 0.0;
    double inferenceTimeMs = 0.0;
    std::vector<Hand> hands;
};

struct Gesture {
    std::string handedness;
    float handednessScore = 0.0f;
    std::string categoryName;
    std::string displayName;
    float score = 0.0f;
    std::vector<Landmark> landmarks;
    std::vector<Landmark> worldLandmarks;
};

struct GestureResult {
    double timestampMs = 0.0;
    double inferenceTimeMs = 0.0;
    std::vector<Gesture> gestures;
};

struct Pose {
    std::vector<Landmark> landmarks;
    std::vector<Landmark> worldLandmarks;
    bool segmentationMaskAvailable = false;
};

struct PoseResult {
    double timestampMs = 0.0;
    double inferenceTimeMs = 0.0;
    std::vector<Pose> poses;

    // Compatibility view for existing single-pose code. Mirrors poses.front().
    std::vector<Landmark> landmarks;
    std::vector<Landmark> worldLandmarks;
    bool segmentationMaskAvailable = false;
};

struct Face {
    std::vector<Landmark> landmarks;
    std::map<std::string, float> blendshapes;
    std::array<float, 16> facialTransformationMatrix{};
};

struct FaceResult {
    double timestampMs = 0.0;
    double inferenceTimeMs = 0.0;
    std::vector<Face> faces;
};

struct RuntimeModelsStatus {
    bool hand = false;
    bool pose = false;
    bool face = false;
    bool gesture = false;
};

struct RuntimeGpuInfo {
    std::string webglVendor;
    std::string webglRenderer;
    std::string webglVersion;
    std::string webglShadingLanguageVersion;
};

struct RuntimeStats {
    float sourceFPS = 0.0f;
    float inferenceFPS = 0.0f;
    float averageInferenceTimeMs = 0.0f;
    float frameAgeMs = 0.0f;
    float bridgeLatencyMs = 0.0f;
};

struct RuntimeStatus {
    bool ready = false;
    bool cameraReady = false;
    bool modelReady = false;
    bool pipelineReady = false;
    std::string activeDelegate;
    bool fallback = false;
    std::string reason;
    std::string stage;
    std::string detail;
    std::string wasmPath;
    RuntimeModelsStatus models;
    RuntimeGpuInfo gpu;
    int processingWidth = 0;
    int processingHeight = 0;
};

} // namespace tcx::mediapipe
