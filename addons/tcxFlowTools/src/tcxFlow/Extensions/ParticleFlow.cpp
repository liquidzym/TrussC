#include "ParticleFlow.h"
#include "particles/particles.glsl.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tcx::flow {

namespace {

struct ParticlePointVertex {
    float lookup[2];
    float corner[2];
};

int variantCode(ParticleFlowVariant variant) {
    switch (variant) {
        case ParticleFlowVariant::Attractor: return 1;
        case ParticleFlowVariant::Impulse: return 2;
        case ParticleFlowVariant::Flow:
        default: return 0;
    }
}

float hash01(int value) {
    unsigned int x = static_cast<unsigned int>(value) * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return static_cast<float>(x & 0xffffu) / 65535.0f;
}

float spreadValue(float base, float spread, float randomValue, float minimum) {
    const float amount = std::max(0.0f, spread);
    const float scale = 1.0f + (randomValue * 2.0f - 1.0f) * amount;
    return std::max(minimum, base * scale);
}

float particleMassForIndex(const ParticleFlowSettings& settings, int index) {
    return spreadValue(std::max(0.05f, settings.mass), settings.massSpread, hash01(index * 97 + 13), 0.05f);
}

float particleLifespanForIndex(const ParticleFlowSettings& settings, int index) {
    return spreadValue(std::max(0.0001f, settings.lifetime), settings.lifespanSpread, hash01(index * 131 + 29), 0.0001f);
}

float particleSizeForIndex(const ParticleFlowSettings& settings, int index, float mass) {
    const float size = spreadValue(std::max(0.05f, settings.particleSize), settings.sizeSpread, hash01(index * 173 + 41), 0.05f);
    return size * std::sqrt(std::max(0.05f, mass));
}

} // namespace

ParticleFlow::~ParticleFlow() {
    releaseGpu();
}

void ParticleFlow::setup(int width, int height, const ParticleFlowSettings& settings) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    settings_ = settings;
    reset();
}

void ParticleFlow::reset() {
    releaseGpu();
    pendingSpawns_.clear();
    cpuSpawnCursor_ = 0;
    gpuReady_ = setupGpu();
    if (gpuReady_) {
        positions_.clear();
        velocities_.clear();
        ages_.clear();
        masses_.clear();
        return;
    }

    const int count = std::max(0, settings_.particleCount);
    positions_.resize(static_cast<std::size_t>(count));
    velocities_.assign(static_cast<std::size_t>(count), tc::Vec2(0, 0));
    ages_.assign(static_cast<std::size_t>(count), 0.0f);
    masses_.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float u = static_cast<float>((i * 1103515245u + 12345u) & 0xffffu) / 65535.0f;
        const float v = static_cast<float>((i * 214013u + 2531011u) & 0xffffu) / 65535.0f;
        positions_[static_cast<std::size_t>(i)] = tc::Vec2(u * width_, v * height_);
        masses_[static_cast<std::size_t>(i)] = particleMassForIndex(settings_, i);
    }
}

void ParticleFlow::spawn(const tc::Vec2& position, float radius, int amount) {
    if (amount <= 0) return;
    SpawnRequest request;
    request.position = position;
    request.radius = std::max(1.0f, radius);
    request.amount = amount;
    pendingSpawns_.push_back(request);
}

void ParticleFlow::update(const Fluid2D& fluid, float dt) {
    lastUpdateUsedGpu_ = false;
    if (settings_.useGpuParticles && canUseGpu()) {
        if (!gpuReady_) gpuReady_ = setupGpu();
        if (gpuReady_ && fluid.getVelocityTexture()) {
            updateGpu(fluid, dt);
            lastUpdateUsedGpu_ = true;
            return;
        }
    }

    const float damping = std::clamp(settings_.damping, 0.0f, 1.0f);
    const tc::Vec2 variantCenter(settings_.variantCenter.x * width_, settings_.variantCenter.y * height_);
    applyCpuSpawns(fluid);
    for (std::size_t i = 0; i < positions_.size(); ++i) {
        ages_[i] += dt;
        const int index = static_cast<int>(i);
        const float mass = i < masses_.size() ? masses_[i] : particleMassForIndex(settings_, index);
        const float invMass = 1.0f / std::max(0.05f, mass);
        velocities_[i] += fluid.sampleVelocityAtPosition(positions_[i]) * (dt * invMass);
        if (settings_.variant == ParticleFlowVariant::Attractor) {
            const tc::Vec2 toward = variantCenter - positions_[i];
            const float dist = std::max(1.0f, std::sqrt(toward.x * toward.x + toward.y * toward.y));
            velocities_[i] += toward * (settings_.variantStrength * dt * invMass / dist);
        } else if (settings_.variant == ParticleFlowVariant::Impulse) {
            const tc::Vec2 away = positions_[i] - variantCenter;
            const float dist = std::max(1.0f, std::sqrt(away.x * away.x + away.y * away.y));
            velocities_[i] += away * (settings_.variantStrength * dt * invMass / dist);
        }
        velocities_[i] *= damping;
        positions_[i] += velocities_[i] * (dt * settings_.velocityScale);
        if (settings_.respawn && (ages_[i] > particleLifespanForIndex(settings_, index) || positions_[i].x < 0 || positions_[i].y < 0 ||
                                  positions_[i].x > width_ || positions_[i].y > height_)) {
            if (settings_.resetAgeOnBirth) ages_[i] = 0.0f;
            const float a = static_cast<float>(i) * 2.3999632f;
            positions_[i] = tc::Vec2(width_ * 0.5f + std::cos(a) * settings_.spawnRadius,
                                     height_ * 0.5f + std::sin(a) * settings_.spawnRadius);
            velocities_[i] = birthVelocityForPosition(fluid, positions_[i], index);
            if (i < masses_.size()) masses_[i] = particleMassForIndex(settings_, index + cpuSpawnCursor_);
        }
    }
}

