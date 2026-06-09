#include <tcxArtNet.h>

#include <iostream>

int main() {
    tcx::artnet::Sender sender;
    tcx::artnet::Error error;
    if (!sender.setup(false, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }

    sender.setDestination({ "127.0.0.1", tcx::artnet::DefaultPort });
    if (!sender.setColor(1, 0, trussc::Color(1.0f, 0.0f, 0.0f), &error) ||
        !sender.setColor(1, 3, trussc::Color(0.0f, 1.0f, 0.0f), &error) ||
        !sender.send(&error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "sent full ArtDmx universe 1 to 127.0.0.1:6454 using zero-based channels 0..5\n";
    return 0;
}
