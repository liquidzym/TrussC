#pragma once
// =============================================================================
// tcxPDSP EventQueue — Lock-free SPSC queue for sequencer events
// =============================================================================

#include <atomic>
#include <array>

namespace tcx::pdsp {

enum class EventType {
    NoteOn,
    NoteOff,
    Trigger,
    ParameterChange
};

struct SequenceEvent {
    EventType type = EventType::Trigger;
    uint64_t sampleTime = 0;
    int sampleOffsetInBlock = 0;
    int targetId = -1;
    int note = 60;
    float value0 = 0.0f;
    float value1 = 0.0f;
};

template<int Capacity = 1024>
class EventQueue {
public:
    EventQueue() {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }

    bool push(const SequenceEvent& event) {
        int w = write_.load(std::memory_order_relaxed);
        int next = (w + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire)) return false; // full
        buffer_[w] = event;
        write_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(SequenceEvent& event) {
        int r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false; // empty
        event = buffer_[r];
        read_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return read_.load(std::memory_order_acquire) == write_.load(std::memory_order_acquire);
    }

private:
    std::array<SequenceEvent, Capacity> buffer_;
    std::atomic<int> write_;
    std::atomic<int> read_;
};

} // namespace tcx::pdsp
