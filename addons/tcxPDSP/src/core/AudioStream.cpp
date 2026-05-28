// =============================================================================
// tcxPDSP AudioStream implementation (TrussC AudioEngine hook)
// =============================================================================

#include "core/AudioStream.h"
#include <tc/sound/tcSound.h>
#include <algorithm>
#include <cstdio>

namespace tcx::pdsp {

struct AudioStream::Impl {
    AudioCallback callback;
    AudioStreamSettings settings;
    AudioContext ctx;
    tc::EventListener outputListener;
    bool running = false;
    bool initialized = false;

    void process(tc::AudioOutBuffer& buffer) {
        if (!callback || !buffer.data) return;

        const int blockSize = std::max(1, ctx.bufferSize > 0 ? ctx.bufferSize : buffer.frameCount);
        int processed = 0;
        while (processed < buffer.frameCount) {
            const int blockFrames = std::min(blockSize, buffer.frameCount - processed);
            callback(
                buffer.data + processed * buffer.channels,
                blockFrames,
                buffer.channels
            );
            ctx.advance(static_cast<uint64_t>(blockFrames));
            processed += blockFrames;
        }
    }
};

AudioStream::AudioStream() : impl_(std::make_unique<Impl>()) {}

AudioStream::~AudioStream() {
    stop();
}

bool AudioStream::setup(const AudioStreamSettings& settings) {
    stop();

    impl_->settings = settings;
    impl_->initialized = false;

    tc::AudioSettings engineSettings;
    engineSettings.sampleRate = settings.sampleRate;
    engineSettings.channels = settings.outputChannels;
    engineSettings.bufferSize = settings.bufferSize;

    auto& engine = tc::AudioEngine::getInstance();
    if (!engine.init(engineSettings)) {
        fprintf(stderr, "[tcxPDSP] AudioStream: TrussC AudioEngine init failed\n");
        return false;
    }

    impl_->ctx.sampleRate = engine.getSampleRate();
    impl_->ctx.bufferSize = settings.bufferSize > 0 ? settings.bufferSize : engine.getBufferSize();
    impl_->ctx.outputChannels = engine.getChannels();
    impl_->ctx.currentSample = 0;

    impl_->initialized = true;
    fprintf(stderr, "[tcxPDSP] AudioStream: attached to TrussC AudioEngine (%d Hz, %d ch)\n",
            impl_->ctx.sampleRate, impl_->ctx.outputChannels);
    return true;
}

void AudioStream::setCallback(AudioCallback cb) {
    impl_->callback = std::move(cb);
}

void AudioStream::start() {
    if (!impl_->initialized || impl_->running) return;
    auto* impl = impl_.get();
    // Match the old tcxPDSP bridge order: after built-in Sound mixing, before
    // the app-level audioOut() listener and before clipping/analysis.
    impl_->outputListener = tc::AudioEngine::getInstance().audioOut.listen(
        [impl](tc::AudioOutBuffer& buffer) {
            impl->process(buffer);
        },
        tc::EventPriority::BeforeApp
    );
    impl_->running = true;
}

void AudioStream::stop() {
    if (impl_->running) {
        impl_->outputListener.disconnect();
    }
    impl_->running = false;
}

bool AudioStream::isRunning() const { return impl_->running; }
int  AudioStream::sampleRate()     const { return impl_->ctx.sampleRate; }
int  AudioStream::bufferSize()     const { return impl_->settings.bufferSize; }
int  AudioStream::outputChannels() const { return impl_->ctx.outputChannels; }

const AudioContext& AudioStream::context() const { return impl_->ctx; }
AudioContext& AudioStream::context() { return impl_->ctx; }

} // namespace tcx::pdsp
