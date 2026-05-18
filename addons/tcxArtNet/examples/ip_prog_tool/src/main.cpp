#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtIpProg prog;
    prog.command = 0x07;
    prog.ip = { 10, 0, 0, 50 };
    prog.subnetMask = { 255, 255, 255, 0 };
    prog.portAddress = 0x0001;
    prog.defaultGateway = { 10, 0, 0, 1 };
    std::vector<uint8_t> bytes;
    tcx::artnet::Error error;
    if (!tcx::artnet::Codec::encode(tcx::artnet::Packet { prog }, bytes, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "encoded ArtIpProg for 10.0.0.50: " << tcx::artnet::PacketInspector::bytesToHex(bytes) << "\n";
    return 0;
}
