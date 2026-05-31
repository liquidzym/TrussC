#pragma once
// Bone.h — Skeleton bone data for skinning
#include <TrussC.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace tcx::assimp {

struct VertexBoneData {
    uint16_t indices[4] = {0,0,0,0};
    float    weights[4] = {0,0,0,0};
};

struct Bone {
    std::string name;
    tc::Mat4    offsetMatrix;  // inverse bind pose
    tc::Mat4    finalMatrix;   // animated final transform
    int         nodeIndex = -1;
};

struct Skeleton {
    std::vector<Bone> bones;

    // Map bone name to index for import-time and runtime lookups. If external
    // code mutates bones directly, findBone() still falls back to a linear scan.
    std::unordered_map<std::string, int> boneIndex;

    void clear() {
        bones.clear();
        boneIndex.clear();
    }

    void rebuildIndex() {
        boneIndex.clear();
        for (int i = 0; i < (int)bones.size(); i++) {
            boneIndex[bones[i].name] = i;
        }
    }

    int findBone(const std::string& name) const {
        auto it = boneIndex.find(name);
        if (it != boneIndex.end()) return it->second;

        for (int i = 0; i < (int)bones.size(); i++)
            if (bones[i].name == name) return i;
        return -1;
    }
};

} // namespace tcx::assimp
