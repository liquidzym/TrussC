#include "PhysarumTrailFlow.h"
#include "physarum/physarum.glsl.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

PhysarumTrailFlow::~PhysarumTrailFlow() {
    release();
}

int PhysarumTrailFlow::textureSideForParticleCount(int particleCount) {
    return std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(std::max(1, particleCount))))));
}

tc::Vec2 PhysarumTrailFlow::wrappedNormalizedDelta(const tc::Vec2& current, const tc::Vec2& previous) {
    tc::Vec2 delta = current - previous;
    delta.x -= std::floor(delta.x + 0.5f);
    delta.y -= std::floor(delta.y + 0.5f);
    return delta;
}

void PhysarumTrailFlow::setup(int width, int height, const PhysarumTrailFlowSettings& settings) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    settings_ = settings;
    if (!trailFbo_.isAllocated() || trailFbo_.getWidth() != width_ || trailFbo_.getHeight() != height_) {
        trailFbo_.allocate(width_, height_, 1, tc::TextureFormat::RGBA8);
    }
    gpuReady_ = setupGpu();
    clearTrail();
}

void PhysarumTrailFlow::reset() {
    releaseGpu();
    gpuReady_ = setupGpu();
    clearTrail();
}

void PhysarumTrailFlow::update(const Fluid2D& fluid, float dt) {
    lastUpdateUsedGpu_ = false;
    if (!settings_.useGpuParticles || !gpuReady_ || !gpuAge_.isAllocated() || !gpuInitialState_.isAllocated()) return;
    const tc::Texture* velocityTexture = fluid.getVelocityTexture();
    if (!velocityTexture) return;

    lastDepositVertices_ = 0;
    const float frameStep = std::clamp(std::max(0.0f, dt) * 60.0f, 0.25f, 2.0f);
    const float lifetimeFrames = std::max(1.0f, settings_.lifetime * 60.0f);
    ageUpdatePass_.setTexture("tex0", gpuAge_.read().getTexture());
    ageUpdatePass_.setOptions(lifetimeFrames, frameStep, 0.0f, 0.0f);
    ageUpdatePass_.render(gpuAge_.write());
    gpuAge_.swap();

    fadeTrail();
    const int substeps = std::clamp(settings_.renderSubsteps, 1, 8);
    const float pixelStepScale = frameStep * std::max(0.0f, settings_.velocityScale) / static_cast<float>(substeps);
    for (int i = 0; i < substeps; ++i) {
        updatePass_.setTexture("tex0", gpuState_.read().getTexture());
        updatePass_.setTexture("physarumVelocityTex", *velocityTexture);
        updatePass_.setTexture("physarumAgeTex", gpuAge_.read().getTexture());
        updatePass_.setTexture("physarumInitialTex", gpuInitialState_.getTexture());
        updatePass_.setOptions(pixelStepScale,
                               static_cast<float>(std::max(1, width_)),
                               static_cast<float>(std::max(1, height_)),
                               std::max(0.25f, settings_.maxParticleStep));
        updatePass_.render(gpuState_.write());
        gpuState_.swap();

        trailFbo_.begin();
        depositParticles(*velocityTexture);
        trailFbo_.end();
    }
    lastUpdateUsedGpu_ = true;
}

void PhysarumTrailFlow::draw(float x, float y, float w, float h) const {
    if (!trailFbo_.isAllocated()) return;
    tc::setColor(1.0f);
    trailFbo_.draw(x, y, w, h);
}

void PhysarumTrailFlow::clearTrail() {
    if (!trailFbo_.isAllocated()) return;
    trailFbo_.begin(settings_.backgroundColor.r, settings_.backgroundColor.g,
                    settings_.backgroundColor.b, settings_.backgroundColor.a);
    trailFbo_.end();
}

void PhysarumTrailFlow::release() {
    releaseGpu();
    trailFbo_.clear();
    width_ = 0;
    height_ = 0;
}

