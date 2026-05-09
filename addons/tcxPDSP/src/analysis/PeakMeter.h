#pragma once
// =============================================================================
// tcxPDSP PeakMeter — Peak amplitude meter
// =============================================================================

#include <atomic>
#include <algorithm>

namespace tcx::pdsp {

class PeakMeter {
public:
    void process(const float* buffer, int frames) {
        float peak = 0.0f;
        for (int i = 0; i < frames; i++) {
            float abs = buffer[i] > 0 ? buffer[i] : -buffer[i];
            if (abs > peak) peak = abs;
        }
        current_.store(peak, std::memory_order_relaxed);
    }

    float value() const {
        return current_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<float> current_{0.0f};
};

} // namespace tcx::pdsp
