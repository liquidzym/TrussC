#pragma once
// Autocorrelation pitch detector for mono buffers.

#include <cmath>
#include <algorithm>

namespace tcx::pdsp {

class PitchDetector {
public:
    void prepare(int sampleRate, float minHz = 50.0f, float maxHz = 2000.0f) {
        sampleRate_ = sampleRate;
        minHz_ = std::max(1.0f, minHz);
        maxHz_ = std::max(minHz_, maxHz);
    }

    float process(const float* input, int frames) const {
        if (!input || frames <= 2 || sampleRate_ <= 0) return 0.0f;

        int minLag = std::max(1, static_cast<int>(sampleRate_ / maxHz_));
        int maxLag = std::min(frames - 1, static_cast<int>(sampleRate_ / minHz_));
        if (maxLag <= minLag) return 0.0f;

        float scores[4096];
        int scoreCount = std::min(maxLag + 1, 4096);
        for (int i = 0; i < scoreCount; i++) scores[i] = 0.0f;

        float best = 0.0f;
        int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; lag++) {
            float corr = 0.0f;
            float energyA = 0.0f;
            float energyB = 0.0f;
            for (int i = 0; i < frames - lag; i++) {
                corr += input[i] * input[i + lag];
                energyA += input[i] * input[i];
                energyB += input[i + lag] * input[i + lag];
            }
            float denom = std::sqrt(energyA * energyB);
            float score = denom > 0.0f ? corr / denom : 0.0f;
            if (lag < scoreCount) scores[lag] = score;
            if (score > best) {
                best = score;
                bestLag = lag;
            }
        }

        for (int lag = minLag + 1; lag < std::min(maxLag, scoreCount - 1); lag++) {
            if (scores[lag] > 0.75f && scores[lag] >= scores[lag - 1] && scores[lag] >= scores[lag + 1]) {
                bestLag = lag;
                break;
            }
        }

        return bestLag > 0 ? static_cast<float>(sampleRate_) / static_cast<float>(bestLag) : 0.0f;
    }

private:
    int sampleRate_ = 48000;
    float minHz_ = 50.0f;
    float maxHz_ = 2000.0f;
};

} // namespace tcx::pdsp
