#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtVideoData video;
    video.payload = { 5, 6, 7, 8 };
    std::vector<uint8_t> bytes;
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Error error;
    tcx::artnet::Codec::encode(tcx::artnet::Packet { video }, bytes, &error);
    tcx::artnet::Codec::decode(bytes, decodedPacket, &error);
    std::cout << "decoded ArtVideoData payload bytes: " << std::get<tcx::artnet::ArtVideoData>(decodedPacket).payload.size() << "\n";
    return 0;
}
