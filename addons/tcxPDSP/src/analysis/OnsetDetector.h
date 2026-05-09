#pragma once
// Simple energy-delta onset detector.

#include <cmath>

namespace tcx::pdsp {

class OnsetDetector {
public:
    void prepare(float threshold = 0.08f, float decay = 0.98f) {
        threshold_ = threshold;
        decay_ = decay;
        previousEnergy_ = 0.0f;
        envelope_ = 0.0f;
    }

    bool process(const float* input, int frames) {
        if (!input || frames <= 0) return false;

        float energy = 0.0f;
        for (int i = 0; i < frames; i++) energy += input[i] * input[i];
        energy = std::sqrt(energy / static_cast<float>(frames));

        envelope_ = std::max(energy, envelope_ * decay_);
        float delta = energy - previousEnergy_;
        previousEnergy_ = energy;
        return delta > threshold_ && energy >= envelope_ * 0.75f;
    }

private:
    float threshold_ = 0.08f;
    float decay_ = 0.98f;
    float previousEnergy_ = 0.0f;
    float envelope_ = 0.0f;
};

} // namespace tcx::pdsp
