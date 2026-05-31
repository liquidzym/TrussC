// Model.cpp — Model runtime implementation
#include "tcx/assimp/Model.h"
#include "tcx/assimp/GpuSkinnedRenderer.h"
#include "tcx/assimp/Importer.h"
#include "tcx/assimp/TextureResolver.h"
#include <cmath>
#include <algorithm>
#include <functional>

namespace tcx::assimp {

namespace {
constexpr float kNormalizedModelSize = 200.0f;

tc::Vec3 transformDirection(const tc::Mat4& m, const tc::Vec3& v) {
    tc::Vec3 out;
    out.x = m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z;
    out.y = m.m[4] * v.x + m.m[5] * v.y + m.m[6] * v.z;
    out.z = m.m[8] * v.x + m.m[9] * v.y + m.m[10] * v.z;
    float len = out.length();
    return len > 0.0001f ? out / len : v;
}

tc::Mat4 composeTransform(const tc::Vec3& pos, const tc::Quaternion& rot, const tc::Vec3& scl) {
    return tc::Mat4::translate(pos.x, pos.y, pos.z) *
           rot.toMatrix() *
           tc::Mat4::scale(scl.x, scl.y, scl.z);
}

tc::Quaternion quaternionFromRotationMatrix(const tc::Mat4& m) {
    float trace = m.m[0] + m.m[5] + m.m[10];
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        return tc::Quaternion(0.25f * s,
                              (m.m[9] - m.m[6]) / s,
                              (m.m[2] - m.m[8]) / s,
                              (m.m[4] - m.m[1]) / s).normalized();
    }
    if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
        float s = std::sqrt(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
        return tc::Quaternion((m.m[9] - m.m[6]) / s,
                              0.25f * s,
                              (m.m[1] + m.m[4]) / s,
                              (m.m[2] + m.m[8]) / s).normalized();
    }
    if (m.m[5] > m.m[10]) {
        float s = std::sqrt(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
        return tc::Quaternion((m.m[2] - m.m[8]) / s,
                              (m.m[1] + m.m[4]) / s,
                              0.25f * s,
                              (m.m[6] + m.m[9]) / s).normalized();
    }
    float s = std::sqrt(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
    return tc::Quaternion((m.m[4] - m.m[1]) / s,
                          (m.m[2] + m.m[8]) / s,
                          (m.m[6] + m.m[9]) / s,
                          0.25f * s).normalized();
}

float determinant3x3(const tc::Mat4& m) {
    return m.m[0] * (m.m[5] * m.m[10] - m.m[6] * m.m[9])
         - m.m[1] * (m.m[4] * m.m[10] - m.m[6] * m.m[8])
         + m.m[2] * (m.m[4] * m.m[9] - m.m[5] * m.m[8]);
}

bool decomposeTRS(const tc::Mat4& m, tc::Vec3& pos, tc::Quaternion& rot, tc::Vec3& scl) {
    pos = tc::Vec3(m.m[3], m.m[7], m.m[11]);

    tc::Vec3 c0(m.m[0], m.m[4], m.m[8]);
    tc::Vec3 c1(m.m[1], m.m[5], m.m[9]);
    tc::Vec3 c2(m.m[2], m.m[6], m.m[10]);
    scl = tc::Vec3(c0.length(), c1.length(), c2.length());
    if (scl.x < 0.000001f || scl.y < 0.000001f || scl.z < 0.000001f) {
        rot = tc::Quaternion::identity();
        return false;
    }

    tc::Mat4 r = tc::Mat4::identity();
    r.m[0] = m.m[0] / scl.x;  r.m[1] = m.m[1] / scl.y;  r.m[2] = m.m[2] / scl.z;
    r.m[4] = m.m[4] / scl.x;  r.m[5] = m.m[5] / scl.y;  r.m[6] = m.m[6] / scl.z;
    r.m[8] = m.m[8] / scl.x;  r.m[9] = m.m[9] / scl.y;  r.m[10] = m.m[10] / scl.z;
    if (determinant3x3(r) < 0.0f) {
        scl.z = -scl.z;
        r.m[2] = -r.m[2];
        r.m[6] = -r.m[6];
        r.m[10] = -r.m[10];
    }
    rot = quaternionFromRotationMatrix(r);
    return true;
}
} // namespace

Model::Model()
    : gpuSkinnedRenderer_(std::make_unique<GpuSkinnedRenderer>()) {
}

Model::~Model() = default;

bool Model::load(const std::string& path) {
    clear();
    Importer importer;
    if (!importer.load(path, sceneData_)) return false;

    filePath_ = path;
    loaded_ = true;

    // Reset animator to this scene's data (prevents dangling pointer)
    animator_.stop();
    animator_.setClips(&sceneData_.animations);

    if (scaleNormalize_) normalizeScale();
    uploadToGPU();
    buildRuntimeMaterials();
    if (gpuSkinningEnabled_ && gpuSkinnedRenderer_) {
        gpuSkinnedRenderer_->build(sceneData_);
    }
    return true;
}

void Model::clear() {
    animator_.stop();
    runtimeMeshes_.clear();
    textures_.clear();
    runtimeMaterials_.clear();
    textureCache_.clear();
    boneGlobalOverrides_.clear();
    if (gpuSkinnedRenderer_) gpuSkinnedRenderer_->clear();
    sceneData_ = SceneData{};
    loaded_ = false;
}

tc::Mat4 Model::computeModelMatrix() const {
    tc::Mat4 T = tc::Mat4::translate(position_.x, position_.y, position_.z);
    tc::Mat4 R = rotation_.toMatrix();
    tc::Mat4 S = tc::Mat4::scale(scale_.x, scale_.y, scale_.z);
    return T * R * S;
}

void Model::uploadToGPU() {
    runtimeMeshes_.clear();

    auto addRuntimeMesh = [&](int meshIndex, int nodeIndex) {
        if (meshIndex < 0 || meshIndex >= (int)sceneData_.meshes.size()) return;
        runtimeMeshes_.push_back(RuntimeMesh{});
        auto& src = sceneData_.meshes[meshIndex];
        auto& dst = runtimeMeshes_.back();
        dst.materialIndex = src.materialIndex;
        dst.sourceMeshIndex = meshIndex;
        dst.nodeIndex = nodeIndex;

        for (auto& v : src.vertices) {
            dst.trussMesh.addVertex(v.position);
            dst.trussMesh.addNormal(v.normal);
            dst.trussMesh.addTexCoord(v.texCoord);
            dst.trussMesh.addColor(v.color);
            dst.trussMesh.addTangent(v.tangent);
        }
        for (auto idx : src.indices) {
            dst.trussMesh.addIndex(idx);
        }
    };

    for (size_t ni = 0; ni < sceneData_.nodes.size(); ni++) {
        for (int meshIndex : sceneData_.nodes[ni].meshIndices) {
            addRuntimeMesh(meshIndex, (int)ni);
        }
    }

    if (runtimeMeshes_.empty()) {
        for (size_t i = 0; i < sceneData_.meshes.size(); i++) {
            addRuntimeMesh((int)i, sceneData_.meshes[i].nodeIndex);
        }
    }
}

const tc::Texture* Model::resolveTextureRef(const std::string& textureRef) {
    if (textureRef.empty()) return nullptr;

    std::string key = TextureResolver::normalizeRef(textureRef);
    auto cached = textureCache_.find(key);
    if (cached != textureCache_.end() && cached->second < textures_.size()) {
        return textures_[cached->second].get();
    }

    tc::Pixels pixels;
    bool loaded = TextureResolver::load(sceneData_, filePath_, key, pixels);

    if (!loaded || !pixels.isAllocated()) return nullptr;

    auto texture = std::make_shared<tc::Texture>();
    texture->allocate(pixels, tc::TextureUsage::Immutable, true);
    if (!texture->isAllocated()) return nullptr;

    textures_.push_back(texture);
    size_t index = textures_.size() - 1;
    textureCache_[key] = index;
    return textures_[index].get();
}

void Model::buildRuntimeMaterials() {
    runtimeMaterials_.clear();
    runtimeMaterials_.reserve(sceneData_.materials.size());

    for (auto& src : sceneData_.materials) {
        tc::Material mat;
        tc::Color base = src.baseColor;
        if (base.a == 1.0f && src.opacity != 1.0f) {
            base.a = src.opacity;
        }
        if (base.a == 1.0f && src.diffuseColor.a != 1.0f) {
            base.a = src.diffuseColor.a;
        }
        mat.setBaseColor(base);
        mat.setMetallic(src.metallic);
        mat.setRoughness(src.roughness);
        mat.setEmissive(src.emissiveColor.r, src.emissiveColor.g, src.emissiveColor.b);
        if (src.emissiveColor.r + src.emissiveColor.g + src.emissiveColor.b > 0.001f) {
            mat.setEmissiveStrength(1.0f);
        }

        if (const tc::Texture* tex = resolveTextureRef(src.diffuseTexture)) {
            mat.setBaseColorTexture(tex);
        }
        if (const tc::Texture* tex = resolveTextureRef(src.normalTexture)) {
            mat.setNormalMap(tex);
        }
        if (const tc::Texture* tex = resolveTextureRef(src.metallicRoughnessTexture)) {
            mat.setMetallicRoughnessTexture(tex);
        }
        if (const tc::Texture* tex = resolveTextureRef(src.emissiveTexture)) {
            mat.setEmissiveTexture(tex);
        }
        if (const tc::Texture* tex = resolveTextureRef(src.occlusionTexture)) {
            mat.setOcclusionTexture(tex);
        }

        runtimeMaterials_.push_back(mat);
    }
}

void Model::resetSkinnedMeshes() {
    for (auto& rm : runtimeMeshes_) {
        if (rm.sourceMeshIndex < 0 || rm.sourceMeshIndex >= (int)sceneData_.meshes.size()) continue;
        auto& src = sceneData_.meshes[rm.sourceMeshIndex];
        auto& verts = rm.trussMesh.getVertices();
        auto& normals = rm.trussMesh.getNormals();
        for (size_t vi = 0; vi < src.vertices.size() && vi < verts.size(); vi++) {
            verts[vi] = src.vertices[vi].position;
            if (vi < normals.size()) normals[vi] = src.vertices[vi].normal;
        }
        rm.trussMesh.markGpuDirty();
    }
}

void Model::normalizeScale() {
    if (sceneData_.meshes.empty()) return;
    tc::Vec3 size = sceneData_.globalBounds.size();
    float maxDim = std::max({size.x, size.y, size.z});
    if (maxDim < 0.0001f) return;
    float s = kNormalizedModelSize / maxDim;
    tc::Vec3 center = sceneData_.globalBounds.center();
    scale_ = tc::Vec3(s, s, s);
    position_ = center * -s;
}

void Model::draw() { drawFaces(); }

void Model::drawFaces() {
    if (!loaded_) return;
    auto nodeGlobals = computeNodeGlobalTransforms(animator_.hasActiveClip());
    if (gpuSkinningEnabled_ && gpuSkinnedRenderer_ && !gpuSkinnedRenderer_->isReady()) {
        gpuSkinnedRenderer_->build(sceneData_);
    }
    if (gpuSkinningEnabled_ && isGpuSkinningAvailable()) {
        updateBoneMatrices(nodeGlobals);
        tc::pushMatrix();
        tc::setMatrix(computeModelMatrix());
        gpuSkinnedRenderer_->draw(sceneData_, runtimeMaterials_, nodeGlobals);
        tc::popMatrix();
        return;
    }

    tc::pushMatrix();
    tc::setMatrix(computeModelMatrix());
    for (auto& rm : runtimeMeshes_) {
        drawMeshInstance(rm, false, nodeGlobals);
    }
    tc::popMatrix();
}

void Model::drawWireframe() {
    if (!loaded_) return;
    auto nodeGlobals = computeNodeGlobalTransforms(animator_.hasActiveClip());
    tc::clearMaterial();
    tc::clearLights();
    tc::setColor(0.0f, 0.0f, 0.0f);
    tc::pushMatrix();
    tc::setMatrix(computeModelMatrix());
    for (auto& rm : runtimeMeshes_) {
        drawMeshInstance(rm, true, nodeGlobals);
    }
    tc::popMatrix();
}

void Model::drawBoundingBox() {
    if (!loaded_) return;
    tc::pushMatrix();
    tc::setMatrix(computeModelMatrix());
    tc::noFill();
    tc::setColor(1.0f, 1.0f, 0.0f, 0.5f);
    auto& bb = sceneData_.globalBounds;
    tc::Vec3 c = (bb.min + bb.max) * 0.5f;
    tc::Vec3 s = bb.size();
    tc::pushMatrix();
    tc::translate(c.x, c.y, c.z);
    tc::drawBox(s.x, s.y, s.z);
    tc::popMatrix();
    tc::fill();
    tc::popMatrix();
}

void Model::setPosition(const tc::Vec3& p) { position_ = p; }
void Model::setPosition(float x, float y, float z) { position_ = tc::Vec3(x, y, z); }
void Model::setScale(float s) { scale_ = tc::Vec3(s, s, s); }
void Model::setScale(const tc::Vec3& s) { scale_ = s; }
void Model::setRotation(const tc::Quaternion& q) { rotation_ = q; }
void Model::setRotationEuler(float pitch, float yaw, float roll) {
    rotation_ = tc::Quaternion::fromEuler(pitch, yaw, roll);
}
void Model::setTransform(const tc::Mat4& m) {
    tc::Vec3 pos;
    tc::Quaternion rot;
    tc::Vec3 scl;
    decomposeTRS(m, pos, rot, scl);
    position_ = pos;
    rotation_ = rot;
    scale_ = scl;
}
void Model::setScaleNormalize(bool v) {
    scaleNormalize_ = v;
    if (loaded_ && scaleNormalize_) normalizeScale();
}

void Model::setGpuSkinningEnabled(bool enabled) {
    gpuSkinningEnabled_ = enabled;
    if (!gpuSkinnedRenderer_) {
        gpuSkinnedRenderer_ = std::make_unique<GpuSkinnedRenderer>();
    }
    if (loaded_ && !gpuSkinnedRenderer_->isReady()) {
        gpuSkinnedRenderer_->build(sceneData_);
    }
    if (enabled) {
        updateSkinningState();
    } else if (loaded_) {
        applySkinning();
    }
}

bool Model::isGpuSkinningAvailable() const {
    return gpuSkinnedRenderer_ && gpuSkinnedRenderer_->canDrawScene(sceneData_);
}

// =========================================================================
// Animation
// =========================================================================

void Model::playAnimation(size_t idx) {
    animator_.setClips(&sceneData_.animations);
    animator_.play(idx);
    updateSkinningState();
}

void Model::playAnimation(const std::string& name) {
    animator_.setClips(&sceneData_.animations);
    animator_.play(name);
    updateSkinningState();
}

void Model::stopAnimation() {
    animator_.stop();
    if (boneGlobalOverrides_.empty()) {
        if (!gpuSkinningEnabled_) resetSkinnedMeshes();
        updateSkinningState();
    } else {
        updateSkinningState();
    }
}

void Model::pause() { animator_.pause(); }
void Model::setLoop(bool loop) { animator_.setLoop(loop); }
void Model::setAnimationSpeed(float speed) { animator_.setSpeed(speed); }
void Model::setAnimationTime(float seconds) {
    animator_.setTime(seconds);
    updateSkinningState();
}
void Model::setAnimationNormalizedTime(float t) {
    animator_.setNormalizedTime(t);
    updateSkinningState();
}

void Model::updateAnimation(float dt) {
    if (!animator_.isAdvancing()) return;
    animator_.update(dt);
    updateSkinningState();
}

std::vector<tc::Mat4> Model::computeNodeGlobalTransforms(bool animated) const {
    std::vector<tc::Mat4> globals(sceneData_.nodes.size(), tc::Mat4::identity());

    std::function<void(int)> visit = [&](int idx) {
        if (idx < 0 || idx >= (int)sceneData_.nodes.size()) return;
        const auto& node = sceneData_.nodes[idx];
        tc::Mat4 local = node.localTransform;

        if (animated) {
            tc::Vec3 pos = node.localPosition;
            tc::Quaternion rot = node.localRotation;
            tc::Vec3 scl = node.localScale;
            if (animator_.getNodeTransform(node.name,
                                           node.localPosition,
                                           node.localRotation,
                                           node.localScale,
                                           pos,
                                           rot,
                                           scl)) {
                local = composeTransform(pos, rot, scl);
            }
        }

        if (node.parentIndex >= 0 && node.parentIndex < (int)globals.size()) {
            globals[idx] = globals[node.parentIndex] * local;
        } else {
            globals[idx] = local;
        }

        for (int child : node.childIndices) visit(child);
    };

    if (sceneData_.rootNodeIndex >= 0) {
        visit(sceneData_.rootNodeIndex);
    } else {
        for (int i = 0; i < (int)sceneData_.nodes.size(); i++) {
            if (sceneData_.nodes[i].parentIndex < 0) visit(i);
        }
    }

    return globals;
}

void Model::updateBoneMatrices(const std::vector<tc::Mat4>& nodeGlobals) {
    if (sceneData_.skeleton.bones.empty()) return;
    for (auto& bone : sceneData_.skeleton.bones) {
        int boneIndex = (int)(&bone - sceneData_.skeleton.bones.data());
        auto overrideIt = boneGlobalOverrides_.find(boneIndex);
        if (overrideIt != boneGlobalOverrides_.end()) {
            bone.finalMatrix = overrideIt->second * bone.offsetMatrix;
            continue;
        }
        if (bone.nodeIndex >= 0 && bone.nodeIndex < (int)nodeGlobals.size()) {
            bone.finalMatrix = nodeGlobals[bone.nodeIndex] * bone.offsetMatrix;
        } else {
            bone.finalMatrix = bone.offsetMatrix;
        }
    }
}

void Model::updateSkinningState() {
    if (sceneData_.skeleton.bones.empty()) return;
    if (gpuSkinningEnabled_ && isGpuSkinningAvailable()) {
        auto nodeGlobals = computeNodeGlobalTransforms(animator_.hasActiveClip());
        updateBoneMatrices(nodeGlobals);
    } else {
        applySkinning();
    }
}

void Model::applySkinning() {
    if (sceneData_.skeleton.bones.empty()) return;
    auto nodeGlobals = computeNodeGlobalTransforms(animator_.hasActiveClip());
    updateBoneMatrices(nodeGlobals);

    for (auto& rm : runtimeMeshes_) {
        if (rm.sourceMeshIndex < 0 || rm.sourceMeshIndex >= (int)sceneData_.meshes.size()) continue;
        auto& mesh = sceneData_.meshes[rm.sourceMeshIndex];
        if (mesh.boneData.empty()) continue;
        auto& verts = rm.trussMesh.getVertices();
        auto& normals = rm.trussMesh.getNormals();

        tc::Mat4 meshNodeInv = tc::Mat4::identity();
        if (rm.nodeIndex >= 0 && rm.nodeIndex < (int)nodeGlobals.size()) {
            meshNodeInv = nodeGlobals[rm.nodeIndex].inverted();
        }

        std::vector<tc::Mat4> boneMatrices(sceneData_.skeleton.bones.size(), tc::Mat4::identity());
        for (size_t bi = 0; bi < sceneData_.skeleton.bones.size(); ++bi) {
            boneMatrices[bi] = meshNodeInv * sceneData_.skeleton.bones[bi].finalMatrix;
        }

        for (size_t vi = 0; vi < mesh.vertices.size() && vi < verts.size(); vi++) {
            auto& bd = mesh.boneData[vi];
            tc::Vec3 pos(0,0,0);
            tc::Vec3 normal(0,0,0);
            float totalWeight = 0.0f;
            for (int s = 0; s < 4 && bd.weights[s] > 0.0f; s++) {
                int bi = bd.indices[s];
                if (bi >= 0 && bi < (int)boneMatrices.size()) {
                    const tc::Mat4& bm = boneMatrices[bi];
                    tc::Vec4 tp = bm * tc::Vec4(mesh.vertices[vi].position, 1.0f);
                    pos.x += tp.x * bd.weights[s];
                    pos.y += tp.y * bd.weights[s];
                    pos.z += tp.z * bd.weights[s];
                    normal += transformDirection(bm, mesh.vertices[vi].normal) * bd.weights[s];
                    totalWeight += bd.weights[s];
                }
            }

            if (totalWeight > 0.0001f) {
                verts[vi] = pos / totalWeight;
                if (vi < normals.size()) normals[vi] = normal.normalized();
            } else {
                verts[vi] = mesh.vertices[vi].position;
                if (vi < normals.size()) normals[vi] = mesh.vertices[vi].normal;
            }
        }
        rm.trussMesh.markGpuDirty();
    }
}

void Model::drawMeshInstance(RuntimeMesh& rm, bool wireframe, const std::vector<tc::Mat4>& nodeGlobals) {
    tc::pushMatrix();
    if (rm.nodeIndex >= 0 && rm.nodeIndex < (int)nodeGlobals.size()) {
        tc::setMatrix(nodeGlobals[rm.nodeIndex]);
    }

    if (wireframe) {
        rm.trussMesh.drawWireframe();
    } else {
        if (rm.materialIndex >= 0 && rm.materialIndex < (int)runtimeMaterials_.size()) {
            tc::setMaterial(runtimeMaterials_[rm.materialIndex]);
            tc::setColor(runtimeMaterials_[rm.materialIndex].getBaseColor());
        } else if (rm.materialIndex >= 0 && rm.materialIndex < (int)sceneData_.materials.size()) {
            tc::setColor(sceneData_.materials[rm.materialIndex].diffuseColor);
        } else {
            tc::setColor(0.7f, 0.7f, 0.7f);
        }
        rm.trussMesh.draw();
    }
    tc::popMatrix();
}

// =========================================================================
// Skeleton debug draw
// =========================================================================

void Model::drawSkeleton() {
    if (sceneData_.skeleton.bones.empty()) return;
    tc::clearMaterial();
    tc::clearLights();
    tc::setColor(0.0f, 1.0f, 0.0f);
    tc::pushMatrix();
    tc::setMatrix(computeModelMatrix());
    auto nodeGlobals = computeNodeGlobalTransforms(animator_.hasActiveClip());
    for (auto& bone : sceneData_.skeleton.bones) {
        if (bone.nodeIndex < 0 || bone.nodeIndex >= (int)sceneData_.nodes.size()) continue;
        auto& node = sceneData_.nodes[bone.nodeIndex];
        // Bone position from matrix translation
        tc::Mat4 boneGlobal = nodeGlobals[bone.nodeIndex];
        auto overrideIt = boneGlobalOverrides_.find((int)(&bone - sceneData_.skeleton.bones.data()));
        if (overrideIt != boneGlobalOverrides_.end()) {
            boneGlobal = overrideIt->second;
        }
        tc::Vec3 pos(boneGlobal.m[3], boneGlobal.m[7], boneGlobal.m[11]);
        // Draw bone point and line to parent
        tc::drawPoint(pos);
        if (node.parentIndex >= 0) {
            const auto& parentGlobal = nodeGlobals[node.parentIndex];
            tc::Vec3 ppos(parentGlobal.m[3], parentGlobal.m[7], parentGlobal.m[11]);
            tc::drawLine(ppos, pos);
        }
    }
    tc::popMatrix();
}

BoundingBox Model::getBoundingBox() const { return sceneData_.globalBounds; }
tc::Vec3 Model::getSceneMin() const { return sceneData_.globalBounds.min; }
tc::Vec3 Model::getSceneMax() const { return sceneData_.globalBounds.max; }
tc::Vec3 Model::getSceneCenter() const { return sceneData_.globalBounds.center(); }
tc::Vec3 Model::getSceneSize()   const { return sceneData_.globalBounds.size(); }
size_t Model::getMeshCount() const { return sceneData_.meshes.size(); }
Mesh Model::getMesh(size_t i) {
    return i < sceneData_.meshes.size() ? Mesh(&sceneData_, (int)i) : Mesh();
}
const Mesh Model::getMesh(size_t i) const {
    return i < sceneData_.meshes.size() ? Mesh(const_cast<SceneData*>(&sceneData_), (int)i) : Mesh();
}
std::string Model::getMeshName(size_t i) const {
    return i < sceneData_.meshes.size() ? sceneData_.meshes[i].name : "";
}
size_t Model::getNodeCount() const { return sceneData_.nodes.size(); }
Node Model::getNode(size_t i) {
    return i < sceneData_.nodes.size() ? Node(&sceneData_, (int)i) : Node();
}
const Node Model::getNode(size_t i) const {
    return i < sceneData_.nodes.size() ? Node(const_cast<SceneData*>(&sceneData_), (int)i) : Node();
}
int Model::findNode(const std::string& nodeName) const {
    for (size_t i = 0; i < sceneData_.nodes.size(); ++i) {
        if (sceneData_.nodes[i].name == nodeName) return (int)i;
    }
    return -1;
}
size_t Model::getBoneCount() const { return sceneData_.skeleton.bones.size(); }
std::string Model::getNodeName(size_t i) const {
    return i < sceneData_.nodes.size() ? sceneData_.nodes[i].name : "";
}
std::string Model::getBoneName(size_t i) const {
    return i < sceneData_.skeleton.bones.size() ? sceneData_.skeleton.bones[i].name : "";
}

int Model::findBone(const std::string& boneName) const {
    return sceneData_.skeleton.findBone(boneName);
}

bool Model::setBoneGlobalTransform(size_t boneIndex, const tc::Mat4& globalTransform) {
    if (boneIndex >= sceneData_.skeleton.bones.size()) return false;
    boneGlobalOverrides_[(int)boneIndex] = globalTransform;
    updateSkinningState();
    return true;
}

bool Model::setBoneGlobalTransform(const std::string& boneName, const tc::Mat4& globalTransform) {
    int idx = findBone(boneName);
    if (idx < 0) return false;
    return setBoneGlobalTransform((size_t)idx, globalTransform);
}

bool Model::clearBoneOverride(size_t boneIndex) {
    if (boneIndex >= sceneData_.skeleton.bones.size()) return false;
    bool removed = boneGlobalOverrides_.erase((int)boneIndex) > 0;
    if (removed) {
        if (animator_.hasActiveClip() || !boneGlobalOverrides_.empty()) {
            updateSkinningState();
        } else {
            if (!gpuSkinningEnabled_) resetSkinnedMeshes();
            updateSkinningState();
        }
    }
    return removed;
}

bool Model::clearBoneOverride(const std::string& boneName) {
    int idx = findBone(boneName);
    if (idx < 0) return false;
    return clearBoneOverride((size_t)idx);
}

void Model::clearBoneOverrides() {
    if (boneGlobalOverrides_.empty()) return;
    boneGlobalOverrides_.clear();
    if (animator_.hasActiveClip()) {
        updateSkinningState();
    } else {
        if (!gpuSkinningEnabled_) resetSkinnedMeshes();
        updateSkinningState();
    }
}

} // namespace tcx::assimp
