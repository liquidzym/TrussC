#pragma once

#include "tcx/assimp/SceneData.h"
#include <TrussC.h>
#include <cstdint>
#include <map>
#include <vector>

namespace tcx::assimp {

// Addon-local GPU skinning path. It owns its own sokol buffers and shader
// pipeline so TrussC core does not need new vertex attributes or APIs.
class GpuSkinnedRenderer {
public:
    static constexpr int MaxBones = 128;

    GpuSkinnedRenderer() = default;
    ~GpuSkinnedRenderer();

    GpuSkinnedRenderer(const GpuSkinnedRenderer&) = delete;
    GpuSkinnedRenderer& operator=(const GpuSkinnedRenderer&) = delete;

    void clear();
    bool build(const SceneData& scene);
    bool isReady() const { return ready_; }
    bool canDrawScene(const SceneData& scene) const;

    void draw(const SceneData& scene,
              const std::vector<tc::Material>& materials,
              const std::vector<tc::Mat4>& nodeGlobals);

private:
    struct DrawItem {
        sg_buffer vertexBuffer{};
        sg_buffer indexBuffer{};
        int vertexCount = 0;
        int indexCount = 0;
        int sourceMeshIndex = -1;
        int nodeIndex = -1;
        int materialIndex = -1;
        int boneCount = 0;
    };

    sg_pipeline getPipeline(sg_pixel_format colorFormat, int sampleCount);
    void ensureShader();
    void ensureFallbacks();
    void destroyBuffers();

    std::vector<DrawItem> drawItems_;
    sg_shader shader_{};
    sg_image fallbackWhiteImage_{};
    sg_view fallbackWhiteView_{};
    sg_sampler fallbackSampler_{};
    std::map<int, sg_pipeline> pipelineCache_;
    bool shaderInitialized_ = false;
    bool fallbackInitialized_ = false;
    bool ready_ = false;
};

} // namespace tcx::assimp
