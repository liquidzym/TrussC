#pragma once
// =============================================================================
// tcxPDSP Transport — Sample-accurate musical time base
// =============================================================================

#include <cstdint>
#include <atomic>
#include <algorithm>

namespace tcx::pdsp {

class Transport {
public:
    void prepare(int sampleRate) {
        sampleRate_ = sampleRate;
        samplePosition_ = 0;
    }

    void setBpm(double bpm) {
        bpm_.store(bpm, std::memory_order_relaxed);
    }

    double getBpm() const {
        return bpm_.load(std::memory_order_relaxed);
    }

    void play()  { playing_.store(true, std::memory_order_relaxed); }
    void stop()  { playing_.store(false, std::memory_order_relaxed); }
    // Call reset() only when stopped (no concurrent advance)
    void reset() { samplePosition_ = 0; }
    void reset(uint64_t sample) { samplePosition_ = sample; }

    bool isPlaying() const { return playing_.load(std::memory_order_relaxed); }

    uint64_t currentSample() const { return samplePosition_; }

    void advance(int frames) {
        if (playing_.load(std::memory_order_relaxed)) {
            samplePosition_ += static_cast<uint64_t>(frames);
            if (loopEnabled_.load(std::memory_order_relaxed)) {
                uint64_t start = loopStartSample_.load(std::memory_order_relaxed);
                uint64_t end = loopEndSample_.load(std::memory_order_relaxed);
                if (end > start && samplePosition_ >= end) {
                    uint64_t length = end - start;
                    samplePosition_ = start + ((samplePosition_ - start) % length);
                }
            }
        }
    }

    uint64_t samplesPerBeat() const {
        double bpm = bpm_.load(std::memory_order_relaxed);
        if (bpm <= 0.0) return static_cast<uint64_t>(sampleRate_);
        // 60 seconds / BPM = seconds per beat
        return static_cast<uint64_t>(60.0 * sampleRate_ / bpm);
    }

    int sampleRate() const { return sampleRate_; }

    void setLoopSamples(uint64_t startSample, uint64_t endSample) {
        if (endSample <= startSample) {
            loopEnabled_.store(false, std::memory_order_relaxed);
            return;
        }
        loopStartSample_.store(startSample, std::memory_order_relaxed);
        loopEndSample_.store(endSample, std::memory_order_relaxed);
    }

    void setLoopBeats(double startBeat, double endBeat) {
        startBeat = std::max(0.0, startBeat);
        endBeat = std::max(startBeat, endBeat);
        uint64_t spb = samplesPerBeat();
        setLoopSamples(
            static_cast<uint64_t>(startBeat * static_cast<double>(spb)),
            static_cast<uint64_t>(endBeat * static_cast<double>(spb)));
    }

    void enableLoop(bool enabled) { loopEnabled_.store(enabled, std::memory_order_relaxed); }
    bool isLoopEnabled() const { return loopEnabled_.load(std::memory_order_relaxed); }
    uint64_t loopStartSample() const { return loopStartSample_.load(std::memory_order_relaxed); }
    uint64_t loopEndSample() const { return loopEndSample_.load(std::memory_order_relaxed); }

private:
    int sampleRate_ = 48000;
    std::atomic<double> bpm_{120.0};
    std::atomic<bool> playing_{false};
    std::atomic<bool> loopEnabled_{false};
    std::atomic<uint64_t> loopStartSample_{0};
    std::atomic<uint64_t> loopEndSample_{0};
    uint64_t samplePosition_ = 0;
};

} // namespace tcx::pdsp
