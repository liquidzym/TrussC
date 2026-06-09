#pragma once

#include "../Core/FlowPass.h"
#include "../Core/PingPongBuffer.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

struct PhysarumTrailFlowSettings {
    int particleCount = 100000;
    float lifetime = 1000.0f / 60.0f;
    float velocityScale = 1.0f;
    int renderSubsteps = 3;
    float maxParticleStep = 10.0f;
    float trailLength = 15.0f;
    float trailFadeScale = 0.78f;
    float strokeThickness = 1.15f;
    float inkStrength = 1.12f;
    float dashLength = 0.72f;
    float dashVelocityScale = 0.26f;
    tc::Color backgroundColor = tc::Color(0.985f, 0.940f, 0.870f, 1.0f);
    tc::Color inkColor = tc::Color(0.010f, 0.014f, 0.095f, 1.0f);
    bool useGpuParticles = true;
};

class PhysarumTrailFlow {
public:
    ~PhysarumTrailFlow();

    void setup(int width, int height, const PhysarumTrailFlowSettings& settings = {});
    void reset();
    void update(const Fluid2D& fluid, float dt);
    void draw(float x, float y, float w, float h) const;
    void clearTrail();
    void release();

    PhysarumTrailFlowSettings& settings() { return settings_; }
    const PhysarumTrailFlowSettings& settings() const { return settings_; }
    int particleCount() const { return gpuParticleCount_; }
    int textureSide() const { return textureSide_; }
    int lastDepositVertices() const { return lastDepositVertices_; }
    bool isReady() const { return gpuReady_; }
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }
    const tc::Texture* trailTexture() const;

    static int textureSideForParticleCount(int particleCount);
    static int strokeDepositVerticesPerParticle() { return 6; }
    static tc::Vec2 wrappedNormalizedDelta(const tc::Vec2& current, const tc::Vec2& previous);

private:
    struct ParticleVertex {
        float lookup[2];
        float corner[2];
    };

    bool canUseGpu() const;
    bool setupGpu();
    void releaseGpu();
    void fadeTrail();
    void depositParticles(const tc::Texture& velocityTexture);

    int width_ = 0;
    int height_ = 0;
    PhysarumTrailFlowSettings settings_;
    PingPongBuffer gpuState_;
    tc::Fbo trailFbo_;
    FlowPass spawnPass_;
    FlowPass ageSpawnPass_;
    FlowPass ageUpdatePass_;
    FlowPass updatePass_;
    PingPongBuffer gpuAge_;
    tc::Fbo gpuInitialState_;
    sg_buffer pointVertexBuffer_ = {};
    sg_shader pointShader_ = {};
    sg_pipeline pointPipeline_ = {};
    int textureSide_ = 0;
    int gpuParticleCount_ = 0;
    mutable int lastDepositVertices_ = 0;
    bool gpuReady_ = false;
    bool lastUpdateUsedGpu_ = false;
};

} // namespace tcx::flow
