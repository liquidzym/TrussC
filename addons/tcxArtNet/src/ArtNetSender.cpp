#include "tcx/artnet/ArtNetSender.h"

namespace tcx::artnet {

namespace {

uint8_t takeNextSequence(uint8_t& nextSequence) noexcept {
    const uint8_t sequence = nextSequence;
    nextSequence = (nextSequence == 255) ? 1 : static_cast<uint8_t>(nextSequence + 1);
    return sequence;
}

} // namespace

bool Sender::setup(bool enableBroadcast, Error* error) {
    SenderSettings settings;
    settings.enableBroadcast = enableBroadcast;
    return setup(settings, error);
}

bool Sender::setup(const SenderSettings& settings, Error* error) {
    settings_ = settings;
    socket_.close();
    if (!socket_.open(error)) {
        return false;
    }
    if (!socket_.setReuseAddress(true, error)) {
        return false;
    }
    if (!socket_.setBroadcast(settings.enableBroadcast, error)) {
        return false;
    }
    if (!socket_.bind(settings.localBindIp, settings.localPort, error)) {
        return false;
    }
    nextDmxSequence_ = 1;
    return true;
}

void Sender::close() {
    socket_.close();
}

bool Sender::recover(Error* error) {
    return setup(settings_, error);
}

bool Sender::sendPacket(const Endpoint& endpoint, const Packet& packet, Error* error) {
    if (!socket_.isOpen()) {
        setError(error, ErrorCode::NotOpen, "sender socket is not open; call setup() or recover() before sending");
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
    packet.sequence = takeNextSequence(nextDmxSequence_);
    packet.universe = universe;
    packet.data.assign(dmx.begin(), dmx.end());
    return sendPacket(endpoint, Packet { std::move(packet) }, error);
}

bool Sender::sendSync(const Endpoint& endpoint, Error* error) {
    return sendPacket(endpoint, Packet { ArtSync {} }, error);
}

} // namespace tcx::artnet
