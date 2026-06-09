#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

#include "tcColor.h"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace tcx::artnet {

struct SenderSettings {
    bool enableBroadcast = false;
    std::string localBindIp = "0.0.0.0";
    uint16_t localPort = 0;
};

class Sender {
public:
    Sender() = default;
    ~Sender();

    Sender(const Sender&) = delete;
    Sender& operator=(const Sender&) = delete;

    bool setup(bool enableBroadcast = false, Error* error = nullptr);
    bool setup(const SenderSettings& settings, Error* error = nullptr);
    void close();
    bool recover(Error* error = nullptr);

    Sender& setDestination(const Endpoint& endpoint);
    Sender& addDestination(const Endpoint& endpoint);
    void clearDestinations();
    [[nodiscard]] std::vector<Endpoint> destinations() const;

    bool setChannel(uint16_t universe, size_t channel, uint8_t value, Error* error = nullptr);
    bool setChannel(const UniverseAddress& universe, size_t channel, uint8_t value, Error* error = nullptr);
    bool setChannels(uint16_t universe, size_t startChannel, std::span<const uint8_t> values, Error* error = nullptr);
    bool setChannels(const UniverseAddress& universe, size_t startChannel, std::span<const uint8_t> values, Error* error = nullptr);
    bool setColor(uint16_t universe, size_t startChannel, const trussc::Color& color, Error* error = nullptr);
    bool setColor(const UniverseAddress& universe, size_t startChannel, const trussc::Color& color, Error* error = nullptr);
    bool clear(uint16_t universe, Error* error = nullptr);
    bool clear(const UniverseAddress& universe, Error* error = nullptr);
    void clearAll();
    bool removeUniverse(uint16_t universe, Error* error = nullptr);
    bool removeUniverse(const UniverseAddress& universe, Error* error = nullptr);
    void removeAllUniverses();

    [[nodiscard]] uint8_t getChannel(uint16_t universe, size_t channel, Error* error = nullptr) const;
    [[nodiscard]] uint8_t getChannel(const UniverseAddress& universe, size_t channel, Error* error = nullptr) const;
    [[nodiscard]] std::vector<uint8_t> getDmx(uint16_t universe, Error* error = nullptr) const;
    [[nodiscard]] std::vector<uint8_t> getDmx(const UniverseAddress& universe, Error* error = nullptr) const;
    [[nodiscard]] size_t getUniverseCount() const;
    [[nodiscard]] std::vector<uint16_t> getUniverses() const;

    bool send(Error* error = nullptr);
    bool send(const Endpoint& endpoint, Error* error = nullptr);
    bool sendUniverse(uint16_t universe, Error* error = nullptr);
    bool sendUniverse(const UniverseAddress& universe, Error* error = nullptr);
    bool sendUniverse(const Endpoint& endpoint, uint16_t universe, Error* error = nullptr);
    bool sendUniverse(const Endpoint& endpoint, const UniverseAddress& universe, Error* error = nullptr);

    bool startAutoSend(double fps = 30.0, Error* error = nullptr);
    void stopAutoSend();
    [[nodiscard]] bool isAutoSending() const noexcept { return autoSendRunning_; }

    bool sendPacket(const Endpoint& endpoint, const Packet& packet, Error* error = nullptr);
    bool sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error = nullptr);
    bool sendSync(const Endpoint& endpoint, Error* error = nullptr);

    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] SocketDiagnostics diagnostics() const { return socket_.diagnostics(); }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    struct UniverseSnapshot {
        uint16_t portAddress = 0;
        std::vector<uint8_t> data;
    };

    bool sendPacketUnlocked(const Endpoint& endpoint, const Packet& packet, Error* error);
    bool sendDmxUnlocked(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error);
    [[nodiscard]] std::vector<UniverseSnapshot> snapshotUniverses() const;
    [[nodiscard]] std::vector<Endpoint> snapshotDestinations() const;
    void autoSendLoop(double fps);

    UdpSocket socket_;
    SenderSettings settings_;
    Statistics statistics_;
    uint8_t nextDmxSequence_ = 1;
    mutable std::mutex stateMutex_;
    mutable std::mutex sendMutex_;
    std::vector<Endpoint> destinations_;
    std::map<uint16_t, std::vector<uint8_t>> universes_;
    std::thread autoSendThread_;
    std::atomic<bool> autoSendRunning_ { false };
};

} // namespace tcx::artnet
