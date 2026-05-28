#pragma once
// =============================================================================
// tcxPDSP AudioStream — Minimal output wrapper for TrussC AudioEngine
// =============================================================================
// Reuses TrussC's global audio device through AudioEngine::audioOut.
// Usage:
//   tcx::pdsp::AudioStream stream;
//   stream.setup({ .sampleRate=48000, .bufferSize=256, .outputChannels=2 });
//   stream.setCallback([](float* out, int frames, int ch) { /* fill buffer */ });
//   stream.start();

#include "core/AudioContext.h"
#include <functional>
#include <memory>

namespace tcx::pdsp {

struct AudioStreamSettings {
    int sampleRate     = 48000;
    int bufferSize     = 256;
    int outputChannels = 2;
};

using AudioCallback = std::function<void(float* output, int numFrames, int numChannels)>;

class AudioStream {
public:
    AudioStream();
    ~AudioStream();

    AudioStream(const AudioStream&) = delete;
    AudioStream& operator=(const AudioStream&) = delete;

    bool setup(const AudioStreamSettings& settings);
    void setCallback(AudioCallback cb);
    void start();
    void stop();
    bool isRunning() const;
    const AudioContext& context() const;
    AudioContext& context();

    int sampleRate()     const;
    int bufferSize()     const;
    int outputChannels() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx::pdsp
