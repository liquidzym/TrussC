#pragma once

#include "ArtNetTypes.h"

#include <memory>
#include <span>
#include <string>

namespace tcx::artnet {

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) noexcept;
    UdpSocket& operator=(UdpSocket&&) noexcept;

    bool open(Error* error = nullptr);
    bool bind(uint16_t port, Error* error = nullptr);
    bool bind(const std::string& localIp, uint16_t port, Error* error = nullptr);
    bool close();

    bool setNonBlocking(bool enabled, Error* error = nullptr);
    bool setReuseAddress(bool enabled, Error* error = nullptr);
    bool setBroadcast(bool enabled, Error* error = nullptr);

    bool sendTo(std::span<const uint8_t> data, const Endpoint& endpoint, Error* error = nullptr);
    bool receiveFrom(std::span<uint8_t> buffer, size_t& bytesReceived, Endpoint& sender, Error* error = nullptr);

    [[nodiscard]] bool isOpen() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx::artnet
