#include "test_common.h"

void test_art_nzs() {
    using namespace tcx::artnet;
    ArtNzs nzs;
    nzs.startCode = 1;
    nzs.universe = { 0, 0, 1 };
    nzs.data = { 9, 8, 7 };
    auto decoded = roundTrip(nzs);
    require(decoded.startCode == 1, "ArtNzs start code survives round trip");
    require(decoded.data == nzs.data, "ArtNzs payload survives round trip");

    Error error;
    std::vector<uint8_t> bytes;
    nzs.startCode = 0;
    require(!Codec::encode(Packet { nzs }, bytes, &error), "ArtNzs start code zero is rejected");
    nzs.startCode = RdmStartCode;
    require(!Codec::encode(Packet { nzs }, bytes, &error), "ArtNzs RDM start code is rejected");
    require(error.code == ErrorCode::UnsupportedRdm, "ArtNzs RDM start code reports unsupported RDM");
}
