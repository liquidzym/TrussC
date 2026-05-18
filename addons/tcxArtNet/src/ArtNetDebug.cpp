#include "tcx/artnet/ArtNetDebug.h"

#include "tcx/artnet/ArtNetOpcode.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace tcx::artnet {

namespace {

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

template <typename T>
std::string_view packetTypeName() {
    if constexpr (std::is_same_v<T, ArtPoll>) return "ArtPoll";
    else if constexpr (std::is_same_v<T, ArtPollReply>) return "ArtPollReply";
    else if constexpr (std::is_same_v<T, ArtDiagData>) return "ArtDiagData";
    else if constexpr (std::is_same_v<T, ArtCommand>) return "ArtCommand";
    else if constexpr (std::is_same_v<T, ArtDataRequest>) return "ArtDataRequest";
    else if constexpr (std::is_same_v<T, ArtDataReply>) return "ArtDataReply";
    else if constexpr (std::is_same_v<T, ArtDmx>) return "ArtDmx";
    else if constexpr (std::is_same_v<T, ArtNzs>) return "ArtNzs";
    else if constexpr (std::is_same_v<T, ArtSync>) return "ArtSync";
    else if constexpr (std::is_same_v<T, ArtAddress>) return "ArtAddress";
    else if constexpr (std::is_same_v<T, ArtInput>) return "ArtInput";
    else if constexpr (std::is_same_v<T, ArtTodRequest>) return "ArtTodRequest";
    else if constexpr (std::is_same_v<T, ArtTodData>) return "ArtTodData";
    else if constexpr (std::is_same_v<T, ArtTodControl>) return "ArtTodControl";
    else if constexpr (std::is_same_v<T, ArtMedia>) return "ArtMedia";
    else if constexpr (std::is_same_v<T, ArtMediaPatch>) return "ArtMediaPatch";
    else if constexpr (std::is_same_v<T, ArtMediaControl>) return "ArtMediaControl";
    else if constexpr (std::is_same_v<T, ArtMediaControlReply>) return "ArtMediaControlReply";
    else if constexpr (std::is_same_v<T, ArtTimeCode>) return "ArtTimeCode";
    else if constexpr (std::is_same_v<T, ArtTimeSync>) return "ArtTimeSync";
    else if constexpr (std::is_same_v<T, ArtTrigger>) return "ArtTrigger";
    else if constexpr (std::is_same_v<T, ArtDirectory>) return "ArtDirectory";
    else if constexpr (std::is_same_v<T, ArtDirectoryReply>) return "ArtDirectoryReply";
    else if constexpr (std::is_same_v<T, ArtVideoSetup>) return "ArtVideoSetup";
    else if constexpr (std::is_same_v<T, ArtVideoPalette>) return "ArtVideoPalette";
    else if constexpr (std::is_same_v<T, ArtVideoData>) return "ArtVideoData";
    else if constexpr (std::is_same_v<T, ArtFirmwareMaster>) return "ArtFirmwareMaster";
    else if constexpr (std::is_same_v<T, ArtFirmwareReply>) return "ArtFirmwareReply";
    else if constexpr (std::is_same_v<T, ArtFileTnMaster>) return "ArtFileTnMaster";
    else if constexpr (std::is_same_v<T, ArtFileFnMaster>) return "ArtFileFnMaster";
    else if constexpr (std::is_same_v<T, ArtFileFnReply>) return "ArtFileFnReply";
    else if constexpr (std::is_same_v<T, ArtIpProg>) return "ArtIpProg";
    else if constexpr (std::is_same_v<T, ArtIpProgReply>) return "ArtIpProgReply";
    else return "UnsupportedPacket";
}

} // namespace

std::string PacketInspector::bytesToHex(std::span<const uint8_t> bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) out << ' ';
        out << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return out.str();
}

std::vector<uint8_t> PacketInspector::decodeHexBytes(std::string_view hex) {
    std::vector<uint8_t> bytes;
    int high = -1;
    for (char c : hex) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ':') {
            continue;
        }
        const int value = hexValue(c);
        if (value < 0) {
            high = -1;
            continue;
        }
        if (high < 0) {
            high = value;
        } else {
            bytes.push_back(static_cast<uint8_t>((high << 4) | value));
            high = -1;
        }
    }
    return bytes;
}

std::string PacketInspector::summarize(const Packet& packet) {
    return std::visit([](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        std::ostringstream out;
        if constexpr (std::is_same_v<T, ArtDmx>) {
            out << "ArtDmx universe "
                << static_cast<int>(value.universe.net) << '/'
                << static_cast<int>(value.universe.subnet) << '/'
                << static_cast<int>(value.universe.universe)
                << " channels " << value.data.size()
                << " sequence " << static_cast<int>(value.sequence);
        } else if constexpr (std::is_same_v<T, ArtPollReply>) {
            out << "ArtPollReply " << value.shortName
                << " bind " << static_cast<int>(value.bindIndex)
                << " ports " << value.numberOfPorts;
        } else if constexpr (std::is_same_v<T, UnsupportedPacket>) {
            out << "Unsupported opcode 0x" << std::hex << value.opcode;
        } else {
            out << packetTypeName<T>();
        }
        return out.str();
    }, packet);
}

} // namespace tcx::artnet
