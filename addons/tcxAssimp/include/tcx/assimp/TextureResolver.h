#pragma once
// TextureResolver.h - Resolves Assimp texture references into TrussC pixels.
#include "tcx/assimp/SceneData.h"
#include <TrussC.h>
#include <filesystem>
#include <string>

namespace tcx::assimp {

class TextureResolver {
public:
    static std::string normalizeRef(std::string ref);
    static std::filesystem::path resolvePath(const std::string& modelPath, const std::string& textureRef);

    static bool loadExternal(const std::string& modelPath, const std::string& textureRef, tc::Pixels& outPixels);
    static bool loadEmbedded(const SceneData& sceneData, const std::string& textureRef, tc::Pixels& outPixels);
    static bool load(const SceneData& sceneData, const std::string& modelPath, const std::string& textureRef, tc::Pixels& outPixels);
};

} // namespace tcx::assimp
