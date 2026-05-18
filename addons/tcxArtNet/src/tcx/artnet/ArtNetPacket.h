#pragma once

#include "ArtNetTypes.h"

#include <variant>

namespace tcx::artnet {

using Packet = std::variant<
    ArtPoll,
    ArtPollReply,
    ArtDiagData,
    ArtCommand,
    ArtDataRequest,
    ArtDataReply,
    ArtDmx,
    ArtNzs,
    ArtSync,
    ArtAddress,
    ArtInput,
    ArtTodRequest,
    ArtTodData,
    ArtTodControl,
    ArtMedia,
    ArtMediaPatch,
    ArtMediaControl,
    ArtMediaControlReply,
    ArtTimeCode,
    ArtTimeSync,
    ArtTrigger,
    ArtDirectory,
    ArtDirectoryReply,
    ArtVideoSetup,
    ArtVideoPalette,
    ArtVideoData,
    ArtFirmwareMaster,
    ArtFirmwareReply,
    ArtFileTnMaster,
    ArtFileFnMaster,
    ArtFileFnReply,
    ArtIpProg,
    ArtIpProgReply,
    UnsupportedPacket
>;

} // namespace tcx::artnet
