#include "test_common.h"

void test_art_ip_prog() {
    tcx::artnet::ArtIpProg prog;
    prog.command = 0x07;
    prog.ip = { 10, 0, 0, 25 };
    prog.subnetMask = { 255, 255, 255, 0 };
    prog.defaultGateway = { 10, 0, 0, 1 };
    auto decoded = roundTrip(prog);
    require(decoded.command == 0x07, "ArtIpProg command survives round trip");
    require(decoded.ip[3] == 25, "ArtIpProg IP survives round trip");
    require(decoded.defaultGateway[3] == 1, "ArtIpProg gateway survives round trip");

    tcx::artnet::ArtIpProgReply reply;
    reply.ip = prog.ip;
    reply.subnetMask = prog.subnetMask;
    reply.defaultGateway = prog.defaultGateway;
    auto decodedReply = roundTrip(reply);
    require(decodedReply.ip[0] == 10 && decodedReply.subnetMask[0] == 255, "ArtIpProgReply survives round trip");
}
