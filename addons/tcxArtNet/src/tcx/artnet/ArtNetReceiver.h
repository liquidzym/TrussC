#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

#include <atomic>
#include <functional>
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
    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] SocketDiagnostics diagnostics() const { return socket_.diagnostics(); }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    void threadLoop();
    bool receiveOne(Error* error);

    UdpSocket socket_;
    PacketCallback callback_;
    mutable std::mutex callbackMutex_;
    std::thread thread_;
    std::atomic<bool> running_ { false };
    ReceiverSettings settings_;
    Statistics statistics_;
    std::vector<uint8_t> recvBuffer_;
};

} // namespace tcx::artnet
