#include "tcx/artnet/ArtNetNode.h"

#include <array>
#include <sstream>

namespace tcx::artnet {

namespace {

bool sendPacket(UdpSocket& socket, const Endpoint& endpoint, const Packet& packet, Statistics& statistics, Error* error) {
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

bool Node::setup(const NodeSettings& settings, Error* error) {
    close();
    settings_ = settings;
    if (!socket_.open(error)) return false;
    if (!socket_.setReuseAddress(true, error)) return false;
    if (!socket_.bind(settings.port, error)) return false;
    if (!socket_.setNonBlocking(true, error)) return false;
    return true;
}

void Node::close() {
    socket_.close();
}

void Node::update() {
    Error error;
    while (receiveOne(&error)) {
    }
}

void Node::setDmxOutputCallback(std::function<void(const ArtDmx&)> callback) {
    dmxCallback_ = std::move(callback);
}

void Node::setNzsCallback(std::function<void(const ArtNzs&)> callback) {
    nzsCallback_ = std::move(callback);
}

void Node::setSyncCallback(std::function<void(const ArtSync&)> callback) {
    syncCallback_ = std::move(callback);
}

void Node::setAddressCallback(std::function<void(const ArtAddress&)> callback) {
    addressCallback_ = std::move(callback);
}

void Node::setTriggerCallback(std::function<void(const ArtTrigger&)> callback) {
    triggerCallback_ = std::move(callback);
}

void Node::setTimeCodeCallback(std::function<void(const ArtTimeCode&)> callback) {
    timeCodeCallback_ = std::move(callback);
}

bool Node::sendPollReply(const Endpoint& controller, Error* error) {
    return sendPacket(socket_, controller, Packet { makePollReply() }, statistics_, error);
}

bool Node::sendDiagnostics(const Endpoint& controller, std::string_view message, Error* error) {
    ArtDiagData diag;
    diag.priority = DiagPriority::Medium;
    diag.message = std::string(message);
    return sendPacket(socket_, controller, Packet { std::move(diag) }, statistics_, error);
}

bool Node::receiveOne(Error* error) {
    std::array<uint8_t, 1024> buffer {};
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

void Node::handlePacket(const Packet& packet, const Endpoint& sender) {
    if (std::holds_alternative<UnsupportedPacket>(packet)) {
        statistics_.unsupportedPackets++;
        return;
    }
    if (std::holds_alternative<ArtPoll>(packet)) {
        if (settings_.respondToPoll) {
            Error error;
            sendPollReply(sender, &error);
        }
    } else if (const auto* dmx = std::get_if<ArtDmx>(&packet)) {
        statistics_.dmxFramesReceived++;
        if (dmxCallback_) dmxCallback_(*dmx);
    } else if (const auto* nzs = std::get_if<ArtNzs>(&packet)) {
        if (nzsCallback_) nzsCallback_(*nzs);
    } else if (const auto* sync = std::get_if<ArtSync>(&packet)) {
        if (settings_.enableArtSync && syncCallback_) syncCallback_(*sync);
    } else if (const auto* address = std::get_if<ArtAddress>(&packet)) {
        if (settings_.enableArtAddress && addressCallback_) addressCallback_(*address);
    } else if (const auto* trigger = std::get_if<ArtTrigger>(&packet)) {
        if (triggerCallback_) triggerCallback_(*trigger);
    } else if (const auto* timeCode = std::get_if<ArtTimeCode>(&packet)) {
        if (timeCodeCallback_) timeCodeCallback_(*timeCode);
    } else if (const auto* input = std::get_if<ArtInput>(&packet)) {
        (void)input;
    }
}

ArtPollReply Node::makePollReply() const {
    ArtPollReply reply;
    reply.port = settings_.port;
    reply.shortName = settings_.shortName;
    reply.longName = settings_.longName;
    reply.nodeReport = "#0001 [0000] tcxArtNet ready";
    reply.oemCode = settings_.oemCode;
    reply.estaManufacturerCode = settings_.estaManufacturerCode;
    reply.bindIndex = settings_.bindIndex;
    reply.numberOfPorts = static_cast<uint16_t>(std::min<size_t>(4, settings_.inputPorts.size() + settings_.outputPorts.size()));
    for (size_t i = 0; i < settings_.outputPorts.size() && i < 4; ++i) {
        reply.portTypes[i] = 0x80;
        reply.goodOutput[i] = settings_.outputPorts[i].enabled ? 0x80 : 0x00;
        reply.swOut[i] = static_cast<uint8_t>(settings_.outputPorts[i].address.toPortAddress() & 0xff);
    }
    for (size_t i = 0; i < settings_.inputPorts.size() && i < 4; ++i) {
        reply.portTypes[i] |= 0x40;
        reply.goodInput[i] = settings_.inputPorts[i].enabled ? 0x80 : 0x00;
        reply.swIn[i] = static_cast<uint8_t>(settings_.inputPorts[i].address.toPortAddress() & 0xff);
    }
    reply.style = 0x00;
    reply.status1 = 0xd0;
    reply.status2 = 0x08;
    return reply;
}

} // namespace tcx::artnet
