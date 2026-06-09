#include <tcxArtNet.h>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    tcx::artnet::Receiver receiver;
    tcx::artnet::Error error;
    if (!receiver.setup(tcx::artnet::DefaultPort, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }

    std::cout << "waiting for ArtDmx on UDP 6454\n";
    for (int i = 0; i < 500; ++i) {
        receiver.poll(&error);
        if (receiver.hasNewData()) {
            for (const uint16_t universe : receiver.getUniverses()) {
                std::cout << "ArtDmx universe " << universe
                          << " channel 0 = " << static_cast<int>(receiver.getChannel(universe, 0))
                          << "\n";
            }
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "no ArtDmx received\n";
    return 0;
}
