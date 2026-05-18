#include "tcx/artnet/ArtNetCodec.h"

#include "tcx/artnet/ArtNetOpcode.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace tcx::artnet {

uint16_t readLe16(std::span<const uint8_t> data, size_t offset) noexcept {
    if (offset + 1 >= data.size()) {
        return 0;
    }
    return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
}

uint16_t readBe16(std::span<const uint8_t> data, size_t offset) noexcept {
    if (offset + 1 >= data.size()) {
        return 0;
    }
    return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
}

void writeLe16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

void writeBe16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(value & 0xff));
}

namespace {

template <typename T>
struct always_false : std::false_type {};

bool validateId(std::span<const uint8_t> bytes, Error* error) {
    if (bytes.size() < ArtNetHeaderSize) {
        setError(error, ErrorCode::TruncatedPacket, "packet is shorter than Art-Net header");
        return false;
    }
    if (!std::equal(ArtNetId.begin(), ArtNetId.end(), bytes.begin())) {
        setError(error, ErrorCode::InvalidHeader, "packet does not start with Art-Net ID");
        return false;
    }
    return true;
}

bool validateProtocol(uint16_t version, Error* error) {
    if (version < ProtocolVersion) {
        setError(error, ErrorCode::InvalidProtocolVersion, "Art-Net protocol version is lower than 14");
        return false;
    }
    return true;
}

void appendHeader(std::vector<uint8_t>& out, OpCode opcode) {
    out.insert(out.end(), ArtNetId.begin(), ArtNetId.end());
    writeLe16(out, static_cast<uint16_t>(opcode));
}

void appendProtocol(std::vector<uint8_t>& out, uint16_t version) {
    writeBe16(out, version);
}

std::string readFixedString(std::span<const uint8_t> bytes, size_t offset, size_t length) {
    if (offset >= bytes.size()) {
        return {};
    }
    const size_t end = std::min(bytes.size(), offset + length);
    size_t actualEnd = offset;
    while (actualEnd < end && bytes[actualEnd] != 0) {
        actualEnd++;
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), actualEnd - offset);
}

void writeFixedString(std::vector<uint8_t>& out, std::string_view value, size_t length) {
    const size_t count = std::min(value.size(), length == 0 ? 0 : length - 1);
    out.insert(out.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(count));
    out.resize(out.size() + (length - count), 0);
}

std::vector<uint8_t> tailPayload(std::span<const uint8_t> bytes, size_t offset) {
    if (offset >= bytes.size()) {
        return {};
    }
    return std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
}

bool validateDmxPayload(const std::vector<uint8_t>& data, Error* error) {
    if (data.size() < MinArtDmxChannels || data.size() > MaxDmxChannels) {
        setError(error, ErrorCode::InvalidLength, "ArtDmx length must be in 2..512");
        return false;
    }
    if (data.size() % 2 != 0) {
        setError(error, ErrorCode::InvalidLength, "ArtDmx length must be even");
        return false;
    }
    return true;
}

bool validateNzsPayload(const ArtNzs& nzs, Error* error) {
    if (nzs.startCode == 0) {
        setError(error, ErrorCode::InvalidPacket, "ArtNzs start code must be non-zero");
        return false;
    }
    if (nzs.startCode == RdmStartCode) {
        setError(error, ErrorCode::UnsupportedRdm, "ArtNzs RDM start code is intentionally unsupported");
        return false;
    }
    if (nzs.data.empty() || nzs.data.size() > MaxDmxChannels) {
        setError(error, ErrorCode::InvalidLength, "ArtNzs data length must be 1..512");
        return false;
    }
    return true;
}

bool decodeDmxLike(std::span<const uint8_t> bytes, ArtDmx& dmx, Error* error) {
    if (bytes.size() < 18) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDmx packet is shorter than 18 bytes");
        return false;
    }
    dmx.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(dmx.protocolVersion, error)) {
        return false;
    }
    dmx.sequence = bytes[12];
    dmx.physical = bytes[13];
    const uint16_t portAddress = readLe16(bytes, 14);
    auto universe = UniverseAddress::fromPortAddress(portAddress);
    if (!universe) {
        setError(error, ErrorCode::InvalidUniverse, "ArtDmx port-address is outside 15-bit range");
        return false;
    }
    dmx.universe = *universe;
    const uint16_t length = readBe16(bytes, 16);
    if (18 + length > bytes.size()) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDmx payload is truncated");
        return false;
    }
    dmx.data.assign(bytes.begin() + 18, bytes.begin() + 18 + length);
    return validateDmxPayload(dmx.data, error);
}

