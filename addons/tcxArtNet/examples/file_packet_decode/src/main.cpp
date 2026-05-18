#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtFileFnReply file;
    file.payload = { 1, 1, 2 };
    std::vector<uint8_t> bytes;
    tcx::artnet::Packet decodedPacket;
    tcx::artnet::Error error;
    tcx::artnet::Codec::encode(tcx::artnet::Packet { file }, bytes, &error);
    tcx::artnet::Codec::decode(bytes, decodedPacket, &error);
    std::cout << "decoded ArtFileFnReply payload bytes: " << std::get<tcx::artnet::ArtFileFnReply>(decodedPacket).payload.size() << "\n";
    return 0;
}
