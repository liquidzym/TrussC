#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::Node node;
    tcx::artnet::NodeSettings settings;
    settings.shortName = "tcxNode";
    settings.outputPorts.push_back({ tcx::artnet::UniverseAddress { 0, 0, 1 }, false, true, true });
    tcx::artnet::Error error;
    if (!node.setup(settings, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "tcxArtNet virtual node listening on UDP 6454\n";
    node.update();
    return 0;
}