bool decodeNzs(std::span<const uint8_t> bytes, ArtNzs& nzs, Error* error) {
    if (bytes.size() < 18) {
        setError(error, ErrorCode::TruncatedPacket, "ArtNzs packet is shorter than 18 bytes");
        return false;
    }
    nzs.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(nzs.protocolVersion, error)) {
        return false;
    }
    nzs.sequence = bytes[12];
    nzs.startCode = bytes[13];
    const uint16_t portAddress = readLe16(bytes, 14);
    auto universe = UniverseAddress::fromPortAddress(portAddress);
    if (!universe) {
        setError(error, ErrorCode::InvalidUniverse, "ArtNzs port-address is outside 15-bit range");
        return false;
    }
    nzs.universe = *universe;
    const uint16_t length = readBe16(bytes, 16);
    if (18 + length > bytes.size()) {
        setError(error, ErrorCode::TruncatedPacket, "ArtNzs payload is truncated");
        return false;
    }
    nzs.data.assign(bytes.begin() + 18, bytes.begin() + 18 + length);
    return validateNzsPayload(nzs, error);
}

void encodeDmxLike(const ArtDmx& dmx, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::Dmx);
    appendProtocol(out, dmx.protocolVersion);
    out.push_back(dmx.sequence);
    out.push_back(dmx.physical);
    writeLe16(out, dmx.universe.toPortAddress());
    writeBe16(out, static_cast<uint16_t>(dmx.data.size()));
    out.insert(out.end(), dmx.data.begin(), dmx.data.end());
}

void encodeNzs(const ArtNzs& nzs, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::Nzs);
    appendProtocol(out, nzs.protocolVersion);
    out.push_back(nzs.sequence);
    out.push_back(nzs.startCode);
    writeLe16(out, nzs.universe.toPortAddress());
    writeBe16(out, static_cast<uint16_t>(nzs.data.size()));
    out.insert(out.end(), nzs.data.begin(), nzs.data.end());
}

template <typename PacketType>
bool decodeGenericPayload(std::span<const uint8_t> bytes, PacketType& out, Error* error) {
    if (bytes.size() < 12) {
        setError(error, ErrorCode::TruncatedPacket, "packet is shorter than generic Art-Net protocol header");
        return false;
    }
    out.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(out.protocolVersion, error)) {
        return false;
    }
    out.payload = tailPayload(bytes, 12);
    return true;
}

template <typename PacketType>
void encodeGenericPayload(const PacketType& packet, OpCode opcode, std::vector<uint8_t>& out) {
    appendHeader(out, opcode);
    appendProtocol(out, packet.protocolVersion);
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());
}

bool decodePoll(std::span<const uint8_t> bytes, ArtPoll& poll, Error* error) {
    if (bytes.size() < 14) {
        setError(error, ErrorCode::TruncatedPacket, "ArtPoll packet is shorter than 14 bytes");
        return false;
    }
    poll.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(poll.protocolVersion, error)) {
        return false;
    }
    const uint8_t talkTo = bytes[12];
    poll.requestDiagnostics = (talkTo & 0x04) != 0;
    poll.diagnosticsUnicast = (talkTo & 0x08) != 0;
    poll.targetedMode = (talkTo & 0x20) != 0;
    poll.diagnosticPriority = bytes[13];
    if (bytes.size() >= 22) {
        poll.targetPortAddressTop = readBe16(bytes, 14);
        poll.targetPortAddressBottom = readBe16(bytes, 16);
        poll.estaManufacturerCode = readBe16(bytes, 18);
        poll.oemCode = readBe16(bytes, 20);
    }
    return true;
}

void encodePoll(const ArtPoll& poll, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::Poll);
    appendProtocol(out, poll.protocolVersion);
    uint8_t talkTo = 0;
    if (poll.requestDiagnostics) talkTo |= 0x04;
    if (poll.diagnosticsUnicast) talkTo |= 0x08;
    if (poll.targetedMode) talkTo |= 0x20;
    out.push_back(talkTo);
    out.push_back(poll.diagnosticPriority);
    writeBe16(out, poll.targetPortAddressTop);
    writeBe16(out, poll.targetPortAddressBottom);
    writeBe16(out, poll.estaManufacturerCode);
    writeBe16(out, poll.oemCode);
}

