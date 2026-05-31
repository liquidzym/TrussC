#include "tcx/artnet/ArtNetReceiver.h"

#include <array>
#include <chrono>

namespace tcx::artnet {

Receiver::Receiver() = default;

Receiver::~Receiver() {
    close();
}

bool Receiver::setup(uint16_t port, Error* error) {
    ReceiverSettings settings;
    settings.port = port;
    return setup(settings, error);
}

bool Receiver::setup(const ReceiverSettings& settings, Error* error) {
    close();
    settings_ = settings;
    if (!socket_.open(error)) return false;
    if (!socket_.setReuseAddress(true, error)) return false;
    if (!socket_.bind(settings.localBindIp, settings.port, error)) return false;
    if (!socket_.setNonBlocking(true, error)) return false;
    recvBuffer_.resize(65535);
    return true;
}

void Receiver::close() {
    stopThread();
    socket_.close();
}

bool Receiver::poll(Error* error) {
    bool receivedAny = false;
    while (receiveOne(error)) {
        receivedAny = true;
    }
    return receivedAny;
}

bool Receiver::startThread(Error* error) {
    if (running_) {
        return true;
    }
    if (!socket_.isOpen()) {
        setError(error, ErrorCode::NotOpen, "receiver socket is not open");
        return false;
    }
    running_ = true;
    thread_ = std::thread(&Receiver::threadLoop, this);
    return true;
}

void Receiver::stopThread() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Receiver::setPacketCallback(PacketCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = std::move(callback);
}

void Receiver::threadLoop() {
    while (running_) {
        Error error;
        if (!receiveOne(&error) && error.code == ErrorCode::None) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool Receiver::receiveOne(Error* error) {
    if (recvBuffer_.empty()) {
        recvBuffer_.resize(65535);
    }
    size_t bytesReceived = 0;
    Endpoint sender;
    Error receiveError;
    if (!socket_.receiveFrom(recvBuffer_, bytesReceived, sender, &receiveError)) {
        if (receiveError.code != ErrorCode::None) {
            setError(error, receiveError.code, receiveError.message);
        } else {
            setError(error, ErrorCode::None, "no UDP packet available");
        }
        return false;
    }

    Packet packet;
    Error decodeError;
    if (!Codec::decode(std::span<const uint8_t>(recvBuffer_.data(), bytesReceived), packet, &decodeError)) {
        statistics_.invalidPackets++;
        setError(error, decodeError.code, decodeError.message);
        return false;
    }

    statistics_.packetsReceived++;
    if (std::holds_alternative<UnsupportedPacket>(packet)) {
        statistics_.unsupportedPackets++;
    }
    if (std::holds_alternative<ArtDmx>(packet)) {
        statistics_.dmxFramesReceived++;
    }
    if (std::holds_alternative<ArtPollReply>(packet)) {
        statistics_.pollRepliesReceived++;
    }

    PacketCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = callback_;
    }
    if (callback) {
        callback(packet, sender);
    }
    return true;
}

} // namespace tcx::artnet
