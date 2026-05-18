#include "test_common.h"

void test_art_sync() {
    auto decoded = roundTrip(tcx::artnet::ArtSync {});
    require(decoded.protocolVersion == tcx::artnet::ProtocolVersion, "ArtSync protocol version survives round trip");
}