bool decodePollReply(std::span<const uint8_t> bytes, ArtPollReply& reply, Error* error) {
    if (bytes.size() < 207) {
        setError(error, ErrorCode::TruncatedPacket, "ArtPollReply must be at least 207 bytes");
        return false;
    }
    std::copy(bytes.begin() + 10, bytes.begin() + 14, reply.ipAddress.begin());
    reply.port = readLe16(bytes, 14);
    reply.firmwareVersion = readBe16(bytes, 16);
    reply.netSwitch = bytes[18];
    reply.subSwitch = bytes[19];
    reply.oemCode = readBe16(bytes, 20);
    reply.ubeaVersion = bytes[22];
    reply.status1 = bytes[23];
    reply.estaManufacturerCode = readBe16(bytes, 24);
    reply.shortName = readFixedString(bytes, 26, 18);
    reply.longName = readFixedString(bytes, 44, 64);
    reply.nodeReport = readFixedString(bytes, 108, 64);
    reply.numberOfPorts = readBe16(bytes, 172);
    std::copy(bytes.begin() + 174, bytes.begin() + 178, reply.portTypes.begin());
    std::copy(bytes.begin() + 178, bytes.begin() + 182, reply.goodInput.begin());
    std::copy(bytes.begin() + 182, bytes.begin() + 186, reply.goodOutput.begin());
    std::copy(bytes.begin() + 186, bytes.begin() + 190, reply.swIn.begin());
    std::copy(bytes.begin() + 190, bytes.begin() + 194, reply.swOut.begin());
    reply.style = bytes[194];
    if (bytes.size() >= 207) {
        std::copy(bytes.begin() + 195, bytes.begin() + 201, reply.macAddress.begin());
        std::copy(bytes.begin() + 201, bytes.begin() + 205, reply.bindIp.begin());
        reply.bindIndex = bytes[205];
        reply.status2 = bytes[206];
    }
    if (bytes.size() >= 208) {
        reply.status3 = bytes[207];
    }
    return true;
}

void encodePollReply(const ArtPollReply& reply, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::PollReply);
    out.insert(out.end(), reply.ipAddress.begin(), reply.ipAddress.end());
    writeLe16(out, reply.port);
    writeBe16(out, reply.firmwareVersion);
    out.push_back(reply.netSwitch);
    out.push_back(reply.subSwitch);
    writeBe16(out, reply.oemCode);
    out.push_back(reply.ubeaVersion);
    out.push_back(reply.status1);
    writeBe16(out, reply.estaManufacturerCode);
    writeFixedString(out, reply.shortName, 18);
    writeFixedString(out, reply.longName, 64);
    writeFixedString(out, reply.nodeReport, 64);
    writeBe16(out, reply.numberOfPorts);
    out.insert(out.end(), reply.portTypes.begin(), reply.portTypes.end());
    out.insert(out.end(), reply.goodInput.begin(), reply.goodInput.end());
    out.insert(out.end(), reply.goodOutput.begin(), reply.goodOutput.end());
    out.insert(out.end(), reply.swIn.begin(), reply.swIn.end());
    out.insert(out.end(), reply.swOut.begin(), reply.swOut.end());
    out.push_back(reply.style);
    out.insert(out.end(), reply.macAddress.begin(), reply.macAddress.end());
    out.insert(out.end(), reply.bindIp.begin(), reply.bindIp.end());
    out.push_back(reply.bindIndex);
    out.push_back(reply.status2);
    out.push_back(reply.status3);
    if (out.size() < 239) {
        out.resize(239, 0);
    }
}

bool decodeDiagData(std::span<const uint8_t> bytes, ArtDiagData& diag, Error* error) {
    if (bytes.size() < 18) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDiagData packet is too short");
        return false;
    }
    diag.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(diag.protocolVersion, error)) return false;
    diag.priority = static_cast<DiagPriority>(bytes[13]);
    diag.logicalPort = bytes[14];
    const uint16_t length = readBe16(bytes, 16);
    if (18 + length > bytes.size()) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDiagData message is truncated");
        return false;
    }
    diag.message.assign(reinterpret_cast<const char*>(bytes.data() + 18), length);
    return true;
}

