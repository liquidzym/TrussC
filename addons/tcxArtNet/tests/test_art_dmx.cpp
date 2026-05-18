#include "test_common.h"

void test_art_dmx() {
    using namespace tcx::artnet;
    ArtDmx dmx;
    dmx.sequence = 0;
    dmx.universe = { 2, 3, 4 };
    dmx.data = { 10, 20, 30, 40 };
    auto decoded = roundTrip(dmx);
    require(decoded.sequence == 0, "ArtDmx sequence zero disables ordering");
    require(decoded.universe.toPortAddress() == dmx.universe.toPortAddress(), "ArtDmx universe survives round trip");
    require(decoded.data == dmx.data, "ArtDmx payload survives round trip");

    dmx.sequence = 255;
    decoded = roundTrip(dmx);
    require(decoded.sequence == 255, "ArtDmx sequence 1..255 is preserved");

    Error error;
    dmx.data = { 1, 2, 3 };
    std::vector<uint8_t> bytes;
    require(!Codec::encode(Packet { dmx }, bytes, &error), "ArtDmx odd length is rejected");
    require(error.code == ErrorCode::InvalidLength, "ArtDmx odd length reports invalid length");
    dmx.data.assign(514, 0);
    require(!Codec::encode(Packet { dmx }, bytes, &error), "ArtDmx length over 512 is rejected");
}
