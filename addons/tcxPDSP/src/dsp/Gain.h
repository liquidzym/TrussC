#pragma once
// =============================================================================
// tcxPDSP Gain — Simple volume control
// =============================================================================

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"

namespace tcx::pdsp {

class Gain : public AudioNode {
public:
    Gain() : input_(this, 0), output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        outputBuffer.allocate(1, ctx.bufferSize);
        gain_.prepare(ctx.sampleRate, 5.0f);
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);
        copyConnectedInput(input_, frames);

        float* out = outputBuffer.channel(0);

        for (int i = 0; i < frames; i++) {
            float g = gain_.next();
            out[i] *= g;  // Process in-place on output buffer (data copied in via input)
        }
    }

    PatchNode& in()  { return input_; }
    PatchNode& out() { return output_; }

    void setGain(float value) { gain_.set(value); }

private:
    Parameter gain_{1.0f};
    PatchNode input_;
    PatchNode output_;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
};

} // namespace tcx::pdsp
