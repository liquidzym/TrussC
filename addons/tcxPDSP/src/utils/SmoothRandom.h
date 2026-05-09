#pragma once
// Smooth random generator for control-rate modulation.

#include "utils/Random.h"
#include <algorithm>

namespace tcx::pdsp {

class SmoothRandom {
public:
    void prepare(int sampleRate, float rateHz = 1.0f) {
        sampleRate_ = sampleRate;
        setRate(rateHz);
        current_ = target_ = nextTarget();
        phase_ = 0;
    }

    void setRate(float hz) {
        if (hz < 0.001f) hz = 0.001f;
        periodSamples_ = std::max(1, static_cast<int>(sampleRate_ / hz));
    }

    float process() {
        if (phase_ >= periodSamples_) {
            current_ = target_;
            target_ = nextTarget();
            phase_ = 0;
        }
        float t = static_cast<float>(phase_) / static_cast<float>(periodSamples_);
        t = t * t * (3.0f - 2.0f * t);
        phase_++;
        return current_ + (target_ - current_) * t;
    }

private:
    float nextTarget() { return rng_.next() * 2.0f - 1.0f; }

    int sampleRate_ = 48000;
    int periodSamples_ = 48000;
    int phase_ = 0;
    float current_ = 0.0f;
    float target_ = 0.0f;
    Random rng_;
};

} // namespace tcx::pdsp
