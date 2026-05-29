// TextureResolver.cpp - Assimp texture reference resolution.
#include "tcx/assimp/TextureResolver.h"
#include <algorithm>
#include <cctype>

namespace tcx::assimp {

namespace {
namespace fs = std::filesystem;

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}
} // namespace

std::string TextureResolver::normalizeRef(std::string ref) {
    std::replace(ref.begin(), ref.end(), '\\', '/');
    return ref;
}

fs::path TextureResolver::resolvePath(const std::string& modelPath, const std::string& textureRef) {
    if (textureRef.empty() || textureRef[0] == '*') return {};

    std::string ref = normalizeRef(textureRef);
    fs::path texPath(ref);
    if (texPath.is_absolute() && fs::exists(texPath)) return texPath;

    fs::path modelDir = fs::path(modelPath).parent_path();
    if (ref.rfind("//", 0) == 0) {
        ref = ref.substr(2);
    }

    fs::path candidate = (modelDir / fs::path(ref)).lexically_normal();
    if (fs::exists(candidate)) return candidate;

    // Case-insensitive filename fallback helps when models move between filesystems.
    fs::path parent = candidate.parent_path();
    std::string wantedLower = lowerCopy(candidate.filename().string());
    if (fs::exists(parent) && fs::is_directory(parent)) {
        for (const auto& entry : fs::directory_iterator(parent)) {
            if (lowerCopy(entry.path().filename().string()) == wantedLower) {
                return entry.path();
            }
        }
    }

    return candidate;
}

bool TextureResolver::loadExternal(const std::string& modelPath, const std::string& textureRef, tc::Pixels& outPixels) {
    fs::path resolved = resolvePath(modelPath, textureRef);
    return !resolved.empty() && fs::exists(resolved) && outPixels.load(resolved);
}

bool TextureResolver::loadEmbedded(const SceneData& sceneData, const std::string& textureRef, tc::Pixels& outPixels) {
    std::string key = normalizeRef(textureRef);
    if (key.empty() || key[0] != '*') return false;

    int idx = -1;
    try {
        idx = std::stoi(key.substr(1));
    } catch (...) {
        return false;
    }

    if (idx < 0 || idx >= (int)sceneData.textures.size()) return false;
    const auto& texData = sceneData.textures[idx];
    if (texData.pixels.empty()) return false;

    if (texData.height == 0) {
        return outPixels.loadFromMemory(texData.pixels.data(), (int)texData.pixels.size());
    }

    int channels = texData.channels > 0 ? texData.channels : 4;
    outPixels.setFromPixels(texData.pixels.data(), texData.width, texData.height, channels);
    return outPixels.isAllocated();
}

bool TextureResolver::load(const SceneData& sceneData,
                           const std::string& modelPath,
                           const std::string& textureRef,
                           tc::Pixels& outPixels) {
    std::string key = normalizeRef(textureRef);
    if (key.empty()) return false;
    if (key[0] == '*') return loadEmbedded(sceneData, key, outPixels);
    return loadExternal(modelPath, key, outPixels);
}

} // namespace tcx::assimp
