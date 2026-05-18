#include "tcx/artnet/ArtNetNode.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <sstream>
#include <string_view>
#include <vector>

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

std::array<uint8_t, 4> parseIpv4(std::string_view ip) {
    std::array<uint8_t, 4> out {};
    size_t offset = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        const size_t dot = ip.find('.', offset);
        const size_t end = dot == std::string_view::npos ? ip.size() : dot;
        unsigned int value = 0;
        const auto* first = ip.data() + offset;
        const auto* last = ip.data() + end;
        const auto result = std::from_chars(first, last, value);
        if (result.ec != std::errc {} || result.ptr != last || value > 255) {
            return {};
        }
        out[i] = static_cast<uint8_t>(value);
        if (i < out.size() - 1) {
            if (dot == std::string_view::npos) {
                return {};
            }
            offset = dot + 1;
        } else if (dot != std::string_view::npos) {
            return {};
        }
    }
    return out;
}

std::string ipv4ToString(const std::array<uint8_t, 4>& ip) {
    std::ostringstream out;
    out << static_cast<int>(ip[0]) << '.'
        << static_cast<int>(ip[1]) << '.'
        << static_cast<int>(ip[2]) << '.'
        << static_cast<int>(ip[3]);
    return out.str();
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

void Node::setInputCallback(std::function<void(const ArtInput&)> callback) {
    inputCallback_ = std::move(callback);
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
        if (settings_.enableArtAddress) {
            if (!address->shortName.empty()) settings_.shortName = address->shortName;
            if (!address->longName.empty()) settings_.longName = address->longName;
            if (addressCallback_) addressCallback_(*address);
        }
    } else if (const auto* trigger = std::get_if<ArtTrigger>(&packet)) {
        if (triggerCallback_) triggerCallback_(*trigger);
    } else if (const auto* timeCode = std::get_if<ArtTimeCode>(&packet)) {
        if (timeCodeCallback_) timeCodeCallback_(*timeCode);
    } else if (const auto* input = std::get_if<ArtInput>(&packet)) {
        if (settings_.enableArtInput && inputCallback_) inputCallback_(*input);
    } else if (const auto* ipProg = std::get_if<ArtIpProg>(&packet)) {
        if (settings_.enableIpProg) {
            settings_.ipAddress = ipv4ToString(ipProg->ip);
            settings_.bindIpAddress = settings_.ipAddress;
            settings_.subnetMask = ipProg->subnetMask;
            settings_.defaultGateway = ipProg->defaultGateway;
            ArtIpProgReply reply;
            reply.ip = ipProg->ip;
            reply.subnetMask = ipProg->subnetMask;
            reply.portAddress = ipProg->portAddress;
            reply.status = 0x40;
            reply.defaultGateway = ipProg->defaultGateway;
            Error error;
            sendPacket(socket_, sender, Packet { reply }, statistics_, &error);
        }
    }
}

ArtPollReply Node::makePollReply() const {
    ArtPollReply reply;
    reply.port = settings_.port;
    reply.ipAddress = parseIpv4(settings_.ipAddress);
    reply.bindIp = parseIpv4(settings_.bindIpAddress);
    reply.shortName = settings_.shortName;
    reply.longName = settings_.longName;
    reply.nodeReport = "#0001 [0000] tcxArtNet ready";
    reply.oemCode = settings_.oemCode;
    reply.estaManufacturerCode = settings_.estaManufacturerCode;
    reply.bindIndex = settings_.bindIndex;
    reply.numberOfPorts = static_cast<uint16_t>(std::min<size_t>(4, settings_.inputPorts.size() + settings_.outputPorts.size()));
    if (!settings_.outputPorts.empty()) {
        reply.netSwitch = settings_.outputPorts.front().address.net;
        reply.subSwitch = settings_.outputPorts.front().address.subnet;
    } else if (!settings_.inputPorts.empty()) {
        reply.netSwitch = settings_.inputPorts.front().address.net;
        reply.subSwitch = settings_.inputPorts.front().address.subnet;
    }
    for (size_t i = 0; i < settings_.outputPorts.size() && i < 4; ++i) {
        reply.portTypes[i] = 0x80;
        reply.goodOutput[i] = settings_.outputPorts[i].enabled ? 0x80 : 0x00;
        reply.swOut[i] = settings_.outputPorts[i].address.universe;
    }
    for (size_t i = 0; i < settings_.inputPorts.size() && i < 4; ++i) {
        reply.portTypes[i] |= 0x40;
        reply.goodInput[i] = settings_.inputPorts[i].enabled ? 0x80 : 0x00;
        reply.swIn[i] = settings_.inputPorts[i].address.universe;
    }
    reply.style = 0x00;
    reply.status1 = 0xd0;
    reply.status2 = 0x08;
    return reply;
}

} // namespace tcx::artnet
