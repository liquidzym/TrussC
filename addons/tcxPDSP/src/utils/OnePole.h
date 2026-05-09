#pragma once
// One-pole low-pass / high-pass utility filter.

#include <cmath>

namespace tcx::pdsp {

class OnePole {
public:
    enum class Mode { LowPass, HighPass };

    void prepare(int sampleRate, float cutoffHz = 1000.0f, Mode mode = Mode::LowPass) {
        sampleRate_ = sampleRate;
        mode_ = mode;
        setCutoff(cutoffHz);
        z_ = 0.0f;
    }

    void setCutoff(float hz) {
        if (hz < 1.0f) hz = 1.0f;
        float x = std::exp(-6.283185307f * hz / static_cast<float>(sampleRate_));
        a_ = 1.0f - x;
    }

    void setMode(Mode mode) { mode_ = mode; }

    float process(float input) {
        z_ += a_ * (input - z_);
        return mode_ == Mode::LowPass ? z_ : input - z_;
    }

    void reset(float value = 0.0f) { z_ = value; }

private:
    int sampleRate_ = 48000;
    Mode mode_ = Mode::LowPass;
    float a_ = 0.1f;
    float z_ = 0.0f;
};

} // namespace tcx::pdsp
