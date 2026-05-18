#include "test_common.h"

void test_pixel_mapper() {
    using namespace tcx::artnet;
    std::vector<uint8_t> rgb(171 * 3, 0);
    for (size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<uint8_t>(i & 0xff);
    std::vector<ArtDmx> frames;
    Error error;
    PixelToDmxOptions options;
    options.format = PixelFormat::RGB;
    options.channelsPerUniverse = 510;
    require(PixelMapper::splitPixelsToUniverses(rgb, UniverseAddress { 0, 0, 0 }, options, frames, &error), error.message.c_str());
    require(frames.size() == 2, "RGB mapper splits after 510 channels");
    require(frames[0].data.size() == 510, "RGB first universe uses 510 channels");
    require(frames[1].universe.universe == 1, "RGB mapper increments universe");

    std::vector<uint8_t> grb = { 1, 2, 3, 4, 5, 6 };
    options.format = PixelFormat::GRB;
    require(PixelMapper::splitPixelsToUniverses(grb, UniverseAddress { 0, 0, 3 }, options, frames, &error), error.message.c_str());
    require(frames[0].data[0] == 2 && frames[0].data[1] == 1, "GRB mapper swaps red and green");

    std::vector<uint8_t> rgbw(129 * 4, 0);
    options.format = PixelFormat::RGBW;
    options.channelsPerUniverse = 510;
    require(PixelMapper::splitPixelsToUniverses(rgbw, UniverseAddress { 0, 0, 4 }, options, frames, &error), error.message.c_str());
    require(frames[0].data.size() == 512, "RGBW default uses 512 channels");

    std::vector<uint8_t> rgba = {
        10, 20, 30, 40,
        50, 60, 70, 80
    };
    options.format = PixelFormat::RGBA;
    require(PixelMapper::splitPixelsToUniverses(rgba, UniverseAddress { 0, 0, 5 }, options, frames, &error), error.message.c_str());
    require(frames.size() == 1, "RGBA mapper keeps small payload in one universe");
    require(frames[0].data == rgba, "RGBA mapper preserves RGBA channel order");

    std::vector<uint8_t> bgra = { 1, 2, 3, 4 };
    options.format = PixelFormat::BGRA;
    require(PixelMapper::splitPixelsToUniverses(bgra, UniverseAddress { 0, 0, 6 }, options, frames, &error), error.message.c_str());
    require((frames[0].data == std::vector<uint8_t> { 3, 2, 1, 4 }), "BGRA mapper converts to RGBA-like DMX order");
}
