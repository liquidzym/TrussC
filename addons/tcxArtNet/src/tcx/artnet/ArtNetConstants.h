#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tcx::artnet {

inline constexpr std::array<uint8_t, 8> ArtNetId {
    'A', 'r', 't', '-', 'N', 'e', 't', 0x00
};

inline constexpr uint16_t DefaultPort = 6454;
inline constexpr uint16_t ProtocolVersion = 14;
inline constexpr size_t MaxDmxChannels = 512;
inline constexpr size_t MinArtDmxChannels = 2;
inline constexpr size_t ArtNetHeaderSize = 10;
inline constexpr uint8_t RdmStartCode = 0xcc;

} // namespace tcx::artnet
