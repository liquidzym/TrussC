#include "test_common.h"

void test_header() {
    using namespace tcx::artnet;
    ArtDmx dmx;
    dmx.universe = { 1, 2, 3 };
    dmx.data = { 1, 2 };
    std::vector<uint8_t> bytes;
    Error error;
    require(Codec::encode(Packet { dmx }, bytes, &error), "ArtDmx encodes");
    require(std::equal(ArtNetId.begin(), ArtNetId.end(), bytes.begin()), "Art-Net ID is written");
    require(readLe16(bytes, 8) == static_cast<uint16_t>(OpCode::Dmx), "opcode is little-endian");
    require(readBe16(bytes, 10) == ProtocolVersion, "protocol version is big-endian");
    require(Codec::readOpcode(bytes, &error) == OpCode::Dmx, "readOpcode returns ArtDmx");
}
