#pragma once
// =============================================================================
// tcxPDSP FilterSVF — State-Variable Filter (LP/HP/BP/Notch)
// =============================================================================
// Based on Andrew Simper's stable SVF topology.

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
#include <algorithm>

namespace tcx::pdsp {

class FilterSVF : public AudioNode {
public:
    enum class Mode { LowPass, HighPass, BandPass, Notch };

    FilterSVF() : input_(this, 0), output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        sampleRate_ = static_cast<float>(ctx.sampleRate);
        outputBuffer.allocate(1, ctx.bufferSize);
        cutoff_.prepare(ctx.sampleRate, 10.0f);
        resonance_.prepare(ctx.sampleRate, 10.0f);
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);
        copyConnectedInput(input_, frames);

        float* out = outputBuffer.channel(0);
        float lp = lp_, bp = bp_;

        for (int i = 0; i < frames; i++) {
            float f = std::max(20.0f, std::min(cutoff_.next(), sampleRate_ * 0.49f));
            float q = std::max(0.5f, std::min(resonance_.next(), 25.0f));

            // Compute filter coefficients
            float g = std::tan(3.14159265f * f / sampleRate_);
            float R = 1.0f / q;
            float gDiv = 1.0f / (1.0f + g * (g + R));

            float hp = (out[i] - (2.0f * R + g) * bp - lp) * gDiv;
            bp = g * hp + bp;
            lp = g * bp + lp;

            switch (mode_) {
                case Mode::LowPass:  out[i] = lp; break;
                case Mode::HighPass: out[i] = hp; break;
                case Mode::BandPass: out[i] = bp; break;
                case Mode::Notch:    out[i] = hp + lp; break;
            }
        }

        lp_ = lp; bp_ = bp;
    }

    PatchNode& in()  { return input_; }
    PatchNode& out() { return output_; }

    void setCutoff(float hz)    { cutoff_.set(hz); }
    void setResonance(float q)  { resonance_.set(q); }
    void setMode(Mode m)        { mode_ = m; }

private:
    Parameter cutoff_{1000.0f};
    Parameter resonance_{0.707f};
    Mode mode_ = Mode::LowPass;
    PatchNode input_;
    PatchNode output_;
    float sampleRate_ = 48000.0f;
    float lp_ = 0.0f, bp_ = 0.0f;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
};

} // namespace tcx::pdsp
