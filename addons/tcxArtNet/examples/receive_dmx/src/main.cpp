#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::Node node;
    tcx::artnet::NodeSettings settings;
    settings.respondToPoll = false;
    tcx::artnet::Error error;
    if (!node.setup(settings, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    node.setDmxOutputCallback([](const tcx::artnet::ArtDmx& dmx) {
        std::cout << "ArtDmx universe " << dmx.universe.toPortAddress() << " channels " << dmx.data.size() << "\n";
    });
    std::cout << "waiting for ArtDmx on UDP 6454\n";
    node.update();
    return 0;
}
