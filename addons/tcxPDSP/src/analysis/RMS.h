#pragma once
// =============================================================================
// tcxPDSP RMS — Running RMS meter (audio-reactive bridge)
// =============================================================================

#include <atomic>
#include <cmath>

namespace tcx::pdsp {

class RMS {
public:
    void process(const float* buffer, int frames) {
        float sum = 0.0f;
        for (int i = 0; i < frames; i++) {
            sum += buffer[i] * buffer[i];
        }
        float rms = std::sqrt(sum / static_cast<float>(frames));
        current_.store(rms, std::memory_order_relaxed);
    }

    float value() const {
        return current_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<float> current_{0.0f};
};

} // namespace tcx::pdsp
