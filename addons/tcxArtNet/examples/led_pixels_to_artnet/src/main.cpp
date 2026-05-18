#include <tcxArtNet.h>

#include <iostream>
#include <vector>

int main() {
    std::vector<uint8_t> pixels(171 * 3, 128);
    std::vector<tcx::artnet::ArtDmx> frames;
    tcx::artnet::PixelToDmxOptions options;
    options.format = tcx::artnet::PixelFormat::RGB;
    tcx::artnet::Error error;
    if (!tcx::artnet::PixelMapper::splitPixelsToUniverses(pixels, { 0, 0, 1 }, options, frames, &error)) {
        std::cerr << error.message << "\n";
        return 1;
    }
    std::cout << "mapped RGB pixels to " << frames.size() << " ArtDmx frames\n";
    return 0;
}
