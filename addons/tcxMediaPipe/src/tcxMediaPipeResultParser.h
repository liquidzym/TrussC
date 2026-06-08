#pragma once

#include "tcxMediaPipeResult.h"

#include "nlohmann/json.hpp"

#include <string>

namespace tcx::mediapipe {

using Json = nlohmann::json;

Json parseBridgeMessageJson(const std::string& message);
std::string messageType(const Json& value);
RuntimeStatus parseRuntimeStatus(const Json& value);
RuntimeStats parseRuntimeStats(const Json& value, double receivedAtEpochMs = 0.0);
HandResult parseHandResult(const Json& value);
GestureResult parseGestureResult(const Json& value);
PoseResult parsePoseResult(const Json& value);
FaceResult parseFaceResult(const Json& value);

RuntimeStatus parseRuntimeStatus(const std::string& message);
RuntimeStats parseRuntimeStats(const std::string& message, double receivedAtEpochMs = 0.0);
HandResult parseHandResult(const std::string& message);
GestureResult parseGestureResult(const std::string& message);
PoseResult parsePoseResult(const std::string& message);
FaceResult parseFaceResult(const std::string& message);

inline RuntimeStatus parseRuntimeStatus(const char* message) {
    return parseRuntimeStatus(std::string(message));
}

inline RuntimeStats parseRuntimeStats(const char* message, double receivedAtEpochMs = 0.0) {
    return parseRuntimeStats(std::string(message), receivedAtEpochMs);
}

inline HandResult parseHandResult(const char* message) {
    return parseHandResult(std::string(message));
}

inline GestureResult parseGestureResult(const char* message) {
    return parseGestureResult(std::string(message));
}

inline PoseResult parsePoseResult(const char* message) {
    return parsePoseResult(std::string(message));
}

inline FaceResult parseFaceResult(const char* message) {
    return parseFaceResult(std::string(message));
}

} // namespace tcx::mediapipe
