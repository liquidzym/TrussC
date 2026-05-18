#include "test_common.h"

void test_art_timecode() {
    tcx::artnet::ArtTimeCode timeCode;
    timeCode.frames = 12;
    timeCode.seconds = 34;
    timeCode.minutes = 56;
    timeCode.hours = 1;
    timeCode.type = tcx::artnet::TimeCodeType::EBU;
    timeCode.streamId = 9;
    auto decodedTimeCode = roundTrip(timeCode);
    require(decodedTimeCode.frames == 12 && decodedTimeCode.seconds == 34, "ArtTimeCode clock fields survive round trip");
    require(decodedTimeCode.type == tcx::artnet::TimeCodeType::EBU, "ArtTimeCode type survives round trip");
    require(decodedTimeCode.streamId == 9, "ArtTimeCode streamId survives round trip");

    tcx::artnet::ArtTimeSync sync;
    sync.hours = 2;
    sync.minutes = 3;
    sync.seconds = 4;
    sync.days = 5;
    sync.month = 6;
    sync.year = 2026;
    auto decodedSync = roundTrip(sync);
    require(decodedSync.hours == 2 && decodedSync.year == 2026, "ArtTimeSync survives round trip");
}
