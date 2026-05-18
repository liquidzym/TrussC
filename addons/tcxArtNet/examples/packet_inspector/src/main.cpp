#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtDmx dmx;
    dmx.sequence = 1;
    dmx.universe = { 1, 2, 3 };
    dmx.data = { 255, 0, 0, 0 };

    tcx::artnet::Packet packet { dmx };
    std::vector<uint8_t> bytes;
    tcx::artnet::Error error;
    if (!tcx::artnet::Codec::encode(packet, bytes, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }

    std::cout << tcx::artnet::PacketInspector::summarize(packet) << "\n";
    std::cout << tcx::artnet::PacketInspector::bytesToHex(bytes) << "\n";
    return 0;
}
