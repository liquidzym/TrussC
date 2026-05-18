#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtMedia media;
    media.payload = { 1, 2, 3, 4 };
    std::vector<uint8_t> bytes;
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Error error;
    tcx::artnet::Codec::encode(tcx::artnet::Packet { media }, bytes, &error);
    tcx::artnet::Codec::decode(bytes, decodedPacket, &error);
    std::cout << "decoded ArtMedia payload bytes: " << std::get<tcx::artnet::ArtMedia>(decodedPacket).payload.size() << "\n";
    return 0;
}
