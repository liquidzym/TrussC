#include "tcxsd/ImagePreprocess.h"

#include <algorithm>
#include <cmath>
#include <system_error>
#include <vector>

namespace tcx::sd {
namespace {

int clampByte(int value) {
    return std::clamp(value, 0, 255);
}

bool prepareSource(
    const fs::path& sourcePath,
    ImagePreprocessResult& result,
    trussc::Pixels& pixels) {
    result.sourcePath = sourcePath;
    if (sourcePath.empty()) {
        result.error = "MODEL_ASSET_MISSING: source image path is empty.";
        return false;
    }
    if (!pixels.load(sourcePath)) {
        result.error = "MODEL_ASSET_MISSING: failed to load source image: " + sourcePath.string();
        return false;
    }
    if (!pixels.isAllocated() || pixels.isFloat() || pixels.getChannels() < 3) {
        result.error = "Unsupported source image format for preprocessing: " + sourcePath.string();
        return false;
    }
    result.width = pixels.getWidth();
    result.height = pixels.getHeight();
    return true;
}

bool saveRgba(
    const fs::path& outputPath,
    int width,
    int height,
    const std::vector<unsigned char>& rgba,
    ImagePreprocessResult& result) {
    result.outputPath = outputPath;
    std::error_code ec;
    const auto parent = outputPath.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            result.error = "Failed to create preprocessing output directory: " + parent.string() + " (" + ec.message() + ")";
            return false;
        }
    }

    trussc::Pixels out;
    out.setFromPixels(rgba.data(), width, height, 4);
    if (!out.save(outputPath)) {
        result.error = "Failed to save preprocessed image: " + outputPath.string();
        return false;
    }
    result.ok = true;
    return true;
}

std::vector<unsigned char> grayscale(const trussc::Pixels& pixels) {
    const int width = pixels.getWidth();
    const int height = pixels.getHeight();
    const int channels = pixels.getChannels();
    const unsigned char* data = pixels.getData();
    std::vector<unsigned char> gray(static_cast<size_t>(width) * static_cast<size_t>(height), 0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t source = (static_cast<size_t>(y) * width + x) * channels;
            const int r = data[source + 0];
            const int g = data[source + 1];
            const int b = data[source + 2];
            gray[static_cast<size_t>(y) * width + x] = static_cast<unsigned char>(
                clampByte(static_cast<int>(std::round(0.299 * r + 0.587 * g + 0.114 * b))));
        }
    }
    return gray;
}

float outsideDistanceToRect(int x, int y, int left, int top, int right, int bottom) {
    const int dx = std::max({left - x, 0, x - right});
    const int dy = std::max({top - y, 0, y - bottom});
    return std::sqrt(static_cast<float>(dx * dx + dy * dy));
}

} // namespace

const char* toString(ControlPreprocessor preprocessor) {
    switch (preprocessor) {
        case ControlPreprocessor::SobelCanny: return "sobel_canny";
    }
    return "sobel_canny";
}

