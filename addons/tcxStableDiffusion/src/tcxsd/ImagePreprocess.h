#pragma once

#include "tcxsd/Types.h"

namespace tcx::sd {

enum class ControlPreprocessor {
    SobelCanny,
};

struct ControlPreprocessOptions {
    ControlPreprocessor preprocessor = ControlPreprocessor::SobelCanny;
    int lowThreshold = 32;
    int highThreshold = 96;
    bool invert = false;
};

struct InpaintMaskOptions {
    float marginRatio = 0.24f;
    int featherPixels = 0;
    bool whiteInside = true;
};

struct ImagePreprocessResult {
    bool ok = false;
    fs::path sourcePath;
    fs::path outputPath;
    int width = 0;
    int height = 0;
    std::string error;
    std::map<std::string, std::string> metadata;
};

ImagePreprocessResult preprocessControlImage(
    const fs::path& sourcePath,
    const fs::path& outputPath,
    const ControlPreprocessOptions& options = {});

ImagePreprocessResult createInpaintMask(
    const fs::path& sourcePath,
    const fs::path& outputPath,
    const InpaintMaskOptions& options = {});

const char* toString(ControlPreprocessor preprocessor);

} // namespace tcx::sd