void encodeDiagData(const ArtDiagData& diag, std::vector<uint8_t>& out, Error* error) {
    appendHeader(out, OpCode::DiagData);
    appendProtocol(out, diag.protocolVersion);
    out.push_back(0);
    out.push_back(static_cast<uint8_t>(diag.priority));
    out.push_back(diag.logicalPort);
    out.push_back(0);
    const size_t len = std::min<size_t>(diag.message.size(), 512);
    if (diag.message.size() > len) {
        setError(error, ErrorCode::InvalidLength, "diagnostic message truncated to 512 bytes");
    }
    writeBe16(out, static_cast<uint16_t>(len));
    out.insert(out.end(), diag.message.begin(), diag.message.begin() + static_cast<std::ptrdiff_t>(len));
}

bool decodeCommand(std::span<const uint8_t> bytes, ArtCommand& command, Error* error) {
    if (bytes.size() < 16) {
        setError(error, ErrorCode::TruncatedPacket, "ArtCommand packet is too short");
        return false;
    }
    command.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(command.protocolVersion, error)) return false;
    command.estaManufacturerCode = readBe16(bytes, 12);
    const uint16_t length = readBe16(bytes, 14);
    if (16 + length > bytes.size()) {
        setError(error, ErrorCode::TruncatedPacket, "ArtCommand text is truncated");
        return false;
    }
    command.command.assign(reinterpret_cast<const char*>(bytes.data() + 16), length);
    return true;
}

void encodeCommand(const ArtCommand& command, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::Command);
    appendProtocol(out, command.protocolVersion);
    writeBe16(out, command.estaManufacturerCode);
    writeBe16(out, static_cast<uint16_t>(std::min<size_t>(command.command.size(), 512)));
    out.insert(out.end(), command.command.begin(), command.command.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(command.command.size(), 512)));
}

bool decodeDataRequest(std::span<const uint8_t> bytes, ArtDataRequest& request, Error* error) {
    if (bytes.size() < 18) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDataRequest packet is too short");
        return false;
    }
    request.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(request.protocolVersion, error)) return false;
    request.estaManufacturerCode = readBe16(bytes, 12);
    request.oemCode = readBe16(bytes, 14);
    request.requestCode = readBe16(bytes, 16);
    return true;
}

void encodeDataRequest(const ArtDataRequest& request, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::DataRequest);
    appendProtocol(out, request.protocolVersion);
    writeBe16(out, request.estaManufacturerCode);
    writeBe16(out, request.oemCode);
    writeBe16(out, request.requestCode);
    out.resize(out.size() + 22, 0);
}

bool decodeDataReply(std::span<const uint8_t> bytes, ArtDataReply& reply, Error* error) {
    if (bytes.size() < 20) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDataReply packet is too short");
        return false;
    }
    reply.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(reply.protocolVersion, error)) return false;
    reply.estaManufacturerCode = readBe16(bytes, 12);
    reply.oemCode = readBe16(bytes, 14);
    reply.requestCode = readBe16(bytes, 16);
    const uint16_t length = readBe16(bytes, 18);
    if (20 + length > bytes.size()) {
        setError(error, ErrorCode::TruncatedPacket, "ArtDataReply payload is truncated");
        return false;
    }
    reply.data.assign(bytes.begin() + 20, bytes.begin() + 20 + length);
    return true;
}

void encodeDataReply(const ArtDataReply& reply, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::DataReply);
    appendProtocol(out, reply.protocolVersion);
    writeBe16(out, reply.estaManufacturerCode);
    writeBe16(out, reply.oemCode);
    writeBe16(out, reply.requestCode);
    writeBe16(out, static_cast<uint16_t>(std::min<size_t>(reply.data.size(), 65535)));
    out.insert(out.end(), reply.data.begin(), reply.data.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(reply.data.size(), 65535)));
}

bool decodeAddress(std::span<const uint8_t> bytes, ArtAddress& address, Error* error) {
    if (bytes.size() < 106) {
        setError(error, ErrorCode::TruncatedPacket, "ArtAddress packet is too short");
        return false;
    }
    address.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(address.protocolVersion, error)) return false;
    address.netSwitch = bytes[12];
    address.bindIndex = bytes[13];
    address.shortName = readFixedString(bytes, 14, 18);
    address.longName = readFixedString(bytes, 32, 64);
    std::copy(bytes.begin() + 96, bytes.begin() + 100, address.swIn.begin());
    std::copy(bytes.begin() + 100, bytes.begin() + 104, address.swOut.begin());
    address.subSwitch = bytes[104];
    address.command = bytes[105];
    return true;
}

