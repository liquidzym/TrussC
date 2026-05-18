#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::ArtTrigger trigger;
    trigger.key = 1;
    trigger.subKey = 2;
    trigger.payload = { 7, 8, 9 };
    auto decoded = std::get<tcx::artnet::ArtTrigger>([&] {
        std::vector<uint8_t> bytes;
        tcx::artnet::Packet packet { trigger };
        tcx::artnet::Packet out;
        tcx::artnet::Error error;
        tcx::artnet::Codec::encode(packet, bytes, &error);
        tcx::artnet::Codec::decode(bytes, out, &error);
        return out;
    }());
    std::cout << "trigger key " << static_cast<int>(decoded.key)
              << " subkey " << static_cast<int>(decoded.subKey)
              << " payload " << decoded.payload.size() << "\n";
    return 0;
}
