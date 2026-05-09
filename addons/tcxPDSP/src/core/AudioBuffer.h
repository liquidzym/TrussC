#pragma once
// =============================================================================
// tcxPDSP AudioBuffer — Planar float buffer (channel-first layout)
// =============================================================================
// channel 0: frame0 frame1 frame2 ...
// channel 1: frame0 frame1 frame2 ...
// Allocated once in prepare(), never reallocated during process().

#include <vector>
#include <cstring>

namespace tcx::pdsp {

class AudioBuffer {
public:
    void allocate(int channels, int frames) {
        numChannels_ = channels;
        numFrames_   = frames;
        data_.resize(channels * frames, 0.0f);
    }

    void clear() {
        std::memset(data_.data(), 0, data_.size() * sizeof(float));
    }

    float* channel(int ch) {
        return data_.data() + ch * numFrames_;
    }

    const float* channel(int ch) const {
        return data_.data() + ch * numFrames_;
    }

    float& sample(int ch, int frame) {
        return data_[ch * numFrames_ + frame];
    }

    int channels() const { return numChannels_; }
    int frames()   const { return numFrames_; }
    bool empty()   const { return data_.empty(); }

private:
    int numChannels_ = 0;
    int numFrames_   = 0;
    std::vector<float> data_;
};

} // namespace tcx::pdsp
