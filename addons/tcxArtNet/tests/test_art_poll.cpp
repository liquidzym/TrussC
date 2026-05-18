#include "test_common.h"

void test_art_poll() {
    tcx::artnet::ArtPoll poll;
    poll.targetedMode = true;
    poll.requestDiagnostics = true;
    poll.diagnosticsUnicast = true;
    poll.diagnosticPriority = 0x40;
    poll.targetPortAddressBottom = 12;
    poll.targetPortAddressTop = 4096;
    auto decoded = roundTrip(poll);
    require(decoded.targetedMode, "ArtPoll targeted mode survives round trip");
    require(decoded.requestDiagnostics, "ArtPoll diagnostics request survives round trip");
    require(decoded.diagnosticsUnicast, "ArtPoll diagnostics unicast survives round trip");
    require(decoded.targetPortAddressBottom == 12, "ArtPoll target bottom survives round trip");
    require(decoded.targetPortAddressTop == 4096, "ArtPoll target top survives round trip");
}
