#include <tcxArtNet.h>

#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string endpointText(const tcx::artnet::Endpoint& endpoint) {
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

} // namespace

int main(int argc, char** argv) {
    const std::string targetIp = argc > 1 ? argv[1] : "127.0.0.1";
    const std::string bindIp = argc > 2 ? argv[2] : "0.0.0.0";
    const bool broadcast = argc > 3 && std::string(argv[3]) == "broadcast";

    tcx::artnet::ControllerSettings settings;
    settings.localBindIp = bindIp;
    settings.localPort = 0;
    settings.enableBroadcast = broadcast;
    settings.autoPoll = false;

    tcx::artnet::Controller controller;
    tcx::artnet::Error error;
    if (!controller.setup(settings, &error)) {
        std::cerr << "setup failed: " << error.message << "\n";
        return 1;
    }

    std::vector<uint8_t> dmx(510, 0);
    for (size_t i = 0; i < dmx.size(); i += 3) {
        dmx[i] = 255;
        if (i + 1 < dmx.size()) dmx[i + 1] = 255;
        if (i + 2 < dmx.size()) dmx[i + 2] = 255;
    }

    const tcx::artnet::Endpoint target { targetIp, tcx::artnet::DefaultPort };
    const tcx::artnet::UniverseAddress universe { 0, 0, 0 };
    for (int i = 0; i < 3; ++i) {
        if (!controller.sendDmx(target, universe, dmx, &error)) {
            const auto diagnostics = controller.networkDiagnostics();
            std::cerr << "send failed: " << error.message
                      << " target=" << endpointText(diagnostics.lastTarget)
                      << " bind=" << endpointText(diagnostics.socket.actualLocalEndpoint)
                      << " recovery=" << tcx::artnet::recoveryStateName(diagnostics.recoveryState) << "\n";
            controller.recover(&error);
            return 2;
        }
        const auto diagnostics = controller.networkDiagnostics();
        std::cout << "sent frame " << i + 1
                  << " target=" << endpointText(diagnostics.lastTarget)
                  << " bind=" << endpointText(diagnostics.socket.actualLocalEndpoint)
                  << " packetsSent=" << controller.statistics().packetsSent << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    return 0;
}
