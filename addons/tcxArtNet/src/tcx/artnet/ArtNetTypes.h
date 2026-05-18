#pragma once

#include "ArtNetConstants.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tcx::artnet {

enum class ErrorCode {
    None,
    InvalidPacket,
    InvalidHeader,
    InvalidOpcode,
    UnsupportedOpcode,
    UnsupportedRdm,
    UnsupportedSacn,
    InvalidProtocolVersion,
    InvalidLength,
    TruncatedPacket,
    SocketError,
    BindFailed,
    SendFailed,
    ReceiveFailed,
    InvalidAddress,
    InvalidUniverse,
    InvalidConfiguration,
    UnsupportedCommand,
    NotOpen
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
};

void setError(Error* error, ErrorCode code, std::string_view message);

struct AddressingOptions {
    bool allowDeprecatedPortAddressZero = true;
};

struct UniverseAddress {
    uint8_t net = 0;
    uint8_t subnet = 0;
    uint8_t universe = 0;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] uint16_t toPortAddress() const noexcept;

    static std::optional<UniverseAddress> fromPortAddress(uint16_t portAddress) noexcept;
    static std::optional<UniverseAddress> next(const UniverseAddress& address) noexcept;
};

struct Endpoint {
    std::string ip;
    uint16_t port = DefaultPort;
};

struct PacketHeader {
    uint16_t opcode = 0;
    uint16_t protocolVersion = ProtocolVersion;
};

enum class DiagPriority : uint8_t {
    Low = 0x10,
    Medium = 0x40,
    High = 0x80,
    Critical = 0xe0,
    Volatile = 0xf0
};

enum class TimeCodeType : uint8_t {
    Film = 0,
    EBU = 1,
    DF = 2,
    SMPTE = 3
};

enum class PixelFormat {
    RGB,
    RGBW,
    GRB,
    GRBW,
    BGR,
    BGRA,
    RGBA
};

struct ArtPoll {
    uint16_t protocolVersion = ProtocolVersion;
    bool targetedMode = false;
    bool requestDiagnostics = false;
    bool diagnosticsUnicast = false;
    uint8_t diagnosticPriority = 0;
    uint16_t targetPortAddressTop = 32767;
    uint16_t targetPortAddressBottom = 1;
    uint16_t estaManufacturerCode = 0;
    uint16_t oemCode = 0;
};

struct ArtPollReply {
    std::array<uint8_t, 4> ipAddress {};
    uint16_t port = DefaultPort;
    uint16_t firmwareVersion = 0;
    uint8_t netSwitch = 0;
    uint8_t subSwitch = 0;
    uint16_t oemCode = 0;
    uint8_t ubeaVersion = 0;
    uint8_t status1 = 0;
    uint16_t estaManufacturerCode = 0;
    std::string shortName;
    std::string longName;
    std::string nodeReport;
    uint16_t numberOfPorts = 0;
    std::array<uint8_t, 4> portTypes {};
    std::array<uint8_t, 4> goodInput {};
    std::array<uint8_t, 4> goodOutput {};
    std::array<uint8_t, 4> swIn {};
    std::array<uint8_t, 4> swOut {};
    uint8_t style = 0;
    std::array<uint8_t, 6> macAddress {};
    std::array<uint8_t, 4> bindIp {};
    uint8_t bindIndex = 1;
    uint8_t status2 = 0;
    uint8_t status3 = 0;
};

struct ArtDiagData {
    uint16_t protocolVersion = ProtocolVersion;
    DiagPriority priority = DiagPriority::Low;
    std::string message;
};

struct ArtCommand {
    uint16_t protocolVersion = ProtocolVersion;
    uint16_t estaManufacturerCode = 0;
    std::string command;
};

struct ArtDataRequest {
    uint16_t protocolVersion = ProtocolVersion;
    uint16_t estaManufacturerCode = 0;
    uint16_t oemCode = 0;
    uint8_t requestCode = 0;
};

struct ArtDataReply {
    uint16_t protocolVersion = ProtocolVersion;
    uint16_t estaManufacturerCode = 0;
    uint16_t oemCode = 0;
    uint8_t requestCode = 0;
    std::vector<uint8_t> data;
};