void encodeAddress(const ArtAddress& address, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::Address);
    appendProtocol(out, address.protocolVersion);
    out.push_back(address.netSwitch);
    out.push_back(address.bindIndex);
    writeFixedString(out, address.shortName, 18);
    writeFixedString(out, address.longName, 64);
    out.insert(out.end(), address.swIn.begin(), address.swIn.end());
    out.insert(out.end(), address.swOut.begin(), address.swOut.end());
    out.push_back(address.subSwitch);
    out.push_back(address.command);
}

bool decodeInput(std::span<const uint8_t> bytes, ArtInput& input, Error* error) {
    if (bytes.size() < 20) {
        setError(error, ErrorCode::TruncatedPacket, "ArtInput packet is too short");
        return false;
    }
    input.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(input.protocolVersion, error)) return false;
    input.bindIndex = bytes[13];
    input.numPorts = static_cast<uint8_t>(std::min<uint16_t>(readBe16(bytes, 14), 4));
    std::copy(bytes.begin() + 16, bytes.begin() + 20, input.input.begin());
    return true;
}

void encodeInput(const ArtInput& input, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::Input);
    appendProtocol(out, input.protocolVersion);
    out.push_back(0);
    out.push_back(input.bindIndex);
    writeBe16(out, input.numPorts);
    out.insert(out.end(), input.input.begin(), input.input.end());
}

bool decodeTimeCode(std::span<const uint8_t> bytes, ArtTimeCode& timeCode, Error* error) {
    if (bytes.size() < 19) {
        setError(error, ErrorCode::TruncatedPacket, "ArtTimeCode packet is too short");
        return false;
    }
    timeCode.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(timeCode.protocolVersion, error)) return false;
    timeCode.streamId = bytes[13];
    timeCode.frames = bytes[14];
    timeCode.seconds = bytes[15];
    timeCode.minutes = bytes[16];
    timeCode.hours = bytes[17];
    timeCode.type = static_cast<TimeCodeType>(bytes[18]);
    return true;
}

void encodeTimeCode(const ArtTimeCode& timeCode, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::TimeCode);
    appendProtocol(out, timeCode.protocolVersion);
    out.push_back(0);
    out.push_back(timeCode.streamId);
    out.push_back(timeCode.frames);
    out.push_back(timeCode.seconds);
    out.push_back(timeCode.minutes);
    out.push_back(timeCode.hours);
    out.push_back(static_cast<uint8_t>(timeCode.type));
}

bool decodeTimeSync(std::span<const uint8_t> bytes, ArtTimeSync& sync, Error* error) {
    if (bytes.size() < 19) {
        setError(error, ErrorCode::TruncatedPacket, "ArtTimeSync packet is too short");
        return false;
    }
    sync.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(sync.protocolVersion, error)) return false;
    sync.hours = bytes[12];
    sync.minutes = bytes[13];
    sync.seconds = bytes[14];
    sync.days = bytes[15];
    sync.month = bytes[16];
    sync.year = readBe16(bytes, 17);
    return true;
}

void encodeTimeSync(const ArtTimeSync& sync, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::TimeSync);
    appendProtocol(out, sync.protocolVersion);
    out.push_back(sync.hours);
    out.push_back(sync.minutes);
    out.push_back(sync.seconds);
    out.push_back(sync.days);
    out.push_back(sync.month);
    writeBe16(out, sync.year);
}

bool decodeTrigger(std::span<const uint8_t> bytes, ArtTrigger& trigger, Error* error) {
    if (bytes.size() < 18) {
        setError(error, ErrorCode::TruncatedPacket, "ArtTrigger packet is too short");
        return false;
    }
    trigger.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(trigger.protocolVersion, error)) return false;
    trigger.oemCode = readBe16(bytes, 14);
    trigger.key = bytes[16];
    trigger.subKey = bytes[17];
    const size_t length = std::min<size_t>(512, bytes.size() - 18);
    trigger.payload.assign(bytes.begin() + 18, bytes.begin() + 18 + static_cast<std::ptrdiff_t>(length));
    return true;
}

bool encodeTrigger(const ArtTrigger& trigger, std::vector<uint8_t>& out, Error* error) {
    if (trigger.payload.size() > 512) {
        setError(error, ErrorCode::InvalidLength, "ArtTrigger payload must be 512 bytes or less");
        return false;
    }
    appendHeader(out, OpCode::Trigger);
    appendProtocol(out, trigger.protocolVersion);
    out.push_back(0);
    out.push_back(0);
    writeBe16(out, trigger.oemCode);
    out.push_back(trigger.key);
    out.push_back(trigger.subKey);
    out.insert(out.end(), trigger.payload.begin(), trigger.payload.end());
    out.resize(out.size() + (512 - trigger.payload.size()), 0);
    return true;
}

