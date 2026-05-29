#include "tcx/assimp/Mesh.h"

#include <stdexcept>

namespace tcx::assimp {

namespace {
const std::string kEmptyName;
}

bool Mesh::isValid() const {
    return scene_ && index_ >= 0 && index_ < (int)scene_->meshes.size();
}

const std::string& Mesh::getName() const {
    return isValid() ? scene_->meshes[index_].name : kEmptyName;
}

const MeshData& Mesh::data() const {
    if (!isValid()) throw std::out_of_range("tcx::assimp::Mesh is invalid");
    return scene_->meshes[index_];
}

MeshData& Mesh::data() {
    if (!isValid()) throw std::out_of_range("tcx::assimp::Mesh is invalid");
    return scene_->meshes[index_];
}

BoundingBox Mesh::getBoundingBox() const {
    return isValid() ? scene_->meshes[index_].bounds : BoundingBox{};
}

int Mesh::getMaterialIndex() const {
    return isValid() ? scene_->meshes[index_].materialIndex : -1;
}

size_t Mesh::getVertexCount() const {
    return isValid() ? scene_->meshes[index_].vertices.size() : 0;
}

size_t Mesh::getIndexCount() const {
    return isValid() ? scene_->meshes[index_].indices.size() : 0;
}

bool Mesh::hasSkinning() const {
    return isValid() && !scene_->meshes[index_].boneData.empty();
}

} // namespace tcx::assimp
