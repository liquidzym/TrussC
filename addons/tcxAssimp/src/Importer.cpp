// Importer.cpp — Assimp scene import implementation
#include "tcx/assimp/Importer.h"
#include "tcx/assimp/AssimpConvert.h"
#include "tcx/assimp/Animation.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <cstdio>
#include <functional>

namespace tcx::assimp {

namespace {
void expandTransformedBounds(BoundingBox& bounds, const BoundingBox& local, const tc::Mat4& transform) {
    const tc::Vec3 corners[8] = {
        {local.min.x, local.min.y, local.min.z},
        {local.max.x, local.min.y, local.min.z},
        {local.min.x, local.max.y, local.min.z},
        {local.max.x, local.max.y, local.min.z},
        {local.min.x, local.min.y, local.max.z},
        {local.max.x, local.min.y, local.max.z},
        {local.min.x, local.max.y, local.max.z},
        {local.max.x, local.max.y, local.max.z},
    };

    for (const auto& corner : corners) {
        bounds.expand(transform * corner);
    }
}

std::vector<tc::Mat4> computeNodeGlobals(const SceneData& scene) {
    std::vector<tc::Mat4> globals(scene.nodes.size(), tc::Mat4::identity());
    std::function<void(int)> visit = [&](int idx) {
        if (idx < 0 || idx >= (int)scene.nodes.size()) return;
        const auto& node = scene.nodes[idx];
        if (node.parentIndex >= 0 && node.parentIndex < (int)globals.size()) {
            globals[idx] = globals[node.parentIndex] * node.localTransform;
        } else {
            globals[idx] = node.localTransform;
        }
        for (int child : node.childIndices) visit(child);
    };

    if (scene.rootNodeIndex >= 0) {
        visit(scene.rootNodeIndex);
    } else {
        for (int i = 0; i < (int)scene.nodes.size(); i++) {
            if (scene.nodes[i].parentIndex < 0) visit(i);
        }
    }
    return globals;
}

static unsigned int defaultFlags() {
    return aiProcess_Triangulate |
           aiProcess_GenSmoothNormals |
           aiProcess_JoinIdenticalVertices |
           aiProcess_LimitBoneWeights |
           aiProcess_RemoveRedundantMaterials |
           aiProcess_SortByPType |
           aiProcess_FindInvalidData |
           aiProcess_GenUVCoords |
           aiProcess_TransformUVCoords |
           aiProcess_CalcTangentSpace;
}

void importNode(const aiScene* scene, aiNode* aiNode, int parentIdx, SceneData& out) {
    NodeData node;
    node.name = aiNode->mName.C_Str();
    node.parentIndex = parentIdx;
    node.localTransform = toMat4(aiNode->mTransformation);
    aiVector3D scale;
    aiQuaternion rotation;
    aiVector3D position;
    aiNode->mTransformation.Decompose(scale, rotation, position);
    node.localPosition = toVec3(position);
    node.localRotation = toQuat(rotation);
    node.localScale = toVec3(scale);

    // Mesh references
    for (unsigned int i = 0; i < aiNode->mNumMeshes; i++)
        node.meshIndices.push_back(aiNode->mMeshes[i]);

    int myIdx = (int)out.nodes.size();
    out.nodes.push_back(std::move(node));

    if (parentIdx >= 0)
        out.nodes[parentIdx].childIndices.push_back(myIdx);
    else
        out.rootNodeIndex = myIdx;

    for (unsigned int i = 0; i < aiNode->mNumChildren; i++)
        importNode(scene, aiNode->mChildren[i], myIdx, out);
}

void importMesh(const aiScene* scene, int idx, SceneData& out) {
    auto* aiMesh = scene->mMeshes[idx];
    MeshData mesh;
    mesh.name = aiMesh->mName.C_Str();
    mesh.materialIndex = aiMesh->mMaterialIndex;
    mesh.vertices.reserve(aiMesh->mNumVertices);

    for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
        VertexData v;
        v.position = toVec3(aiMesh->mVertices[i]);
        if (aiMesh->mNormals)     v.normal   = toVec3(aiMesh->mNormals[i]);
        if (aiMesh->mTextureCoords[0]) v.texCoord = tc::Vec2(aiMesh->mTextureCoords[0][i].x, aiMesh->mTextureCoords[0][i].y);
        if (aiMesh->mTangents)    v.tangent  = toVec4(aiMesh->mTangents[i]);
        if (aiMesh->mColors[0])   v.color    = toColor(aiMesh->mColors[0][i]);
        mesh.vertices.push_back(v);
        mesh.bounds.expand(v.position);
    }

