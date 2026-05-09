#pragma once
// =============================================================================
// tcxPDSP LockFreeQueue — Generic SPSC lock-free ring buffer queue
// =============================================================================
// Single-producer, single-consumer. No locks, no allocations in audio path.
// Template on type T and capacity N (must be power-of-2 for best perf).

#include <array>
#include <atomic>
#include <cstddef>

namespace tcx::pdsp {

template<typename T, int Capacity = 1024>
class LockFreeQueue {
public:
    LockFreeQueue() {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }

    // Producer: push one item. Returns false if full.
    bool push(const T& item) {
        int w = write_.load(std::memory_order_relaxed);
        int next = (w + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire)) return false;
        buffer_[w] = item;
        write_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer: pop one item. Returns false if empty.
    bool pop(T& item) {
        int r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;
        item = buffer_[r];
        read_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    // Non-consuming peek. Returns nullptr if empty.
    const T* peek() const {
        int r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return nullptr;
        return &buffer_[r];
    }

    bool empty() const {
        return read_.load(std::memory_order_acquire) ==
               write_.load(std::memory_order_acquire);
    }

    int size() const {
        int w = write_.load(std::memory_order_acquire);
        int r = read_.load(std::memory_order_acquire);
        int diff = w - r;
        return diff >= 0 ? diff : diff + Capacity;
    }

    // Clear queue — call only when quiescent (no concurrent push/pop)
    void clear() {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> buffer_;
    std::atomic<int> write_;
    std::atomic<int> read_;
};

} // namespace tcx::pdsp
