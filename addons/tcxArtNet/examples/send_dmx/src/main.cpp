#include <tcxArtNet.h>

#include <array>
#include <iostream>

int main() {
    tcx::artnet::Controller controller;
    tcx::artnet::ControllerSettings settings;
    settings.localPort = 0;
    tcx::artnet::Error error;
    if (!controller.setup(settings, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }

    std::array<uint8_t, 6> dmx { 255, 0, 0, 0, 255, 0 };
    tcx::artnet::Endpoint endpoint { "127.0.0.1", tcx::artnet::DefaultPort };
    tcx::artnet::UniverseAddress universe { 0, 0, 1 };
    if (!controller.sendDmx(endpoint, universe, dmx, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "sent ArtDmx universe 0:0:1 to 127.0.0.1:6454\n";
    return 0;
}