    for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
        auto& face = aiMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            mesh.indices.push_back(face.mIndices[j]);
    }

    // Bone weights
    mesh.boneData.resize(aiMesh->mNumVertices);
    for (unsigned int bi = 0; bi < aiMesh->mNumBones; bi++) {
        auto* aiBone = aiMesh->mBones[bi];
        int globalBoneIndex = out.skeleton.findBone(aiBone->mName.C_Str());
        if (globalBoneIndex < 0) continue;
        for (unsigned int wi = 0; wi < aiBone->mNumWeights; wi++) {
            auto& w = aiBone->mWeights[wi];
            if (w.mVertexId >= aiMesh->mNumVertices) continue;
            auto& bd = mesh.boneData[w.mVertexId];
            // Find first empty slot
            for (int s = 0; s < 4; s++) {
                if (bd.weights[s] == 0.0f) {
                    bd.indices[s] = (uint16_t)globalBoneIndex;
                    bd.weights[s] = w.mWeight;
                    break;
                }
            }
        }
    }

    for (int ni = 0; ni < (int)out.nodes.size(); ni++) {
        for (int meshIndex : out.nodes[ni].meshIndices) {
            if (meshIndex == idx) {
                mesh.nodeIndex = ni;
                break;
            }
        }
        if (mesh.nodeIndex >= 0) break;
    }

    out.meshes.push_back(std::move(mesh));
}

void importMaterial(const aiScene* scene, int idx, SceneData& out) {
    auto* aiMat = scene->mMaterials[idx];
    MaterialData mat;
    aiString name;
    if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
        mat.name = name.C_Str();

    aiColor4D c;
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS)  mat.diffuseColor = toColor(c);
    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, c) == AI_SUCCESS) mat.specularColor = toColor(c);
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, c) == AI_SUCCESS) mat.emissiveColor = toColor(c);

    float f;
    if (aiMat->Get(AI_MATKEY_OPACITY, f) == AI_SUCCESS) mat.opacity = f;
    if (aiMat->Get(AI_MATKEY_SHININESS, f) == AI_SUCCESS) mat.shininess = f;

    int twoSided = 0;
    if (aiMat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) mat.twoSided = (twoSided != 0);

    // PBR (glTF)
    if (aiMat->Get(AI_MATKEY_BASE_COLOR, c) == AI_SUCCESS) mat.baseColor = toColor(c);
    if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, f) == AI_SUCCESS) mat.metallic = f;
    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, f) == AI_SUCCESS) mat.roughness = f;
    if (aiMat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, f) == AI_SUCCESS) mat.alphaCutoff = f;

    aiString alphaMode;
    if (aiMat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
        mat.alphaMode = alphaMode.C_Str();
    }

    // Textures
    aiString texPath;
    if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        mat.diffuseTexture = texPath.C_Str();
    if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_NORMAL_CAMERA, 0, &texPath) == AI_SUCCESS)
        mat.normalTexture = texPath.C_Str();
    if (aiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS)
        mat.metallicRoughnessTexture = texPath.C_Str();
    if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS)
        mat.emissiveTexture = texPath.C_Str();
    if (aiMat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS ||
        aiMat->GetTexture(aiTextureType_LIGHTMAP, 0, &texPath) == AI_SUCCESS)
        mat.occlusionTexture = texPath.C_Str();

    out.materials.push_back(std::move(mat));
}

void importEmbeddedTexture(const aiScene* scene, int idx, SceneData& out) {
    auto* aiTex = scene->mTextures[idx];
    TextureData tex;
    tex.embedded = true;
    tex.width = (int)aiTex->mWidth;
    tex.height = (int)aiTex->mHeight;

    if (aiTex->mHeight == 0) {
        tex.pixels.assign(reinterpret_cast<const uint8_t*>(aiTex->pcData),
                          reinterpret_cast<const uint8_t*>(aiTex->pcData) + aiTex->mWidth);
    } else {
        size_t sz = aiTex->mWidth * aiTex->mHeight * 4;
        tex.pixels.assign(reinterpret_cast<const uint8_t*>(aiTex->pcData),
                          reinterpret_cast<const uint8_t*>(aiTex->pcData) + sz);
        tex.channels = 4;
    }
    out.textures.push_back(std::move(tex));
}

