#include "tcx/artnet/ArtNetController.h"

#include <algorithm>
#include <array>
#include <vector>

namespace tcx::artnet {

namespace {

bool sendEncoded(UdpSocket& socket, const Endpoint& endpoint, const Packet& packet, Statistics& statistics, Error* error) {
    std::vector<uint8_t> bytes;
    if (!Codec::encode(packet, bytes, error)) {
        return false;
    }
    if (!socket.sendTo(bytes, endpoint, error)) {
        return false;
    }
    statistics.packetsSent++;
    if (std::holds_alternative<ArtDmx>(packet)) {
        statistics.dmxFramesSent++;
    }
    return true;
}

} // namespace

bool Controller::setup(const ControllerSettings& settings, Error* error) {
    close();
    settings_ = settings;
    lastPoll_ = {};
    if (!socket_.open(error)) return false;
    if (!socket_.setReuseAddress(true, error)) return false;
    if (!socket_.setBroadcast(settings.enableBroadcast, error)) return false;
    if (!socket_.bind(settings.localBindIp, settings.localPort, error)) return false;
    if (!socket_.setNonBlocking(true, error)) return false;
    return true;
}

void Controller::close() {
    socket_.close();
    std::lock_guard<std::mutex> lock(nodesMutex_);
    nodes_.clear();
}

bool Controller::pollNodes(Error* error) {
    ArtPoll poll;
    poll.requestDiagnostics = settings_.enableDiagnostics;
    poll.diagnosticsUnicast = true;
    poll.targetedMode = settings_.enableTargetedPoll;
    poll.targetPortAddressBottom = settings_.targetPortAddressBottom;
    poll.targetPortAddressTop = settings_.targetPortAddressTop;
    const Endpoint target { settings_.directedBroadcastIp, DefaultPort };
    return sendEncoded(socket_, target, Packet { poll }, statistics_, error);
}

void Controller::update() {
    Error error;
    const auto now = std::chrono::steady_clock::now();
    if (settings_.autoPoll &&
        (lastPoll_ == std::chrono::steady_clock::time_point {} || now - lastPoll_ >= settings_.pollInterval)) {
        pollNodes(&error);
        lastPoll_ = now;
    }
    while (receiveOne(&error)) {
    }
    pruneExpiredNodes(std::chrono::steady_clock::now());
}

std::vector<NodeInfo> Controller::getDiscoveredNodes() const {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    return nodes_;
}

bool Controller::sendDmx(const Endpoint& endpoint, const UniverseAddress& universe, std::span<const uint8_t> dmx, Error* error) {
    ArtDmx packet;
    packet.universe = universe;
    packet.data.assign(dmx.begin(), dmx.end());
    return sendEncoded(socket_, endpoint, Packet { std::move(packet) }, statistics_, error);
}

bool Controller::sendMultiUniverseDmx(
    const Endpoint& endpoint,
    const UniverseAddress& firstUniverse,
    std::span<const uint8_t> data,
    size_t channelsPerUniverse,
    Error* error
) {
    if (channelsPerUniverse == 0 || channelsPerUniverse > MaxDmxChannels) {
        setError(error, ErrorCode::InvalidLength, "channelsPerUniverse must be 1..512");
        return false;
    }
    UniverseAddress universe = firstUniverse;
    for (size_t offset = 0; offset < data.size(); offset += channelsPerUniverse) {
        const size_t count = std::min(channelsPerUniverse, data.size() - offset);
        if (count < MinArtDmxChannels || count % 2 != 0) {
            setError(error, ErrorCode::InvalidLength, "each ArtDmx universe chunk must be even and at least two channels");
            return false;
        }
        if (!sendDmx(endpoint, universe, data.subspan(offset, count), error)) {
            return false;
        }
        if (offset + count < data.size()) {
            auto nextUniverse = UniverseAddress::next(universe);
            if (!nextUniverse) {
                setError(error, ErrorCode::InvalidUniverse, "multi-universe send exceeded Art-Net address range");
                return false;
            }
            universe = *nextUniverse;
        }
    }
    return true;
}

bool Controller::sendSync(const std::string& directedBroadcastIp, Error* error) {
    Endpoint target { directedBroadcastIp.empty() ? settings_.directedBroadcastIp : directedBroadcastIp, DefaultPort };
    return sendEncoded(socket_, target, Packet { ArtSync {} }, statistics_, error);
}

bool Controller::sendAddressCommand(const Endpoint& endpoint, const ArtAddressCommand& command, Error* error) {
    return sendEncoded(socket_, endpoint, Packet { command }, statistics_, error);
}

bool Controller::sendTrigger(const Endpoint& endpoint, const ArtTrigger& trigger, Error* error) {
    return sendEncoded(socket_, endpoint, Packet { trigger }, statistics_, error);
}

bool Controller::sendTimeCode(const Endpoint& endpoint, const ArtTimeCode& timecode, Error* error) {
    return sendEncoded(socket_, endpoint, Packet { timecode }, statistics_, error);
}

bool Controller::receiveOne(Error* error) {
    std::vector<uint8_t> buffer(65535);
    size_t bytesReceived = 0;
    Endpoint sender;
    Error receiveError;
    if (!socket_.receiveFrom(buffer, bytesReceived, sender, &receiveError)) {
        if (receiveError.code != ErrorCode::None) {
            setError(error, receiveError.code, receiveError.message);
        }
        return false;
    }

    Packet packet;
    Error decodeError;
    if (!Codec::decode(std::span<const uint8_t>(buffer.data(), bytesReceived), packet, &decodeError)) {
        statistics_.invalidPackets++;
        setError(error, decodeError.code, decodeError.message);
        return false;
    }
    statistics_.packetsReceived++;
    handlePacket(packet, sender);
    return true;
}

void Controller::handlePacket(const Packet& packet, const Endpoint& sender) {
    if (std::holds_alternative<UnsupportedPacket>(packet)) {
        statistics_.unsupportedPackets++;
        return;
    }
    if (const auto* reply = std::get_if<ArtPollReply>(&packet)) {
        statistics_.pollRepliesReceived++;
        std::lock_guard<std::mutex> lock(nodesMutex_);
        auto existing = std::find_if(nodes_.begin(), nodes_.end(), [&](const NodeInfo& info) {
            return info.endpoint.ip == sender.ip &&
                   info.endpoint.port == sender.port &&
                   info.reply.bindIndex == reply->bindIndex;
        });
        if (existing == nodes_.end()) {
            nodes_.push_back(NodeInfo { sender, *reply, std::chrono::steady_clock::now() });
        } else {
            existing->reply = *reply;
            existing->lastSeen = std::chrono::steady_clock::now();
        }
    }
}

void Controller::pruneExpiredNodes(std::chrono::steady_clock::time_point now) {
    if (settings_.pollTimeout <= std::chrono::milliseconds::zero()) {
        return;
    }
    std::lock_guard<std::mutex> lock(nodesMutex_);
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(), [&](const NodeInfo& info) {
            return now - info.lastSeen > settings_.pollTimeout;
        }),
        nodes_.end()
    );
}

} // namespace tcx::artnet
