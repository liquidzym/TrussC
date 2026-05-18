#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtInput input;
    input.numPorts = 4;
    input.input = { 1, 0, 1, 0 };
    std::vector<uint8_t> bytes;
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Error error;
    if (!tcx::artnet::Codec::encode(tcx::artnet::Packet { input }, bytes, &error) ||
        !tcx::artnet::Codec::decode(bytes, decodedPacket, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    const auto& decoded = std::get<tcx::artnet::ArtInput>(decodedPacket);
    std::cout << "decoded ArtInput ports: " << static_cast<int>(decoded.numPorts) << "\n";
    return 0;
}