// =========================================================================
// Bone / Skeleton import
// =========================================================================

void importSkeleton(const aiScene* scene, SceneData& out) {
    out.skeleton.bones.clear();
    // Collect all bones from all meshes
    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++) {
        auto* aiMesh = scene->mMeshes[mi];
        for (unsigned int bi = 0; bi < aiMesh->mNumBones; bi++) {
            auto* aiBone = aiMesh->mBones[bi];
            std::string boneName = aiBone->mName.C_Str();
            // Avoid duplicates
            if (out.skeleton.findBone(boneName) >= 0) continue;

            Bone bone;
            bone.name = boneName;
            bone.offsetMatrix = toMat4(aiBone->mOffsetMatrix);
            // Find node index matching this bone name
            for (int ni = 0; ni < (int)out.nodes.size(); ni++) {
                if (out.nodes[ni].name == boneName) {
                    bone.nodeIndex = ni;
                    break;
                }
            }
            out.skeleton.bones.push_back(std::move(bone));
        }
    }
}

void importAnimations(const aiScene* scene, SceneData& out) {
    for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
        auto* aiAnim = scene->mAnimations[i];
        AnimationClip clip;
        clip.name = aiAnim->mName.C_Str();
        clip.durationTicks = aiAnim->mDuration;
        clip.ticksPerSecond = aiAnim->mTicksPerSecond > 0 ? aiAnim->mTicksPerSecond : 25.0;

        for (unsigned int j = 0; j < aiAnim->mNumChannels; j++) {
            auto* aiCh = aiAnim->mChannels[j];
            NodeAnimationChannel ch;
            ch.nodeName = aiCh->mNodeName.C_Str();

            for (unsigned int k = 0; k < aiCh->mNumPositionKeys; k++) {
                auto v = toVec3(aiCh->mPositionKeys[k].mValue);
                ch.positionKeys.push_back({aiCh->mPositionKeys[k].mTime, v});
            }
            for (unsigned int k = 0; k < aiCh->mNumRotationKeys; k++) {
                ch.rotationKeys.push_back({aiCh->mRotationKeys[k].mTime, toQuat(aiCh->mRotationKeys[k].mValue)});
            }
            for (unsigned int k = 0; k < aiCh->mNumScalingKeys; k++) {
                auto v = toVec3(aiCh->mScalingKeys[k].mValue);
                ch.scaleKeys.push_back({aiCh->mScalingKeys[k].mTime, v});
            }
            clip.channels.push_back(std::move(ch));
            clip.channelIndex[clip.channels.back().nodeName] = clip.channels.size() - 1;
        }
        out.animations.push_back(std::move(clip));
    }
}
} // namespace

struct Importer::Impl {
    Assimp::Importer importer;
};

Importer::Importer() : impl_(std::make_unique<Impl>()) {}
Importer::~Importer() = default;

bool Importer::load(const std::string& path, SceneData& out) {
    const aiScene* scene = impl_->importer.ReadFile(path, defaultFlags());
    if (!scene || !scene->mRootNode) {
        error_ = impl_->importer.GetErrorString();
        return false;
    }

    out = SceneData{};

    // Import materials
    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
        importMaterial(scene, i, out);

    // Import embedded textures
    for (unsigned int i = 0; i < scene->mNumTextures; i++)
        importEmbeddedTexture(scene, i, out);

    // Import node hierarchy
    out.nodes.reserve(scene->mNumMeshes * 2); // rough estimate
    importNode(scene, scene->mRootNode, -1, out);

    importSkeleton(scene, out);

    // Import meshes after nodes/skeleton so mesh instances and bone weights
    // can reference the addon's global node/bone indices.
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        importMesh(scene, i, out);

    // Import animations
    importAnimations(scene, out);

    // Compute scene-space bounds, including node transforms and instances.
    auto nodeGlobals = computeNodeGlobals(out);
    for (size_t ni = 0; ni < out.nodes.size(); ni++) {
        for (int meshIndex : out.nodes[ni].meshIndices) {
            if (meshIndex >= 0 && meshIndex < (int)out.meshes.size()) {
                expandTransformedBounds(out.globalBounds, out.meshes[meshIndex].bounds, nodeGlobals[ni]);
            }
        }
    }
    if (out.nodes.empty()) {
        for (auto& m : out.meshes) {
            out.globalBounds.expand(m.bounds.min);
            out.globalBounds.expand(m.bounds.max);
        }
    }

    return true;
}

} // namespace tcx::assimp
