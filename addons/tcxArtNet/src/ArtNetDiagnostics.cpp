#include "tcx/artnet/ArtNetDiagnostics.h"

#include "tcx/artnet/ArtNetSocket.h"

#include <charconv>
#include <sstream>
#include <utility>

namespace tcx::artnet {

const char* recoveryStateName(RecoveryState state) noexcept {
    switch (state) {
        case RecoveryState::Idle:
            return "idle";
        case RecoveryState::Recovering:
            return "recovering";
        case RecoveryState::Recovered:
            return "recovered";
        case RecoveryState::Failed:
            return "failed";
    }
    return "unknown";
}

bool parseIpv4Address(std::string_view ip, std::array<uint8_t, 4>& out, Error* error) {
    std::array<uint8_t, 4> parsed {};
    size_t offset = 0;
    for (size_t i = 0; i < parsed.size(); ++i) {
        const size_t dot = ip.find('.', offset);
        const size_t end = dot == std::string_view::npos ? ip.size() : dot;
        if (end == offset) {
            setError(error, ErrorCode::InvalidAddress, "invalid IPv4 address: empty octet");
            return false;
        }

        unsigned int value = 0;
        const auto* first = ip.data() + offset;
        const auto* last = ip.data() + end;
        const auto result = std::from_chars(first, last, value);
        if (result.ec != std::errc {} || result.ptr != last || value > 255) {
            setError(error, ErrorCode::InvalidAddress, "invalid IPv4 address: " + std::string(ip));
            return false;
        }
        parsed[i] = static_cast<uint8_t>(value);

        if (i < parsed.size() - 1) {
            if (dot == std::string_view::npos) {
                setError(error, ErrorCode::InvalidAddress, "invalid IPv4 address: " + std::string(ip));
                return false;
            }
            offset = dot + 1;
        } else if (dot != std::string_view::npos) {
            setError(error, ErrorCode::InvalidAddress, "invalid IPv4 address: " + std::string(ip));
            return false;
        }
    }
    out = parsed;
    return true;
}

std::string ipv4AddressToString(const std::array<uint8_t, 4>& ip) {
    std::ostringstream out;
    out << static_cast<int>(ip[0]) << '.'
        << static_cast<int>(ip[1]) << '.'
        << static_cast<int>(ip[2]) << '.'
        << static_cast<int>(ip[3]);
    return out.str();
}

bool makeDirectedBroadcastIp(
    std::string_view ip,
    std::string_view subnetMask,
    std::string& out,
    Error* error
) {
    std::array<uint8_t, 4> address {};
    std::array<uint8_t, 4> mask {};
    if (!parseIpv4Address(ip, address, error)) {
        return false;
    }
    if (!parseIpv4Address(subnetMask, mask, error)) {
        return false;
    }

    std::array<uint8_t, 4> broadcast {};
    for (size_t i = 0; i < broadcast.size(); ++i) {
        broadcast[i] = static_cast<uint8_t>(address[i] | static_cast<uint8_t>(~mask[i]));
    }
    out = ipv4AddressToString(broadcast);
    return true;
}

bool makeDirectedBroadcastEndpoint(
    std::string_view ip,
    std::string_view subnetMask,
    uint16_t port,
    Endpoint& out,
    Error* error
) {
    std::string broadcastIp;
    if (!makeDirectedBroadcastIp(ip, subnetMask, broadcastIp, error)) {
        return false;
    }
    out = Endpoint { std::move(broadcastIp), port };
    return true;
}

UdpOutputProbeResult probeUdpOutput(const UdpOutputProbeOptions& options, std::span<const uint8_t> payload) {
    UdpOutputProbeResult result;
    result.target = options.target;

    UdpSocket socket;
    Error error;
    result.ok = socket.open(&error) &&
        socket.setReuseAddress(true, &error) &&
        socket.setBroadcast(options.enableBroadcast, &error) &&
        socket.bind(options.localBindIp, 0, &error) &&
        (!options.nonBlocking || socket.setNonBlocking(true, &error)) &&
        socket.sendTo(payload, options.target, &error);

    result.socket = socket.diagnostics();
    if (result.ok) {
        result.bytesSent = payload.size();
    } else {
        result.error = error;
    }
    return result;
}

} // namespace tcx::artnet
