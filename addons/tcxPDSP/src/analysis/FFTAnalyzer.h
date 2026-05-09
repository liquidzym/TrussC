#pragma once
// Lightweight real-input DFT analyzer for verification and audio-reactive use.
// This is intentionally dependency-free; replace with an optimized FFT backend if
// large realtime spectra are needed.

#include <vector>
#include <cmath>
#include <algorithm>

namespace tcx::pdsp {

class FFTAnalyzer {
public:
    void prepare(int fftSize) {
        size_ = std::max(8, fftSize);
        magnitudes_.assign(size_ / 2, 0.0f);
    }

    void process(const float* input, int frames) {
        if (size_ <= 0 || magnitudes_.empty() || !input || frames <= 0) return;

        const int n = std::min(size_, frames);
        const float twoPi = 6.283185307179586f;

        for (int bin = 0; bin < size_ / 2; bin++) {
            float re = 0.0f;
            float im = 0.0f;
            for (int i = 0; i < n; i++) {
                float window = 0.5f - 0.5f * std::cos(twoPi * i / std::max(1, n - 1));
                float phase = twoPi * bin * i / size_;
                float sample = input[i] * window;
                re += sample * std::cos(phase);
                im -= sample * std::sin(phase);
            }
            magnitudes_[bin] = std::sqrt(re * re + im * im) / std::max(1, n);
        }
    }

    float magnitude(int bin) const {
        if (bin < 0 || bin >= static_cast<int>(magnitudes_.size())) return 0.0f;
        return magnitudes_[bin];
    }

    int peakBin() const {
        if (magnitudes_.empty()) return 0;
        return static_cast<int>(std::max_element(magnitudes_.begin(), magnitudes_.end()) - magnitudes_.begin());
    }

    int size() const { return size_; }
    const std::vector<float>& magnitudes() const { return magnitudes_; }

private:
    int size_ = 0;
    std::vector<float> magnitudes_;
};

} // namespace tcx::pdsp
