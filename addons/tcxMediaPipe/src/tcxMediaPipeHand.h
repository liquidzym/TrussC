#pragma once

#include <array>
#include <utility>

namespace tcx::mediapipe {

inline constexpr std::array<std::pair<int, int>, 20> kHandConnections = {{
    {0, 1}, {1, 2}, {2, 3}, {3, 4},
    {0, 5}, {5, 6}, {6, 7}, {7, 8},
    {5, 9}, {9, 10}, {10, 11}, {11, 12},
    {9, 13}, {13, 14}, {14, 15}, {15, 16},
    {0, 17}, {17, 18}, {18, 19}, {19, 20}
}};

} // namespace tcx::mediapipe
