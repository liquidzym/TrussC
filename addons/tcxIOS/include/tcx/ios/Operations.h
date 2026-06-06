#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace tcx::ios {

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
