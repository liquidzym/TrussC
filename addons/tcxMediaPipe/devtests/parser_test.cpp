#include "tcxMediaPipeResultParser.h"
#include "tcxMediaPipeSettings.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectNear(float actual, float expected, float epsilon, const char* message) {
    if (std::fabs(actual - expected) > epsilon) {
        throw std::runtime_error(message);
    }
}

void testRuntimeStatusParser() {
    const std::string json = R"json({
      "type": "runtime_status",
      "ready": true,
      "activeDelegate": "CPU",
      "fallback": true,
      "reason": "GPU initialization failed",
      "stage": "camera_stream",
      "detail": "Camera stream acquired: FaceTime HD Camera 640x480",
      "wasmPath": "/wasm",
      "cameraReady": true,
      "modelReady": true,
      "pipelineReady": true,
      "models": { "hand": true, "pose": false, "face": true, "gesture": true },
      "gpu": {
        "webglVendor": "Google Inc.",
        "webglRenderer": "ANGLE Metal Renderer: AMD Radeon Pro 5600M",
        "webglVersion": "WebGL 2.0",
        "webglShadingLanguageVersion": "WebGL GLSL ES 3.00"
      },
      "processingWidth": 480,
      "processingHeight": 360
    })json";

    const auto status = tcx::mediapipe::parseRuntimeStatus(json);
    expect(status.ready, "runtime status should be ready");
    expect(status.cameraReady, "runtime status camera ready");
    expect(status.modelReady, "runtime status model ready");
    expect(status.pipelineReady, "runtime status pipeline ready");
    expect(status.activeDelegate == "CPU", "runtime status delegate");
    expect(status.fallback, "runtime status fallback");
    expect(status.reason == "GPU initialization failed", "runtime status reason");
    expect(status.stage == "camera_stream", "runtime status stage");
    expect(status.detail == "Camera stream acquired: FaceTime HD Camera 640x480", "runtime status detail");
    expect(status.wasmPath == "/wasm", "runtime status wasm path");
    expect(status.models.hand, "runtime status hand model");
    expect(!status.models.pose, "runtime status pose model");
    expect(status.models.face, "runtime status face model");
    expect(status.models.gesture, "runtime status gesture model");
    expect(status.gpu.webglVendor == "Google Inc.", "runtime status gpu vendor");
    expect(status.gpu.webglRenderer == "ANGLE Metal Renderer: AMD Radeon Pro 5600M",
           "runtime status gpu renderer");
    expect(status.gpu.webglVersion == "WebGL 2.0", "runtime status gpu version");
    expect(status.gpu.webglShadingLanguageVersion == "WebGL GLSL ES 3.00",
           "runtime status gpu shading language");
    expect(status.processingWidth == 480, "runtime status processing width");
    expect(status.processingHeight == 360, "runtime status processing height");
}

void testHandResultParser() {
    const std::string json = R"json({
      "type": "hand_result",
      "timestampMs": 123456.7,
      "inferenceTimeMs": 8.4,
      "hands": [{
        "handedness": "Left",
        "score": 0.98,
        "landmarks": [{ "x": 0.5, "y": 0.4, "z": -0.01, "visibility": 0.2, "presence": 0.3 }],
        "worldLandmarks": [{ "x": 1.0, "y": 2.0, "z": 3.0 }]
      }]
    })json";

    const auto result = tcx::mediapipe::parseHandResult(json);
    expectNear(static_cast<float>(result.timestampMs), 123456.7f, 0.01f, "hand timestamp");
    expectNear(static_cast<float>(result.inferenceTimeMs), 8.4f, 0.01f, "hand inference time");
    expect(result.hands.size() == 1, "hand count");
    expect(result.hands[0].handedness == "Left", "hand handedness");
    expectNear(result.hands[0].score, 0.98f, 0.001f, "hand score");
    expect(result.hands[0].landmarks.size() == 1, "hand landmarks");
    expectNear(result.hands[0].landmarks[0].x, 0.5f, 0.001f, "hand landmark x");
    expectNear(result.hands[0].landmarks[0].visibility, 0.2f, 0.001f, "hand landmark visibility");
    expect(result.hands[0].worldLandmarks.size() == 1, "hand world landmarks");
    expectNear(result.hands[0].worldLandmarks[0].z, 3.0f, 0.001f, "hand world landmark z");
}

void testRuntimeStatsParser() {
    const std::string json = R"json({
      "type": "hand_result",
      "timestampMs": 123456.7,
      "inferenceTimeMs": 8.4,
      "stats": {
        "sourceFPS": 29.5,
        "inferenceFPS": 27.25,
        "averageInferenceTimeMs": 9.75,
        "sentAtEpochMs": 1710000000100.0
      },
      "hands": []
    })json";

    const auto stats = tcx::mediapipe::parseRuntimeStats(json, 1710000000125.0);
    expectNear(stats.sourceFPS, 29.5f, 0.001f, "stats source fps");
    expectNear(stats.inferenceFPS, 27.25f, 0.001f, "stats inference fps");
    expectNear(stats.averageInferenceTimeMs, 9.75f, 0.001f, "stats average inference");
    expectNear(stats.bridgeLatencyMs, 25.0f, 0.001f, "stats bridge latency");
}

