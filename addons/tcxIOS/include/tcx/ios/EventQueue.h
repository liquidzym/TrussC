#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace tcx::ios {

class EventQueue {
public:
    void post(std::function<void()> fn);
    std::size_t drain();
    std::size_t pending() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<std::function<void()>> queue_;
};

EventQueue& eventQueue();
void update();

} // namespace tcx::ios
