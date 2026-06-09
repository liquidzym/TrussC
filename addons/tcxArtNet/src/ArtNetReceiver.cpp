#include "tcx/artnet/ArtNetReceiver.h"

#include <array>
#include <chrono>
#include <algorithm>

namespace tcx::artnet {

namespace {

bool resolveUniverse(uint16_t portAddress, UniverseAddress& universe, Error* error) {
    auto resolved = UniverseAddress::fromPortAddress(portAddress);
    if (!resolved) {
        setError(error, ErrorCode::InvalidUniverse, "universe must be in the Art-Net 15-bit port-address range 0..32767");
        return false;
    }
    universe = *resolved;
    return true;
}

bool validateUniverse(const UniverseAddress& universe, Error* error) {
    if (!universe.isValid()) {
        setError(error, ErrorCode::InvalidUniverse, "universe address is outside the Art-Net range");
        return false;
    }
    return true;
}

bool validateChannel(size_t channel, Error* error) {
    if (channel >= MaxDmxChannels) {
        setError(error, ErrorCode::InvalidLength, "DMX channel must be zero-based in the range 0..511");
        return false;
    }
    return true;
}

} // namespace

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
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        universes_.clear();
        hasNewData_ = false;
    }
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

bool Receiver::hasUniverse(uint16_t universe) const {
    UniverseAddress address;
    return resolveUniverse(universe, address, nullptr) && hasUniverse(address);
}

bool Receiver::hasUniverse(const UniverseAddress& universe) const {
    if (!universe.isValid()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    return universes_.find(universe.toPortAddress()) != universes_.end();
}

bool Receiver::hasNewData() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    const bool hadNewData = hasNewData_;
    hasNewData_ = false;
    return hadNewData;
}

uint8_t Receiver::getChannel(uint16_t universe, size_t channel, Error* error) const {
    UniverseAddress address;
    if (!resolveUniverse(universe, address, error)) {
        return 0;
    }
    return getChannel(address, channel, error);
}

uint8_t Receiver::getChannel(const UniverseAddress& universe, size_t channel, Error* error) const {
    if (!validateUniverse(universe, error) || !validateChannel(channel, error)) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto found = universes_.find(universe.toPortAddress());
    if (found == universes_.end() || channel >= found->second.size()) {
        return 0;
    }
    return found->second[channel];
}

std::vector<uint8_t> Receiver::getDmx(uint16_t universe, Error* error) const {
    UniverseAddress address;
    if (!resolveUniverse(universe, address, error)) {
        return {};
    }
    return getDmx(address, error);
}

std::vector<uint8_t> Receiver::getDmx(const UniverseAddress& universe, Error* error) const {
    if (!validateUniverse(universe, error)) {
        return {};
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    const auto found = universes_.find(universe.toPortAddress());
    if (found == universes_.end()) {
        return {};
    }
    return found->second;
}

std::vector<uint16_t> Receiver::getUniverses() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::vector<uint16_t> universes;
    universes.reserve(universes_.size());
    for (const auto& [portAddress, _] : universes_) {
        universes.push_back(portAddress);
    }
    return universes;
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
        storeDmx(std::get<ArtDmx>(packet));
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

void Receiver::storeDmx(const ArtDmx& dmx) {
    if (!dmx.universe.isValid()) {
        return;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto& data = universes_[dmx.universe.toPortAddress()];
    if (data.size() != MaxDmxChannels) {
        data.assign(MaxDmxChannels, 0);
    }
    const size_t count = std::min(dmx.data.size(), data.size());
    std::copy_n(dmx.data.begin(), count, data.begin());
    hasNewData_ = true;
}

} // namespace tcx::artnet