void ParticleFlow::draw(float x, float y, float w, float h) const {
    if (lastUpdateUsedGpu_ && gpuReady_) {
        drawGpu(x, y, w, h);
        return;
    }
    if (width_ <= 0 || height_ <= 0) return;
    const float sx = w / width_;
    const float sy = h / height_;
    const std::size_t maxDraw = std::min<std::size_t>(positions_.size(), 4096);
    for (std::size_t i = 0; i < maxDraw; ++i) {
        const int index = static_cast<int>(i);
        const float lifespan = particleLifespanForIndex(settings_, index);
        const float normalizedAge = std::clamp(ages_[i] / std::max(0.0001f, lifespan), 0.0f, 1.0f);
        const float fade = std::pow(1.0f - normalizedAge, std::max(0.0001f, settings_.ageFadePower));
        const float mass = i < masses_.size() ? masses_[i] : particleMassForIndex(settings_, index);
        tc::setColor(tc::Color(settings_.particleColor.r, settings_.particleColor.g, settings_.particleColor.b,
                               settings_.particleColor.a * fade));
        tc::drawCircle(x + positions_[i].x * sx, y + positions_[i].y * sy,
                       particleSizeForIndex(settings_, index, mass));
    }
}

bool ParticleFlow::canUseGpu() const {
    return settings_.useGpuParticles && sg_isvalid() && !tc::headless::isActive();
}

bool ParticleFlow::setupGpu() {
    if (!canUseGpu()) return false;
    gpuSide_ = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(std::max(1, settings_.particleCount))))));
    gpuParticleCount_ = gpuSide_ * gpuSide_;
    gpuState_.allocate(gpuSide_, gpuSide_, TextureFormat::RGBA32F, "particles-state");
    gpuState_.clear();

    gpuSpawnPass_.setup(FlowPassKind::ParticlesSpawn);
    gpuUpdatePass_.setup(FlowPassKind::ParticlesUpdate);
    gpuSpawnPass_.setColor(tc::Color(1.0f, 1.0f, 1.0f, std::max(0.05f, settings_.mass)));
    gpuSpawnPass_.setTexelExtra(std::max(0.0f, settings_.massSpread), std::max(0.0f, settings_.sizeSpread));
    gpuSpawnPass_.setOptions(0.37f, 0.61f, 0.19f, 0.83f);
    gpuSpawnPass_.render(gpuState_.write());
    gpuState_.swap();

    std::vector<ParticlePointVertex> vertices;
    vertices.reserve(static_cast<std::size_t>(gpuParticleCount_) * 6);
    const float invSide = 1.0f / static_cast<float>(gpuSide_);
    const float corners[6][2] = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f},
        {-1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f},
    };
    for (int y = 0; y < gpuSide_; ++y) {
        for (int x = 0; x < gpuSide_; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) * invSide;
            const float v = (static_cast<float>(y) + 0.5f) * invSide;
            for (const auto& c : corners) {
                ParticlePointVertex vertex{};
                vertex.lookup[0] = u;
                vertex.lookup[1] = v;
                vertex.corner[0] = c[0];
                vertex.corner[1] = c[1];
                vertices.push_back(vertex);
            }
        }
    }
    sg_buffer_desc vbufDesc = {};
    vbufDesc.data.ptr = vertices.data();
    vbufDesc.data.size = vertices.size() * sizeof(ParticlePointVertex);
    vbufDesc.label = "tcxFlowTools-particle-point-vertices";
    gpuPointVertexBuffer_ = sg_make_buffer(&vbufDesc);

    gpuPointShader_ = sg_make_shader(tcx_flow_particles_points_shader_desc(sg_query_backend()));
    sg_pipeline_desc pipDesc = {};
    pipDesc.shader = gpuPointShader_;
    pipDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    pipDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    pipDesc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pipDesc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    pipDesc.depth.write_enabled = false;
    pipDesc.cull_mode = SG_CULLMODE_NONE;
    pipDesc.colors[0].blend.enabled = true;
    pipDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pipDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pipDesc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA;
    pipDesc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pipDesc.label = "tcxFlowTools-particle-point-pipeline";
    gpuPointPipeline_ = sg_make_pipeline(&pipDesc);

    const bool ok = sg_query_buffer_state(gpuPointVertexBuffer_) == SG_RESOURCESTATE_VALID &&
                    sg_query_shader_state(gpuPointShader_) == SG_RESOURCESTATE_VALID &&
                    sg_query_pipeline_state(gpuPointPipeline_) == SG_RESOURCESTATE_VALID;
    if (!ok) {
        releaseGpu();
        return false;
    }
    return true;
}

