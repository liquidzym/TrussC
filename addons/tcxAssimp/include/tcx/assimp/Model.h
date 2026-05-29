#pragma once
// Model.h — User-facing 3D model (wraps SceneData + TrussC runtime)
#include "tcx/assimp/SceneData.h"
#include "tcx/assimp/Animation.h"
#include "tcx/assimp/Bone.h"
#include "tcx/assimp/Mesh.h"
#include "tcx/assimp/Node.h"
#include <TrussC.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace tcx::assimp {

class GpuSkinnedRenderer;

class Model {
public:
    Model();
    ~Model();

    // Load 3D model file
    bool load(const std::string& path);

    // Clear all data
    void clear();

    // Draw the model (all meshes, with materials)
    void draw();
    void drawFaces();
    void drawWireframe();
    void drawBoundingBox();
    void drawSkeleton();
    void update(float dt) { updateAnimation(dt); }

    // Transform
    void setPosition(const tc::Vec3& pos);
    void setPosition(float x, float y, float z);
    void setScale(float s);
    void setScale(const tc::Vec3& s);
    void setRotation(const tc::Quaternion& rot);
    void setRotationEuler(float pitch, float yaw, float roll);
    void setTransform(const tc::Mat4& m);

    const tc::Vec3& getPosition() const { return position_; }
    const tc::Vec3& getScale()    const { return scale_; }
    const tc::Quaternion& getRotation() const { return rotation_; }

    // Scale normalization
    void setScaleNormalize(bool enabled);

    // Bounding box
    BoundingBox getBoundingBox() const;
    tc::Vec3 getSceneMin() const;
    tc::Vec3 getSceneMax() const;
    tc::Vec3 getSceneCenter() const;
    tc::Vec3 getSceneSize()   const;

    // Mesh access
    size_t getMeshCount() const;
    Mesh getMesh(size_t idx);
    const Mesh getMesh(size_t idx) const;
    std::string getMeshName(size_t idx) const;

    // Node access
    size_t getNodeCount() const;
    Node getNode(size_t idx);
    const Node getNode(size_t idx) const;
    int findNode(const std::string& nodeName) const;
    size_t getBoneCount() const;
    std::string getNodeName(size_t idx) const;
    std::string getBoneName(size_t idx) const;

    // Animation (Phase 2)
    bool hasAnimations() const { return !sceneData_.animations.empty(); }
    size_t getAnimationCount() const { return sceneData_.animations.size(); }
    std::string getAnimationName(size_t i) const {
        return i < sceneData_.animations.size() ? sceneData_.animations[i].name : "";
    }
    float getAnimationDuration(size_t i) const {
        return i < sceneData_.animations.size() ? (float)sceneData_.animations[i].durationSeconds() : 0.0f;
    }
    void playAnimation(size_t idx);
    void playAnimation(const std::string& name);
    void play(size_t idx) { playAnimation(idx); }
    void play(const std::string& name) { playAnimation(name); }
    void stopAnimation();
    void stop() { stopAnimation(); }
    void pause();
    void setLoop(bool loop);
    void setAnimationSpeed(float speed);
    void setAnimationTime(float seconds);
    void setAnimationNormalizedTime(float t);
    void updateAnimation(float dt);
    tcx::assimp::Animator& animator() { return animator_; }

    // Addon-local GPU skinning. When enabled and available, drawFaces() uses a
    // dedicated skinned vertex shader and avoids per-frame CPU vertex rewrites.
    void setGpuSkinningEnabled(bool enabled);
    bool isGpuSkinningEnabled() const { return gpuSkinningEnabled_; }
    bool isGpuSkinningAvailable() const;

    // Manual bone overrides use model-space bone global matrices.
    bool setBoneGlobalTransform(size_t boneIndex, const tc::Mat4& globalTransform);
    bool setBoneGlobalTransform(const std::string& boneName, const tc::Mat4& globalTransform);
    bool clearBoneOverride(size_t boneIndex);
    bool clearBoneOverride(const std::string& boneName);
    void clearBoneOverrides();
    int findBone(const std::string& boneName) const;

    // Info
    const std::string& getFilePath() const { return filePath_; }
    bool isLoaded() const { return loaded_; }
    const SceneData& getSceneData() const { return sceneData_; }

private:
    SceneData sceneData_;
    std::string filePath_;
    bool loaded_ = false;
    bool scaleNormalize_ = false;

    tc::Vec3 position_{0,0,0};
    tc::Vec3 scale_{1,1,1};
    tc::Quaternion rotation_{0,0,0,1}; // identity

    // TrussC runtime data
    struct RuntimeMesh {
        tc::Mesh trussMesh;
        int materialIndex = -1;
        int sourceMeshIndex = -1;
        int nodeIndex = -1;
    };

    tc::Mat4 computeModelMatrix() const;
    void uploadToGPU();
    void buildRuntimeMaterials();
    const tc::Texture* resolveTextureRef(const std::string& textureRef);
    void normalizeScale();
    void resetSkinnedMeshes();
    void updateBoneMatrices(const std::vector<tc::Mat4>& nodeGlobals);
    void updateSkinningState();
    void applySkinning();
    std::vector<tc::Mat4> computeNodeGlobalTransforms(bool animated) const;
    void drawMeshInstance(RuntimeMesh& rm, bool wireframe, const std::vector<tc::Mat4>& nodeGlobals);

    std::vector<RuntimeMesh> runtimeMeshes_;
    std::vector<std::shared_ptr<tc::Texture>> textures_;
    std::vector<tc::Material> runtimeMaterials_;
    std::unordered_map<std::string, size_t> textureCache_;
    std::unordered_map<int, tc::Mat4> boneGlobalOverrides_;
    std::unique_ptr<GpuSkinnedRenderer> gpuSkinnedRenderer_;
    bool gpuSkinningEnabled_ = false;
    Animator animator_;
};

} // namespace tcx::assimp
