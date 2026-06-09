#include "tcx/artnet/ArtNetController.h"

#include <algorithm>
#include <array>
#include <vector>

namespace tcx::artnet {

namespace {

uint8_t takeNextSequence(uint8_t& nextSequence) noexcept {
    const uint8_t sequence = nextSequence;
    nextSequence = (nextSequence == 255) ? 1 : static_cast<uint8_t>(nextSequence + 1);
    return sequence;
}

bool sendEncoded(
    UdpSocket& socket,
    const Endpoint& endpoint,
    const Packet& packet,
    Statistics& statistics,
    ControllerNetworkDiagnostics* diagnostics,
    Error* error
) {
    if (diagnostics) {
        diagnostics->lastTarget = endpoint;
    }
    std::vector<uint8_t> bytes;
    Error localError;
    if (!Codec::encode(packet, bytes, &localError)) {
        if (diagnostics) {
            diagnostics->lastError = localError;
        }
        setError(error, localError.code, localError.message);
        return false;
    }
    if (!socket.sendTo(bytes, endpoint, &localError)) {
        if (diagnostics) {
            diagnostics->socket = socket.diagnostics();
            diagnostics->lastError = localError;
            diagnostics->recoveryState = RecoveryState::Failed;
        }
        setError(error, localError.code, localError.message);
        return false;
    }
    if (diagnostics) {
        diagnostics->socket = socket.diagnostics();
        diagnostics->lastError = {};
    }
    statistics.packetsSent++;
    if (std::holds_alternative<ArtDmx>(packet)) {
        statistics.dmxFramesSent++;
    }
    return true;
}

} // namespace

SessionSettings::SessionSettings() {
    sender.enableBroadcast = true;
    controller.localPort = 0;
}

Session::~Session() {
    close();
}

bool Session::setup(const SessionSettings& settings, Error* error) {
    close();
    settings_ = settings;

    if (settings.startReceiverThread && !settings.setupReceiver) {
        setError(error, ErrorCode::InvalidConfiguration, "Session startReceiverThread requires setupReceiver");
        return false;
    }

    if (settings.setupSender) {
        if (!sender_.setup(settings.sender, error)) {
            close();
            return false;
        }
        senderReady_ = true;
    }

    if (settings.setupReceiver) {
        if (!receiver_.setup(settings.receiver, error)) {
            close();
            return false;
        }
        receiverReady_ = true;
        if (settings.startReceiverThread && !receiver_.startThread(error)) {
            close();
            return false;
        }
    }

    if (settings.setupController) {
        if (!controller_.setup(settings.controller, error)) {
            close();
            return false;
        }
        controllerReady_ = true;
    }

    return true;
}

void Session::close() {
    receiver_.close();
    controller_.close();
    sender_.close();
    senderReady_ = false;
    receiverReady_ = false;
    controllerReady_ = false;
}

bool Session::recover(Error* error) {
    return setup(settings_, error);
}

void Session::update() {
    Error error;
    if (controllerReady_) {
        controller_.update();
    }
    if (receiverReady_ && !settings_.startReceiverThread) {
        receiver_.poll(&error);
    }
}

bool Session::pollNodes(Error* error) {
    if (!controllerReady_) {
        setError(error, ErrorCode::InvalidConfiguration, "Session controller is not set up");
        return false;
    }
    return controller_.pollNodes(error);
}

std::vector<NodeInfo> Session::getDiscoveredNodes() const {
    if (!controllerReady_) {
        return {};
    }
    return controller_.getDiscoveredNodes();
}

bool Controller::setup(const ControllerSettings& settings, Error* error) {
    close();
    settings_ = settings;
    lastPoll_ = {};
    diagnostics_ = {};
    diagnostics_.enableBroadcast = settings.enableBroadcast;
    diagnostics_.directedBroadcastIp = settings.directedBroadcastIp;
    recvBuffer_.resize(65535);
    nextDmxSequence_ = 1;

    Error localError;
    if (!socket_.open(&localError) ||
        !socket_.setReuseAddress(true, &localError) ||
        !socket_.setBroadcast(settings.enableBroadcast, &localError) ||
        !socket_.bind(settings.localBindIp, settings.localPort, &localError) ||
        !socket_.setNonBlocking(true, &localError)) {
        diagnostics_.socket = socket_.diagnostics();
        diagnostics_.lastError = localError;
        diagnostics_.recoveryState = RecoveryState::Failed;
        setError(error, localError.code, localError.message);
        return false;
    }

    diagnostics_.socket = socket_.diagnostics();
    diagnostics_.lastError = {};
    diagnostics_.recoveryState = RecoveryState::Idle;
    return true;
}

void Controller::close() {
    socket_.close();
    diagnostics_.socket = socket_.diagnostics();
    std::lock_guard<std::mutex> lock(nodesMutex_);
    nodes_.clear();
}

bool Controller::recover(Error* error) {
    const ControllerSettings settings = settings_;
    const uint64_t nextAttempt = diagnostics_.recoveryAttempts + 1;
    diagnostics_.recoveryState = RecoveryState::Recovering;
    diagnostics_.recoveryAttempts = nextAttempt;

    Error localError;
    const bool ok = setup(settings, &localError);
    diagnostics_.recoveryAttempts = nextAttempt;
    diagnostics_.recoveryState = ok ? RecoveryState::Recovered : RecoveryState::Failed;
    if (!ok) {
        diagnostics_.lastError = localError;
        setError(error, localError.code, localError.message);
        return false;
    }
    return true;
}

bool Controller::pollNodes(Error* error) {
    ArtPoll poll;
    poll.requestDiagnostics = settings_.enableDiagnostics;
    poll.diagnosticsUnicast = true;
    poll.targetedMode = settings_.enableTargetedPoll;
    poll.targetPortAddressBottom = settings_.targetPortAddressBottom;
    poll.targetPortAddressTop = settings_.targetPortAddressTop;
    const Endpoint target { settings_.directedBroadcastIp, DefaultPort };
    return sendEncoded(socket_, target, Packet { poll }, statistics_, &diagnostics_, error);
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
    packet.sequence = takeNextSequence(nextDmxSequence_);
    packet.universe = universe;
    packet.data.assign(dmx.begin(), dmx.end());
    return sendEncoded(socket_, endpoint, Packet { std::move(packet) }, statistics_, &diagnostics_, error);
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
    return sendEncoded(socket_, target, Packet { ArtSync {} }, statistics_, &diagnostics_, error);
}

bool Controller::sendAddressCommand(const Endpoint& endpoint, const ArtAddressCommand& command, Error* error) {
    return sendEncoded(socket_, endpoint, Packet { command }, statistics_, &diagnostics_, error);
}

bool Controller::sendTrigger(const Endpoint& endpoint, const ArtTrigger& trigger, Error* error) {
    return sendEncoded(socket_, endpoint, Packet { trigger }, statistics_, &diagnostics_, error);
}

bool Controller::sendTimeCode(const Endpoint& endpoint, const ArtTimeCode& timecode, Error* error) {
    return sendEncoded(socket_, endpoint, Packet { timecode }, statistics_, &diagnostics_, error);
}

ControllerNetworkDiagnostics Controller::networkDiagnostics() const {
    ControllerNetworkDiagnostics diagnostics = diagnostics_;
    diagnostics.socket = socket_.diagnostics();
    return diagnostics;
}

bool Controller::receiveOne(Error* error) {
    if (recvBuffer_.empty()) {
        recvBuffer_.resize(65535);
    }
    size_t bytesReceived = 0;
    Endpoint sender;
    Error receiveError;
    if (!socket_.receiveFrom(recvBuffer_, bytesReceived, sender, &receiveError)) {
        if (receiveError.code != ErrorCode::None) {
            setError(error, receiveError.code, receiveError.message);
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
