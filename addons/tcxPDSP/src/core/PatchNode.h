#pragma once
// =============================================================================
// tcxPDSP PatchNode — Lightweight patch point for >> connection syntax
// =============================================================================
// Usage:  osc.out() >> gain.in();
//         gain.out() >> processor.out(0);

#include <vector>
#include <cassert>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace tcx::pdsp {

class AudioNode;

class PatchNode {
public:
    struct Connection {
        PatchNode* source = nullptr;
        PatchNode* destination = nullptr;
    };

    PatchNode() = default;
    PatchNode(PatchNode&&) noexcept = default;
    PatchNode& operator=(PatchNode&&) noexcept = default;
    PatchNode(const PatchNode&) = delete;
    PatchNode& operator=(const PatchNode&) = delete;

    explicit PatchNode(AudioNode* owner, int ch = 0)
        : owner_(owner), channel_(ch) {}

    void connect(PatchNode& destination) {
        std::unique_lock<std::shared_mutex> lock(connectionMutex());
        bool changed = false;
        if (destination.source_ && destination.source_ != this) {
            auto* oldSource = destination.source_;
            oldSource->destinations_.erase(
                std::remove(oldSource->destinations_.begin(), oldSource->destinations_.end(), &destination),
                oldSource->destinations_.end());
            auto& conns = connections();
            conns.erase(
                std::remove_if(conns.begin(), conns.end(),
                    [&](const Connection& c) {
                        return c.destination == &destination;
                    }),
                conns.end());
            changed = true;
        }
        destination.source_ = this;
        for (auto* dst : destinations_) {
            if (dst == &destination) {
                if (changed) connectionGeneration().fetch_add(1, std::memory_order_release);
                return;
            }
        }
        destinations_.push_back(&destination);
        connections().push_back({this, &destination});
        connectionGeneration().fetch_add(1, std::memory_order_release);
    }

    void disconnect(PatchNode& destination) {
        std::unique_lock<std::shared_mutex> lock(connectionMutex());
        for (auto it = destinations_.begin(); it != destinations_.end(); ++it) {
            if (*it == &destination) {
                destinations_.erase(it);
                if (destination.source_ == this) {
                    destination.source_ = nullptr;
                }
                auto& conns = connections();
                conns.erase(
                    std::remove_if(conns.begin(), conns.end(),
                        [&](const Connection& c) {
                            return c.source == this && c.destination == &destination;
                        }),
                    conns.end());
                connectionGeneration().fetch_add(1, std::memory_order_release);
                return;
            }
        }
    }

    AudioNode* owner()  const { return owner_; }
    int        channel() const { return channel_; }

    const std::vector<PatchNode*>& destinations() const { return destinations_; }
    std::vector<PatchNode*> destinationSnapshot() const {
        std::shared_lock<std::shared_mutex> lock(connectionMutex());
        return destinations_;
    }
    PatchNode* source() const {
        std::shared_lock<std::shared_mutex> lock(connectionMutex());
        return source_;
    }
    static std::vector<Connection> allConnections() {
        std::shared_lock<std::shared_mutex> lock(connectionMutex());
        return connections();
    }
    static uint64_t graphGeneration() {
        return connectionGeneration().load(std::memory_order_acquire);
    }

private:
    static std::shared_mutex& connectionMutex() {
        static std::shared_mutex mutex;
        return mutex;
    }

    static std::vector<Connection>& connections() {
        static std::vector<Connection> all;
        return all;
    }

    static std::atomic<uint64_t>& connectionGeneration() {
        static std::atomic<uint64_t> generation{0};
        return generation;
    }

    AudioNode* owner_   = nullptr;
    int        channel_ = 0;
    std::vector<PatchNode*> destinations_;
    PatchNode* source_ = nullptr;
};

inline PatchNode& operator>>(PatchNode& src, PatchNode& dst) {
    src.connect(dst);
    return dst;
}

} // namespace tcx::pdsp
