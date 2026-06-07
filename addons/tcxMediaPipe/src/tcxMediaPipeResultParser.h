#pragma once

#include "tcxMediaPipeResult.h"

#include <string>

namespace tcx::mediapipe {

RuntimeStatus parseRuntimeStatus(const std::string& message);
RuntimeStats parseRuntimeStats(const std::string& message, double receivedAtEpochMs = 0.0);
HandResult parseHandResult(const std::string& message);
GestureResult parseGestureResult(const std::string& message);
PoseResult parsePoseResult(const std::string& message);
FaceResult parseFaceResult(const std::string& message);

} // namespace tcx::mediapipe
