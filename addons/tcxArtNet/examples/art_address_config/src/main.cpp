#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::Sender sender;
    tcx::artnet::Error error;
    if (!sender.setup(false, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    tcx::artnet::ArtAddress address;
    address.shortName = "tcx";
    address.longName = "tcxArtNet configured node";
    address.swOut = { 1, 2, 3, 4 };
    if (!sender.sendPacket({ "127.0.0.1", tcx::artnet::DefaultPort }, tcx::artnet::Packet { address }, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "sent ArtAddress config packet\n";
    return 0;
}
