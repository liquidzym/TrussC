#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::Controller controller;
    tcx::artnet::ControllerSettings settings;
    settings.directedBroadcastIp = "2.255.255.255";
    tcx::artnet::Error error;
    if (!controller.setup(settings, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    if (!controller.pollNodes(&error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    controller.update();
    std::cout << "poll sent\n";
    std::cout << "nodes discovered: " << controller.getDiscoveredNodes().size() << "\n";
    return 0;
}
