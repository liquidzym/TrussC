#include "tcx/artnet/ArtNetSender.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace tcx::artnet {

namespace {

uint8_t takeNextSequence(uint8_t& nextSequence) noexcept {
    const uint8_t sequence = nextSequence;
    nextSequence = (nextSequence == 255) ? 1 : static_cast<uint8_t>(nextSequence + 1);
    return sequence;
}

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

bool validateChannelBlock(size_t startChannel, size_t count, Error* error) {
    if (count == 0) {
        setError(error, ErrorCode::InvalidLength, "DMX channel block must not be empty");
        return false;
    }
    if (startChannel >= MaxDmxChannels || count > MaxDmxChannels || startChannel + count > MaxDmxChannels) {
        setError(error, ErrorCode::InvalidLength, "DMX channel block must fit zero-based channels 0..511");
        return false;
    }
    return true;
}

std::vector<uint8_t>& ensureUniverse(std::map<uint16_t, std::vector<uint8_t>>& universes, const UniverseAddress& universe) {
    auto& data = universes[universe.toPortAddress()];
    if (data.size() != MaxDmxChannels) {
        data.assign(MaxDmxChannels, 0);
    }
    return data;
}

uint8_t colorByte(float value) {
    return static_cast<uint8_t>(std::clamp(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f), 0L, 255L));
}

} // namespace

Sender::~Sender() {
    close();
}

bool Sender::setup(bool enableBroadcast, Error* error) {
    SenderSettings settings;
    settings.enableBroadcast = enableBroadcast;
    return setup(settings, error);
}

bool Sender::setup(const SenderSettings& settings, Error* error) {
    settings_ = settings;
    stopAutoSend();
    std::lock_guard<std::mutex> lock(sendMutex_);
    socket_.close();
    if (!socket_.open(error)) {
        return false;
    }
    if (!socket_.setReuseAddress(true, error)) {
        return false;
    }
    if (!socket_.setBroadcast(settings.enableBroadcast, error)) {
        return false;
    }
    if (!socket_.bind(settings.localBindIp, settings.localPort, error)) {
        return false;
    }
    nextDmxSequence_ = 1;
    return true;
}

void Sender::close() {
    stopAutoSend();
    std::lock_guard<std::mutex> lock(sendMutex_);
    socket_.close();
}

bool Sender::recover(Error* error) {
    return setup(settings_, error);
}

Sender& Sender::setDestination(const Endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    destinations_.clear();
    destinations_.push_back(endpoint);
    return *this;
}

Sender& Sender::addDestination(const Endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    destinations_.push_back(endpoint);
    return *this;
}

void Sender::clearDestinations() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    destinations_.clear();
}

std::vector<Endpoint> Sender::destinations() const {
    return snapshotDestinations();
}

bool Sender::setChannel(uint16_t universe, size_t channel, uint8_t value, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && setChannel(address, channel, value, error);
}

