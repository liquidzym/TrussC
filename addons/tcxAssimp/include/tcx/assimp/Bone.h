#pragma once
// Bone.h — Skeleton bone data for skinning
#include <TrussC.h>
#include <string>
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
    // Map bone name → index
    int findBone(const std::string& name) const {
        for (int i = 0; i < (int)bones.size(); i++)
            if (bones[i].name == name) return i;
        return -1;
    }
};

} // namespace tcx::assimp
