#pragma once
// =============================================================================
// tcxPDSP SineOsc — Band-limited sine wave oscillator
// =============================================================================

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>

namespace tcx::pdsp {

class SineOsc : public AudioNode {
public:
    SineOsc() : output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        sampleRate_ = static_cast<float>(ctx.sampleRate);
        outputBuffer.allocate(1, ctx.bufferSize);
        frequency_.prepare(ctx.sampleRate, 5.0f);
        phase_ = 0.0f;
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);

        float* out = outputBuffer.channel(0);
        float freq = frequency_.next();
        float sr = sampleRate_;
        float phaseIncrement = freq / sr;

        for (int i = 0; i < frames; i++) {
            out[i] = std::sin(phase_ * 6.283185307f);
            phase_ += phaseIncrement;
            if (phase_ >= 1.0f) phase_ -= std::floor(phase_);
        }
    }

    PatchNode& out() { return output_; }

    void setFrequency(float hz) {
        frequency_.set(hz);
    }

    void setPhase(float normalized) {
        phase_ = normalized - std::floor(normalized);
    }

private:
    Parameter frequency_{440.0f};
    PatchNode output_;
    float phase_ = 0.0f;
    float sampleRate_ = 48000.0f;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
};

} // namespace tcx::pdsp
