#pragma once

#include "ArtNetOpcode.h"
#include "ArtNetPacket.h"

#include <optional>
#include <span>
#include <vector>

namespace tcx::artnet {

[[nodiscard]] uint16_t readLe16(std::span<const uint8_t> data, size_t offset) noexcept;
[[nodiscard]] uint16_t readBe16(std::span<const uint8_t> data, size_t offset) noexcept;
void writeLe16(std::vector<uint8_t>& out, uint16_t value);
void writeBe16(std::vector<uint8_t>& out, uint16_t value);

class Codec {
public:
    static bool decode(std::span<const uint8_t> bytes, Packet& outPacket, Error* error = nullptr);
    static bool encode(const Packet& packet, std::vector<uint8_t>& outBytes, Error* error = nullptr);
    static std::optional<OpCode> readOpcode(std::span<const uint8_t> bytes, Error* error = nullptr);
};

} // namespace tcx::artnet
