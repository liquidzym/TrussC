#pragma once

#include "Types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

struct MultipeerConfig {
    std::string serviceType = "tcxios";
    std::string displayName;
    bool autoInvite = true;
    bool autoAccept = true;
};

struct MultipeerPeer {
    std::string identifier;
    std::string displayName;
    bool connected = false;
};

struct MultipeerMessage {
    MultipeerPeer peer;
    std::vector<std::uint8_t> data;
};

using MultipeerPeerHandler = std::function<void(const std::vector<MultipeerPeer>&)>;
using MultipeerMessageHandler = std::function<void(const MultipeerMessage&)>;

class Multipeer {
public:
    void start(const MultipeerConfig& config, Completion<void> done);
    void stop();
    std::vector<MultipeerPeer> peers() const;
    void send(const std::vector<std::uint8_t>& data, Completion<void> done);
    void setPeerHandler(MultipeerPeerHandler handler);
    void setMessageHandler(MultipeerMessageHandler handler);
};

Multipeer& multipeer();

} // namespace tcx::ios