bool decodeIpProg(std::span<const uint8_t> bytes, ArtIpProg& ipProg, Error* error) {
    if (bytes.size() < 34) {
        setError(error, ErrorCode::TruncatedPacket, "ArtIpProg packet is too short");
        return false;
    }
    ipProg.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(ipProg.protocolVersion, error)) return false;
    ipProg.command = bytes[14];
    std::copy(bytes.begin() + 16, bytes.begin() + 20, ipProg.ip.begin());
    std::copy(bytes.begin() + 20, bytes.begin() + 24, ipProg.subnetMask.begin());
    ipProg.portAddress = readBe16(bytes, 24);
    std::copy(bytes.begin() + 26, bytes.begin() + 30, ipProg.defaultGateway.begin());
    return true;
}

void encodeIpProg(const ArtIpProg& ipProg, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::IpProg);
    appendProtocol(out, ipProg.protocolVersion);
    out.push_back(0);
    out.push_back(0);
    out.push_back(ipProg.command);
    out.push_back(0);
    out.insert(out.end(), ipProg.ip.begin(), ipProg.ip.end());
    out.insert(out.end(), ipProg.subnetMask.begin(), ipProg.subnetMask.end());
    writeBe16(out, ipProg.portAddress);
    out.insert(out.end(), ipProg.defaultGateway.begin(), ipProg.defaultGateway.end());
    out.resize(out.size() + 4, 0);
}

bool decodeIpProgReply(std::span<const uint8_t> bytes, ArtIpProgReply& reply, Error* error) {
    if (bytes.size() < 34) {
        setError(error, ErrorCode::TruncatedPacket, "ArtIpProgReply packet is too short");
        return false;
    }
    reply.protocolVersion = readBe16(bytes, 10);
    if (!validateProtocol(reply.protocolVersion, error)) return false;
    std::copy(bytes.begin() + 16, bytes.begin() + 20, reply.ip.begin());
    std::copy(bytes.begin() + 20, bytes.begin() + 24, reply.subnetMask.begin());
    reply.portAddress = readBe16(bytes, 24);
    reply.status = bytes[26];
    std::copy(bytes.begin() + 28, bytes.begin() + 32, reply.defaultGateway.begin());
    return true;
}

void encodeIpProgReply(const ArtIpProgReply& reply, std::vector<uint8_t>& out) {
    appendHeader(out, OpCode::IpProgReply);
    appendProtocol(out, reply.protocolVersion);
    out.resize(out.size() + 4, 0);
    out.insert(out.end(), reply.ip.begin(), reply.ip.end());
    out.insert(out.end(), reply.subnetMask.begin(), reply.subnetMask.end());
    writeBe16(out, reply.portAddress);
    out.push_back(reply.status);
    out.push_back(0);
    out.insert(out.end(), reply.defaultGateway.begin(), reply.defaultGateway.end());
    out.resize(out.size() + 2, 0);
}

template <typename T>
bool assignGenericDecoded(std::span<const uint8_t> bytes, Packet& outPacket, Error* error) {
    T packet;
    if (decodeGenericPayload(bytes, packet, error)) {
        outPacket = std::move(packet);
        return true;
    }
    return false;
}

} // namespace

std::optional<OpCode> Codec::readOpcode(std::span<const uint8_t> bytes, Error* error) {
    if (!validateId(bytes, error)) {
        return std::nullopt;
    }
    const uint16_t rawOpcode = readLe16(bytes, 8);
    auto opcode = opcodeFromValue(rawOpcode);
    if (!opcode) {
        setError(error, ErrorCode::UnsupportedOpcode, "unknown Art-Net opcode");
    }
    return opcode;
}

