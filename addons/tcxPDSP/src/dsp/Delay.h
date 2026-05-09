#pragma once
// =============================================================================
// tcxPDSP Delay — Simple delay line with feedback
// =============================================================================

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <vector>
#include <algorithm>

namespace tcx::pdsp {

class Delay : public AudioNode {
public:
    Delay() : input_(this, 0), output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        sampleRate_ = static_cast<float>(ctx.sampleRate);
        outputBuffer.allocate(1, ctx.bufferSize);
        // Max 2 seconds delay
        delayBuffer_.resize(static_cast<size_t>(sampleRate_ * 2.1f), 0.0f);
        writePos_ = 0;
        time_.prepare(ctx.sampleRate, 20.0f);
        feedback_.prepare(ctx.sampleRate, 20.0f);
        wet_.prepare(ctx.sampleRate, 10.0f);
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);
        copyConnectedInput(input_, frames);

        float* out = outputBuffer.channel(0);
        int bufLen = static_cast<int>(delayBuffer_.size());

        for (int i = 0; i < frames; i++) {
            float fb = feedback_.next();
            float w  = wet_.next();
            float t = time_.next();
            float delaySamples = t * sampleRate_;
            float readPos = writePos_ - delaySamples;
            if (readPos < 0) readPos += bufLen;
            int r0 = static_cast<int>(readPos) % bufLen;
            int r1 = (r0 + 1) % bufLen;
            float frac = readPos - std::floor(readPos);

            float delayed = delayBuffer_[r0] * (1.0f - frac) + delayBuffer_[r1] * frac;
            float wetSig = delayed * w;
            float drySig = out[i] * (1.0f - w);

            // Write to delay buffer (input + feedback)
            delayBuffer_[writePos_] = out[i] + delayed * fb;
            writePos_ = (writePos_ + 1) % bufLen;

            out[i] = drySig + wetSig;
        }
    }

    PatchNode& in()  { return input_; }
    PatchNode& out() { return output_; }

    void setDelayTime(float seconds) { time_.set(seconds); }
    void setFeedback(float value)    { feedback_.set(std::max(0.0f, std::min(value, 0.99f))); }
    void setWet(float value)         { wet_.set(std::max(0.0f, std::min(value, 1.0f))); }

private:
    Parameter time_{0.3f};
    Parameter feedback_{0.3f};
    Parameter wet_{0.5f};
    PatchNode input_, output_;
    std::vector<float> delayBuffer_;
    int writePos_ = 0;
    float sampleRate_ = 48000.0f;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
};

} // namespace tcx::pdsp
