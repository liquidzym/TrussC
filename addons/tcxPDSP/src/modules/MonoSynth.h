#pragma once
// =============================================================================
// tcxPDSP MonoSynth — Subtractive monosynth (osc → filter → envelope)
// =============================================================================

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "dsp/SineOsc.h"
#include "dsp/FilterSVF.h"
#include "dsp/ADSR.h"

namespace tcx::pdsp {

class MonoSynth : public AudioNode {
public:
    MonoSynth() : output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        sampleRate_ = static_cast<float>(ctx.sampleRate);
        outputBuffer.allocate(1, ctx.bufferSize);
        osc_.prepare(ctx);
        filter_.prepare(ctx);
        ampEnv_.setSampleRate(sampleRate_);
        filterEnv_.setSampleRate(sampleRate_);
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);

        float* out = outputBuffer.channel(0);
        osc_.process(ctx, frames);
        float* oscOut = osc_.output().channel(0);

        // Write osc+envelope → filter's output buffer (filter processes in-place)
        float* filtBuf = filter_.output().channel(0);
        ensureFilterBuffer(frames);
        for (int i = 0; i < frames; i++) {
            filtBuf[i] = oscOut[i] * ampEnv_.process() * velocity_;
        }

        // Filter processes in-place on its own buffer
        filter_.process(ctx, frames);

        // Copy result to our output
        for (int i = 0; i < frames; i++) {
            out[i] = filtBuf[i];
        }
    }

    void noteOn(int midiNote, float velocity) {
        float freq = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
        osc_.setFrequency(freq);
        ampEnv_.noteOn();
        filterEnv_.noteOn();
        velocity_ = velocity;
    }

    void noteOff() {
        ampEnv_.noteOff();
        filterEnv_.noteOff();
    }

    void setFrequency(float hz)        { osc_.setFrequency(hz); }
    void setCutoff(float hz)           { filter_.setCutoff(hz); }
    void setResonance(float q)         { filter_.setResonance(q); }
    void setFilterMode(FilterSVF::Mode m) { filter_.setMode(m); }
    void setAttack(float s)            { ampEnv_.setAttack(s); }
    void setDecay(float s)             { ampEnv_.setDecay(s); }
    void setSustain(float lvl)         { ampEnv_.setSustain(lvl); }
    void setRelease(float s)           { ampEnv_.setRelease(s); }

    PatchNode& out() { return output_; }

private:
    SineOsc osc_;
    FilterSVF filter_;
    ADSR ampEnv_;
    ADSR filterEnv_;
    PatchNode output_;
    float sampleRate_ = 48000.0f;
    float velocity_ = 1.0f;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
    void ensureFilterBuffer(int frames) {
        auto& fb = filter_.output();
        if (fb.frames() < frames) {
            fb.allocate(1, frames);
        }
    }
};

} // namespace tcx::pdsp
