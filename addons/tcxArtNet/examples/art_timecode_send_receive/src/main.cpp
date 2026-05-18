#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtTimeCode tc;
    tc.hours = 1;
    tc.minutes = 2;
    tc.seconds = 3;
    tc.frames = 4;
    tc.streamId = 1;
    tcx::artnet::Packet packet { tc };
    std::vector<uint8_t> bytes;
    tcx::artnet::Error error;
    if (!tcx::artnet::Codec::encode(packet, bytes, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Codec::decode(bytes, decodedPacket, &error);
    const auto& decoded = std::get<tcx::artnet::ArtTimeCode>(decodedPacket);
    std::cout << "timecode 01:02:03:04 stream " << static_cast<int>(decoded.streamId) << "\n";
    return 0;
}
