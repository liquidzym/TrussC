#include "test_common.h"

void test_packet_validation() {
    using namespace tcx::artnet;
    Error error;
    Packet packet;
    std::vector<uint8_t> badId = { 'B', 'a', 'd', 0, 0, 0, 0, 0, 0, 0 };
    require(!Codec::decode(badId, packet, &error), "invalid Art-Net ID is rejected");
    require(error.code == ErrorCode::InvalidHeader, "invalid Art-Net ID reports invalid header");

    std::vector<uint8_t> unknown(12, 0);
    std::copy(ArtNetId.begin(), ArtNetId.end(), unknown.begin());
    unknown[8] = 0x34;
    unknown[9] = 0x12;
    unknown[10] = 0;
    unknown[11] = ProtocolVersion;
    require(Codec::decode(unknown, packet, &error), "unknown opcode decodes as UnsupportedPacket");
    require(std::holds_alternative<UnsupportedPacket>(packet), "unknown opcode yields UnsupportedPacket");
    require(std::get<UnsupportedPacket>(packet).reason == ErrorCode::UnsupportedOpcode, "unknown opcode reason is UnsupportedOpcode");

    std::vector<uint8_t> rdm(12, 0);
    std::copy(ArtNetId.begin(), ArtNetId.end(), rdm.begin());
    rdm[8] = 0x00;
    rdm[9] = 0x83;
    rdm[10] = 0;
    rdm[11] = ProtocolVersion;
    require(Codec::decode(rdm, packet, &error), "RDM opcode decodes as unsupported");
    require(std::get<UnsupportedPacket>(packet).reason == ErrorCode::UnsupportedRdm, "RDM opcode reason is UnsupportedRdm");

    ArtDmx dmx;
    dmx.universe = { 0, 0, 0 };
    dmx.data = { 1, 2, 3, 4 };
    std::vector<uint8_t> bytes;
    require(Codec::encode(Packet { dmx }, bytes, &error), "valid ArtDmx encodes before truncation");
    bytes.pop_back();
    require(!Codec::decode(bytes, packet, &error), "truncated ArtDmx is rejected");
    require(error.code == ErrorCode::TruncatedPacket, "truncated ArtDmx reports truncation");
}