ParticleFlow::NormalizedSpawn ParticleFlow::normalizeSpawn(const SpawnRequest& request) const {
    NormalizedSpawn spawn;
    if (request.amount <= 0 || request.radius <= 0.0f) return spawn;
    spawn.center = tc::Vec2(std::clamp(request.position.x / static_cast<float>(std::max(1, width_)), 0.0f, 1.0f),
                            std::clamp(request.position.y / static_cast<float>(std::max(1, height_)), 0.0f, 1.0f));
    spawn.radius = std::clamp(request.radius / static_cast<float>(std::max(1, std::max(width_, height_))), 0.0f, 1.0f);
    spawn.probability = std::clamp(static_cast<float>(request.amount) /
                                       static_cast<float>(std::max(1, particleCount())),
                                   0.0f, 1.0f);
    spawn.active = spawn.probability > 0.0f && spawn.radius > 0.0f;
    return spawn;
}

tc::Vec2 ParticleFlow::birthVelocityForPosition(const Fluid2D& fluid, const tc::Vec2& position, int index) const {
    if (!settings_.birthFromVelocity) return tc::Vec2(0, 0);
    tc::Vec2 velocity = fluid.sampleVelocityAtPosition(position) * settings_.birthVelocityScale;
    const float jitter = std::max(0.0f, settings_.birthVelocityJitter);
    if (jitter > 0.0f) {
        const float angle = hash01(index * 313 + 71) * 6.2831853f;
        const float radius = (hash01(index * 337 + 97) * 2.0f - 1.0f) * jitter;
        velocity += tc::Vec2(std::cos(angle), std::sin(angle)) * radius;
    }
    return velocity;
}

void ParticleFlow::applyCpuSpawns(const Fluid2D& fluid) {
    if (pendingSpawns_.empty() || positions_.empty()) return;
    for (const auto& request : pendingSpawns_) {
        const int amount = std::max(0, request.amount);
        for (int n = 0; n < amount; ++n) {
            const int idx = cpuSpawnCursor_ % static_cast<int>(positions_.size());
            cpuSpawnCursor_ = (cpuSpawnCursor_ + 1) % static_cast<int>(positions_.size());
            const float a = hash01(idx * 17 + n * 31) * 6.2831853f;
            const float r = std::sqrt(hash01(idx * 23 + n * 47)) * request.radius;
            positions_[static_cast<std::size_t>(idx)] = tc::Vec2(
                std::clamp(request.position.x + std::cos(a) * r, 0.0f, static_cast<float>(width_)),
                std::clamp(request.position.y + std::sin(a) * r, 0.0f, static_cast<float>(height_)));
            velocities_[static_cast<std::size_t>(idx)] = birthVelocityForPosition(fluid, positions_[static_cast<std::size_t>(idx)], idx);
            if (settings_.resetAgeOnBirth) ages_[static_cast<std::size_t>(idx)] = 0.0f;
        }
    }
    pendingSpawns_.clear();
}

