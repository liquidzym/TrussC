#pragma once

#include "ArtNetCodec.h"
#include "ArtNetSocket.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

namespace tcx::artnet {

class Receiver {
public:
    using PacketCallback = std::function<void(const Packet&, const Endpoint&)>;

    Receiver();
    ~Receiver();

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    bool setup(uint16_t port = DefaultPort, Error* error = nullptr);
    void close();
    bool poll(Error* error = nullptr);
    bool startThread(Error* error = nullptr);
    void stopThread();

    void setPacketCallback(PacketCallback callback);
    [[nodiscard]] const Statistics& statistics() const noexcept { return statistics_; }
    void resetStatistics() noexcept { statistics_ = {}; }

private:
    void threadLoop();
    bool receiveOne(Error* error);

    UdpSocket socket_;
    PacketCallback callback_;
    mutable std::mutex callbackMutex_;
    std::thread thread_;
    std::atomic<bool> running_ { false };
    Statistics statistics_;
};

} // namespace tcx::artnet
