#include "tcx/assimp/Node.h"

#include <stdexcept>

namespace tcx::assimp {

namespace {
tc::Mat4 composeTransform(const tc::Vec3& pos, const tc::Quaternion& rot, const tc::Vec3& scale) {
    return tc::Mat4::translate(pos.x, pos.y, pos.z)
         * rot.toMatrix()
         * tc::Mat4::scale(scale.x, scale.y, scale.z);
}

const std::string kEmptyName;
}

bool Node::isValid() const {
    return scene_ && index_ >= 0 && index_ < (int)scene_->nodes.size();
}

const std::string& Node::getName() const {
    return isValid() ? scene_->nodes[index_].name : kEmptyName;
}

tc::Mat4 Node::getLocalTransform() const {
    return isValid() ? scene_->nodes[index_].localTransform : tc::Mat4::identity();
}

tc::Mat4 Node::getGlobalTransform() const {
    if (!isValid()) return tc::Mat4::identity();
    const auto& node = scene_->nodes[index_];
    if (node.parentIndex < 0) return node.localTransform;
    return Node(scene_, node.parentIndex).getGlobalTransform() * node.localTransform;
}

void Node::setLocalTransform(const tc::Mat4& transform) {
    if (!isValid()) return;
    auto& node = scene_->nodes[index_];
    node.localTransform = transform;
    node.localPosition = tc::Vec3(transform.m[3], transform.m[7], transform.m[11]);
}

void Node::setPosition(const tc::Vec3& position) {
    if (!isValid()) return;
    auto& node = scene_->nodes[index_];
    node.localPosition = position;
    node.localTransform = composeTransform(node.localPosition, node.localRotation, node.localScale);
}

void Node::setRotation(const tc::Quaternion& rotation) {
    if (!isValid()) return;
    auto& node = scene_->nodes[index_];
    node.localRotation = rotation;
    node.localTransform = composeTransform(node.localPosition, node.localRotation, node.localScale);
}

void Node::setScale(const tc::Vec3& scale) {
    if (!isValid()) return;
    auto& node = scene_->nodes[index_];
    node.localScale = scale;
    node.localTransform = composeTransform(node.localPosition, node.localRotation, node.localScale);
}

Node Node::getParent() const {
    if (!isValid()) return {};
    return Node(scene_, scene_->nodes[index_].parentIndex);
}

size_t Node::getChildCount() const {
    return isValid() ? scene_->nodes[index_].childIndices.size() : 0;
}

Node Node::getChild(size_t index) const {
    if (!isValid() || index >= scene_->nodes[index_].childIndices.size()) return {};
    return Node(scene_, scene_->nodes[index_].childIndices[index]);
}

bool Node::hasMesh() const {
    return getMeshInstanceCount() > 0;
}

size_t Node::getMeshInstanceCount() const {
    return isValid() ? scene_->nodes[index_].meshIndices.size() : 0;
}

} // namespace tcx::assimp
