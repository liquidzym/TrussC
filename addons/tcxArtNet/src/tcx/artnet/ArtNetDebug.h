#pragma once

#include "ArtNetPacket.h"

#include <span>
#include <string>
#include <vector>

namespace tcx::artnet {

class PacketInspector {
public:
    [[nodiscard]] static std::string bytesToHex(std::span<const uint8_t> bytes);
    [[nodiscard]] static std::vector<uint8_t> decodeHexBytes(std::string_view hex);
    [[nodiscard]] static std::string summarize(const Packet& packet);
};

} // namespace tcx::artnet
