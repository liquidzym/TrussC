#pragma once
// =============================================================================
// tcxPDSP Oscillator — Abstract base for all oscillators
// =============================================================================
// Provides shared phase accumulator, frequency management, and waveform
// generation. Subclasses override processSample().

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>

namespace tcx::pdsp {

class Oscillator : public AudioNode {
public:
    Oscillator() : output_(this, 0) {
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
        float sr = sampleRate_;

        for (int i = 0; i < frames; i++) {
            float freq = frequency_.next();
            out[i] = processSample(phase_);
            phase_ += freq / sr;
            if (phase_ >= 1.0f) phase_ -= std::floor(phase_);
        }
    }

    PatchNode& out() { return output_; }

    void setFrequency(float hz) { frequency_.set(hz); }
    void setPhase(float normalized) { phase_ = normalized - std::floor(normalized); }

    float getFrequency() const { return frequency_.getTarget(); }
    float getPhase()     const { return phase_; }

protected:
    virtual float processSample(float phase) = 0;

    Parameter frequency_{440.0f};
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;

private:
    PatchNode output_;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
};

// Built-in oscillator types using the base class

class SineOscillator : public Oscillator {
protected:
    float processSample(float phase) override {
        return std::sin(phase * 6.283185307f);
    }
};

class TriangleOscillator : public Oscillator {
protected:
    float processSample(float phase) override {
        return phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
    }
};

class SawOscillator : public Oscillator {
protected:
    float processSample(float phase) override {
        return 2.0f * phase - 1.0f;
    }
};

class SquareOscillator : public Oscillator {
protected:
    float processSample(float phase) override {
        return phase < 0.5f ? 1.0f : -1.0f;
    }
};

} // namespace tcx::pdsp
