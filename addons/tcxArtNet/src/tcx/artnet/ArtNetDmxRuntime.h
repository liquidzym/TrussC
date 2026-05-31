#pragma once

#include "ArtNetPacket.h"

#include <chrono>
#include <map>
#include <optional>

namespace tcx::artnet {

struct DmxUniverseState {
    UniverseAddress universe;
    std::vector<uint8_t> data;
    uint8_t lastSequence = 0;
    bool hasSequence = false;
    uint64_t acceptedFrames = 0;
    uint64_t droppedFrames = 0;
};

class DmxReceiverState {
public:
    bool processDmx(const ArtDmx& dmx, bool requireSync = false);
    size_t processSync(const ArtSync& sync);
    size_t prunePending(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    [[nodiscard]] std::optional<std::vector<uint8_t>> getUniverseData(const UniverseAddress& universe) const;
    [[nodiscard]] const DmxUniverseState* getUniverseState(const UniverseAddress& universe) const;
    [[nodiscard]] size_t pendingCount() const noexcept { return pending_.size(); }
    void setPendingTimeout(std::chrono::milliseconds timeout) noexcept { pendingTimeout_ = timeout; }
    void clear();

private:
    struct PendingDmx {
        ArtDmx dmx;
        std::chrono::steady_clock::time_point receivedAt;
    };

    static uint16_t keyFor(const UniverseAddress& universe) noexcept;
    bool sequenceAccepted(DmxUniverseState& state, const ArtDmx& dmx);
    void commit(const ArtDmx& dmx);

    std::map<uint16_t, DmxUniverseState> universes_;
    std::map<uint16_t, PendingDmx> pending_;
    std::chrono::milliseconds pendingTimeout_ { 1000 };
};

} // namespace tcx::artnet
