#pragma once

#include <array>
#include <utility>

namespace tcx::mediapipe {

inline constexpr std::array<std::pair<int, int>, 18> kPoseConnections = {{
    {11, 12}, {11, 13}, {13, 15}, {12, 14}, {14, 16},
    {11, 23}, {12, 24}, {23, 24}, {23, 25}, {25, 27},
    {24, 26}, {26, 28}, {27, 29}, {29, 31}, {28, 30},
    {30, 32}, {15, 17}, {16, 18}
}};

} // namespace tcx::mediapipe
