#include "tcxMediaPipeResultParser.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tcx::mediapipe {
namespace {

void requireMessageType(const Json& value, const char* expectedType) {
    const std::string type = messageType(value);
    if (type != expectedType) {
        throw std::invalid_argument(std::string("Expected MediaPipe message type '") +
                                    expectedType + "', got '" + type + "'");
    }
}

Landmark parseLandmark(const Json& value) {
    Landmark landmark;
    landmark.x = value.value("x", 0.0f);
    landmark.y = value.value("y", 0.0f);
    landmark.z = value.value("z", 0.0f);
    landmark.visibility = value.value("visibility", 0.0f);
    landmark.presence = value.value("presence", 0.0f);
    return landmark;
}

std::vector<Landmark> parseLandmarks(const Json& value) {
    std::vector<Landmark> landmarks;
    if (!value.is_array()) {
        return landmarks;
    }
    landmarks.reserve(value.size());
    for (const auto& item : value) {
        landmarks.push_back(parseLandmark(item));
    }
    return landmarks;
}

Pose parsePoseValue(const Json& value) {
    Pose pose;
    pose.landmarks = parseLandmarks(value.value("landmarks", Json::array()));
    pose.worldLandmarks = parseLandmarks(value.value("worldLandmarks", Json::array()));
    pose.segmentationMaskAvailable = value.value("segmentationMaskAvailable", false);
    return pose;
}

} // namespace

Json parseBridgeMessageJson(const std::string& message) {
    return Json::parse(message);
}

std::string messageType(const Json& value) {
    return value.value("type", "");
}

RuntimeStatus parseRuntimeStatus(const Json& value) {
    requireMessageType(value, "runtime_status");
    RuntimeStatus status;
    status.ready = value.value("ready", false);
    status.cameraReady = value.value("cameraReady", false);
    status.modelReady = value.value("modelReady", false);
    status.pipelineReady = value.value("pipelineReady", status.ready);
    status.activeDelegate = value.value("activeDelegate", "");
    status.fallback = value.value("fallback", false);
    status.reason = value.value("reason", "");
    status.stage = value.value("stage", "");
    status.detail = value.value("detail", "");
    status.wasmPath = value.value("wasmPath", "");

    const auto models = value.find("models");
    if (models != value.end() && models->is_object()) {
        status.models.hand = models->value("hand", false);
        status.models.pose = models->value("pose", false);
        status.models.face = models->value("face", false);
        status.models.gesture = models->value("gesture", false);
    }

    const auto gpu = value.find("gpu");
    if (gpu != value.end() && gpu->is_object()) {
        status.gpu.webglVendor = gpu->value("webglVendor", "");
        status.gpu.webglRenderer = gpu->value("webglRenderer", "");
        status.gpu.webglVersion = gpu->value("webglVersion", "");
        status.gpu.webglShadingLanguageVersion = gpu->value("webglShadingLanguageVersion", "");
    }
    status.processingWidth = value.value("processingWidth", 0);
    status.processingHeight = value.value("processingHeight", 0);
    return status;
}

RuntimeStatus parseRuntimeStatus(const std::string& message) {
    return parseRuntimeStatus(parseBridgeMessageJson(message));
}

RuntimeStats parseRuntimeStats(const Json& value, double receivedAtEpochMs) {
    RuntimeStats stats;
    stats.averageInferenceTimeMs = value.value("inferenceTimeMs", 0.0f);

    const auto statsValue = value.find("stats");
    if (statsValue == value.end() || !statsValue->is_object()) {
        return stats;
    }

    stats.sourceFPS = statsValue->value("sourceFPS", 0.0f);
    stats.inferenceFPS = statsValue->value("inferenceFPS", 0.0f);
    stats.averageInferenceTimeMs = statsValue->value("averageInferenceTimeMs", stats.averageInferenceTimeMs);

    const double explicitLatency = statsValue->value("bridgeLatencyMs", 0.0);
    const double sentAtEpochMs = statsValue->value("sentAtEpochMs", 0.0);
    if (receivedAtEpochMs > 0.0 && sentAtEpochMs > 0.0 && receivedAtEpochMs >= sentAtEpochMs) {
        stats.bridgeLatencyMs = static_cast<float>(receivedAtEpochMs - sentAtEpochMs);
    } else {
        stats.bridgeLatencyMs = static_cast<float>(explicitLatency);
    }
    return stats;
}

RuntimeStats parseRuntimeStats(const std::string& message, double receivedAtEpochMs) {
    return parseRuntimeStats(parseBridgeMessageJson(message), receivedAtEpochMs);
}

