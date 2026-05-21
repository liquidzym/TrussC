#include <tcxArtNet.h>

#include <array>
#include <iostream>
#include <string>

namespace {

std::array<uint8_t, 28> makeProbePayload() {
    std::array<uint8_t, 28> payload {};
    payload[0] = 'A';
    payload[1] = 'r';
    payload[2] = 't';
    payload[3] = '-';
    payload[4] = 'N';
    payload[5] = 'e';
    payload[6] = 't';
    return payload;
}

std::string endpointText(const tcx::artnet::Endpoint& endpoint) {
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

} // namespace

int main(int argc, char** argv) {
    const std::string bindIp = argc > 1 ? argv[1] : "0.0.0.0";
    const std::string targetIp = argc > 2 ? argv[2] : "127.0.0.1";
    const bool broadcast = argc > 3 && std::string(argv[3]) == "broadcast";

    tcx::artnet::UdpOutputProbeOptions options;
    options.localBindIp = bindIp;
    options.enableBroadcast = broadcast;
    options.target = { targetIp, tcx::artnet::DefaultPort };

    const auto payload = makeProbePayload();
    const auto result = tcx::artnet::probeUdpOutput(options, payload);
    const auto bound = result.socket.actualLocalEndpoint;

    if (!result.ok) {
        std::cerr << "probe failed: " << result.error.message
                  << " target=" << endpointText(result.target)
                  << " requestedBind=" << result.socket.requestedBindIp
                  << " actualBind=" << endpointText(bound) << "\n";
        return 1;
    }

    std::cout << "probe ok bytes=" << result.bytesSent
              << " target=" << endpointText(result.target)
              << " requestedBind=" << result.socket.requestedBindIp
              << " actualBind=" << endpointText(bound)
              << " broadcast=" << (result.socket.broadcast ? "true" : "false") << "\n";
    return 0;
}