void ParticleFlow::releaseGpu() {
    if (sg_isvalid()) {
        gpuState_.release();
        if (gpuPointPipeline_.id) sg_destroy_pipeline(gpuPointPipeline_);
        if (gpuPointShader_.id) sg_destroy_shader(gpuPointShader_);
        if (gpuPointVertexBuffer_.id) sg_destroy_buffer(gpuPointVertexBuffer_);
    }
    gpuPointPipeline_ = {};
    gpuPointShader_ = {};
    gpuPointVertexBuffer_ = {};
    gpuReady_ = false;
    gpuParticleCount_ = 0;
    gpuSide_ = 0;
    lastUpdateUsedGpu_ = false;
}

void ParticleFlow::updateGpu(const Fluid2D& fluid, float dt) {
    const tc::Texture* velocityTexture = fluid.getVelocityTexture();
    if (!velocityTexture) return;
    const std::vector<SpawnRequest> spawns = std::move(pendingSpawns_);
    pendingSpawns_.clear();
    gpuUpdatePass_.setTexture("tex0", gpuState_.read().getTexture());
    gpuUpdatePass_.setTexture("particleVelocityTex", *velocityTexture);
    const float normalizedVelocity = std::max(0.0f, dt) * settings_.velocityScale / std::max(1, std::max(width_, height_));
    gpuUpdatePass_.setColor(tc::Color(std::clamp(settings_.variantCenter.x, 0.0f, 1.0f),
                                      std::clamp(settings_.variantCenter.y, 0.0f, 1.0f),
                                      std::max(0.0f, settings_.variantStrength),
                                      settings_.birthFromVelocity ? 1.0f : 0.0f));
    gpuUpdatePass_.setTexelExtra(0.0f, 0.0f);
    gpuUpdatePass_.setOptions(normalizedVelocity, std::max(0.0f, dt), settings_.lifetime,
                              static_cast<float>(variantCode(settings_.variant)));
    gpuUpdatePass_.render(gpuState_.write());
    gpuState_.swap();

    for (const auto& request : spawns) {
        applyGpuSpawn(normalizeSpawn(request), *velocityTexture);
    }
}

void ParticleFlow::applyGpuSpawn(const NormalizedSpawn& spawn, const tc::Texture& velocityTexture) {
    if (!spawn.active) return;
    gpuUpdatePass_.setTexture("tex0", gpuState_.read().getTexture());
    gpuUpdatePass_.setTexture("particleVelocityTex", velocityTexture);
    gpuUpdatePass_.setColor(tc::Color(spawn.center.x, spawn.center.y,
                                      std::max(0.0f, settings_.variantStrength),
                                      settings_.birthFromVelocity ? 1.0f : 0.0f));
    gpuUpdatePass_.setTexelExtra(spawn.radius, spawn.probability);
    gpuUpdatePass_.setOptions(settings_.birthFromVelocity
                                  ? std::max(0.0f, settings_.birthVelocityScale) /
                                        static_cast<float>(std::max(1, std::max(width_, height_)))
                                  : 0.0f,
                              0.0f, settings_.lifetime, 0.0f);
    gpuUpdatePass_.render(gpuState_.write());
    gpuState_.swap();
}

void ParticleFlow::drawGpu(float x, float y, float w, float h) const {
    if (!gpuReady_ || !gpuPointPipeline_.id || !gpuPointVertexBuffer_.id || !gpuState_.isAllocated()) return;
    tc::ensureSwapchainPass();
    sgl_draw();

    FlowPassParams params;
    params.color[0] = settings_.particleColor.r;
    params.color[1] = settings_.particleColor.g;
    params.color[2] = settings_.particleColor.b;
    params.color[3] = settings_.particleColor.a;
    params.resolution[0] = static_cast<float>(std::max(1, tc::getWindowWidth()));
    params.resolution[1] = static_cast<float>(std::max(1, tc::getWindowHeight()));
    params.resolution[2] = w;
    params.resolution[3] = h;
    params.texel[0] = x;
    params.texel[1] = y;
    params.options[0] = std::max(0.0f, settings_.sizeSpread);
    params.options[1] = std::max(0.0001f, settings_.ageFadePower);
    params.options[2] = settings_.lifetime;
    params.options[3] = settings_.particleSize;

    sg_apply_pipeline(gpuPointPipeline_);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = gpuPointVertexBuffer_;
    bind.views[0] = gpuState_.read().getTextureView();
    bind.samplers[0] = gpuState_.read().getSampler();
    sg_apply_bindings(&bind);
    sg_range range{&params, sizeof(params)};
    sg_apply_uniforms(0, &range);
    sg_draw(0, gpuParticleCount_ * 6, 1);

    sg_reset_state_cache();
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, static_cast<float>(tc::getWindowWidth()), static_cast<float>(tc::getWindowHeight()), 0.0f, -10000.0f, 10000.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();
}

} // namespace tcx::flow
