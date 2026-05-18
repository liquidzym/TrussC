#include "test_common.h"

void test_art_input() {
    tcx::artnet::ArtInput input;
    input.bindIndex = 4;
    input.numPorts = 2;
    input.input = { 0x01, 0x00, 0x01, 0x00 };
    auto decoded = roundTrip(input);
    require(decoded.bindIndex == 4, "ArtInput bind index survives round trip");
    require(decoded.numPorts == 2, "ArtInput port count survives round trip");
    require(decoded.input[0] == 0x01 && decoded.input[2] == 0x01, "ArtInput flags survive round trip");
}
