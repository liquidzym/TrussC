#include "test_common.h"

void test_network_diagnostics() {
    using namespace tcx::artnet;

    Error error;
    std::string broadcastIp;
    require(makeDirectedBroadcastIp("192.168.1.10", "255.255.255.0", broadcastIp, &error), error.message.c_str());
    require(broadcastIp == "192.168.1.255", "directed /24 broadcast uses target network");
    require(makeDirectedBroadcastIp("10.1.2.34", "255.255.0.0", broadcastIp, &error), error.message.c_str());
    require(broadcastIp == "10.1.255.255", "directed /16 broadcast uses subnet mask");
    require(!makeDirectedBroadcastIp("10.1.2.34", "255.255.0", broadcastIp, &error), "invalid subnet mask is rejected");
    require(error.code == ErrorCode::InvalidAddress, "invalid subnet reports InvalidAddress");

    UdpSocket socket;
    require(socket.open(&error), error.message.c_str());
    require(socket.setReuseAddress(true, &error), error.message.c_str());
    require(socket.setBroadcast(true, &error), error.message.c_str());
    require(socket.bind("127.0.0.1", 0, &error), error.message.c_str());
    require(socket.setNonBlocking(true, &error), error.message.c_str());

    const auto socketDiagnostics = socket.diagnostics();
    require(socketDiagnostics.open, "socket diagnostics reports open socket");
    require(socketDiagnostics.reuseAddress, "socket diagnostics reports reuse-address option");
    require(socketDiagnostics.broadcast, "socket diagnostics reports broadcast option");
    require(socketDiagnostics.nonBlocking, "socket diagnostics reports nonblocking option");
    require(socketDiagnostics.requestedBindIp == "127.0.0.1", "socket diagnostics reports requested bind IP");
    require(socketDiagnostics.requestedBindPort == 0, "socket diagnostics reports requested bind port");
    require(socketDiagnostics.actualLocalEndpoint.ip == "127.0.0.1", "socket diagnostics reports actual bind IP");
    require(socketDiagnostics.actualLocalEndpoint.port != 0, "socket diagnostics reports actual ephemeral port");
    socket.close();

    Controller controller;
    ControllerSettings settings;
    settings.localBindIp = "127.0.0.1";
    settings.localPort = 0;
    settings.enableBroadcast = true;
    settings.autoPoll = false;
    require(controller.setup(settings, &error), error.message.c_str());

    auto controllerDiagnostics = controller.networkDiagnostics();
    require(controllerDiagnostics.socket.open, "controller diagnostics reports open socket");
    require(controllerDiagnostics.socket.actualLocalEndpoint.ip == "127.0.0.1", "controller diagnostics reports actual bind IP");
    require(controllerDiagnostics.socket.actualLocalEndpoint.port != 0, "controller diagnostics reports actual port");
    require(controllerDiagnostics.enableBroadcast, "controller diagnostics reports broadcast setting");
    require(controllerDiagnostics.recoveryState == RecoveryState::Idle, "controller starts in idle recovery state");

    require(controller.recover(&error), error.message.c_str());
    controllerDiagnostics = controller.networkDiagnostics();
    require(controllerDiagnostics.recoveryState == RecoveryState::Recovered, "controller recover reports recovered state");
    require(controllerDiagnostics.recoveryAttempts == 1, "controller recover increments attempts");
    require(controllerDiagnostics.socket.open, "controller socket is open after recovery");
    controller.close();
}