HandResult parseHandResult(const Json& value) {
    requireMessageType(value, "hand_result");
    HandResult result;
    result.timestampMs = value.value("timestampMs", 0.0);
    result.inferenceTimeMs = value.value("inferenceTimeMs", 0.0);

    const auto hands = value.find("hands");
    if (hands != value.end() && hands->is_array()) {
        result.hands.reserve(hands->size());
        for (const auto& handValue : *hands) {
            Hand hand;
            hand.handedness = handValue.value("handedness", "");
            hand.score = handValue.value("score", 0.0f);
            hand.landmarks = parseLandmarks(handValue.value("landmarks", Json::array()));
            hand.worldLandmarks = parseLandmarks(handValue.value("worldLandmarks", Json::array()));
            result.hands.push_back(std::move(hand));
        }
    }

    return result;
}

HandResult parseHandResult(const std::string& message) {
    return parseHandResult(parseBridgeMessageJson(message));
}

GestureResult parseGestureResult(const Json& value) {
    requireMessageType(value, "gesture_result");
    GestureResult result;
    result.timestampMs = value.value("timestampMs", 0.0);
    result.inferenceTimeMs = value.value("inferenceTimeMs", 0.0);

    const auto gestures = value.find("gestures");
    if (gestures != value.end() && gestures->is_array()) {
        result.gestures.reserve(gestures->size());
        for (const auto& gestureValue : *gestures) {
            Gesture gesture;
            gesture.handedness = gestureValue.value("handedness", "");
            gesture.handednessScore = gestureValue.value("handednessScore", 0.0f);
            gesture.categoryName = gestureValue.value("categoryName", "");
            gesture.displayName = gestureValue.value("displayName", "");
            gesture.score = gestureValue.value("score", 0.0f);
            gesture.landmarks = parseLandmarks(gestureValue.value("landmarks", Json::array()));
            gesture.worldLandmarks = parseLandmarks(gestureValue.value("worldLandmarks", Json::array()));
            result.gestures.push_back(std::move(gesture));
        }
    }

    return result;
}

GestureResult parseGestureResult(const std::string& message) {
    return parseGestureResult(parseBridgeMessageJson(message));
}

PoseResult parsePoseResult(const Json& value) {
    requireMessageType(value, "pose_result");
    PoseResult result;
    result.timestampMs = value.value("timestampMs", 0.0);
    result.inferenceTimeMs = value.value("inferenceTimeMs", 0.0);

    const auto poses = value.find("poses");
    if (poses != value.end() && poses->is_array()) {
        result.poses.reserve(poses->size());
        for (const auto& poseValue : *poses) {
            result.poses.push_back(parsePoseValue(poseValue));
        }
    } else {
        Pose pose = parsePoseValue(value);
        if (!pose.landmarks.empty() || !pose.worldLandmarks.empty() || pose.segmentationMaskAvailable) {
            result.poses.push_back(std::move(pose));
        }
    }

    if (!result.poses.empty()) {
        result.landmarks = result.poses.front().landmarks;
        result.worldLandmarks = result.poses.front().worldLandmarks;
        result.segmentationMaskAvailable = result.poses.front().segmentationMaskAvailable;
    }
    return result;
}

PoseResult parsePoseResult(const std::string& message) {
    return parsePoseResult(parseBridgeMessageJson(message));
}

FaceResult parseFaceResult(const Json& value) {
    requireMessageType(value, "face_result");
    FaceResult result;
    result.timestampMs = value.value("timestampMs", 0.0);
    result.inferenceTimeMs = value.value("inferenceTimeMs", 0.0);

    const auto faces = value.find("faces");
    if (faces != value.end() && faces->is_array()) {
        result.faces.reserve(faces->size());
        for (const auto& faceValue : *faces) {
            Face face;
            face.landmarks = parseLandmarks(faceValue.value("landmarks", Json::array()));

            const auto blendshapes = faceValue.find("blendshapes");
            if (blendshapes != faceValue.end() && blendshapes->is_object()) {
                for (const auto& [name, score] : blendshapes->items()) {
                    face.blendshapes[name] = score.get<float>();
                }
            }

            const auto matrix = faceValue.find("facialTransformationMatrix");
            if (matrix != faceValue.end() && matrix->is_array()) {
                const size_t count = std::min<size_t>(matrix->size(), face.facialTransformationMatrix.size());
                for (size_t i = 0; i < count; ++i) {
                    face.facialTransformationMatrix[i] = (*matrix)[i].get<float>();
                }
            }

            result.faces.push_back(std::move(face));
        }
    }

    return result;
}

FaceResult parseFaceResult(const std::string& message) {
    return parseFaceResult(parseBridgeMessageJson(message));
}

} // namespace tcx::mediapipe
