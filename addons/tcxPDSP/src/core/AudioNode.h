#pragma once
// =============================================================================
// tcxPDSP AudioNode — Base class for all DSP modules
// =============================================================================

#include "core/AudioContext.h"
#include "core/AudioBuffer.h"
#include "core/PatchNode.h"
#include <algorithm>

namespace tcx::pdsp {

class AudioNode {
public:
    virtual ~AudioNode() = default;

    virtual void prepare(AudioContext& ctx) = 0;
    virtual void process(AudioContext& ctx, int frames) = 0;

    bool isPrepared() const { return prepared_; }
    bool isActive()   const { return active_; }
    void setActive(bool a) { active_ = a; }

    AudioBuffer& output() { return outputBuffer; }
    const AudioBuffer& output() const { return outputBuffer; }

protected:
    bool copyConnectedInput(const PatchNode& input, int frames, int outputChannels = 1) {
        if (outputBuffer.empty()) return false;

        PatchNode* source = input.source();
        if (!source || !source->owner()) return false;

        const AudioBuffer& src = source->owner()->output();
        if (src.empty()) return false;

        outputBuffer.clear();

        const int channels = std::min(outputChannels, outputBuffer.channels());
        const int copyFrames = std::min(frames, std::min(outputBuffer.frames(), src.frames()));

        for (int ch = 0; ch < channels; ch++) {
            const int srcCh = std::min(source->channel() + ch, src.channels() - 1);
            std::copy_n(src.channel(srcCh), copyFrames, outputBuffer.channel(ch));
        }

        return true;
    }

    AudioBuffer outputBuffer;
    bool prepared_ = false;
    bool active_   = true;
};

} // namespace tcx::pdsp