const tc::Texture* PhysarumTrailFlow::trailTexture() const {
    return trailFbo_.isAllocated() ? &trailFbo_.getTexture() : nullptr;
}

bool PhysarumTrailFlow::canUseGpu() const {
    return settings_.useGpuParticles && sg_isvalid() && !tc::headless::isActive();
}

bool PhysarumTrailFlow::setupGpu() {
    releaseGpu();
    if (!canUseGpu()) return false;

    textureSide_ = textureSideForParticleCount(settings_.particleCount);
    gpuParticleCount_ = textureSide_ * textureSide_;
    gpuState_.allocate(textureSide_, textureSide_, tc::TextureFormat::RGBA32F, "physarum-state");
    gpuInitialState_.allocate(textureSide_, textureSide_, 1, tc::TextureFormat::RGBA32F);
    gpuAge_.allocate(textureSide_, textureSide_, tc::TextureFormat::RGBA32F, "physarum-age");
    gpuState_.read().getTexture().setFilter(tc::TextureFilter::Nearest);
    gpuState_.write().getTexture().setFilter(tc::TextureFilter::Nearest);
    gpuInitialState_.getTexture().setFilter(tc::TextureFilter::Nearest);
    gpuAge_.read().getTexture().setFilter(tc::TextureFilter::Nearest);
    gpuAge_.write().getTexture().setFilter(tc::TextureFilter::Nearest);

    spawnPass_.setup(FlowPassKind::PhysarumSpawn);
    ageSpawnPass_.setup(FlowPassKind::PhysarumAgeSpawn);
    ageUpdatePass_.setup(FlowPassKind::PhysarumAgeUpdate);
    updatePass_.setup(FlowPassKind::PhysarumUpdate);
    if (!spawnPass_.isReady() || !ageSpawnPass_.isReady() || !ageUpdatePass_.isReady() || !updatePass_.isReady()) {
        releaseGpu();
        return false;
    }
    spawnPass_.setOptions(static_cast<float>(std::max(1, width_)),
                          static_cast<float>(std::max(1, height_)),
                          0.37f,
                          0.83f);
    spawnPass_.render(gpuInitialState_);
    spawnPass_.render(gpuState_.write());
    gpuState_.swap();

    ageSpawnPass_.setOptions(std::max(1.0f, settings_.lifetime * 60.0f), 0.0f, 0.37f, 0.83f);
    ageSpawnPass_.render(gpuAge_.write());
    gpuAge_.swap();

    std::vector<ParticleVertex> vertices;
    vertices.reserve(static_cast<std::size_t>(gpuParticleCount_) * 6);
    const float invSide = 1.0f / static_cast<float>(textureSide_);
    const float corners[6][2] = {
        {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f},
        {-1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f},
    };
    for (int y = 0; y < textureSide_; ++y) {
        for (int x = 0; x < textureSide_; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) * invSide;
            const float v = (static_cast<float>(y) + 0.5f) * invSide;
            for (const auto& corner : corners) {
                ParticleVertex vertex{};
                vertex.lookup[0] = u;
                vertex.lookup[1] = v;
                vertex.corner[0] = corner[0];
                vertex.corner[1] = corner[1];
                vertices.push_back(vertex);
            }
        }
    }

    sg_buffer_desc vbufDesc = {};
    vbufDesc.data.ptr = vertices.data();
    vbufDesc.data.size = vertices.size() * sizeof(ParticleVertex);
    vbufDesc.label = "tcxFlowTools-physarum-particle-vertices";
    pointVertexBuffer_ = sg_make_buffer(&vbufDesc);

    pointShader_ = sg_make_shader(tcx_flow_physarum_deposit_shader_desc(sg_query_backend()));
    sg_pipeline_desc pipDesc = {};
    pipDesc.shader = pointShader_;
    pipDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    pipDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    pipDesc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pipDesc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    pipDesc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    pipDesc.depth.write_enabled = false;
    pipDesc.cull_mode = SG_CULLMODE_NONE;
    pipDesc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    pipDesc.colors[0].blend.enabled = true;
    pipDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pipDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pipDesc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    pipDesc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pipDesc.label = "tcxFlowTools-physarum-deposit-pipeline";
    pointPipeline_ = sg_make_pipeline(&pipDesc);

    const bool ok = sg_query_buffer_state(pointVertexBuffer_) == SG_RESOURCESTATE_VALID &&
                    sg_query_shader_state(pointShader_) == SG_RESOURCESTATE_VALID &&
                    sg_query_pipeline_state(pointPipeline_) == SG_RESOURCESTATE_VALID;
    if (!ok) {
        releaseGpu();
        return false;
    }
    return true;
}

