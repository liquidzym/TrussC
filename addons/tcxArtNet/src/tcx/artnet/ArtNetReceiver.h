#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace tcx::artnet {

struct ReceiverSettings {
    uint16_t port = DefaultPort;
    std::string localBindIp = "0.0.0.0";
};

class Receiver {
public:
    using PacketCallback = std::function<void(const Packet&, const Endpoint&)>;

    Receiver();
    ~Receiver();

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    bool setup(uint16_t port = DefaultPort, Error* error = nullptr);
    bool setup(const ReceiverSettings& settings, Error* error = nullptr);
    void close();
    bool poll(Error* error = nullptr);
    bool startThread(Error* error = nullptr);
    void stopThread();

    void setPacketCallback(PacketCallback callback);
    [[nodiscard]] bool hasUniverse(uint16_t universe) const;
    [[nodiscard]] bool hasUniverse(const UniverseAddress& universe) const;
    [[nodiscard]] bool hasNewData();
    [[nodiscard]] uint8_t getChannel(uint16_t universe, size_t channel, Error* error = nullptr) const;
    [[nodiscard]] uint8_t getChannel(const UniverseAddress& universe, size_t channel, Error* error = nullptr) const;
    [[nodiscard]] std::vector<uint8_t> getDmx(uint16_t universe, Error* error = nullptr) const;
    [[nodiscard]] std::vector<uint8_t> getDmx(const UniverseAddress& universe, Error* error = nullptr) const;
    [[nodiscard]] std::vector<uint16_t> getUniverses() const;
    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] SocketDiagnostics diagnostics() const { return socket_.diagnostics(); }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    void threadLoop();
    bool receiveOne(Error* error);
    void storeDmx(const ArtDmx& dmx);

    UdpSocket socket_;
    PacketCallback callback_;
    mutable std::mutex callbackMutex_;
    mutable std::mutex stateMutex_;
    std::map<uint16_t, std::vector<uint8_t>> universes_;
    bool hasNewData_ = false;
    std::thread thread_;
    std::atomic<bool> running_ { false };
    ReceiverSettings settings_;
    Statistics statistics_;
    std::vector<uint8_t> recvBuffer_;
};

} // namespace tcx::artnet
