#include "test_common.h"

void test_art_diag_data() {
    tcx::artnet::ArtDiagData diag;
    diag.priority = tcx::artnet::DiagPriority::High;
    diag.message = "diagnostic message";
    auto decoded = roundTrip(diag);
    require(decoded.priority == tcx::artnet::DiagPriority::High, "ArtDiagData priority survives round trip");
    require(decoded.message == "diagnostic message", "ArtDiagData message survives round trip");
}
