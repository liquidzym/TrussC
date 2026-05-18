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
    std::array<uint8_t, 1020> data {};
    data.fill(64);
    tcx::artnet::Endpoint node { "127.0.0.1", tcx::artnet::DefaultPort };
    if (!controller.sendMultiUniverseDmx(node, { 0, 0, 1 }, data, 510, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    controller.sendSync("2.255.255.255", &error);
    std::cout << "sent 2 universes and ArtSync\n";
    return 0;
}