void testPoseResultParser() {
    const std::string json = R"json({
      "type": "pose_result",
      "timestampMs": 11.0,
      "inferenceTimeMs": 12.5,
      "poses": [{
        "landmarks": [{ "x": 0.1, "y": 0.2, "z": 0.3, "visibility": 0.9, "presence": 0.8 }],
        "worldLandmarks": [{ "x": 0.4, "y": 0.5, "z": 0.6 }],
        "segmentationMaskAvailable": false
      }, {
        "landmarks": [{ "x": 0.7, "y": 0.8, "z": 0.9, "visibility": 0.6, "presence": 0.5 }],
        "worldLandmarks": [{ "x": 1.4, "y": 1.5, "z": 1.6 }],
        "segmentationMaskAvailable": true
      }]
    })json";

    const auto result = tcx::mediapipe::parsePoseResult(json);
    expectNear(static_cast<float>(result.inferenceTimeMs), 12.5f, 0.01f, "pose inference time");
    expect(result.poses.size() == 2, "pose count");
    expect(result.poses[0].landmarks.size() == 1, "first pose landmarks");
    expectNear(result.poses[0].landmarks[0].presence, 0.8f, 0.001f, "first pose presence");
    expect(result.poses[1].landmarks.size() == 1, "second pose landmarks");
    expectNear(result.poses[1].landmarks[0].x, 0.7f, 0.001f, "second pose landmark x");
    expect(result.poses[1].worldLandmarks.size() == 1, "second pose world landmarks");
    expect(result.poses[1].segmentationMaskAvailable, "second pose segmentation flag");
    expect(result.landmarks.size() == 1, "pose compatibility landmarks");
    expectNear(result.landmarks[0].presence, 0.8f, 0.001f, "pose compatibility presence");
    expect(!result.segmentationMaskAvailable, "pose segmentation flag");
}

void testLegacyPoseResultParser() {
    const std::string json = R"json({
      "type": "pose_result",
      "timestampMs": 11.0,
      "inferenceTimeMs": 12.5,
      "landmarks": [{ "x": 0.1, "y": 0.2, "z": 0.3, "visibility": 0.9, "presence": 0.8 }],
      "worldLandmarks": [{ "x": 0.4, "y": 0.5, "z": 0.6 }],
      "segmentationMaskAvailable": true
    })json";

    const auto result = tcx::mediapipe::parsePoseResult(json);
    expect(result.poses.size() == 1, "legacy pose count");
    expect(result.poses[0].landmarks.size() == 1, "legacy pose landmarks");
    expectNear(result.poses[0].landmarks[0].presence, 0.8f, 0.001f, "legacy pose presence");
    expect(result.poses[0].worldLandmarks.size() == 1, "legacy pose world landmarks");
    expect(result.poses[0].segmentationMaskAvailable, "legacy pose segmentation flag");
    expect(result.landmarks.size() == 1, "legacy compatibility landmarks");
    expect(result.segmentationMaskAvailable, "legacy compatibility segmentation flag");
}

void testFaceResultParser() {
    const std::string json = R"json({
      "type": "face_result",
      "timestampMs": 22.0,
      "inferenceTimeMs": 14.2,
      "faces": [{
        "landmarks": [{ "x": 0.7, "y": 0.8, "z": 0.9 }],
        "blendshapes": { "jawOpen": 0.25, "eyeBlinkLeft": 0.0 },
        "facialTransformationMatrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 5, 6, 1]
      }]
    })json";

    const auto result = tcx::mediapipe::parseFaceResult(json);
    expect(result.faces.size() == 1, "face count");
    expect(result.faces[0].landmarks.size() == 1, "face landmarks");
    expectNear(result.faces[0].blendshapes.at("jawOpen"), 0.25f, 0.001f, "face blendshape");
    expectNear(result.faces[0].facialTransformationMatrix[12], 4.0f, 0.001f, "face matrix");
}

