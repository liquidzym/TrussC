#include "tcx/artnet/ArtNetTypes.h"

#include <algorithm>

namespace tcx::artnet {

void setError(Error* error, ErrorCode code, std::string_view message) {
    if (!error) {
        return;
    }
    error->code = code;
    error->message = std::string(message);
}

bool UniverseAddress::isValid() const noexcept {
    return net <= 127 && subnet <= 15 && universe <= 15;
}

uint16_t UniverseAddress::toPortAddress() const noexcept {
    return static_cast<uint16_t>(((net & 0x7f) << 8) | ((subnet & 0x0f) << 4) | (universe & 0x0f));
}

std::optional<UniverseAddress> UniverseAddress::fromPortAddress(uint16_t portAddress) noexcept {
    if (portAddress > 32767) {
        return std::nullopt;
    }
    return UniverseAddress {
        static_cast<uint8_t>((portAddress >> 8) & 0x7f),
        static_cast<uint8_t>((portAddress >> 4) & 0x0f),
        static_cast<uint8_t>(portAddress & 0x0f)
    };
}

std::optional<UniverseAddress> UniverseAddress::next(const UniverseAddress& address) noexcept {
    if (!address.isValid()) {
        return std::nullopt;
    }
    const uint16_t nextAddress = static_cast<uint16_t>(address.toPortAddress() + 1);
    return fromPortAddress(nextAddress);
}

namespace {

size_t channelCount(PixelFormat format) {
    switch (format) {
        case PixelFormat::RGB:
        case PixelFormat::GRB:
        case PixelFormat::BGR:
            return 3;
        case PixelFormat::RGBW:
        case PixelFormat::GRBW:
        case PixelFormat::BGRA:
        case PixelFormat::RGBA:
            return 4;
    }
    return 3;
}

void appendPixel(std::vector<uint8_t>& out, std::span<const uint8_t> pixel, PixelFormat format) {
    switch (format) {
        case PixelFormat::RGB:
            out.insert(out.end(), { pixel[0], pixel[1], pixel[2] });
            break;
        case PixelFormat::RGBW:
            out.insert(out.end(), { pixel[0], pixel[1], pixel[2], pixel[3] });
            break;
        case PixelFormat::GRB:
            out.insert(out.end(), { pixel[1], pixel[0], pixel[2] });
            break;
        case PixelFormat::GRBW:
            out.insert(out.end(), { pixel[1], pixel[0], pixel[2], pixel[3] });
            break;
        case PixelFormat::BGR:
            out.insert(out.end(), { pixel[2], pixel[1], pixel[0] });
            break;
        case PixelFormat::BGRA:
            out.insert(out.end(), { pixel[2], pixel[1], pixel[0], pixel[3] });
            break;
        case PixelFormat::RGBA:
            out.insert(out.end(), { pixel[0], pixel[1], pixel[2], pixel[3] });
            break;
    }
}

size_t normalizedChannelsPerUniverse(const PixelToDmxOptions& options) {
    if ((options.format == PixelFormat::RGBW || options.format == PixelFormat::GRBW ||
         options.format == PixelFormat::RGBA || options.format == PixelFormat::BGRA) &&
        options.channelsPerUniverse == 510) {
        return 512;
    }
    return options.channelsPerUniverse;
}

} // namespace

bool PixelMapper::splitPixelsToUniverses(
    std::span<const uint8_t> pixels,
    const UniverseAddress& firstUniverse,
    const PixelToDmxOptions& options,
    std::vector<ArtDmx>& outFrames,
    Error* error
) {
    outFrames.clear();
    if (!firstUniverse.isValid()) {
        setError(error, ErrorCode::InvalidUniverse, "first universe is out of Art-Net range");
        return false;
    }

    const size_t pixelChannels = channelCount(options.format);
    if (pixels.empty()) {
        setError(error, ErrorCode::InvalidLength, "pixel buffer is empty");
        return false;
    }
    if (pixels.size() % pixelChannels != 0) {
        setError(error, ErrorCode::InvalidLength, "pixel buffer length is not divisible by pixel format channel count");
        return false;
    }

    const size_t channelsPerUniverse = normalizedChannelsPerUniverse(options);
    if (channelsPerUniverse == 0 || channelsPerUniverse > MaxDmxChannels) {
        setError(error, ErrorCode::InvalidLength, "channels per universe must be 1..512");
        return false;
    }

    std::vector<uint8_t> stream;
    stream.reserve(pixels.size());
    for (size_t offset = 0; offset < pixels.size(); offset += pixelChannels) {
        appendPixel(stream, pixels.subspan(offset, pixelChannels), options.format);
    }

    UniverseAddress universe = firstUniverse;
    for (size_t offset = 0; offset < stream.size(); offset += channelsPerUniverse) {
        const size_t count = std::min(channelsPerUniverse, stream.size() - offset);
        if (options.strict && count < MinArtDmxChannels) {
            setError(error, ErrorCode::InvalidLength, "last universe would contain fewer than two DMX channels");
            return false;
        }

        ArtDmx frame;
        frame.universe = universe;
        frame.data.assign(stream.begin() + static_cast<std::ptrdiff_t>(offset),
                          stream.begin() + static_cast<std::ptrdiff_t>(offset + count));
        if (frame.data.size() < MinArtDmxChannels) {
            frame.data.resize(MinArtDmxChannels, 0);
        } else if (frame.data.size() % 2 != 0) {
            frame.data.push_back(0);
        }
        outFrames.push_back(std::move(frame));

        if (offset + count < stream.size()) {
            auto nextUniverse = UniverseAddress::next(universe);
            if (!nextUniverse) {
                setError(error, ErrorCode::InvalidUniverse, "pixel data exceeds Art-Net port-address range");
                return false;
            }
            universe = *nextUniverse;
        }
    }
    return true;
}

} // namespace tcx::artnet
