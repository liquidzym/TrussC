#include "test_common.h"

void test_opcode() {
    using namespace tcx::artnet;
    require(opcodeFromValue(0x5000) == OpCode::Dmx, "0x5000 maps to ArtDmx");
    require(opcodeFromValue(0x8300) == OpCode::Rdm, "RDM opcode is recognized");
    require(!opcodeFromValue(0x1234), "unknown opcode is not mapped");
    require(opcodeName(OpCode::IpProgReply) == "ArtIpProgReply", "opcode name is stable");
}
