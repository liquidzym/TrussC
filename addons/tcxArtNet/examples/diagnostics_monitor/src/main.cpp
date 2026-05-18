#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtDiagData diag;
    diag.priority = tcx::artnet::DiagPriority::High;
    diag.message = "lamp online";
    std::vector<uint8_t> bytes;
    tcx::artnet::Packet packet { diag };
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Error error;
    tcx::artnet::Codec::encode(packet, bytes, &error);
    tcx::artnet::Codec::decode(bytes, decodedPacket, &error);
    const auto& decoded = std::get<tcx::artnet::ArtDiagData>(decodedPacket);
    std::cout << "diag priority " << static_cast<int>(decoded.priority) << ": " << decoded.message << "\n";
    return 0;
}
