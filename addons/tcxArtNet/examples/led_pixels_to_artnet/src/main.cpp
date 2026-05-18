#include <tcxArtNet.h>

#include <iostream>
#include <vector>

int main() {
    std::vector<uint8_t> pixels = {
        100, 60, 20, 0,
        20, 80, 40, 0
    };
    std::vector<tcx::artnet::ArtDmx> frames;
    tcx::artnet::PixelToDmxOptions options;
    options.format = tcx::artnet::PixelFormat::RGBW;
    options.extractWhite = true;
    options.brightness = 0.75f;
    tcx::artnet::Error error;
    if (!tcx::artnet::PixelMapper::splitPixelsToUniverses(pixels, { 0, 0, 1 }, options, frames, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "mapped RGBW pixels with white extraction to " << frames.size() << " ArtDmx frames\n";
    return 0;
}
