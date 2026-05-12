#pragma once

#include <TrussC.h>
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace tcx::flow {

using TextureFormat = tc::TextureFormat;

struct FlowSize {
    int width = 0;
    int height = 0;

    bool valid() const { return width > 0 && height > 0; }
};

struct BridgeSettings {
    float threshold = 0.0f;
    float gain = 1.0f;
    float decay = 0.95f;
    float blurRadius = 0.0f;
    float velocityScale = 1.0f;
    float densityScale = 1.0f;
    float temperatureScale = 1.0f;
    bool invert = false;
    bool useAlphaAsMask = false;
    bool mirrorX = false;
    bool mirrorY = false;
};

inline float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

} // namespace tcx::flow
