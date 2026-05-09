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
    bool running = false;
    bool initialized = false;

    static void outputCallback(float* output, int frameCount, int channels, void* userData) {
        auto* impl = static_cast<Impl*>(userData);
        if (impl && impl->callback && output) {
            const int blockSize = std::max(1, impl->ctx.bufferSize);
            int processed = 0;
            while (processed < frameCount) {
                const int blockFrames = std::min(blockSize, frameCount - processed);
                impl->callback(
                    output + processed * channels,
                    blockFrames,
                    channels
                );
                impl->ctx.advance(static_cast<uint64_t>(blockFrames));
                processed += blockFrames;
            }
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
    impl_->ctx.sampleRate = tc::AudioEngine::SAMPLE_RATE;
    impl_->ctx.bufferSize = settings.bufferSize;
    impl_->ctx.outputChannels = tc::AudioEngine::NUM_CHANNELS;
    impl_->ctx.currentSample = 0;

    if (!tc::AudioEngine::getInstance().init()) {
        fprintf(stderr, "[tcxPDSP] AudioStream: TrussC AudioEngine init failed\n");
        return false;
    }

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
    tc::AudioEngine::getInstance().setOutputCallback(&Impl::outputCallback, impl_.get());
    impl_->running = true;
}

void AudioStream::stop() {
    if (impl_->running) {
        tc::AudioEngine::getInstance().clearOutputCallback();
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