ImagePreprocessResult preprocessControlImage(
    const fs::path& sourcePath,
    const fs::path& outputPath,
    const ControlPreprocessOptions& options) {
    ImagePreprocessResult result;
    trussc::Pixels source;
    if (!prepareSource(sourcePath, result, source)) {
        return result;
    }

    const int width = source.getWidth();
    const int height = source.getHeight();
    int low = clampByte(options.lowThreshold);
    int high = clampByte(options.highThreshold);
    if (low > high) {
        std::swap(low, high);
    }

    const auto gray = grayscale(source);
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<unsigned char> strong(count, 0);
    std::vector<unsigned char> weak(count, 0);

    auto at = [&](int x, int y) -> int {
        return gray[static_cast<size_t>(y) * width + x];
    };

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const int gx =
                -at(x - 1, y - 1) + at(x + 1, y - 1) +
                -2 * at(x - 1, y) + 2 * at(x + 1, y) +
                -at(x - 1, y + 1) + at(x + 1, y + 1);
            const int gy =
                -at(x - 1, y - 1) - 2 * at(x, y - 1) - at(x + 1, y - 1) +
                at(x - 1, y + 1) + 2 * at(x, y + 1) + at(x + 1, y + 1);
            const int magnitude = clampByte(static_cast<int>(std::round(std::sqrt(static_cast<double>(gx * gx + gy * gy)) / 4.0)));
            const size_t index = static_cast<size_t>(y) * width + x;
            if (magnitude >= high) {
                strong[index] = 1;
            } else if (magnitude >= low) {
                weak[index] = 1;
            }
        }
    }

    const unsigned char background = options.invert ? 255 : 0;
    const unsigned char edge = options.invert ? 0 : 255;
    std::vector<unsigned char> rgba(count * 4, background);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t index = static_cast<size_t>(y) * width + x;
            bool keep = strong[index] != 0;
            if (!keep && weak[index] != 0) {
                for (int oy = -1; oy <= 1 && !keep; ++oy) {
                    for (int ox = -1; ox <= 1 && !keep; ++ox) {
                        const int nx = x + ox;
                        const int ny = y + oy;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                            continue;
                        }
                        keep = strong[static_cast<size_t>(ny) * width + nx] != 0;
                    }
                }
            }
            const unsigned char value = keep ? edge : background;
            const size_t out = index * 4;
            rgba[out + 0] = value;
            rgba[out + 1] = value;
            rgba[out + 2] = value;
            rgba[out + 3] = 255;
        }
    }

    result.metadata["preprocessor"] = toString(options.preprocessor);
    result.metadata["low_threshold"] = std::to_string(low);
    result.metadata["high_threshold"] = std::to_string(high);
    result.metadata["invert"] = options.invert ? "true" : "false";
    saveRgba(outputPath, width, height, rgba, result);
    return result;
}

ImagePreprocessResult createInpaintMask(
    const fs::path& sourcePath,
    const fs::path& outputPath,
    const InpaintMaskOptions& options) {
    ImagePreprocessResult result;
    trussc::Pixels source;
    if (!prepareSource(sourcePath, result, source)) {
        return result;
    }

    const int width = source.getWidth();
    const int height = source.getHeight();
    const float marginRatio = std::clamp(options.marginRatio, 0.0f, 0.49f);
    const int marginX = std::clamp(static_cast<int>(std::round(width * marginRatio)), 0, std::max(0, width / 2 - 1));
    const int marginY = std::clamp(static_cast<int>(std::round(height * marginRatio)), 0, std::max(0, height / 2 - 1));
    const int left = marginX;
    const int top = marginY;
    const int right = width - marginX - 1;
    const int bottom = height - marginY - 1;
    const int feather = std::max(0, options.featherPixels);

    std::vector<unsigned char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool inside = x >= left && x <= right && y >= top && y <= bottom;
            float alpha = inside ? 1.0f : 0.0f;
            if (feather > 0) {
                if (inside) {
                    const int innerDistance = std::min({x - left, right - x, y - top, bottom - y});
                    alpha = std::clamp(static_cast<float>(innerDistance + 1) / static_cast<float>(feather + 1), 0.0f, 1.0f);
                } else {
                    const float distance = outsideDistanceToRect(x, y, left, top, right, bottom);
                    alpha = std::clamp(1.0f - distance / static_cast<float>(feather + 1), 0.0f, 1.0f);
                }
            }
            const float maskValue = options.whiteInside ? alpha : 1.0f - alpha;
            const unsigned char value = static_cast<unsigned char>(clampByte(static_cast<int>(std::round(maskValue * 255.0f))));
            const size_t out = (static_cast<size_t>(y) * width + x) * 4;
            rgba[out + 0] = value;
            rgba[out + 1] = value;
            rgba[out + 2] = value;
            rgba[out + 3] = 255;
        }
    }

    result.metadata["preprocessor"] = "center_inpaint_mask";
    result.metadata["margin_ratio"] = std::to_string(marginRatio);
    result.metadata["feather_pixels"] = std::to_string(feather);
    result.metadata["white_inside"] = options.whiteInside ? "true" : "false";
    saveRgba(outputPath, width, height, rgba, result);
    return result;
}

} // namespace tcx::sd
