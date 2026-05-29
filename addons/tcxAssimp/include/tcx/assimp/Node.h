#pragma once

#include "tcx/assimp/SceneData.h"
#include <string>

namespace tcx::assimp {

class Node {
public:
    Node() = default;
    Node(SceneData* scene, int index) : scene_(scene), index_(index) {}

    bool isValid() const;
    int getIndex() const { return index_; }
    const std::string& getName() const;

    tc::Mat4 getLocalTransform() const;
    tc::Mat4 getGlobalTransform() const;

    void setLocalTransform(const tc::Mat4& transform);
    void setPosition(const tc::Vec3& position);
    void setRotation(const tc::Quaternion& rotation);
    void setScale(const tc::Vec3& scale);

    Node getParent() const;
    size_t getChildCount() const;
    Node getChild(size_t index) const;

    bool hasMesh() const;
    size_t getMeshInstanceCount() const;

private:
    SceneData* scene_ = nullptr;
    int index_ = -1;
};

} // namespace tcx::assimp
