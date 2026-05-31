#include "tcx/artnet/ArtNetDmxRuntime.h"

namespace tcx::artnet {

uint16_t DmxReceiverState::keyFor(const UniverseAddress& universe) noexcept {
    return universe.toPortAddress();
}

bool DmxReceiverState::sequenceAccepted(DmxUniverseState& state, const ArtDmx& dmx) {
    if (dmx.sequence == 0) {
        state.hasSequence = false;
        return true;
    }
    if (state.hasSequence && dmx.sequence == state.lastSequence) {
        state.droppedFrames++;
        return false;
    }
    state.hasSequence = true;
    state.lastSequence = dmx.sequence;
    return true;
}

void DmxReceiverState::commit(const ArtDmx& dmx) {
    auto& state = universes_[keyFor(dmx.universe)];
    state.universe = dmx.universe;
    state.data = dmx.data;
    state.acceptedFrames++;
}

bool DmxReceiverState::processDmx(const ArtDmx& dmx, bool requireSync) {
    const auto now = std::chrono::steady_clock::now();
    prunePending(now);
    auto& state = universes_[keyFor(dmx.universe)];
    state.universe = dmx.universe;
    if (!sequenceAccepted(state, dmx)) {
        return false;
    }
    if (requireSync) {
        pending_[keyFor(dmx.universe)] = PendingDmx { dmx, now };
    } else {
        commit(dmx);
    }
    return true;
}

size_t DmxReceiverState::processSync(const ArtSync&) {
    prunePending();
    const size_t committed = pending_.size();
    for (const auto& [_, pending] : pending_) {
        commit(pending.dmx);
    }
    pending_.clear();
    return committed;
}

size_t DmxReceiverState::prunePending(std::chrono::steady_clock::time_point now) {
    if (pendingTimeout_ <= std::chrono::milliseconds::zero()) {
        return 0;
    }
    const size_t before = pending_.size();
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (now - it->second.receivedAt > pendingTimeout_) {
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
    return before - pending_.size();
}

std::optional<std::vector<uint8_t>> DmxReceiverState::getUniverseData(const UniverseAddress& universe) const {
    const auto found = universes_.find(keyFor(universe));
    if (found == universes_.end() || found->second.data.empty()) {
        return std::nullopt;
    }
    return found->second.data;
}

const DmxUniverseState* DmxReceiverState::getUniverseState(const UniverseAddress& universe) const {
    const auto found = universes_.find(keyFor(universe));
    if (found == universes_.end()) {
        return nullptr;
    }
    return &found->second;
}

void DmxReceiverState::clear() {
    universes_.clear();
    pending_.clear();
}

} // namespace tcx::artnet
