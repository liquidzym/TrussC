#pragma once
// =============================================================================
// tcxPDSP PatchNode — Lightweight patch point for >> connection syntax
// =============================================================================
// Usage:  osc.out() >> gain.in();
//         gain.out() >> processor.out(0);

#include <vector>
#include <cassert>
#include <algorithm>

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
        destination.source_ = this;
        for (auto* dst : destinations_) {
            if (dst == &destination) return;
        }
        destinations_.push_back(&destination);
        connections().push_back({this, &destination});
    }

    void disconnect(PatchNode& destination) {
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
                return;
            }
        }
    }

    AudioNode* owner()  const { return owner_; }
    int        channel() const { return channel_; }

    const std::vector<PatchNode*>& destinations() const { return destinations_; }
    PatchNode* source() const { return source_; }
    static const std::vector<Connection>& allConnections() { return connections(); }

private:
    static std::vector<Connection>& connections() {
        static std::vector<Connection> all;
        return all;
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
