#pragma once
// =============================================================================
// tcxPDSP SmoothedValue — Linear ramp for click-free parameter changes
// =============================================================================
// Audio-thread safe: call next() in process(), setTarget() from anywhere.

#include <cmath>
#include <algorithm>

namespace tcx::pdsp {

class SmoothedValue {
public:
    SmoothedValue() = default;

    explicit SmoothedValue(float v) : current_(v), target_(v) {}

    void reset(float value) {
        current_   = value;
        target_    = value;
        step_      = 0.0f;
        remaining_ = 0;
    }

    void setTarget(float value) {
        target_ = value;
    }

    void setTime(float milliseconds, int sampleRate) {
        if (milliseconds <= 0.0f) {
            current_ = target_;
            step_ = 0.0f;
            remaining_ = 0;
            return;
        }
        float samples = sampleRate * milliseconds * 0.001f;
        remaining_ = static_cast<int>(samples);
        if (remaining_ > 0) {
            step_ = (target_ - current_) / static_cast<float>(remaining_);
        } else {
            current_ = target_;
            step_ = 0.0f;
        }
    }

    float next() {
        if (remaining_ > 0) {
            current_ += step_;
            remaining_--;
            if (remaining_ == 0) {
                current_ = target_;
            }
        }
        return current_;
    }

    float getCurrent() const { return current_; }
    float getTarget()  const { return target_; }

private:
    float current_   = 0.0f;
    float target_    = 0.0f;
    float step_      = 0.0f;
    int   remaining_ = 0;
};

} // namespace tcx::pdsp
