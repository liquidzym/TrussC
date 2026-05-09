#pragma once
// =============================================================================
// tcxPDSP EnvelopeFollower — Smooth envelope tracker (audio-reactive bridge)
// =============================================================================
// processSample() from audio thread → value() from main thread.
// env_ is atomic for lock-free cross-thread read.

#include <atomic>
#include <cmath>

namespace tcx::pdsp {

class EnvelopeFollower {
public:
    void prepare(int sampleRate) {
        sampleRate_ = static_cast<float>(sampleRate);
        env_.store(0.0f, std::memory_order_relaxed);
    }

    // Call from main thread before audio starts
    void setAttack(float seconds) {
        attackCoeff_ = 1.0f - std::exp(-1.0f / (seconds * sampleRate_));
    }

    void setRelease(float seconds) {
        releaseCoeff_ = 1.0f - std::exp(-1.0f / (seconds * sampleRate_));
    }

    // Audio thread
    float processSample(float x) {
        float e = env_.load(std::memory_order_relaxed);
        float absX = x > 0 ? x : -x;
        if (absX > e) {
            e += attackCoeff_ * (absX - e);
        } else {
            e += releaseCoeff_ * (absX - e);
        }
        env_.store(e, std::memory_order_relaxed);
        return e;
    }

    // Main thread
    float value() const {
        return env_.load(std::memory_order_relaxed);
    }

private:
    float sampleRate_ = 48000.0f;
    std::atomic<float> env_{0.0f};
    float attackCoeff_  = 0.01f;   // set before audio starts
    float releaseCoeff_ = 0.001f;  // set before audio starts
};

} // namespace tcx::pdsp
