#include "test_common.h"

#include <algorithm>

namespace {

using namespace tcx::artnet;

std::vector<uint8_t> encodePacket(const Packet& packet) {
    std::vector<uint8_t> bytes;
    Error error;
    require(Codec::encode(packet, bytes, &error), error.message.c_str());
    return bytes;
}

void requireHeader(const std::vector<uint8_t>& bytes, OpCode opcode, const char* message) {
    require(bytes.size() >= ArtNetHeaderSize, message);
    require(std::equal(ArtNetId.begin(), ArtNetId.end(), bytes.begin()), "fixture starts with Art-Net ID");
    require(readLe16(bytes, 8) == static_cast<uint16_t>(opcode), message);
    require(readBe16(bytes, 10) == ProtocolVersion, "fixture protocol version is Art-Net 4");
}

} // namespace

void test_protocol_fixtures() {
    {
        auto bytes = encodePacket(Packet { ArtSync {} });
        requireHeader(bytes, OpCode::Sync, "ArtSync opcode is correct");
        require(bytes.size() == 14, "ArtSync includes the two Aux bytes required by the wire format");
        require(bytes[12] == 0 && bytes[13] == 0, "ArtSync Aux bytes are zero");
    }

    {
        ArtInput input;
        input.bindIndex = 3;
        input.numPorts = 4;
        input.input = { 0x01, 0x00, 0x01, 0x00 };
        auto bytes = encodePacket(Packet { input });
        requireHeader(bytes, OpCode::Input, "ArtInput opcode is correct");
        require(bytes.size() == 20, "ArtInput is the 20-byte Art-Net 4 layout");
        require(bytes[12] == 0, "ArtInput has Filler1 at byte 12");
        require(bytes[13] == 3, "ArtInput BindIndex is at byte 13");
        require(bytes[14] == 0 && bytes[15] == 4, "ArtInput NumPorts is a big-endian 16-bit field");
        require(bytes[16] == 0x01 && bytes[18] == 0x01, "ArtInput status bytes start at byte 16");
    }

    {
        ArtDiagData diag;
        diag.priority = DiagPriority::High;
        diag.logicalPort = 7;
        diag.message = "OK";
        auto bytes = encodePacket(Packet { diag });
        requireHeader(bytes, OpCode::DiagData, "ArtDiagData opcode is correct");
        require(bytes.size() == 20, "ArtDiagData data starts after the 18-byte header");
        require(bytes[12] == 0, "ArtDiagData has Filler1 at byte 12");
        require(bytes[13] == static_cast<uint8_t>(DiagPriority::High), "ArtDiagData priority is at byte 13");
        require(bytes[14] == 7, "ArtDiagData logical port is at byte 14");
        require(bytes[15] == 0, "ArtDiagData has Filler3 at byte 15");
        require(bytes[16] == 0 && bytes[17] == 2, "ArtDiagData length is at bytes 16..17");
    }

    {
        ArtTimeCode timeCode;
        timeCode.streamId = 9;
        timeCode.frames = 10;
        timeCode.seconds = 20;
        timeCode.minutes = 30;
        timeCode.hours = 1;
        timeCode.type = TimeCodeType::EBU;
        auto bytes = encodePacket(Packet { timeCode });
        requireHeader(bytes, OpCode::TimeCode, "ArtTimeCode opcode is correct");
        require(bytes.size() == 19, "ArtTimeCode uses the Art-Net 4 StreamId layout");
        require(bytes[12] == 0, "ArtTimeCode has Filler1 at byte 12");
        require(bytes[13] == 9, "ArtTimeCode StreamId is at byte 13");
        require(bytes[14] == 10 && bytes[15] == 20 && bytes[16] == 30 && bytes[17] == 1, "ArtTimeCode time fields start at byte 14");
        require(bytes[18] == static_cast<uint8_t>(TimeCodeType::EBU), "ArtTimeCode type is at byte 18");
    }

    {
        ArtTrigger trigger;
        trigger.oemCode = 0xffff;
        trigger.key = 1;
        trigger.subKey = 2;
        trigger.payload = { 'G', 'O' };
        auto bytes = encodePacket(Packet { trigger });
        requireHeader(bytes, OpCode::Trigger, "ArtTrigger opcode is correct");
        require(bytes.size() == 530, "ArtTrigger always carries the fixed 512-byte payload");
        require(bytes[12] == 0 && bytes[13] == 0, "ArtTrigger has two filler bytes");
        require(bytes[14] == 0xff && bytes[15] == 0xff, "ArtTrigger OEM is at bytes 14..15");
        require(bytes[16] == 1 && bytes[17] == 2, "ArtTrigger key fields are at bytes 16..17");
        require(bytes[18] == 'G' && bytes[19] == 'O' && bytes[529] == 0, "ArtTrigger payload starts at byte 18 and is padded");
    }

    {
        ArtDataRequest request;
        request.estaManufacturerCode = 0x1234;
        request.oemCode = 0x4567;
        request.requestCode = 0x0002;
        auto bytes = encodePacket(Packet { request });
        requireHeader(bytes, OpCode::DataRequest, "ArtDataRequest opcode is correct");
        require(bytes.size() == 40, "ArtDataRequest includes the 22-byte spare tail");
        require(bytes[16] == 0x00 && bytes[17] == 0x02, "ArtDataRequest request code is 16-bit big-endian");
        require(bytes[18] == 0 && bytes[39] == 0, "ArtDataRequest spare bytes are zero");
    }

    {
        ArtIpProg ipProg;
        ipProg.command = 0x97;
        ipProg.ip = { 2, 1, 2, 3 };
        ipProg.subnetMask = { 255, 0, 0, 0 };
        ipProg.portAddress = 0x1234;
        ipProg.defaultGateway = { 2, 1, 2, 1 };
        auto bytes = encodePacket(Packet { ipProg });
        requireHeader(bytes, OpCode::IpProg, "ArtIpProg opcode is correct");
        require(bytes.size() == 34, "ArtIpProg uses the 34-byte Art-Net 4 layout");
        require(bytes[12] == 0 && bytes[13] == 0, "ArtIpProg has two filler bytes before Command");
        require(bytes[14] == 0x97 && bytes[15] == 0, "ArtIpProg Command is at byte 14 and Filler4 at byte 15");
        require(bytes[16] == 2 && bytes[19] == 3, "ArtIpProg IP starts at byte 16");
        require(bytes[24] == 0x12 && bytes[25] == 0x34, "ArtIpProg ProgPort is at bytes 24..25");
        require(bytes[26] == 2 && bytes[29] == 1, "ArtIpProg default gateway starts at byte 26");
    }

    {
        ArtIpProgReply reply;
        reply.ip = { 10, 0, 0, 5 };
        reply.subnetMask = { 255, 0, 0, 0 };
        reply.portAddress = 0x5000;
        reply.status = 0x40;
        reply.defaultGateway = { 10, 0, 0, 1 };
        auto bytes = encodePacket(Packet { reply });
        requireHeader(bytes, OpCode::IpProgReply, "ArtIpProgReply opcode is correct");
        require(bytes.size() == 34, "ArtIpProgReply uses the 34-byte Art-Net 4 layout");
        require(bytes[12] == 0 && bytes[15] == 0, "ArtIpProgReply has four filler bytes");
        require(bytes[16] == 10 && bytes[19] == 5, "ArtIpProgReply IP starts at byte 16");
        require(bytes[24] == 0x50 && bytes[25] == 0x00, "ArtIpProgReply ProgPort is at bytes 24..25");
        require(bytes[26] == 0x40, "ArtIpProgReply status is at byte 26");
        require(bytes[28] == 10 && bytes[31] == 1, "ArtIpProgReply default gateway starts at byte 28");
    }
}
