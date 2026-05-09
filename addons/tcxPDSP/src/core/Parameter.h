#pragma once
// =============================================================================
// tcxPDSP Parameter — Thread-safe parameter with audio-rate smoothing
// =============================================================================
// set() from main thread → next() in audio thread.
// No locks in audio path — uses atomic for target.

#include "core/SmoothedValue.h"
#include <atomic>

namespace tcx::pdsp {

class Parameter {
public:
    explicit Parameter(float defaultValue = 0.0f)
        : target_(defaultValue), smoother_(defaultValue) {}

    void set(float value) {
        target_.store(value, std::memory_order_relaxed);
    }

    float getTarget() const {
        return target_.load(std::memory_order_relaxed);
    }

    // Call from main thread before audio starts (sets up smoother)
    void prepare(int sampleRate, float smoothingMs = 10.0f) {
        sampleRate_ = sampleRate;
        smoothingMs_ = smoothingMs;
        float t = target_.load(std::memory_order_relaxed);
        smoother_.reset(t);  // start from current target, no jump
        smoother_.setTime(smoothingMs, sampleRate);
    }

    float next() {
        float t = target_.load(std::memory_order_relaxed);
        if (t != smoother_.getTarget()) {
            smoother_.setTarget(t);
            smoother_.setTime(smoothingMs_, sampleRate_);
        }
        return smoother_.next();
    }

private:
    std::atomic<float> target_;
    SmoothedValue      smoother_;
    float smoothingMs_ = 10.0f;
    int   sampleRate_  = 48000;
};

} // namespace tcx::pdsp
