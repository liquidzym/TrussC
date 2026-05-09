#pragma once
// =============================================================================
// tcxPDSP Mixer — Multi-channel summing mixer with per-channel level & pan
// =============================================================================
// Each channel is mono in, stereo out with gain + pan.
// Input data arrives via Processor graph routing or direct buffer access.

#include "core/AudioNode.h"
#include "core/AudioBuffer.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

namespace tcx::pdsp {

class Mixer : public AudioNode {
public:
    struct Channel {
        PatchNode input;
        AudioBuffer buffer;   // mono input buffer per channel
        Parameter level{1.0f};
        Parameter pan{0.0f};  // -1=left, 0=center, 1=right

        Channel() = default;
    };

    int addInput() {
        int idx = static_cast<int>(channels_.size());
        channels_.push_back(std::make_unique<Channel>());
        return idx;
    }

    PatchNode& in(int index)  { return channels_[index]->input; }
    PatchNode& out()          { return outNode_; }

    void setLevel(int index, float level) { channels_[index]->level.set(level); }
    void setPan(int index, float pan)     { channels_[index]->pan.set(pan); }

    // Direct audio input (call from routing system or user code)
    AudioBuffer& inputBuffer(int index) { return channels_[index]->buffer; }

    int numChannels() const { return static_cast<int>(channels_.size()); }

    void prepare(AudioContext& ctx) override {
        for (auto& ch : channels_) {
            ch->buffer.allocate(1, ctx.bufferSize);
            ch->level.prepare(ctx.sampleRate, 5.0f);
            ch->pan.prepare(ctx.sampleRate, 10.0f);
        }
        outputBuffer.allocate(2, ctx.bufferSize);
        prepared_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);

        float* outL = outputBuffer.channel(0);
        float* outR = outputBuffer.channel(1);

        // Clear outputs
        for (int i = 0; i < frames; i++) {
            outL[i] = 0.0f;
            outR[i] = 0.0f;
        }

        // Sum all channels
        for (auto& ch : channels_) {
            if (ch->buffer.frames() < frames) {
                ch->buffer.allocate(1, frames);
            }
            if (auto* source = ch->input.source()) {
                if (auto* owner = source->owner()) {
                    const auto& srcBuffer = owner->output();
                    if (!srcBuffer.empty()) {
                        ch->buffer.clear();
                        int srcCh = std::min(source->channel(), srcBuffer.channels() - 1);
                        int copyFrames = std::min(frames, srcBuffer.frames());
                        std::copy_n(srcBuffer.channel(srcCh), copyFrames, ch->buffer.channel(0));
                    }
                }
            }

            float lvl = ch->level.next();
            float pan = ch->pan.next();
            // Constant-power pan law: gainL² + gainR² = constant
            float gainL = lvl * std::sqrt(0.5f * (1.0f - pan));
            float gainR = lvl * std::sqrt(0.5f * (1.0f + pan));

            const float* src = ch->buffer.channel(0);
            for (int i = 0; i < frames; i++) {
                outL[i] += src[i] * gainL;
                outR[i] += src[i] * gainR;
            }
        }

        // Soft clip
        for (int i = 0; i < frames; i++) {
            if (outL[i] >  1.0f) outL[i] =  1.0f;
            if (outL[i] < -1.0f) outL[i] = -1.0f;
            if (outR[i] >  1.0f) outR[i] =  1.0f;
            if (outR[i] < -1.0f) outR[i] = -1.0f;
        }
    }

private:
    std::vector<std::unique_ptr<Channel>> channels_;
    PatchNode outNode_{this, 0};

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames || outputBuffer.channels() < 2) {
            outputBuffer.allocate(2, frames);
        }
    }
};

} // namespace tcx::pdsp
