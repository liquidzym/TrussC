#pragma once

#include "Types.h"

#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

enum class NetworkPathStatus {
    Unknown,
    Satisfied,
    Unsatisfied,
    RequiresConnection
};

enum class NetworkInterface {
    WiFi,
    Cellular,
    WiredEthernet,
    Loopback,
    Other
};

struct NetworkPath {
    NetworkPathStatus status = NetworkPathStatus::Unknown;
    bool expensive = false;
    bool constrained = false;
    std::vector<NetworkInterface> interfaces;
};

using NetworkPathHandler = std::function<void(const NetworkPath&)>;

class NetworkStatus {
public:
    NetworkPath current() const;
    void start(NetworkPathHandler handler);
    void stop();
    bool isRunning() const;
};

NetworkStatus& networkStatus();

std::string toString(NetworkPathStatus status);
std::string toString(NetworkInterface interfaceType);

} // namespace tcx::ios
