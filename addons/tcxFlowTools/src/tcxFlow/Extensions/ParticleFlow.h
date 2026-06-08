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
    float lifespanSpread = 0.0f;
    float mass = 1.0f;
    float massSpread = 0.0f;
    float sizeSpread = 0.0f;
    float birthVelocityScale = 1.0f;
    float birthVelocityJitter = 0.0f;
    float ageFadePower = 1.0f;
    tc::Vec2 variantCenter = tc::Vec2(0.5f, 0.5f);
    tc::Color particleColor = tc::Color(1.0f, 1.0f, 1.0f, 0.65f);
    ParticleFlowVariant variant = ParticleFlowVariant::Flow;
    bool respawn = true;
    bool useGpuParticles = true;
    bool birthFromVelocity = false;
    bool resetAgeOnBirth = true;
};

class ParticleFlow {
public:
    ~ParticleFlow();

    void setup(int width, int height, const ParticleFlowSettings& settings = {});
    void reset();
    void spawn(const tc::Vec2& position, float radius, int amount);
    void update(const Fluid2D& fluid, float dt);
    void draw(float x, float y, float w, float h) const;

    ParticleFlowSettings& settings() { return settings_; }
    const ParticleFlowSettings& settings() const { return settings_; }
    int particleCount() const { return gpuParticleCount_ > 0 ? gpuParticleCount_ : static_cast<int>(positions_.size()); }
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }

private:
    struct SpawnRequest {
        tc::Vec2 position;
        float radius = 1.0f;
        int amount = 0;
    };

    struct NormalizedSpawn {
        tc::Vec2 center;
        float radius = 0.0f;
        float probability = 0.0f;
        bool active = false;
    };

    bool canUseGpu() const;
    bool setupGpu();
    void releaseGpu();
    NormalizedSpawn normalizeSpawn(const SpawnRequest& request) const;
    tc::Vec2 birthVelocityForPosition(const Fluid2D& fluid, const tc::Vec2& position, int index) const;
    void applyCpuSpawns(const Fluid2D& fluid);
    void applyGpuSpawn(const NormalizedSpawn& spawn, const tc::Texture& velocityTexture);
    void updateGpu(const Fluid2D& fluid, float dt);
    void drawGpu(float x, float y, float w, float h) const;

    int width_ = 0;
    int height_ = 0;
    ParticleFlowSettings settings_;
    std::vector<tc::Vec2> positions_;
    std::vector<tc::Vec2> velocities_;
    std::vector<float> ages_;
    std::vector<float> masses_;
    std::vector<SpawnRequest> pendingSpawns_;
    int gpuSide_ = 0;
    int gpuParticleCount_ = 0;
    int cpuSpawnCursor_ = 0;
    bool gpuReady_ = false;
    bool lastUpdateUsedGpu_ = false;
    PingPongBuffer gpuState_;
    sg_buffer gpuPointVertexBuffer_ = {};
    sg_shader gpuPointShader_ = {};
    sg_pipeline gpuPointPipeline_ = {};
    FlowPass gpuSpawnPass_;
    FlowPass gpuUpdatePass_;
};

} // namespace tcx::flow
