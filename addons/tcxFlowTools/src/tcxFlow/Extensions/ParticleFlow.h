#pragma once

#include "../Core/FlowPass.h"
#include "../Core/PingPongBuffer.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

enum class ParticleFlowVariant {
    Flow = 0,
    Attractor = 1,
    Impulse = 2
};

struct ParticleFlowSettings {
    int particleCount = 65536;
    float lifetime = 5.0f;
    float velocityScale = 1.0f;
    float damping = 0.995f;
    float spawnRadius = 1.0f;
    float variantStrength = 0.0f;
    float particleSize = 1.4f;
    tc::Vec2 variantCenter = tc::Vec2(0.5f, 0.5f);
    tc::Color particleColor = tc::Color(1.0f, 1.0f, 1.0f, 0.65f);
    ParticleFlowVariant variant = ParticleFlowVariant::Flow;
    bool respawn = true;
    bool useGpuParticles = true;
};

class ParticleFlow {
public:
    ~ParticleFlow();

    void setup(int width, int height, const ParticleFlowSettings& settings = {});
    void reset();
    void update(const Fluid2D& fluid, float dt);
    void draw(float x, float y, float w, float h) const;

    ParticleFlowSettings& settings() { return settings_; }
    const ParticleFlowSettings& settings() const { return settings_; }
    int particleCount() const { return gpuParticleCount_ > 0 ? gpuParticleCount_ : static_cast<int>(positions_.size()); }
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }

private:
    bool canUseGpu() const;
    bool setupGpu();
    void releaseGpu();
    void updateGpu(const Fluid2D& fluid, float dt);
    void drawGpu(float x, float y, float w, float h) const;

    int width_ = 0;
    int height_ = 0;
    ParticleFlowSettings settings_;
    std::vector<tc::Vec2> positions_;
    std::vector<tc::Vec2> velocities_;
    std::vector<float> ages_;
    int gpuSide_ = 0;
    int gpuParticleCount_ = 0;
    bool gpuReady_ = false;
    bool lastUpdateUsedGpu_ = false;
    PingPongBuffer gpuState_;
    tc::Texture gpuSeedTexture_;
    sg_buffer gpuPointVertexBuffer_ = {};
    sg_shader gpuPointShader_ = {};
    sg_pipeline gpuPointPipeline_ = {};
    FlowPass gpuSpawnPass_;
    FlowPass gpuUpdatePass_;
};

} // namespace tcx::flow
