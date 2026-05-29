#pragma once

#include "tcx/assimp/SceneData.h"
#include <string>

namespace tcx::assimp {

class Mesh {
public:
    Mesh() = default;
    Mesh(SceneData* scene, int index) : scene_(scene), index_(index) {}

    bool isValid() const;
    int getIndex() const { return index_; }
    const std::string& getName() const;

    const MeshData& data() const;
    MeshData& data();

    BoundingBox getBoundingBox() const;
    int getMaterialIndex() const;
    size_t getVertexCount() const;
    size_t getIndexCount() const;
    bool hasSkinning() const;

private:
    SceneData* scene_ = nullptr;
    int index_ = -1;
};

} // namespace tcx::assimp
