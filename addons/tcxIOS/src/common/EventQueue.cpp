#include "tcx/ios/EventQueue.h"

#include <utility>

namespace tcx::ios {

void EventQueue::post(std::function<void()> fn) {
    if (!fn) return;
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(fn));
}

std::size_t EventQueue::drain() {
    std::vector<std::function<void()>> work;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        work.swap(queue_);
    }

    for (auto& fn : work) {
        if (fn) fn();
    }
    return work.size();
}

std::size_t EventQueue::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void EventQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

} // namespace tcx::ios
