#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

namespace tcx::artnet {

struct SenderSettings {
    bool enableBroadcast = false;
    std::string localBindIp = "0.0.0.0";
    uint16_t localPort = 0;
};

class Sender {
public:
    bool setup(bool enableBroadcast = false, Error* error = nullptr);
    bool setup(const SenderSettings& settings, Error* error = nullptr);
    void close();

    bool sendPacket(const Endpoint& endpoint, const Packet& packet, Error* error = nullptr);
    bool sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error = nullptr);
    bool sendSync(const Endpoint& endpoint, Error* error = nullptr);

    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] SocketDiagnostics diagnostics() const { return socket_.diagnostics(); }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    UdpSocket socket_;
    SenderSettings settings_;
    Statistics statistics_;
};

} // namespace tcx::artnet