bool Sender::setChannel(const UniverseAddress& universe, size_t channel, uint8_t value, Error* error) {
    if (!validateUniverse(universe, error) || !validateChannel(channel, error)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto& data = ensureUniverse(universes_, universe);
    data[channel] = value;
    return true;
}

bool Sender::setChannels(uint16_t universe, size_t startChannel, std::span<const uint8_t> values, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && setChannels(address, startChannel, values, error);
}

bool Sender::setChannels(const UniverseAddress& universe, size_t startChannel, std::span<const uint8_t> values, Error* error) {
    if (!validateUniverse(universe, error) || !validateChannelBlock(startChannel, values.size(), error)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto& data = ensureUniverse(universes_, universe);
    std::copy(values.begin(), values.end(), data.begin() + static_cast<std::ptrdiff_t>(startChannel));
    return true;
}

bool Sender::setColor(uint16_t universe, size_t startChannel, const trussc::Color& color, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && setColor(address, startChannel, color, error);
}

bool Sender::setColor(const UniverseAddress& universe, size_t startChannel, const trussc::Color& color, Error* error) {
    const std::array<uint8_t, 3> rgb { colorByte(color.r), colorByte(color.g), colorByte(color.b) };
    return setChannels(universe, startChannel, rgb, error);
}

bool Sender::clear(uint16_t universe, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && clear(address, error);
}

bool Sender::clear(const UniverseAddress& universe, Error* error) {
    if (!validateUniverse(universe, error)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto& data = ensureUniverse(universes_, universe);
    std::fill(data.begin(), data.end(), 0);
    return true;
}

void Sender::clearAll() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    for (auto& [_, data] : universes_) {
        if (data.size() != MaxDmxChannels) {
            data.assign(MaxDmxChannels, 0);
        } else {
            std::fill(data.begin(), data.end(), 0);
        }
    }
}

bool Sender::removeUniverse(uint16_t universe, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && removeUniverse(address, error);
}

bool Sender::removeUniverse(const UniverseAddress& universe, Error* error) {
    if (!validateUniverse(universe, error)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(stateMutex_);
    universes_.erase(universe.toPortAddress());
    return true;
}

void Sender::removeAllUniverses() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    universes_.clear();
}

uint8_t Sender::getChannel(uint16_t universe, size_t channel, Error* error) const {
    UniverseAddress address;
    if (!resolveUniverse(universe, address, error)) {
        return 0;
    }
    return getChannel(address, channel, error);
}

uint8_t Sender::getChannel(const UniverseAddress& universe, size_t channel, Error* error) const {
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

std::vector<uint8_t> Sender::getDmx(uint16_t universe, Error* error) const {
    UniverseAddress address;
    if (!resolveUniverse(universe, address, error)) {
        return {};
    }
    return getDmx(address, error);
}

std::vector<uint8_t> Sender::getDmx(const UniverseAddress& universe, Error* error) const {
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

size_t Sender::getUniverseCount() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return universes_.size();
}

std::vector<uint16_t> Sender::getUniverses() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::vector<uint16_t> universes;
    universes.reserve(universes_.size());
    for (const auto& [portAddress, _] : universes_) {
        universes.push_back(portAddress);
    }
    return universes;
}

bool Sender::send(Error* error) {
    const auto destinations = snapshotDestinations();
    if (destinations.empty()) {
        if (snapshotUniverses().empty()) {
            return true;
        }
        setError(error, ErrorCode::InvalidConfiguration, "no Sender destination configured");
        return false;
    }
    bool ok = true;
    for (const auto& endpoint : destinations) {
        ok = send(endpoint, error) && ok;
    }
    return ok;
}

bool Sender::send(const Endpoint& endpoint, Error* error) {
    const auto universes = snapshotUniverses();
    for (const auto& frame : universes) {
        auto universe = UniverseAddress::fromPortAddress(frame.portAddress);
        if (!universe) {
            setError(error, ErrorCode::InvalidUniverse, "stored universe is outside the Art-Net range");
            return false;
        }
        if (!sendDmx(endpoint, *universe, frame.data, error)) {
            return false;
        }
    }
    return true;
}

bool Sender::sendUniverse(uint16_t universe, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && sendUniverse(address, error);
}

bool Sender::sendUniverse(const UniverseAddress& universe, Error* error) {
    const auto destinations = snapshotDestinations();
    if (destinations.empty()) {
        setError(error, ErrorCode::InvalidConfiguration, "no Sender destination configured");
        return false;
    }
    bool ok = true;
    for (const auto& endpoint : destinations) {
        ok = sendUniverse(endpoint, universe, error) && ok;
    }
    return ok;
}

bool Sender::sendUniverse(const Endpoint& endpoint, uint16_t universe, Error* error) {
    UniverseAddress address;
    return resolveUniverse(universe, address, error) && sendUniverse(endpoint, address, error);
}

bool Sender::sendUniverse(const Endpoint& endpoint, const UniverseAddress& universe, Error* error) {
    if (!validateUniverse(universe, error)) {
        return false;
    }
    std::vector<uint8_t> data;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        const auto found = universes_.find(universe.toPortAddress());
        if (found == universes_.end()) {
            setError(error, ErrorCode::InvalidUniverse, "Sender universe is not active");
            return false;
        }
        data = found->second;
    }
    return sendDmx(endpoint, universe, data, error);
}

bool Sender::startAutoSend(double fps, Error* error) {
    if (!socket_.isOpen()) {
        setError(error, ErrorCode::NotOpen, "sender socket is not open; call setup() before startAutoSend()");
        return false;
    }
    const double safeFps = std::clamp(fps, 1.0, 44.0);
    stopAutoSend();
    autoSendRunning_ = true;
    autoSendThread_ = std::thread(&Sender::autoSendLoop, this, safeFps);
    return true;
}

void Sender::stopAutoSend() {
    autoSendRunning_ = false;
    if (autoSendThread_.joinable()) {
        autoSendThread_.join();
    }
}

bool Sender::sendPacket(const Endpoint& endpoint, const Packet& packet, Error* error) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    return sendPacketUnlocked(endpoint, packet, error);
}

bool Sender::sendPacketUnlocked(const Endpoint& endpoint, const Packet& packet, Error* error) {
    if (!socket_.isOpen()) {
        setError(error, ErrorCode::NotOpen, "sender socket is not open; call setup() or recover() before sending");
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!Codec::encode(packet, bytes, error)) {
        return false;
    }
    if (!socket_.sendTo(bytes, endpoint, error)) {
        return false;
    }
    statistics_.packetsSent++;
    if (std::holds_alternative<ArtDmx>(packet)) {
        statistics_.dmxFramesSent++;
    }
    return true;
}

bool Sender::sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    return sendDmxUnlocked(endpoint, universe, dmx, error);
}

bool Sender::sendDmxUnlocked(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error) {
    ArtDmx packet;
    packet.sequence = takeNextSequence(nextDmxSequence_);
    packet.universe = universe;
    packet.data.assign(dmx.begin(), dmx.end());
    return sendPacketUnlocked(endpoint, Packet { std::move(packet) }, error);
}

bool Sender::sendSync(const Endpoint& endpoint, Error* error) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    return sendPacketUnlocked(endpoint, Packet { ArtSync {} }, error);
}

std::vector<Sender::UniverseSnapshot> Sender::snapshotUniverses() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    std::vector<UniverseSnapshot> snapshots;
    snapshots.reserve(universes_.size());
    for (const auto& [portAddress, data] : universes_) {
        snapshots.push_back(UniverseSnapshot { portAddress, data });
    }
    return snapshots;
}

std::vector<Endpoint> Sender::snapshotDestinations() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return destinations_;
}

void Sender::autoSendLoop(double fps) {
    const auto interval = std::chrono::duration<double>(1.0 / fps);
    auto nextTick = std::chrono::steady_clock::now();
    while (autoSendRunning_) {
        Error error;
        send(&error);
        nextTick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
        std::this_thread::sleep_until(nextTick);
        if (std::chrono::steady_clock::now() - nextTick > interval) {
            nextTick = std::chrono::steady_clock::now();
        }
    }
}

} // namespace tcx::artnet
