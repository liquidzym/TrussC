#pragma once
// =============================================================================
// tcxPDSP AudioContext — Shared audio configuration and timing
// =============================================================================

#include <cstdint>

namespace tcx::pdsp {

struct AudioContext {
    int sampleRate      = 48000;
    int bufferSize      = 256;
    int outputChannels  = 2;
    uint64_t currentSample = 0;

    double secondsPerSample() const {
        return 1.0 / static_cast<double>(sampleRate);
    }

    double currentTimeSeconds() const {
        return static_cast<double>(currentSample) / static_cast<double>(sampleRate);
    }

    void advance(int frames) {
        currentSample += static_cast<uint64_t>(frames);
    }
};

} // namespace tcx::pdsp