struct ArtDmx {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t sequence = 0;
    uint8_t physical = 0;
    UniverseAddress universe;
    std::vector<uint8_t> data;
};

struct ArtNzs {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t sequence = 0;
    uint8_t startCode = 1;
    UniverseAddress universe;
    std::vector<uint8_t> data;
};

struct ArtSync {
    uint16_t protocolVersion = ProtocolVersion;
};

struct ArtAddress {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t netSwitch = 0;
    uint8_t bindIndex = 1;
    std::string shortName;
    std::string longName;
    std::array<uint8_t, 4> swIn {};
    std::array<uint8_t, 4> swOut {};
    uint8_t subSwitch = 0;
    uint8_t command = 0;
};

using ArtAddressCommand = ArtAddress;

struct ArtInput {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t bindIndex = 1;
    uint8_t numPorts = 0;
    std::array<uint8_t, 4> input {};
};

struct ArtTodRequest {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtTodData {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtTodControl {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtMedia {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtMediaPatch {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtMediaControl {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtMediaControlReply {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtTimeCode {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t frames = 0;
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
    TimeCodeType type = TimeCodeType::SMPTE;
    uint8_t streamId = 0;
};

struct ArtTimeSync {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t hours = 0;
    uint8_t minutes = 0;
    uint8_t seconds = 0;
    uint8_t days = 0;
    uint8_t month = 0;
    uint16_t year = 0;
};

struct ArtTrigger {
    uint16_t protocolVersion = ProtocolVersion;
    uint16_t oemCode = 0;
    uint8_t key = 0;
    uint8_t subKey = 0;
    std::vector<uint8_t> payload;
};

struct ArtDirectory {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtDirectoryReply {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtVideoSetup {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtVideoPalette {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtVideoData {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtFirmwareMaster {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtFirmwareReply {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtFileTnMaster {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtFileFnMaster {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtFileFnReply {
    uint16_t protocolVersion = ProtocolVersion;
    std::vector<uint8_t> payload;
};

struct ArtIpProg {
    uint16_t protocolVersion = ProtocolVersion;
    uint8_t command = 0;
    std::array<uint8_t, 4> ip {};
    std::array<uint8_t, 4> subnetMask {};
    std::array<uint8_t, 4> defaultGateway {};
};

struct ArtIpProgReply {
    uint16_t protocolVersion = ProtocolVersion;
    std::array<uint8_t, 4> ip {};
    std::array<uint8_t, 4> subnetMask {};
    std::array<uint8_t, 4> defaultGateway {};
};

struct UnsupportedPacket {
    uint16_t opcode = 0;
    ErrorCode reason = ErrorCode::UnsupportedOpcode;
    std::vector<uint8_t> raw;
};

struct NodePort {
    UniverseAddress address;
    bool input = false;
    bool output = true;
    bool enabled = true;
};

struct NodeInfo {
    Endpoint endpoint;
    ArtPollReply reply;
    std::chrono::steady_clock::time_point lastSeen {};
};

struct Statistics {
    uint64_t packetsReceived = 0;
    uint64_t packetsSent = 0;
    uint64_t invalidPackets = 0;
    uint64_t unsupportedPackets = 0;
    uint64_t droppedPackets = 0;
    uint64_t dmxFramesReceived = 0;
    uint64_t dmxFramesSent = 0;
    uint64_t pollRepliesReceived = 0;
    double packetsPerSecond = 0.0;
};

struct PixelToDmxOptions {
    PixelFormat format = PixelFormat::RGB;
    size_t channelsPerUniverse = 510;
    bool useArtSync = true;
    bool strict = true;
};

class PixelMapper {
public:
    static bool splitPixelsToUniverses(
        std::span<const uint8_t> pixels,
        const UniverseAddress& firstUniverse,
        const PixelToDmxOptions& options,
        std::vector<ArtDmx>& outFrames,
        Error* error = nullptr
    );
};

} // namespace tcx::artnet