void PhysarumTrailFlow::releaseGpu() {
    if (sg_isvalid()) {
        gpuState_.release();
        gpuAge_.release();
        gpuInitialState_.clear();
        if (pointPipeline_.id) sg_destroy_pipeline(pointPipeline_);
        if (pointShader_.id) sg_destroy_shader(pointShader_);
        if (pointVertexBuffer_.id) sg_destroy_buffer(pointVertexBuffer_);
    }
    pointPipeline_ = {};
    pointShader_ = {};
    pointVertexBuffer_ = {};
    textureSide_ = 0;
    gpuParticleCount_ = 0;
    gpuReady_ = false;
    lastUpdateUsedGpu_ = false;
    lastDepositVertices_ = 0;
}

void PhysarumTrailFlow::fadeTrail() {
    if (!trailFbo_.isAllocated()) return;
    const float fadeAlpha = std::clamp(settings_.trailFadeScale / std::max(1.0f, settings_.trailLength), 0.004f, 0.14f);
    trailFbo_.begin();
    tc::setColor(settings_.backgroundColor.r, settings_.backgroundColor.g, settings_.backgroundColor.b, fadeAlpha);
    tc::drawRect(0, 0, trailFbo_.getWidth(), trailFbo_.getHeight());
    trailFbo_.end();
}

void PhysarumTrailFlow::depositParticles(const tc::Texture& velocityTexture) {
    if (!gpuReady_ || !pointPipeline_.id || !pointVertexBuffer_.id ||
        !gpuState_.isAllocated() || !gpuAge_.isAllocated()) return;

    sgl_draw();

    FlowPassParams params;
    params.color[0] = settings_.inkColor.r;
    params.color[1] = settings_.inkColor.g;
    params.color[2] = settings_.inkColor.b;
    params.color[3] = settings_.inkColor.a;
    params.resolution[0] = static_cast<float>(std::max(1, trailFbo_.getWidth()));
    params.resolution[1] = static_cast<float>(std::max(1, trailFbo_.getHeight()));
    params.options[0] = std::max(0.1f, settings_.strokeThickness);
    params.options[1] = std::max(0.0f, settings_.inkStrength);
    params.options[2] = std::max(1.0f, settings_.lifetime * 60.0f);
    params.options[3] = std::max(0.0f, settings_.dashVelocityScale);

    sg_apply_pipeline(pointPipeline_);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = pointVertexBuffer_;
    bind.views[0] = gpuState_.read().getTextureView();
    bind.samplers[0] = gpuState_.read().getSampler();
    bind.views[1] = velocityTexture.getView();
    bind.samplers[1] = velocityTexture.getSampler();
    bind.views[2] = gpuAge_.read().getTextureView();
    bind.samplers[2] = gpuAge_.read().getSampler();
    sg_apply_bindings(&bind);
    sg_range range{&params, sizeof(params)};
    sg_apply_uniforms(0, &range);
    const int vertexCount = gpuParticleCount_ * strokeDepositVerticesPerParticle();
    sg_draw(0, vertexCount, 1);
    lastDepositVertices_ += vertexCount;

    sg_reset_state_cache();
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, static_cast<float>(trailFbo_.getWidth()),
              static_cast<float>(trailFbo_.getHeight()), 0.0f, -10000.0f, 10000.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();
}

} // namespace tcx::flow
