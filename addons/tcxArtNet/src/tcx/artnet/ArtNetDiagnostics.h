#pragma once
#include "ArtNetTypes.h"

#include <cstddef>
#include <span>

namespace tcx::artnet {

enum class RecoveryState {
    Idle,
    Recovering,
    Recovered,
    Failed
};

[[nodiscard]] const char* recoveryStateName(RecoveryState state) noexcept;

struct SocketDiagnostics {
    bool open = false;
    bool reuseAddress = false;
    bool broadcast = false;
    bool nonBlocking = false;
    std::string requestedBindIp = "0.0.0.0";
    uint16_t requestedBindPort = 0;
    Endpoint actualLocalEndpoint;
    Error lastError;
};

struct ControllerNetworkDiagnostics {
    SocketDiagnostics socket;
    bool enableBroadcast = false;
    std::string directedBroadcastIp;
    Endpoint lastTarget;
    Error lastError;
    RecoveryState recoveryState = RecoveryState::Idle;
    uint64_t recoveryAttempts = 0;
};

struct UdpOutputProbeOptions {
    Endpoint target;
    std::string localBindIp = "0.0.0.0";
    bool enableBroadcast = false;
    bool nonBlocking = true;
};

struct UdpOutputProbeResult {
    bool ok = false;
    Endpoint target;
    std::size_t bytesSent = 0;
    SocketDiagnostics socket;
    Error error;
};

bool parseIpv4Address(std::string_view ip, std::array<uint8_t, 4>& out, Error* error = nullptr);
std::string ipv4AddressToString(const std::array<uint8_t, 4>& ip);
bool makeDirectedBroadcastIp(
    std::string_view ip,
    std::string_view subnetMask,
    std::string& out,
    Error* error = nullptr
);
bool makeDirectedBroadcastEndpoint(
    std::string_view ip,
    std::string_view subnetMask,
    uint16_t port,
    Endpoint& out,
    Error* error = nullptr
);
UdpOutputProbeResult probeUdpOutput(const UdpOutputProbeOptions& options, std::span<const uint8_t> payload);

} // namespace tcx::artnet
