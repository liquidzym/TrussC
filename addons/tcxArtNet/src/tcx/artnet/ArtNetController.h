#pragma once

#include "ArtNetReceiver.h"
#include "ArtNetSender.h"

#include <mutex>

namespace tcx::artnet {

struct ControllerSettings {
    uint16_t localPort = DefaultPort;
    std::string localBindIp = "0.0.0.0";
    bool enableBroadcast = true;
    std::string directedBroadcastIp = "2.255.255.255";
    std::chrono::milliseconds pollInterval { 2500 };
    std::chrono::milliseconds pollTimeout { 3000 };
    bool autoPoll = false;
    bool enableDiagnostics = false;
    bool enableTargetedPoll = false;
    uint16_t targetPortAddressBottom = 1;
    uint16_t targetPortAddressTop = 32767;
};

struct SessionSettings {
    SenderSettings sender;
    ReceiverSettings receiver;
    ControllerSettings controller;
    bool setupSender = true;
    bool setupReceiver = true;
    bool setupController = true;
    bool startReceiverThread = false;

    SessionSettings();
};

class Controller {
public:
    bool setup(const ControllerSettings& settings, Error* error = nullptr);
    void close();
    bool recover(Error* error = nullptr);

    bool pollNodes(Error* error = nullptr);
    void update();

    [[nodiscard]] std::vector<NodeInfo> getDiscoveredNodes() const;

    bool sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error = nullptr);
    bool sendMultiUniverseDmx(
        const Endpoint& endpoint,
        const UniverseAddress& firstUniverse,
        std::span<const uint8_t> data,
        size_t channelsPerUniverse = 510,
        Error* error = nullptr
    );
    bool sendSync(const std::string& directedBroadcastIp, Error* error = nullptr);
    bool sendAddressCommand(const Endpoint& endpoint, const ArtAddressCommand& command, Error* error = nullptr);
    bool sendTrigger(const Endpoint& endpoint, const ArtTrigger& trigger, Error* error = nullptr);
    bool sendTimeCode(const Endpoint& endpoint, const ArtTimeCode& timecode, Error* error = nullptr);

    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] ControllerNetworkDiagnostics networkDiagnostics() const;
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    bool receiveOne(Error* error);
    void handlePacket(const Packet& packet, const Endpoint& sender);
    void pruneExpiredNodes(std::chrono::steady_clock::time_point now);

    ControllerSettings settings_;
    UdpSocket socket_;
    ControllerNetworkDiagnostics diagnostics_;
    mutable std::mutex nodesMutex_;
    std::vector<NodeInfo> nodes_;
    Statistics statistics_;
    std::chrono::steady_clock::time_point lastPoll_ {};
    std::vector<uint8_t> recvBuffer_;
    uint8_t nextDmxSequence_ = 1;
};

class Session {
public:
    Session() = default;
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool setup(const SessionSettings& settings = SessionSettings {}, Error* error = nullptr);
    void close();
    bool recover(Error* error = nullptr);
    void update();
    bool pollNodes(Error* error = nullptr);

    [[nodiscard]] Sender& sender() noexcept { return sender_; }
    [[nodiscard]] const Sender& sender() const noexcept { return sender_; }
    [[nodiscard]] Receiver& receiver() noexcept { return receiver_; }
    [[nodiscard]] const Receiver& receiver() const noexcept { return receiver_; }
    [[nodiscard]] Controller& controller() noexcept { return controller_; }
    [[nodiscard]] const Controller& controller() const noexcept { return controller_; }
    [[nodiscard]] std::vector<NodeInfo> getDiscoveredNodes() const;
    [[nodiscard]] bool isSenderReady() const noexcept { return senderReady_; }
    [[nodiscard]] bool isReceiverReady() const noexcept { return receiverReady_; }
    [[nodiscard]] bool isControllerReady() const noexcept { return controllerReady_; }

private:
    SessionSettings settings_;
    Sender sender_;
    Receiver receiver_;
    Controller controller_;
    bool senderReady_ = false;
    bool receiverReady_ = false;
    bool controllerReady_ = false;
};

} // namespace tcx::artnet