void testGestureResultParser() {
    const std::string json = R"json({
      "type": "gesture_result",
      "timestampMs": 33.0,
      "inferenceTimeMs": 7.1,
      "gestures": [{
        "handedness": "Right",
        "handednessScore": 0.95,
        "categoryName": "Open_Palm",
        "displayName": "Open palm",
        "score": 0.87,
        "landmarks": [{ "x": 0.3, "y": 0.4, "z": -0.02 }],
        "worldLandmarks": [{ "x": 1.0, "y": 2.0, "z": 3.0 }]
      }]
    })json";

    const auto result = tcx::mediapipe::parseGestureResult(json);
    expectNear(static_cast<float>(result.timestampMs), 33.0f, 0.01f, "gesture timestamp");
    expectNear(static_cast<float>(result.inferenceTimeMs), 7.1f, 0.01f, "gesture inference time");
    expect(result.gestures.size() == 1, "gesture count");
    expect(result.gestures[0].handedness == "Right", "gesture handedness");
    expectNear(result.gestures[0].handednessScore, 0.95f, 0.001f, "gesture handedness score");
    expect(result.gestures[0].categoryName == "Open_Palm", "gesture category");
    expect(result.gestures[0].displayName == "Open palm", "gesture display name");
    expectNear(result.gestures[0].score, 0.87f, 0.001f, "gesture score");
    expect(result.gestures[0].landmarks.size() == 1, "gesture landmarks");
    expectNear(result.gestures[0].landmarks[0].x, 0.3f, 0.001f, "gesture landmark x");
    expect(result.gestures[0].worldLandmarks.size() == 1, "gesture world landmarks");
    expectNear(result.gestures[0].worldLandmarks[0].z, 3.0f, 0.001f, "gesture world landmark z");
}

void testMalformedJsonFails() {
    bool threw = false;
    try {
        (void)tcx::mediapipe::parseHandResult("{");
    } catch (const std::exception&) {
        threw = true;
    }
    expect(threw, "malformed JSON should throw");
}

void testSettingsJsonOverridesDetectionLimits() {
    tcx::mediapipe::Settings settings;
    settings.enableHand = false;
    settings.enablePose = false;
    settings.enableFace = false;
    settings.enableGesture = false;

    const std::string json = R"json({
      "delegate": "CPU",
      "mirror": false,
      "multiPerson": true,
      "maxHands": 6,
      "maxPoses": 3,
      "maxFaces": 5,
      "maxGestures": 7,
      "outputFaceBlendshapes": true,
      "outputFaceTransformationMatrix": true,
      "processingWidth": 320,
      "processingHeight": 240,
      "tasks": {
        "hand": true,
        "pose": true,
        "face": true,
        "gesture": true
      }
    })json";

    std::string error;
    expect(tcx::mediapipe::applySettingsJson(settings, json, &error), "settings JSON should parse");
    expect(error.empty(), "settings JSON should not report error");
    expect(settings.delegate == tcx::mediapipe::Delegate::CPU, "settings delegate");
    expect(!settings.mirror, "settings mirror");
    expect(settings.multiPerson, "settings multi person");
    expect(settings.maxHands == 6, "settings max hands");
    expect(settings.maxPoses == 3, "settings max poses");
    expect(settings.maxFaces == 5, "settings max faces");
    expect(settings.maxGestures == 7, "settings max gestures");
    expect(settings.outputFaceBlendshapes, "settings face blendshape output");
    expect(settings.outputFaceTransformationMatrix, "settings face transform output");
    expect(settings.processingWidth == 320, "settings processing width");
    expect(settings.processingHeight == 240, "settings processing height");
    expect(settings.enableHand, "settings hand task");
    expect(settings.enablePose, "settings pose task");
    expect(settings.enableFace, "settings face task");
    expect(settings.enableGesture, "settings gesture task");
}

void testSettingsJsonRejectsInvalidJson() {
    tcx::mediapipe::Settings settings;
    std::string error;
    expect(!tcx::mediapipe::applySettingsJson(settings, "{", &error), "invalid settings JSON should fail");
    expect(!error.empty(), "invalid settings JSON should report error");
}

void testSettingsJsonClampsRuntimeDimensions() {
    tcx::mediapipe::Settings settings;
    const std::string json = R"json({
      "inputWidth": -640,
      "inputHeight": 0,
      "processingWidth": -320,
      "processingHeight": -240,
      "maxFPS": 0
    })json";

    std::string error;
    expect(tcx::mediapipe::applySettingsJson(settings, json, &error), "dimension JSON should parse");
    expect(settings.inputWidth == 1, "input width should clamp to one");
    expect(settings.inputHeight == 1, "input height should clamp to one");
    expect(settings.processingWidth == 0, "processing width should clamp to zero");
    expect(settings.processingHeight == 0, "processing height should clamp to zero");
    expect(settings.maxFPS == 1, "max FPS should clamp to one");
}

} // namespace

int main() {
    const struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"runtime_status", testRuntimeStatusParser},
        {"hand_result", testHandResultParser},
        {"runtime_stats", testRuntimeStatsParser},
        {"pose_result", testPoseResultParser},
        {"legacy_pose_result", testLegacyPoseResultParser},
        {"face_result", testFaceResultParser},
        {"gesture_result", testGestureResultParser},
        {"malformed_json", testMalformedJsonFails},
        {"settings_json_overrides_detection_limits", testSettingsJsonOverridesDetectionLimits},
        {"settings_json_rejects_invalid_json", testSettingsJsonRejectsInvalidJson},
        {"settings_json_clamps_runtime_dimensions", testSettingsJsonClampsRuntimeDimensions},
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << e.what() << "\n";
        }
    }

    if (failures != 0) {
        std::cerr << failures << " parser test(s) failed\n";
        return 1;
    }
    return 0;
}
