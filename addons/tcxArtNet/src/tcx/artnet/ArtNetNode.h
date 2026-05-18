#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

#include <functional>

namespace tcx::artnet {

struct NodeSettings {
    std::string shortName = "tcxArtNet";
    std::string longName = "TrussC tcxArtNet Node";
    std::string manufacturer = "TrussC";
    std::string ipAddress = "0.0.0.0";
    std::string bindIpAddress = "0.0.0.0";
    std::array<uint8_t, 4> subnetMask {};
    std::array<uint8_t, 4> defaultGateway {};
    uint16_t oemCode = 0x0000;
    uint16_t estaManufacturerCode = 0x0000;
    uint16_t port = DefaultPort;
    uint8_t bindIndex = 1;
    std::vector<NodePort> inputPorts;
    std::vector<NodePort> outputPorts;
    bool respondToPoll = true;
    bool enableDiagnostics = true;
    bool enableArtAddress = true;
    bool enableArtInput = true;
    bool enableArtSync = true;
    bool enableIpProg = false;
};

class Node {
public:
    bool setup(const NodeSettings& settings, Error* error = nullptr);
    void close();

    void update();

    void setDmxOutputCallback(std::function<void(const ArtDmx&)> callback);
    void setNzsCallback(std::function<void(const ArtNzs&)> callback);
    void setSyncCallback(std::function<void(const ArtSync&)> callback);
    void setAddressCallback(std::function<void(const ArtAddress&)> callback);
    void setInputCallback(std::function<void(const ArtInput&)> callback);
    void setTriggerCallback(std::function<void(const ArtTrigger&)> callback);
    void setTimeCodeCallback(std::function<void(const ArtTimeCode&)> callback);

    bool sendPollReply(const Endpoint& controller, Error* error = nullptr);
    bool sendDiagnostics(const Endpoint& controller, std::string_view message, Error* error = nullptr);

    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    bool receiveOne(Error* error);
    void handlePacket(const Packet& packet, const Endpoint& sender);
    [[nodiscard]] ArtPollReply makePollReply() const;

    NodeSettings settings_;
    UdpSocket socket_;
    Statistics statistics_;
    std::function<void(const ArtDmx&)> dmxCallback_;
    std::function<void(const ArtNzs&)> nzsCallback_;
    std::function<void(const ArtSync&)> syncCallback_;
    std::function<void(const ArtAddress&)> addressCallback_;
    std::function<void(const ArtInput&)> inputCallback_;
    std::function<void(const ArtTrigger&)> triggerCallback_;
    std::function<void(const ArtTimeCode&)> timeCodeCallback_;
};

} // namespace tcx::artnet
