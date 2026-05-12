#include "ParticleFlow.h"
#include "particles/particles.glsl.h"

#include <cmath>

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
    gpuReady_ = setupGpu();
    if (gpuReady_) {
        positions_.clear();
        velocities_.clear();
        ages_.clear();
        return;
    }

    const int count = std::max(0, settings_.particleCount);
    positions_.resize(static_cast<std::size_t>(count));
    velocities_.assign(static_cast<std::size_t>(count), tc::Vec2(0, 0));
    ages_.assign(static_cast<std::size_t>(count), 0.0f);
    for (int i = 0; i < count; ++i) {
        const float u = static_cast<float>((i * 1103515245u + 12345u) & 0xffffu) / 65535.0f;
        const float v = static_cast<float>((i * 214013u + 2531011u) & 0xffffu) / 65535.0f;
        positions_[static_cast<std::size_t>(i)] = tc::Vec2(u * width_, v * height_);
    }
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
    for (std::size_t i = 0; i < positions_.size(); ++i) {
        ages_[i] += dt;
        velocities_[i] += fluid.sampleVelocityAtPosition(positions_[i]) * dt;
        if (settings_.variant == ParticleFlowVariant::Attractor) {
            const tc::Vec2 toward = variantCenter - positions_[i];
            const float dist = std::max(1.0f, std::sqrt(toward.x * toward.x + toward.y * toward.y));
            velocities_[i] += toward * (settings_.variantStrength * dt / dist);
        } else if (settings_.variant == ParticleFlowVariant::Impulse) {
            const tc::Vec2 away = positions_[i] - variantCenter;
            const float dist = std::max(1.0f, std::sqrt(away.x * away.x + away.y * away.y));
            velocities_[i] += away * (settings_.variantStrength * dt / dist);
        }
        velocities_[i] *= damping;
        positions_[i] += velocities_[i] * (dt * settings_.velocityScale);
        if (settings_.respawn && (ages_[i] > settings_.lifetime || positions_[i].x < 0 || positions_[i].y < 0 ||
                                  positions_[i].x > width_ || positions_[i].y > height_)) {
            ages_[i] = 0.0f;
            const float a = static_cast<float>(i) * 2.3999632f;
            positions_[i] = tc::Vec2(width_ * 0.5f + std::cos(a) * settings_.spawnRadius,
                                     height_ * 0.5f + std::sin(a) * settings_.spawnRadius);
            velocities_[i] = tc::Vec2(0, 0);
        }
    }
}

void ParticleFlow::draw(float x, float y, float w, float h) const {
    if (lastUpdateUsedGpu_ && gpuReady_) {
        drawGpu(x, y, w, h);
        return;
    }
    if (width_ <= 0 || height_ <= 0) return;
    tc::setColor(settings_.particleColor);
    const float sx = w / width_;
    const float sy = h / height_;
    const std::size_t maxDraw = std::min<std::size_t>(positions_.size(), 4096);
    for (std::size_t i = 0; i < maxDraw; ++i) {
        tc::drawCircle(x + positions_[i].x * sx, y + positions_[i].y * sy, settings_.particleSize);
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

    std::vector<float> seed(static_cast<std::size_t>(gpuParticleCount_) * 4, 0.0f);
    for (int i = 0; i < gpuParticleCount_; ++i) {
        const float a = static_cast<float>((i * 1103515245u + 12345u) & 0xffffu) / 65535.0f;
        const float b = static_cast<float>((i * 214013u + 2531011u) & 0xffffu) / 65535.0f;
        seed[static_cast<std::size_t>(i) * 4 + 0] = a;
        seed[static_cast<std::size_t>(i) * 4 + 1] = b;
        seed[static_cast<std::size_t>(i) * 4 + 2] = 0.0f;
        seed[static_cast<std::size_t>(i) * 4 + 3] = 1.0f;
    }
    gpuSeedTexture_.allocate(gpuSide_, gpuSide_, TextureFormat::RGBA32F, tc::TextureUsage::Dynamic);
    gpuSeedTexture_.setFilter(tc::TextureFilter::Nearest);
    gpuSeedTexture_.loadData(seed.data(), gpuSide_, gpuSide_, 4);

    gpuSpawnPass_.setup(FlowPassKind::ParticlesSpawn);
    gpuUpdatePass_.setup(FlowPassKind::ParticlesUpdate);
    gpuSpawnPass_.setTexture("tex0", gpuSeedTexture_);
    gpuSpawnPass_.setColor(tc::Color(1.0f));
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

void ParticleFlow::releaseGpu() {
    if (sg_isvalid()) {
        gpuState_.release();
        gpuSeedTexture_.clear();
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
    gpuUpdatePass_.setTexture("tex0", gpuState_.read().getTexture());
    gpuUpdatePass_.setTexture("particleVelocityTex", *velocityTexture);
    const float normalizedVelocity = std::max(0.0f, dt) * settings_.velocityScale / std::max(1, std::max(width_, height_));
    gpuUpdatePass_.setColor(tc::Color(std::clamp(settings_.variantCenter.x, 0.0f, 1.0f),
                                      std::clamp(settings_.variantCenter.y, 0.0f, 1.0f),
                                      std::max(0.0f, settings_.variantStrength),
                                      std::clamp(settings_.damping, 0.0f, 1.0f)));
    gpuUpdatePass_.setOptions(normalizedVelocity, std::max(0.0f, dt), settings_.lifetime,
                              static_cast<float>(variantCode(settings_.variant)));
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
