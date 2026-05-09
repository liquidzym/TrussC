#pragma once
// Control-rate helper: recompute a control value every N audio samples and hold
// between updates.

#include <algorithm>
#include <functional>

namespace tcx::pdsp {

class ControlRate {
public:
    void prepare(int sampleRate, float hz = 100.0f) {
        sampleRate_ = sampleRate;
        setRate(hz);
        counter_ = periodSamples_;
    }

    void setRate(float hz) {
        hz = std::max(1.0f, hz);
        periodSamples_ = std::max(1, static_cast<int>(static_cast<float>(sampleRate_) / hz));
    }

    template<typename Fn>
    float process(Fn&& generator) {
        if (counter_ >= periodSamples_) {
            value_ = generator();
            counter_ = 0;
        }
        counter_++;
        return value_;
    }

    float value() const { return value_; }

private:
    int sampleRate_ = 48000;
    int periodSamples_ = 480;
    int counter_ = 480;
    float value_ = 0.0f;
};

} // namespace tcx::pdsp
