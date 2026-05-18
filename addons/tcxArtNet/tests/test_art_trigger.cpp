#include "test_common.h"

void test_art_trigger() {
    using namespace tcx::artnet;
    ArtTrigger trigger;
    trigger.oemCode = 0x1234;
    trigger.key = 5;
    trigger.subKey = 6;
    trigger.payload = { 1, 2, 3, 4 };
    auto decoded = roundTrip(trigger);
    require(decoded.oemCode == 0x1234, "ArtTrigger OEM survives round trip");
    require(decoded.key == 5 && decoded.subKey == 6, "ArtTrigger key fields survive round trip");
    require(decoded.payload == trigger.payload, "ArtTrigger payload survives round trip");

    Error error;
    std::vector<uint8_t> bytes;
    trigger.payload.assign(513, 1);
    require(!Codec::encode(Packet { trigger }, bytes, &error), "ArtTrigger oversized payload is rejected");
}
