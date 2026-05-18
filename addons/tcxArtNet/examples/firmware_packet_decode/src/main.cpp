#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtFirmwareReply firmware;
    firmware.payload = { 0xaa, 0x55 };
    std::vector<uint8_t> bytes;
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Error error;
    tcx::artnet::Codec::encode(tcx::artnet::Packet { firmware }, bytes, &error);
    tcx::artnet::Codec::decode(bytes, decodedPacket, &error);
    std::cout << "decoded ArtFirmwareReply payload bytes: " << std::get<tcx::artnet::ArtFirmwareReply>(decodedPacket).payload.size() << "\n";
    return 0;
}
