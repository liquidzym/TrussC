#pragma once
// SceneData.h — CPU-side asset data
#include <TrussC.h>
#include "Animation.h"
#include "Bone.h"
#include <string>
#include <vector>
#include <cstdint>
#include <limits>

namespace tcx::assimp {

struct BoundingBox {
    tc::Vec3 min{std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max()};
    tc::Vec3 max{std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest()};
    tc::Vec3 center() const { return (min + max) * 0.5f; }
    tc::Vec3 size()   const { return max - min; }
    void expand(const tc::Vec3& p) {
        if(p.x<min.x)min.x=p.x; if(p.y<min.y)min.y=p.y; if(p.z<min.z)min.z=p.z;
        if(p.x>max.x)max.x=p.x; if(p.y>max.y)max.y=p.y; if(p.z>max.z)max.z=p.z;
    }
};

struct VertexData {
    tc::Vec3 position{0,0,0};
    tc::Vec3 normal{0,0,1};
    tc::Vec2 texCoord{0,0};
    tc::Vec4 tangent{1,0,0,1};
    tc::Color color{1,1,1,1};
};

struct MeshData {
    std::string name;
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    std::vector<VertexBoneData> boneData;  // per-vertex bone weights
    int materialIndex = -1;
    int nodeIndex = -1;  // first scene node referencing this mesh
    BoundingBox bounds;
};

struct MaterialData {
    std::string name;
    tc::Color diffuseColor{1,1,1,1};
    tc::Color specularColor{0,0,0,1};
    tc::Color emissiveColor{0,0,0,1};
    float opacity = 1.0f;
    float shininess = 0.0f;
    bool twoSided = false;

    // PBR (glTF)
    tc::Color baseColor{1,1,1,1};
    float metallic = 0.0f;
    float roughness = 0.5f;

    // Texture paths (resolved)
    std::string diffuseTexture;
    std::string normalTexture;
    std::string metallicRoughnessTexture;
    std::string emissiveTexture;
    std::string occlusionTexture;
    std::string alphaMode = "OPAQUE";
    float alphaCutoff = 0.5f;
};

struct TextureData {
    std::string path;           // resolved file path
    bool embedded = false;
    std::vector<uint8_t> pixels;
    int width = 0, height = 0, channels = 0;
};

struct NodeData {
    std::string name;
    tc::Mat4 localTransform = tc::Mat4::identity();
    tc::Vec3 localPosition{0,0,0};
    tc::Quaternion localRotation = tc::Quaternion::identity();
    tc::Vec3 localScale{1,1,1};
    int parentIndex = -1;
    std::vector<int> childIndices;
    std::vector<int> meshIndices; // indices into scene mesh array
};

struct SceneData {
    std::vector<MeshData> meshes;
    std::vector<MaterialData> materials;
    std::vector<TextureData> textures;
    std::vector<NodeData> nodes;
    std::vector<AnimationClip> animations;
    Skeleton skeleton;
    int rootNodeIndex = -1;
    BoundingBox globalBounds;
};

} // namespace tcx::assimp