bool Codec::decode(std::span<const uint8_t> bytes, Packet& outPacket, Error* error) {
    if (!validateId(bytes, error)) {
        return false;
    }

    const uint16_t rawOpcode = readLe16(bytes, 8);
    auto maybeOpcode = opcodeFromValue(rawOpcode);
    if (!maybeOpcode) {
        UnsupportedPacket packet;
        packet.opcode = rawOpcode;
        packet.reason = ErrorCode::UnsupportedOpcode;
        packet.raw.assign(bytes.begin(), bytes.end());
        outPacket = std::move(packet);
        setError(error, ErrorCode::UnsupportedOpcode, "unknown Art-Net opcode");
        return true;
    }

    const OpCode opcode = *maybeOpcode;
    if (opcode == OpCode::Rdm || opcode == OpCode::RdmSub) {
        UnsupportedPacket packet;
        packet.opcode = rawOpcode;
        packet.reason = ErrorCode::UnsupportedRdm;
        packet.raw.assign(bytes.begin(), bytes.end());
        outPacket = std::move(packet);
        setError(error, ErrorCode::UnsupportedRdm, "ArtRdm and ArtRdmSub are intentionally unsupported");
        return true;
    }

    switch (opcode) {
        case OpCode::Poll: {
            ArtPoll packet;
            if (!decodePoll(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::PollReply: {
            ArtPollReply packet;
            if (!decodePollReply(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::DiagData: {
            ArtDiagData packet;
            if (!decodeDiagData(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::Command: {
            ArtCommand packet;
            if (!decodeCommand(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::DataRequest: {
            ArtDataRequest packet;
            if (!decodeDataRequest(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::DataReply: {
            ArtDataReply packet;
            if (!decodeDataReply(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::Dmx: {
            ArtDmx packet;
            if (!decodeDmxLike(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::Nzs: {
            ArtNzs packet;
            if (!decodeNzs(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::Sync: {
            if (bytes.size() < 14) {
                setError(error, ErrorCode::TruncatedPacket, "ArtSync packet is too short");
                return false;
            }
            ArtSync packet;
            packet.protocolVersion = readBe16(bytes, 10);
            if (!validateProtocol(packet.protocolVersion, error)) return false;
            outPacket = packet;
            return true;
        }
        case OpCode::Address: {
            ArtAddress packet;
            if (!decodeAddress(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::Input: {
            ArtInput packet;
            if (!decodeInput(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::TimeCode: {
            ArtTimeCode packet;
            if (!decodeTimeCode(bytes, packet, error)) return false;
            outPacket = packet;
            return true;
        }
        case OpCode::TimeSync: {
            ArtTimeSync packet;
            if (!decodeTimeSync(bytes, packet, error)) return false;
            outPacket = packet;
            return true;
        }
        case OpCode::Trigger: {
            ArtTrigger packet;
            if (!decodeTrigger(bytes, packet, error)) return false;
            outPacket = std::move(packet);
            return true;
        }
        case OpCode::IpProg: {
            ArtIpProg packet;
            if (!decodeIpProg(bytes, packet, error)) return false;
            outPacket = packet;
            return true;
        }
        case OpCode::IpProgReply: {
            ArtIpProgReply packet;
            if (!decodeIpProgReply(bytes, packet, error)) return false;
            outPacket = packet;
            return true;
        }
        case OpCode::TodRequest: return assignGenericDecoded<ArtTodRequest>(bytes, outPacket, error);
        case OpCode::TodData: return assignGenericDecoded<ArtTodData>(bytes, outPacket, error);
        case OpCode::TodControl: return assignGenericDecoded<ArtTodControl>(bytes, outPacket, error);
        case OpCode::Media: return assignGenericDecoded<ArtMedia>(bytes, outPacket, error);
        case OpCode::MediaPatch: return assignGenericDecoded<ArtMediaPatch>(bytes, outPacket, error);
        case OpCode::MediaControl: return assignGenericDecoded<ArtMediaControl>(bytes, outPacket, error);
        case OpCode::MediaControlReply: return assignGenericDecoded<ArtMediaControlReply>(bytes, outPacket, error);
        case OpCode::Directory: return assignGenericDecoded<ArtDirectory>(bytes, outPacket, error);
        case OpCode::DirectoryReply: return assignGenericDecoded<ArtDirectoryReply>(bytes, outPacket, error);
        case OpCode::VideoSetup: return assignGenericDecoded<ArtVideoSetup>(bytes, outPacket, error);
        case OpCode::VideoPalette: return assignGenericDecoded<ArtVideoPalette>(bytes, outPacket, error);
        case OpCode::VideoData: return assignGenericDecoded<ArtVideoData>(bytes, outPacket, error);
        case OpCode::FirmwareMaster: return assignGenericDecoded<ArtFirmwareMaster>(bytes, outPacket, error);
        case OpCode::FirmwareReply: return assignGenericDecoded<ArtFirmwareReply>(bytes, outPacket, error);
        case OpCode::FileTnMaster: return assignGenericDecoded<ArtFileTnMaster>(bytes, outPacket, error);
        case OpCode::FileFnMaster: return assignGenericDecoded<ArtFileFnMaster>(bytes, outPacket, error);
        case OpCode::FileFnReply: return assignGenericDecoded<ArtFileFnReply>(bytes, outPacket, error);
        case OpCode::Rdm:
        case OpCode::RdmSub:
            break;
    }

    UnsupportedPacket packet;
    packet.opcode = rawOpcode;
    packet.reason = ErrorCode::UnsupportedOpcode;
    packet.raw.assign(bytes.begin(), bytes.end());
    outPacket = std::move(packet);
    setError(error, ErrorCode::UnsupportedOpcode, "unsupported Art-Net opcode");
    return true;
}

bool Codec::encode(const Packet& packet, std::vector<uint8_t>& outBytes, Error* error) {
    outBytes.clear();
    return std::visit([&](const auto& value) -> bool {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ArtPoll>) {
            encodePoll(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtPollReply>) {
            encodePollReply(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtDiagData>) {
            encodeDiagData(value, outBytes, error);
            return true;
        } else if constexpr (std::is_same_v<T, ArtCommand>) {
            encodeCommand(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtDataRequest>) {
            encodeDataRequest(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtDataReply>) {
            encodeDataReply(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtDmx>) {
            if (!value.universe.isValid()) {
                setError(error, ErrorCode::InvalidUniverse, "ArtDmx universe is invalid");
                return false;
            }
            if (!validateDmxPayload(value.data, error)) {
                return false;
            }
            encodeDmxLike(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtNzs>) {
            if (!value.universe.isValid()) {
                setError(error, ErrorCode::InvalidUniverse, "ArtNzs universe is invalid");
                return false;
            }
            if (!validateNzsPayload(value, error)) {
                return false;
            }
            encodeNzs(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtSync>) {
            appendHeader(outBytes, OpCode::Sync);
            appendProtocol(outBytes, value.protocolVersion);
            outBytes.push_back(0);
            outBytes.push_back(0);
            return true;
        } else if constexpr (std::is_same_v<T, ArtAddress>) {
            encodeAddress(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtInput>) {
            encodeInput(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtTimeCode>) {
            encodeTimeCode(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtTimeSync>) {
            encodeTimeSync(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtTrigger>) {
            return encodeTrigger(value, outBytes, error);
        } else if constexpr (std::is_same_v<T, ArtIpProg>) {
            encodeIpProg(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtIpProgReply>) {
            encodeIpProgReply(value, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtTodRequest>) {
            encodeGenericPayload(value, OpCode::TodRequest, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtTodData>) {
            encodeGenericPayload(value, OpCode::TodData, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtTodControl>) {
            encodeGenericPayload(value, OpCode::TodControl, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtMedia>) {
            encodeGenericPayload(value, OpCode::Media, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtMediaPatch>) {
            encodeGenericPayload(value, OpCode::MediaPatch, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtMediaControl>) {
            encodeGenericPayload(value, OpCode::MediaControl, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtMediaControlReply>) {
            encodeGenericPayload(value, OpCode::MediaControlReply, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtDirectory>) {
            encodeGenericPayload(value, OpCode::Directory, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtDirectoryReply>) {
            encodeGenericPayload(value, OpCode::DirectoryReply, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtVideoSetup>) {
            encodeGenericPayload(value, OpCode::VideoSetup, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtVideoPalette>) {
            encodeGenericPayload(value, OpCode::VideoPalette, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtVideoData>) {
            encodeGenericPayload(value, OpCode::VideoData, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtFirmwareMaster>) {
            encodeGenericPayload(value, OpCode::FirmwareMaster, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtFirmwareReply>) {
            encodeGenericPayload(value, OpCode::FirmwareReply, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtFileTnMaster>) {
            encodeGenericPayload(value, OpCode::FileTnMaster, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtFileFnMaster>) {
            encodeGenericPayload(value, OpCode::FileFnMaster, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, ArtFileFnReply>) {
            encodeGenericPayload(value, OpCode::FileFnReply, outBytes);
            return true;
        } else if constexpr (std::is_same_v<T, UnsupportedPacket>) {
            setError(error, value.reason, "cannot encode UnsupportedPacket");
            return false;
        } else {
            static_assert(always_false<T>::value, "unhandled packet type");
        }
    }, packet);
}

} // namespace tcx::artnet
