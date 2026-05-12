#pragma once

namespace tcx::flow {

struct OpticalFlowSettings {
    float strength = 1.0f;
    float offset = 1.0f;
    float lambda = 0.01f;
    float threshold = 0.0f;
    float decay = 0.95f;
    float blurRadius = 2.0f;
    float flowScale = 1.0f;
    float temporalSmoothing = 0.5f;
    bool normalizeInput = true;
    bool mirrorX = false;
    bool mirrorY = false;
    bool useLuminance = true;
};

} // namespace tcx::flow
