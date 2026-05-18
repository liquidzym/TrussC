#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::Node node;
    tcx::artnet::NodeSettings settings;
    settings.shortName = "tcxNode";
    settings.ipAddress = "2.0.0.20";
    settings.bindIpAddress = "2.0.0.20";
    settings.enableIpProg = true;
    settings.outputPorts.push_back({ tcx::artnet::UniverseAddress { 0, 0, 1 }, false, true, true });
    tcx::artnet::Error error;
    if (!node.setup(settings, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "tcxArtNet virtual node listening on UDP 6454 at " << settings.ipAddress << "\n";
    std::cout << "ArtAddress renaming and virtual ArtIpProg replies are enabled\n";
    node.update();
    return 0;
}
