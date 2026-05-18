#include "tcx/artnet/ArtNetOpcode.h"

namespace tcx::artnet {

std::optional<OpCode> opcodeFromValue(uint16_t value) noexcept {
    switch (value) {
        case 0x2000: return OpCode::Poll;
        case 0x2100: return OpCode::PollReply;
        case 0x2300: return OpCode::DiagData;
        case 0x2400: return OpCode::Command;
        case 0x2700: return OpCode::DataRequest;
        case 0x2800: return OpCode::DataReply;
        case 0x5000: return OpCode::Dmx;
        case 0x5100: return OpCode::Nzs;
        case 0x5200: return OpCode::Sync;
        case 0x6000: return OpCode::Address;
        case 0x7000: return OpCode::Input;
        case 0x8000: return OpCode::TodRequest;
        case 0x8100: return OpCode::TodData;
        case 0x8200: return OpCode::TodControl;
        case 0x8300: return OpCode::Rdm;
        case 0x8400: return OpCode::RdmSub;
        case 0x9000: return OpCode::Media;
        case 0x9100: return OpCode::MediaPatch;
        case 0x9200: return OpCode::MediaControl;
        case 0x9300: return OpCode::MediaControlReply;
        case 0x9700: return OpCode::TimeCode;
        case 0x9800: return OpCode::TimeSync;
        case 0x9900: return OpCode::Trigger;
        case 0x9a00: return OpCode::Directory;
        case 0x9b00: return OpCode::DirectoryReply;
        case 0xa010: return OpCode::VideoSetup;
        case 0xa020: return OpCode::VideoPalette;
        case 0xa040: return OpCode::VideoData;
        case 0xf200: return OpCode::FirmwareMaster;
        case 0xf300: return OpCode::FirmwareReply;
        case 0xf400: return OpCode::FileTnMaster;
        case 0xf500: return OpCode::FileFnMaster;
        case 0xf600: return OpCode::FileFnReply;
        case 0xf800: return OpCode::IpProg;
        case 0xf900: return OpCode::IpProgReply;
        default: return std::nullopt;
    }
}

std::string_view opcodeName(OpCode opcode) noexcept {
    switch (opcode) {
        case OpCode::Poll: return "ArtPoll";
        case OpCode::PollReply: return "ArtPollReply";
        case OpCode::DiagData: return "ArtDiagData";
        case OpCode::Command: return "ArtCommand";
        case OpCode::DataRequest: return "ArtDataRequest";
        case OpCode::DataReply: return "ArtDataReply";
        case OpCode::Dmx: return "ArtDmx";
        case OpCode::Nzs: return "ArtNzs";
        case OpCode::Sync: return "ArtSync";
        case OpCode::Address: return "ArtAddress";
        case OpCode::Input: return "ArtInput";
        case OpCode::TodRequest: return "ArtTodRequest";
        case OpCode::TodData: return "ArtTodData";
        case OpCode::TodControl: return "ArtTodControl";
        case OpCode::Rdm: return "ArtRdm";
        case OpCode::RdmSub: return "ArtRdmSub";
        case OpCode::Media: return "ArtMedia";
        case OpCode::MediaPatch: return "ArtMediaPatch";
        case OpCode::MediaControl: return "ArtMediaControl";
        case OpCode::MediaControlReply: return "ArtMediaControlReply";
        case OpCode::TimeCode: return "ArtTimeCode";
        case OpCode::TimeSync: return "ArtTimeSync";
        case OpCode::Trigger: return "ArtTrigger";
        case OpCode::Directory: return "ArtDirectory";
        case OpCode::DirectoryReply: return "ArtDirectoryReply";
        case OpCode::VideoSetup: return "ArtVideoSetup";
        case OpCode::VideoPalette: return "ArtVideoPalette";
        case OpCode::VideoData: return "ArtVideoData";
        case OpCode::FirmwareMaster: return "ArtFirmwareMaster";
        case OpCode::FirmwareReply: return "ArtFirmwareReply";
        case OpCode::FileTnMaster: return "ArtFileTnMaster";
        case OpCode::FileFnMaster: return "ArtFileFnMaster";
        case OpCode::FileFnReply: return "ArtFileFnReply";
        case OpCode::IpProg: return "ArtIpProg";
        case OpCode::IpProgReply: return "ArtIpProgReply";
    }
    return "Unknown";
}

} // namespace tcx::artnet
