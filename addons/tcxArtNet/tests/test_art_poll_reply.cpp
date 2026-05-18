#include "test_common.h"

void test_art_poll_reply() {
    tcx::artnet::ArtPollReply reply;
    reply.ipAddress = { 192, 168, 0, 77 };
    reply.shortName = "short";
    reply.longName = "long node";
    reply.nodeReport = "#0001 ready";
    reply.numberOfPorts = 2;
    reply.swOut[0] = 7;
    auto decoded = roundTrip(reply);
    require(decoded.ipAddress[0] == 192 && decoded.ipAddress[3] == 77, "ArtPollReply IP survives round trip");
    require(decoded.shortName == "short", "ArtPollReply short name survives round trip");
    require(decoded.longName == "long node", "ArtPollReply long name survives round trip");
    require(decoded.nodeReport == "#0001 ready", "ArtPollReply node report survives round trip");
    require(decoded.swOut[0] == 7, "ArtPollReply port address survives round trip");
}
