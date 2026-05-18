#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

namespace tcx::artnet {

class Sender {
public:
    bool setup(bool enableBroadcast = false, Error* error = nullptr);
    void close();

    bool sendPacket(const Endpoint& endpoint, const Packet& packet, Error* error = nullptr);
    bool sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error = nullptr);
    bool sendSync(const Endpoint& endpoint, Error* error = nullptr);

    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    UdpSocket socket_;
    Statistics statistics_;
};

} // namespace tcx::artnet
