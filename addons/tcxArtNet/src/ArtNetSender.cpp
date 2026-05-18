#include "tcx/artnet/ArtNetSender.h"

namespace tcx::artnet {

bool Sender::setup(bool enableBroadcast, Error* error) {
    if (!socket_.open(error)) {
        return false;
    }
    if (!socket_.setReuseAddress(true, error)) {
        return false;
    }
    if (!socket_.setBroadcast(enableBroadcast, error)) {
        return false;
    }
    return true;
}

void Sender::close() {
    socket_.close();
}

bool Sender::sendPacket(const Endpoint& endpoint, const Packet& packet, Error* error) {
    if (!socket_.isOpen() && !setup(false, error)) {
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!Codec::encode(packet, bytes, error)) {
        return false;
    }
    if (!socket_.sendTo(bytes, endpoint, error)) {
        return false;
    }
    statistics_.packetsSent++;
    if (std::holds_alternative<ArtDmx>(packet)) {
        statistics_.dmxFramesSent++;
    }
    return true;
}

bool Sender::sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error) {
    ArtDmx packet;
    packet.universe = universe;
    packet.data.assign(dmx.begin(), dmx.end());
    return sendPacket(endpoint, Packet { std::move(packet) }, error);
}

bool Sender::sendSync(const Endpoint& endpoint, Error* error) {
    return sendPacket(endpoint, Packet { ArtSync {} }, error);
}

} // namespace tcx::artnet
