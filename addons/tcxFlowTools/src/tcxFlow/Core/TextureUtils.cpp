#include "TextureUtils.h"

namespace tcx::flow {

TextureFormat chooseRenderableFlowFormat(bool preferFloat) {
#if defined(__EMSCRIPTEN__)
    return TextureFormat::RGBA8;
#else
    return preferFloat ? TextureFormat::RGBA16F : TextureFormat::RGBA8;
#endif
}

const char* textureFormatName(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8: return "RGBA8";
        case TextureFormat::RGBA16F: return "RGBA16F";
        case TextureFormat::RGBA32F: return "RGBA32F";
        case TextureFormat::BGRA8: return "BGRA8";
        case TextureFormat::RGBA16: return "RGBA16";
        case TextureFormat::R8: return "R8";
        case TextureFormat::R16F: return "R16F";
        case TextureFormat::R32F: return "R32F";
        case TextureFormat::RG8: return "RG8";
        case TextureFormat::RG16F: return "RG16F";
        case TextureFormat::RG32F: return "RG32F";
    }
    return "Unknown";
}

} // namespace tcx::flow
