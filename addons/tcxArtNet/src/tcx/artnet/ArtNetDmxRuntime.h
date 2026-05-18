#pragma once

#include "ArtNetPacket.h"

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

    [[nodiscard]] std::optional<std::vector<uint8_t>> getUniverseData(const UniverseAddress& universe) const;
    [[nodiscard]] const DmxUniverseState* getUniverseState(const UniverseAddress& universe) const;
    void clear();

private:
    static uint16_t keyFor(const UniverseAddress& universe) noexcept;
    bool sequenceAccepted(DmxUniverseState& state, const ArtDmx& dmx);
    void commit(const ArtDmx& dmx);

    std::map<uint16_t, DmxUniverseState> universes_;
    std::map<uint16_t, ArtDmx> pending_;
};

} // namespace tcx::artnet
