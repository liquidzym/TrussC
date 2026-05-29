#pragma once
// Importer.h — Assimp-based scene importer
#include "tcx/assimp/SceneData.h"
#include "tcx/assimp/Animation.h"
#include <string>
#include <memory>

namespace tcx::assimp {

class Importer {
public:
    Importer();
    ~Importer();

    Importer(const Importer&) = delete;
    Importer& operator=(const Importer&) = delete;

    bool load(const std::string& path, SceneData& outScene);
    const std::string& getError() const { return error_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string error_;
};

} // namespace tcx::assimp
