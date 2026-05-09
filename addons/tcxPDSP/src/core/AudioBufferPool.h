#pragma once
// Fixed-size AudioBuffer pool. Allocate on setup, borrow/release by index.

#include "core/AudioBuffer.h"
#include <vector>

namespace tcx::pdsp {

class AudioBufferPool {
public:
    void setup(int buffers, int channels, int frames) {
        buffers_.resize(buffers);
        used_.assign(buffers, false);
        for (auto& buffer : buffers_) buffer.allocate(channels, frames);
    }

    AudioBuffer* acquire() {
        for (size_t i = 0; i < buffers_.size(); i++) {
            if (!used_[i]) {
                used_[i] = true;
                buffers_[i].clear();
                return &buffers_[i];
            }
        }
        return nullptr;
    }

    void release(AudioBuffer* buffer) {
        if (!buffer) return;
        for (size_t i = 0; i < buffers_.size(); i++) {
            if (&buffers_[i] == buffer) {
                used_[i] = false;
                return;
            }
        }
    }

    int capacity() const { return static_cast<int>(buffers_.size()); }

private:
    std::vector<AudioBuffer> buffers_;
    std::vector<bool> used_;
};

} // namespace tcx::pdsp
