#pragma once

#include "Types.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace tcx::ios {

template <typename T>
class SingleShotCompletion {
public:
    SingleShotCompletion() = default;
    explicit SingleShotCompletion(Completion<T> done)
        : state_(std::make_shared<State>(std::move(done))) {}

    bool valid() const {
        return state_ && static_cast<bool>(state_->done);
    }

    bool completed() const {
        return state_ && state_->completed.load();
    }

    bool tryComplete(Result<T> result) const {
        if (!state_) return false;
        if (state_->completed.exchange(true)) return false;
        if (state_->done) state_->done(std::move(result));
        return true;
    }

    void operator()(Result<T> result) const {
        (void)tryComplete(std::move(result));
    }

private:
    struct State {
        explicit State(Completion<T> completion)
            : done(std::move(completion)) {}

        Completion<T> done;
        std::atomic_bool completed{false};
    };

    std::shared_ptr<State> state_;
};

struct OperationState {
    std::string identifier;
    std::string label;
    std::atomic_bool cancelled{false};
    std::function<void()> cancelHandler;
};

class OperationHandle {
public:
    OperationHandle() = default;

    bool valid() const;
    const std::string& identifier() const;
    const std::string& label() const;
    void cancel() const;
    bool cancelled() const;

private:
    friend class Operations;
    explicit OperationHandle(std::shared_ptr<OperationState> state);

    std::shared_ptr<OperationState> state_;
};

class Operations {
public:
    OperationHandle create(const std::string& label = {}, std::function<void()> cancelHandler = nullptr);
    OperationHandle get(const std::string& identifier) const;
    bool cancel(const std::string& identifier);
    bool isCancelled(const std::string& identifier) const;
    void remove(const std::string& identifier);
    std::size_t activeCount() const;
};

Operations& operations();

} // namespace tcx::ios
