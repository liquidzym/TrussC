#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace tcx::artnet {

enum class OpCode : uint16_t {
    Poll                = 0x2000,
    PollReply           = 0x2100,
    DiagData            = 0x2300,
    Command             = 0x2400,
    DataRequest         = 0x2700,
    DataReply           = 0x2800,

    Output              = 0x5000,
    Dmx                 = 0x5000,
    Nzs                 = 0x5100,
    Sync                = 0x5200,

    Address             = 0x6000,
    Input               = 0x7000,

    TodRequest          = 0x8000,
    TodData             = 0x8100,
    TodControl          = 0x8200,

    Rdm                 = 0x8300,
    RdmSub              = 0x8400,

    Media               = 0x9000,
    MediaPatch          = 0x9100,
    MediaControl        = 0x9200,
    MediaControlReply   = 0x9300,

    TimeCode            = 0x9700,
    TimeSync            = 0x9800,
    Trigger             = 0x9900,
    Directory           = 0x9a00,
    DirectoryReply      = 0x9b00,

    VideoSetup          = 0xa010,
    VideoPalette        = 0xa020,
    VideoData           = 0xa040,

    FirmwareMaster      = 0xf200,
    FirmwareReply       = 0xf300,
    FileTnMaster        = 0xf400,
    FileFnMaster        = 0xf500,
    FileFnReply         = 0xf600,
    IpProg              = 0xf800,
    IpProgReply         = 0xf900
};

[[nodiscard]] std::optional<OpCode> opcodeFromValue(uint16_t value) noexcept;
[[nodiscard]] std::string_view opcodeName(OpCode opcode) noexcept;

} // namespace tcx::artnet
