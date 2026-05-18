#include "test_common.h"

void test_art_address() {
    tcx::artnet::ArtAddress address;
    address.netSwitch = 3;
    address.bindIndex = 2;
    address.shortName = "fixture";
    address.longName = "fixture long";
    address.swIn = { 1, 2, 3, 4 };
    address.swOut = { 5, 6, 7, 8 };
    address.subSwitch = 9;
    address.command = 0x10;
    auto decoded = roundTrip(address);
    require(decoded.netSwitch == 3, "ArtAddress net switch survives round trip");
    require(decoded.bindIndex == 2, "ArtAddress bind index survives round trip");
    require(decoded.shortName == "fixture", "ArtAddress short name survives round trip");
    require(decoded.longName == "fixture long", "ArtAddress long name survives round trip");
    require(decoded.swIn[2] == 3 && decoded.swOut[3] == 8, "ArtAddress input/output ports survive round trip");
    require(decoded.subSwitch == 9 && decoded.command == 0x10, "ArtAddress command survives round trip");
}
