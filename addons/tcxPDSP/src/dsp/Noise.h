#pragma once
// =============================================================================
// tcxPDSP Noise — White and pink noise generator
// =============================================================================

#include "core/AudioNode.h"
#include "core/PatchNode.h"

namespace tcx::pdsp {

class Noise : public AudioNode {
public:
    enum class Type { White, Pink };

    Noise() : output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        outputBuffer.allocate(1, ctx.bufferSize);
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);

        float* out = outputBuffer.channel(0);

        if (type_ == Type::White) {
            for (int i = 0; i < frames; i++) {
                seed_ = seed_ * 1103515245 + 12345;
                out[i] = ((seed_ >> 16) & 0x7FFF) / 16383.5f - 1.0f;
            }
        } else {
            // Pink noise (Paul Kellet's refined method)
            for (int i = 0; i < frames; i++) {
                seed_ = seed_ * 1103515245 + 12345;
                float white = ((seed_ >> 16) & 0x7FFF) / 16383.5f - 1.0f;

                b0_ = 0.99886f  * b0_ + white * 0.0555179f;
                b1_ = 0.99332f  * b1_ + white * 0.0750759f;
                b2_ = 0.96900f  * b2_ + white * 0.1538520f;
                b3_ = 0.86650f  * b3_ + white * 0.3104856f;
                b4_ = 0.55000f  * b4_ + white * 0.5329522f;
                b5_ = -0.7616f  * b5_ - white * 0.0168980f;

                float pink = b0_ + b1_ + b2_ + b3_ + b4_ + b5_ + b6_ + white * 0.5362f;
                b6_ = white * 0.115926f;
                out[i] = pink * 0.11f;
            }
        }
    }

    void setType(Type t) { type_ = t; }
    PatchNode& out() { return output_; }

private:
    Type type_ = Type::White;
    PatchNode output_;
    uint32_t seed_ = 12345;
    float b0_=0, b1_=0, b2_=0, b3_=0, b4_=0, b5_=0, b6_=0;

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) {
            outputBuffer.allocate(1, frames);
        }
    }
};

} // namespace tcx::pdsp
