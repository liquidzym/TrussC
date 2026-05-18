#include "test_common.h"

void test_cross_platform_socket() {
    using namespace tcx::artnet;
    UdpSocket receiver;
    UdpSocket sender;
    Error error;
    require(receiver.open(&error), error.message.c_str());
    require(receiver.setReuseAddress(true, &error), error.message.c_str());
    require(receiver.bind(64540, &error), error.message.c_str());
    require(receiver.setNonBlocking(true, &error), error.message.c_str());
    require(sender.open(&error), error.message.c_str());

    std::array<uint8_t, 4> payload { 1, 2, 3, 4 };
    require(sender.sendTo(payload, Endpoint { "127.0.0.1", 64540 }, &error), error.message.c_str());

    std::array<uint8_t, 16> buffer {};
    Endpoint endpoint;
    size_t received = 0;
    bool ok = false;
    for (int i = 0; i < 50; ++i) {
        ok = receiver.receiveFrom(buffer, received, endpoint, &error);
        if (ok) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(ok, "UDP localhost receive succeeds");
    require(received == payload.size(), "UDP localhost receive length matches");
    require(buffer[0] == 1 && buffer[3] == 4, "UDP localhost payload matches");
}
