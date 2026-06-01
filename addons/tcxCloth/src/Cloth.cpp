#include "tcxCloth/Cloth.h"
#include "generated/cloth.glsl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tcxCloth {

namespace {

constexpr float kEpsilon = 0.000001f;

float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float x) {
    if (std::abs(edge1 - edge0) <= kEpsilon) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

bool finiteVec(const tc::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

tc::Vec3 safeNormal(const tc::Vec3& v, const tc::Vec3& fallback = tc::Vec3(0.0f, 0.0f, 1.0f)) {
    const float len = v.length();
    if (len <= kEpsilon || !finiteVec(v)) {
        return fallback;
    }
    return v / len;
}

} // namespace

struct ClothStepUniforms {
    float simSize[4] = {};
    float timing[4] = {};
    float wind[4] = {};
    float clothLayout[4] = {};
    float forces[4] = {};
    float stiffness[4] = {};
    float collider[4] = {};
    float options[4] = {};
};

class ClothStepShader : public tc::FullscreenShader {
public:
    void setInput(int slot, const tc::Texture* texture) {
        if (slot < 0 || slot >= static_cast<int>(views_.size())) return;
        if (!texture || !texture->isAllocated()) {
            views_[slot] = {};
            samplers_[slot] = {};
            return;
        }
        views_[slot] = texture->getView();
        samplers_[slot] = texture->getSampler();
    }

protected:
    sg_pipeline_desc createPipelineDesc() override {
        sg_pipeline_desc desc = {};
        desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
        desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
        desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        desc.depth.write_enabled = false;
        desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
        desc.cull_mode = SG_CULLMODE_NONE;
        desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA32F;
        desc.colors[0].blend.enabled = false;
        desc.index_type = SG_INDEXTYPE_UINT16;
        desc.sample_count = 1;
        desc.label = "tcx_cloth_step_pipeline";
        return desc;
    }

    void setupBindings(sg_bindings& bind) override {
        for (int i = 0; i < static_cast<int>(views_.size()); ++i) {
            if (views_[i].id) {
                bind.views[i] = views_[i];
                bind.samplers[i] = samplers_[i];
            }
        }
    }

private:
    std::array<sg_view, 3> views_ = {};
    std::array<sg_sampler, 3> samplers_ = {};
};

struct Cloth::GpuResources {
    std::array<tc::Fbo, 2> position;
    std::array<tc::Fbo, 2> previous;
    int positionReadIndex = 0;
    int previousReadIndex = 0;
    tc::Texture pinMask;
    ClothStepShader stepShader;
    std::vector<float> pinPixels;
    std::vector<float> readback;
    bool ready = false;
    bool pinsDirty = true;
    bool stateDirty = true;
    float elapsed = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;

    tc::Fbo& positionRead() { return position[positionReadIndex]; }
    tc::Fbo& positionWrite() { return position[1 - positionReadIndex]; }
    const tc::Fbo& positionRead() const { return position[positionReadIndex]; }
    tc::Fbo& previousRead() { return previous[previousReadIndex]; }
    tc::Fbo& previousWrite() { return previous[1 - previousReadIndex]; }
    const tc::Fbo& previousRead() const { return previous[previousReadIndex]; }

    void swapPosition() { positionReadIndex = 1 - positionReadIndex; }
    void swapPrevious() { previousReadIndex = 1 - previousReadIndex; }
};

Cloth::Cloth() = default;
Cloth::~Cloth() = default;
Cloth::Cloth(Cloth&&) noexcept = default;
Cloth& Cloth::operator=(Cloth&&) noexcept = default;

void Cloth::setup(const ClothSettings& settings) {
    validateAndStoreSettings(settings);
    chooseBackend();
    buildParticles();
    buildTopology();
    accumulator_ = 0.0f;
    if (activeBackend_ == ClothSettings::SolverBackend::TexturePingPong) {
        setupGpuResources();
    } else {
        releaseGpuResources();
    }
    recomputeNormalsCpu();
    rebuildMeshes();
}

void Cloth::release() {
    particles_.clear();
    constraints_.clear();
    triangleIndices_.clear();
    wireIndices_.clear();
    sphereColliders_.clear();
    planeColliders_.clear();
    accumulator_ = 0.0f;
    fillMesh_.clear();
    wireMesh_.clear();
    releaseGpuResources();
    backendReason_ = "released";
}

void Cloth::reset() {
    for (auto& particle : particles_) {
        const tc::Vec3 target = particle.pinned ? particle.pinnedPosition : particle.initialPosition;
        particle.position = target;
        particle.previousPosition = target;
        particle.acceleration = tc::Vec3(0.0f);
        particle.normal = tc::Vec3(0.0f, 0.0f, 1.0f);
    }
    accumulator_ = 0.0f;
    resetGpuState();
    recomputeNormalsCpu();
    rebuildMeshes();
}

void Cloth::update(float dt) {
    if (particles_.empty()) return;

    if (shouldUseGpuThisFrame()) {
        updateGpu(dt);
        return;
    }

    const float clampedDt = std::clamp(dt, 0.0f, 1.0f / 15.0f);
    const float fixedStep = settings_.fixedTimeStep > 0.0f ? settings_.fixedTimeStep : clampedDt;
    if (fixedStep <= 0.0f) return;

    accumulator_ += clampedDt;
    int stepCount = 0;
    while (accumulator_ + kEpsilon >= fixedStep && stepCount < 8) {
        const int substeps = std::max(1, settings_.substeps);
        const float subDt = fixedStep / static_cast<float>(substeps);
        for (int i = 0; i < substeps; ++i) {
            stepCpu(subDt);
        }
        accumulator_ -= fixedStep;
        ++stepCount;
    }

    if (settings_.recomputeNormals) {
        recomputeNormalsCpu();
    }
    rebuildMeshes();
}

void Cloth::draw() {
    fillMesh_.draw();
}

void Cloth::drawWire() {
    wireMesh_.draw();
}

void Cloth::setGlobalForce(const tc::Vec3& force) {
    globalForce_ = force;
}

void Cloth::setGravity(const tc::Vec3& gravity) {
    gravity_ = gravity;
}

void Cloth::setWind(const tc::Vec3& windDirection, float strength) {
    windDirection_ = safeNormal(windDirection);
    windStrength_ = std::max(0.0f, strength);
}

void Cloth::setSphereColliders(std::span<const SphereCollider> spheres) {
    sphereColliders_.assign(spheres.begin(), spheres.end());
}

void Cloth::setPlaneColliders(std::span<const PlaneCollider> planes) {
    planeColliders_.assign(planes.begin(), planes.end());
}

void Cloth::clearColliders() {
    sphereColliders_.clear();
    planeColliders_.clear();
}

void Cloth::pinParticle(int x, int y, bool pinned) {
    if (!validCoord(x, y)) return;
    auto& particle = particles_[index(x, y)];
    particle.pinned = pinned;
    particle.inverseMass = pinned ? 0.0f : 1.0f;
    if (pinned) {
        particle.pinnedPosition = particle.position;
        particle.previousPosition = particle.position;
    }
    markGpuPinsDirty();
}

void Cloth::pinTopEdge(int step) {
    if (particles_.empty()) return;
    const int stride = std::max(1, step);
    for (int x = 0; x < settings_.columns; ++x) {
        if (x == 0 || x == settings_.columns - 1 || x % stride == 0) {
            pinParticle(x, 0, true);
        }
    }
}

void Cloth::pinCorners() {
    if (particles_.empty()) return;
    pinParticle(0, 0, true);
    pinParticle(settings_.columns - 1, 0, true);
    pinParticle(0, settings_.rows - 1, true);
    pinParticle(settings_.columns - 1, settings_.rows - 1, true);
}

void Cloth::setParticlePosition(int x, int y, const tc::Vec3& position) {
    if (!validCoord(x, y)) return;
    auto& particle = particles_[index(x, y)];
    particle.position = position;
    particle.previousPosition = position;
    if (particle.pinned) {
        particle.pinnedPosition = position;
    }
    resetGpuState();
    recomputeNormalsCpu();
    rebuildMeshes();
}

tc::Vec3 Cloth::particlePosition(int x, int y) const {
    if (!validCoord(x, y)) return {};
    return particles_[index(x, y)].position;
}

bool Cloth::isPinned(int x, int y) const {
    if (!validCoord(x, y)) return false;
    return particles_[index(x, y)].pinned;
}

ClothTopologyInfo Cloth::topologyInfo() const {
    ClothTopologyInfo info;
    info.columns = settings_.columns;
    info.rows = settings_.rows;
    info.particleCount = particles_.size();
    info.triangleIndexCount = triangleIndices_.size();
    info.wireIndexCount = wireIndices_.size();
    info.constraintCount = constraints_.size();
    return info;
}

std::span<const CpuParticle> Cloth::particles() const {
    return {particles_.data(), particles_.size()};
}

std::span<const unsigned int> Cloth::triangleIndices() const {
    return {triangleIndices_.data(), triangleIndices_.size()};
}

std::span<const unsigned int> Cloth::wireIndices() const {
    return {wireIndices_.data(), wireIndices_.size()};
}

int Cloth::index(int x, int y) const {
    return y * settings_.columns + x;
}

bool Cloth::validCoord(int x, int y) const {
    return x >= 0 && y >= 0 && x < settings_.columns && y < settings_.rows;
}

void Cloth::validateAndStoreSettings(const ClothSettings& settings) {
    settings_ = settings;
    settings_.columns = std::max(2, settings_.columns);
    settings_.rows = std::max(2, settings_.rows);
    settings_.width = std::max(1.0f, settings_.width);
    settings_.height = std::max(1.0f, settings_.height);
    settings_.damping = std::clamp(settings_.damping, 0.0f, 0.999f);
    settings_.fixedTimeStep = std::max(0.0f, settings_.fixedTimeStep);
    settings_.substeps = std::max(1, settings_.substeps);
    settings_.constraintIterations = std::max(0, settings_.constraintIterations);
    settings_.structuralStiffness = clamp01(settings_.structuralStiffness);
    settings_.shearStiffness = clamp01(settings_.shearStiffness);
    settings_.bendStiffness = clamp01(settings_.bendStiffness);
}

void Cloth::chooseBackend() {
    switch (settings_.backend) {
        case ClothSettings::SolverBackend::Auto:
            if (canUseTexturePingPong()) {
                activeBackend_ = ClothSettings::SolverBackend::TexturePingPong;
                backendReason_ = "Auto selected TexturePingPong GPU position backend";
            } else {
                activeBackend_ = ClothSettings::SolverBackend::CpuReference;
                backendReason_ = "GPU unavailable for Auto; using CPU reference fallback";
            }
            break;
        case ClothSettings::SolverBackend::CpuReference:
            activeBackend_ = ClothSettings::SolverBackend::CpuReference;
            backendReason_ = "CPU reference backend requested";
            break;
        case ClothSettings::SolverBackend::TexturePingPong:
            if (canUseTexturePingPong()) {
                activeBackend_ = ClothSettings::SolverBackend::TexturePingPong;
                backendReason_ = "TexturePingPong GPU position backend active";
            } else {
                activeBackend_ = ClothSettings::SolverBackend::CpuReference;
                backendReason_ = "GPU unavailable for TexturePingPong; using CPU reference fallback";
            }
            break;
        case ClothSettings::SolverBackend::ComputeStorageBuffer:
            if (canUseTexturePingPong()) {
                activeBackend_ = ClothSettings::SolverBackend::TexturePingPong;
                backendReason_ = "ComputeStorageBuffer is not implemented yet; using TexturePingPong GPU fallback";
            } else {
                activeBackend_ = ClothSettings::SolverBackend::CpuReference;
                backendReason_ = "ComputeStorageBuffer is not implemented yet and GPU is unavailable; using CPU reference fallback";
            }
            break;
    }
}

void Cloth::buildParticles() {
    particles_.clear();
    particles_.reserve(static_cast<std::size_t>(settings_.columns * settings_.rows));

    const float dx = settings_.columns > 1 ? settings_.width / static_cast<float>(settings_.columns - 1) : 0.0f;
    const float dy = settings_.rows > 1 ? settings_.height / static_cast<float>(settings_.rows - 1) : 0.0f;

    for (int y = 0; y < settings_.rows; ++y) {
        const float v = settings_.rows > 1 ? static_cast<float>(y) / static_cast<float>(settings_.rows - 1) : 0.0f;
        for (int x = 0; x < settings_.columns; ++x) {
            const float u = settings_.columns > 1 ? static_cast<float>(x) / static_cast<float>(settings_.columns - 1) : 0.0f;
            CpuParticle particle;
            particle.position = settings_.origin + tc::Vec3(dx * x, dy * y, 0.0f);
            particle.previousPosition = particle.position;
            particle.initialPosition = particle.position;
            particle.pinnedPosition = particle.position;
            particle.uv = tc::Vec2(u, v);
            particles_.push_back(particle);
        }
    }
}

void Cloth::buildTopology() {
    constraints_.clear();
    triangleIndices_.clear();
    wireIndices_.clear();

    triangleIndices_.reserve(static_cast<std::size_t>((settings_.columns - 1) * (settings_.rows - 1) * 6));
    for (int y = 0; y < settings_.rows - 1; ++y) {
        for (int x = 0; x < settings_.columns - 1; ++x) {
            const unsigned int i0 = static_cast<unsigned int>(index(x, y));
            const unsigned int i1 = static_cast<unsigned int>(index(x + 1, y));
            const unsigned int i2 = static_cast<unsigned int>(index(x, y + 1));
            const unsigned int i3 = static_cast<unsigned int>(index(x + 1, y + 1));
            triangleIndices_.push_back(i0);
            triangleIndices_.push_back(i2);
            triangleIndices_.push_back(i1);
            triangleIndices_.push_back(i1);
            triangleIndices_.push_back(i2);
            triangleIndices_.push_back(i3);
        }
    }

    const auto addLine = [this](int a, int b) {
        wireIndices_.push_back(static_cast<unsigned int>(a));
        wireIndices_.push_back(static_cast<unsigned int>(b));
    };
    for (int y = 0; y < settings_.rows; ++y) {
        for (int x = 0; x < settings_.columns - 1; ++x) {
            addLine(index(x, y), index(x + 1, y));
        }
    }
    for (int y = 0; y < settings_.rows - 1; ++y) {
        for (int x = 0; x < settings_.columns; ++x) {
            addLine(index(x, y), index(x, y + 1));
        }
    }

    for (int y = 0; y < settings_.rows; ++y) {
        for (int x = 0; x < settings_.columns; ++x) {
            if (x + 1 < settings_.columns) {
                addConstraint(index(x, y), index(x + 1, y), settings_.structuralStiffness, ConstraintKind::Structural);
            }
            if (y + 1 < settings_.rows) {
                addConstraint(index(x, y), index(x, y + 1), settings_.structuralStiffness, ConstraintKind::Structural);
            }
        }
    }

    if (settings_.enableShearConstraints) {
        for (int y = 0; y + 1 < settings_.rows; ++y) {
            for (int x = 0; x + 1 < settings_.columns; ++x) {
                addConstraint(index(x, y), index(x + 1, y + 1), settings_.shearStiffness, ConstraintKind::Shear);
                addConstraint(index(x + 1, y), index(x, y + 1), settings_.shearStiffness, ConstraintKind::Shear);
            }
        }
    }

    if (settings_.enableBendConstraints) {
        for (int y = 0; y < settings_.rows; ++y) {
            for (int x = 0; x < settings_.columns; ++x) {
                if (x + 2 < settings_.columns) {
                    addConstraint(index(x, y), index(x + 2, y), settings_.bendStiffness, ConstraintKind::Bend);
                }
                if (y + 2 < settings_.rows) {
                    addConstraint(index(x, y), index(x, y + 2), settings_.bendStiffness, ConstraintKind::Bend);
                }
                if (x + 2 < settings_.columns && y + 2 < settings_.rows) {
                    addConstraint(index(x, y), index(x + 2, y + 2), settings_.bendStiffness, ConstraintKind::Bend);
                    addConstraint(index(x + 2, y), index(x, y + 2), settings_.bendStiffness, ConstraintKind::Bend);
                }
            }
        }
    }
}

void Cloth::addConstraint(int a, int b, float stiffness, ConstraintKind kind) {
    if (a < 0 || b < 0 ||
        a >= static_cast<int>(particles_.size()) ||
        b >= static_cast<int>(particles_.size()) ||
        a == b) {
        return;
    }
    ClothConstraint constraint;
    constraint.a = a;
    constraint.b = b;
    constraint.restLength = std::max(kEpsilon, particles_[a].initialPosition.distance(particles_[b].initialPosition));
    constraint.stiffness = clamp01(stiffness);
    constraint.kind = kind;
    constraints_.push_back(constraint);
}

bool Cloth::canUseTexturePingPong() const {
    if (tc::headless::isActive() || !sg_isvalid()) {
        return false;
    }
    const sg_pixelformat_info rgba32f = sg_query_pixelformat(SG_PIXELFORMAT_RGBA32F);
    return rgba32f.render && rgba32f.sample;
}

bool Cloth::shouldUseGpuThisFrame() {
    if (activeBackend_ != ClothSettings::SolverBackend::TexturePingPong) {
        return false;
    }
    if (!canUseTexturePingPong()) {
        activeBackend_ = ClothSettings::SolverBackend::CpuReference;
        backendReason_ = "GPU unavailable for TexturePingPong; using CPU reference fallback";
        return false;
    }
    if (!gpu_ || !gpu_->ready) {
        setupGpuResources();
    }
    return gpu_ && gpu_->ready;
}

void Cloth::setupGpuResources() {
    if (!canUseTexturePingPong()) {
        activeBackend_ = ClothSettings::SolverBackend::CpuReference;
        backendReason_ = "GPU unavailable for TexturePingPong; using CPU reference fallback";
        return;
    }

    gpu_ = std::make_unique<GpuResources>();
    for (auto& state : gpu_->position) {
        state.allocate(settings_.columns, settings_.rows, 1, tc::TextureFormat::RGBA32F);
        if (!state.isAllocated()) {
            releaseGpuResources();
            activeBackend_ = ClothSettings::SolverBackend::CpuReference;
            backendReason_ = "TexturePingPong framebuffer allocation failed; using CPU reference fallback";
            return;
        }
        state.getTexture().setFilter(tc::TextureFilter::Nearest);
        state.getTexture().setWrap(tc::TextureWrap::ClampToEdge);
        state.begin(0.0f, 0.0f, 0.0f, 1.0f);
        state.end();
    }
    for (auto& state : gpu_->previous) {
        state.allocate(settings_.columns, settings_.rows, 1, tc::TextureFormat::RGBA32F);
        if (!state.isAllocated()) {
            releaseGpuResources();
            activeBackend_ = ClothSettings::SolverBackend::CpuReference;
            backendReason_ = "TexturePingPong previous framebuffer allocation failed; using CPU reference fallback";
            return;
        }
        state.getTexture().setFilter(tc::TextureFilter::Nearest);
        state.getTexture().setWrap(tc::TextureWrap::ClampToEdge);
        state.begin(0.0f, 0.0f, 0.0f, 1.0f);
        state.end();
    }

    gpu_->pinMask.allocate(settings_.columns, settings_.rows, tc::TextureFormat::RGBA32F, tc::TextureUsage::Dynamic);
    if (!gpu_->pinMask.isAllocated()) {
        releaseGpuResources();
        activeBackend_ = ClothSettings::SolverBackend::CpuReference;
        backendReason_ = "TexturePingPong pin texture allocation failed; using CPU reference fallback";
        return;
    }
    gpu_->pinMask.setFilter(tc::TextureFilter::Nearest);
    gpu_->pinMask.setWrap(tc::TextureWrap::ClampToEdge);
    gpu_->pinPixels.assign(static_cast<std::size_t>(settings_.columns * settings_.rows * 4), 0.0f);
    gpu_->readback.assign(static_cast<std::size_t>(settings_.columns * settings_.rows * 4), 0.0f);

    if (!gpu_->stepShader.load(tcx_cloth_step_shader_desc)) {
        releaseGpuResources();
        activeBackend_ = ClothSettings::SolverBackend::CpuReference;
        backendReason_ = "TexturePingPong shader load failed; using CPU reference fallback";
        return;
    }

    gpu_->ready = true;
    gpu_->pinsDirty = true;
    gpu_->stateDirty = true;
    backendReason_ = "TexturePingPong GPU position backend active";
}

void Cloth::releaseGpuResources() {
    if (!gpu_) return;
    gpu_->stepShader.clear();
    gpu_->pinMask.clear();
    for (auto& state : gpu_->position) {
        state.clear();
    }
    for (auto& state : gpu_->previous) {
        state.clear();
    }
    gpu_.reset();
}

void Cloth::markGpuPinsDirty() {
    if (gpu_) {
        gpu_->pinsDirty = true;
    }
}

void Cloth::uploadGpuPinsIfNeeded() {
    if (!gpu_ || !gpu_->ready || !gpu_->pinsDirty) return;
    if (gpu_->pinPixels.size() != static_cast<std::size_t>(settings_.columns * settings_.rows * 4)) {
        gpu_->pinPixels.assign(static_cast<std::size_t>(settings_.columns * settings_.rows * 4), 0.0f);
    }
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        const float pinned = particles_[i].pinned ? 1.0f : 0.0f;
        const std::size_t base = i * 4;
        gpu_->pinPixels[base + 0] = pinned;
        gpu_->pinPixels[base + 1] = particles_[i].pinnedPosition.x;
        gpu_->pinPixels[base + 2] = particles_[i].pinnedPosition.y;
        gpu_->pinPixels[base + 3] = particles_[i].pinnedPosition.z;
    }
    gpu_->pinMask.loadData(gpu_->pinPixels.data(), settings_.columns, settings_.rows, 4);
    gpu_->pinsDirty = false;
}

void Cloth::resetGpuState() {
    if (!gpu_ || !gpu_->ready) return;
    gpu_->positionReadIndex = 0;
    gpu_->previousReadIndex = 0;
    gpu_->elapsed = 0.0f;
    gpu_->pinsDirty = true;
    gpu_->stateDirty = true;
}

void Cloth::updateGpu(float dt) {
    if (!gpu_ || !gpu_->ready) return;

    const float clampedDt = std::clamp(dt, 0.0f, 1.0f / 15.0f);
    const float fixedStep = settings_.fixedTimeStep > 0.0f ? settings_.fixedTimeStep : clampedDt;
    if (fixedStep <= 0.0f) return;

    uploadGpuPinsIfNeeded();
    if (gpu_->stateDirty) {
        initializeGpuState();
    }
    gpu_->elapsed += clampedDt;
    accumulator_ += clampedDt;
    int stepCount = 0;
    while (accumulator_ + kEpsilon >= fixedStep && stepCount < 8) {
        const int substeps = std::max(1, settings_.substeps);
        const float subDt = fixedStep / static_cast<float>(substeps);
        for (int i = 0; i < substeps; ++i) {
            stepGpu(subDt);
        }
        accumulator_ -= fixedStep;
        ++stepCount;
    }

    syncParticlesFromGpu();
    if (activeBackend_ != ClothSettings::SolverBackend::TexturePingPong) {
        return;
    }
    if (settings_.recomputeNormals) {
        recomputeNormalsCpu();
    }
    rebuildMeshes();
}

void Cloth::stepGpu(float dt) {
    if (!gpu_ || !gpu_->ready) return;

    solveGpuConstraints(dt);

    renderGpuPass(gpu_->positionWrite(),
                  &gpu_->positionRead().getTexture(),
                  &gpu_->previousRead().getTexture(),
                  dt,
                  1);
    renderGpuPass(gpu_->previousWrite(),
                  &gpu_->positionRead().getTexture(),
                  &gpu_->previousRead().getTexture(),
                  dt,
                  4);
    gpu_->swapPosition();
    gpu_->swapPrevious();

    solveGpuConstraints(dt);

    if (!sphereColliders_.empty()) {
        renderGpuPass(gpu_->positionWrite(),
                      &gpu_->positionRead().getTexture(),
                      &gpu_->previousRead().getTexture(),
                      dt,
                      3);
        gpu_->swapPosition();
    }
}

void Cloth::solveGpuConstraints(float dt) {
    if (!gpu_ || !gpu_->ready) return;

    for (int i = 0; i < settings_.constraintIterations; ++i) {
        renderGpuPass(gpu_->positionWrite(),
                      &gpu_->positionRead().getTexture(),
                      &gpu_->previousRead().getTexture(),
                      dt,
                      2);
        gpu_->swapPosition();
    }
}

void Cloth::initializeGpuState() {
    if (!gpu_ || !gpu_->ready) return;

    renderGpuPass(gpu_->position[0], &gpu_->position[1].getTexture(), &gpu_->previous[1].getTexture(), 0.0f, 0);
    renderGpuPass(gpu_->position[1], &gpu_->position[0].getTexture(), &gpu_->previous[1].getTexture(), 0.0f, 0);
    renderGpuPass(gpu_->previous[0], &gpu_->position[0].getTexture(), &gpu_->previous[1].getTexture(), 0.0f, 0);
    renderGpuPass(gpu_->previous[1], &gpu_->position[0].getTexture(), &gpu_->previous[0].getTexture(), 0.0f, 0);
    gpu_->positionReadIndex = 0;
    gpu_->previousReadIndex = 0;
    gpu_->stateDirty = false;
}

void Cloth::renderGpuPass(tc::Fbo& target,
                          const tc::Texture* position,
                          const tc::Texture* previous,
                          float dt,
                          int mode) {
    if (!gpu_ || !gpu_->ready) return;

    const float dx = settings_.columns > 1 ? settings_.width / static_cast<float>(settings_.columns - 1) : 0.0f;
    const float dy = settings_.rows > 1 ? settings_.height / static_cast<float>(settings_.rows - 1) : 0.0f;
    tc::Vec3 force = gravity_ + globalForce_;
    SphereCollider sphere;
    sphere.radius = -1.0f;
    if (!sphereColliders_.empty()) {
        sphere = sphereColliders_.front();
    }

    ClothStepUniforms uniforms;
    uniforms.simSize[0] = static_cast<float>(settings_.columns);
    uniforms.simSize[1] = static_cast<float>(settings_.rows);
    uniforms.simSize[2] = 1.0f / static_cast<float>(std::max(1, settings_.columns));
    uniforms.simSize[3] = 1.0f / static_cast<float>(std::max(1, settings_.rows));
    uniforms.timing[0] = std::max(0.0f, dt);
    uniforms.timing[1] = settings_.damping;
    uniforms.timing[2] = gpu_->elapsed;
    uniforms.timing[3] = static_cast<float>(mode);
    uniforms.wind[0] = windDirection_.x;
    uniforms.wind[1] = windDirection_.y;
    uniforms.wind[2] = windDirection_.z;
    uniforms.wind[3] = settings_.enableWind ? windStrength_ : 0.0f;
    uniforms.clothLayout[0] = settings_.origin.x;
    uniforms.clothLayout[1] = settings_.origin.y;
    uniforms.clothLayout[2] = settings_.width;
    uniforms.clothLayout[3] = settings_.height;
    uniforms.forces[0] = force.x;
    uniforms.forces[1] = force.y;
    uniforms.forces[2] = force.z;
    uniforms.forces[3] = 0.0f;
    uniforms.stiffness[0] = settings_.structuralStiffness;
    uniforms.stiffness[1] = settings_.shearStiffness;
    uniforms.stiffness[2] = settings_.bendStiffness;
    uniforms.stiffness[3] = 0.0f;
    uniforms.collider[0] = sphere.center.x;
    uniforms.collider[1] = sphere.center.y;
    uniforms.collider[2] = sphere.center.z;
    uniforms.collider[3] = std::max(-1.0f, sphere.radius);
    uniforms.options[0] = settings_.enableShearConstraints ? 1.0f : 0.0f;
    uniforms.options[1] = settings_.enableBendConstraints ? 1.0f : 0.0f;
    uniforms.options[2] = settings_.enableWind ? 1.0f : 0.0f;
    uniforms.options[3] = 0.30f;

    gpu_->stepShader.setInput(0, position);
    gpu_->stepShader.setInput(1, previous);
    gpu_->stepShader.setInput(2, &gpu_->pinMask);
    target.begin(0.0f, 0.0f, 0.0f, 1.0f);
    gpu_->stepShader.setParams(uniforms);
    gpu_->stepShader.draw();
    target.end();
}

void Cloth::syncParticlesFromGpu() {
    if (!gpu_ || !gpu_->ready) return;
    if (!gpu_->positionRead().readPixelsFloat(gpu_->readback.data())) {
        gpu_->ready = false;
        activeBackend_ = ClothSettings::SolverBackend::CpuReference;
        backendReason_ = "TexturePingPong readback unavailable; using CPU reference fallback";
        return;
    }

    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        auto& particle = particles_[i];
        const tc::Vec3 previous = particle.position;
        const std::size_t base = i * 4;
        tc::Vec3 next(gpu_->readback[base + 0], gpu_->readback[base + 1], gpu_->readback[base + 2]);
        if (!finite(next)) {
            next = particle.initialPosition;
        }
        if (particle.pinned) {
            next = particle.pinnedPosition;
        }
        minZ = std::min(minZ, next.z);
        maxZ = std::max(maxZ, next.z);
        particle.previousPosition = previous;
        particle.position = next;
    }
    gpu_->minZ = minZ;
    gpu_->maxZ = maxZ;
    backendReason_ = "TexturePingPong GPU position backend active z=" +
                     tc::toString(maxZ - minZ, 1);
}

void Cloth::stepCpu(float dt) {
    for (int i = 0; i < settings_.constraintIterations; ++i) {
        solveConstraintsCpu();
    }
    accumulateForcesCpu();
    integrateCpu(dt);
    collideCpu();
}

void Cloth::accumulateForcesCpu() {
    for (auto& particle : particles_) {
        particle.acceleration = tc::Vec3(0.0f);
        if (!particle.pinned && particle.inverseMass > 0.0f) {
            particle.acceleration = gravity_ + globalForce_;
        }
    }

    if (!settings_.enableWind || windStrength_ <= 0.0f || particles_.empty()) {
        return;
    }

    for (int y = 0; y < settings_.rows - 1; ++y) {
        for (int x = 0; x < settings_.columns - 1; ++x) {
            addWindForTriangleCpu(index(x + 1, y), index(x, y), index(x, y + 1));
            addWindForTriangleCpu(index(x + 1, y + 1), index(x + 1, y), index(x, y + 1));
        }
    }
}

void Cloth::addWindForTriangleCpu(int a, int b, int c) {
    const tc::Vec3& pa = particles_[a].position;
    const tc::Vec3& pb = particles_[b].position;
    const tc::Vec3& pc = particles_[c].position;
    const tc::Vec3 normal = (pb - pa).cross(pc - pa);
    const float normalLength = normal.length();
    if (normalLength <= kEpsilon || !finite(normal)) return;

    const tc::Vec3 unitNormal = normal / normalLength;
    const float projection = unitNormal.dot(windDirection_);
    if (std::abs(projection) <= kEpsilon) return;

    const tc::Vec3 force = normal * (projection * windStrength_);
    for (int idx : {a, b, c}) {
        auto& particle = particles_[idx];
        if (!particle.pinned && particle.inverseMass > 0.0f) {
            particle.acceleration += force * particle.inverseMass;
        }
    }
}

void Cloth::integrateCpu(float dt) {
    const float dtSq = dt * dt;

    for (auto& particle : particles_) {
        if (particle.pinned || particle.inverseMass <= 0.0f) {
            particle.position = particle.pinnedPosition;
            particle.previousPosition = particle.pinnedPosition;
            particle.acceleration = tc::Vec3(0.0f);
            continue;
        }

        const tc::Vec3 velocity = (particle.position - particle.previousPosition) * (1.0f - settings_.damping);
        const tc::Vec3 next = particle.position + velocity + particle.acceleration * dtSq;
        particle.previousPosition = particle.position;
        particle.position = finite(next) ? next : particle.initialPosition;
        particle.acceleration = tc::Vec3(0.0f);
    }
    for (int i = 0; i < settings_.constraintIterations; ++i) {
        solveConstraintsCpu();
    }
}

void Cloth::solveConstraintsCpu() {
    if (constraints_.empty()) return;

    for (const auto& constraint : constraints_) {
        auto& a = particles_[constraint.a];
        auto& b = particles_[constraint.b];
        const float invA = a.pinned ? 0.0f : a.inverseMass;
        const float invB = b.pinned ? 0.0f : b.inverseMass;
        const float invTotal = invA + invB;
        if (invTotal <= 0.0f) continue;

        const tc::Vec3 delta = b.position - a.position;
        const float len = delta.length();
        if (len <= kEpsilon || !finite(delta)) continue;

        const float error = (len - constraint.restLength) / len;
        const tc::Vec3 correction = delta * (error * constraint.stiffness);
        if (invA > 0.0f) {
            a.position += correction * (invA / invTotal);
        }
        if (invB > 0.0f) {
            b.position -= correction * (invB / invTotal);
        }
        if (!finite(a.position)) a.position = a.initialPosition;
        if (!finite(b.position)) b.position = b.initialPosition;
    }
}

void Cloth::collideCpu() {
    for (auto& particle : particles_) {
        if (particle.pinned) {
            particle.position = particle.pinnedPosition;
            continue;
        }

        for (const auto& sphere : sphereColliders_) {
            const float radius = std::max(0.0f, sphere.radius);
            if (radius <= 0.0f) continue;
            tc::Vec3 delta = particle.position - sphere.center;
            const float dist = delta.length();
            if (dist < radius) {
                const tc::Vec3 dir = dist > kEpsilon ? delta / dist : tc::Vec3(0.0f, 1.0f, 0.0f);
                particle.position = sphere.center + dir * radius;
            }
        }

        for (const auto& plane : planeColliders_) {
            const tc::Vec3 n = safeNormal(plane.normal, tc::Vec3(0.0f, 1.0f, 0.0f));
            const float signedDistance = particle.position.dot(n) + plane.distance;
            if (signedDistance < 0.0f) {
                particle.position += -n * signedDistance;
            }
        }
    }
}

void Cloth::recomputeNormalsCpu() {
    if (particles_.empty()) return;

    for (int y = 0; y < settings_.rows; ++y) {
        for (int x = 0; x < settings_.columns; ++x) {
            const tc::Vec3 c = particlePosition(x, y);
            const tc::Vec3 r = particlePosition(std::min(x + 1, settings_.columns - 1), y);
            const tc::Vec3 l = particlePosition(std::max(x - 1, 0), y);
            const tc::Vec3 u = particlePosition(x, std::max(y - 1, 0));
            const tc::Vec3 d = particlePosition(x, std::min(y + 1, settings_.rows - 1));

            tc::Vec3 n = (r - c).cross(u - c) +
                         (u - c).cross(l - c) +
                         (l - c).cross(d - c) +
                         (d - c).cross(r - c);
            if (n.length() <= kEpsilon || !finite(n)) {
                n = tc::Vec3(0.0f, 0.0f, 1.0f);
            } else {
                n.normalize();
            }
            particles_[index(x, y)].normal = n;
        }
    }
}

void Cloth::rebuildMeshes() {
    fillMesh_.clear();
    fillMesh_.setMode(tc::PrimitiveMode::Triangles);
    for (const auto& particle : particles_) {
        fillMesh_.addVertex(particle.position);
        fillMesh_.addNormal(particle.normal);
        fillMesh_.addTexCoord(particle.uv);
        fillMesh_.addColor(shadedColor(particle.normal, particle.uv));
    }
    fillMesh_.addIndices(triangleIndices_);
    fillMesh_.markGpuDirty();

    wireMesh_.clear();
    wireMesh_.setMode(tc::PrimitiveMode::Lines);
    for (const auto& particle : particles_) {
        wireMesh_.addVertex(particle.position);
        wireMesh_.addColor(tc::Color(0.90f, 0.96f, 1.0f, 0.58f));
    }
    wireMesh_.addIndices(wireIndices_);
    wireMesh_.markGpuDirty();
}

tc::Color Cloth::shadedColor(const tc::Vec3& normal, const tc::Vec2& uv) const {
    const tc::Vec3 light = tc::Vec3(-0.25f, -0.55f, 0.95f).normalized();
    const float diffuse = std::max(0.0f, normal.normalized().dot(light));
    const float shade = 0.34f + 0.66f * diffuse;
    const tc::Color a(0.12f, 0.56f, 0.94f, 0.82f);
    const tc::Color b(0.98f, 0.55f, 0.18f, 0.82f);
    const float mix = clamp01(uv.x * 0.65f + uv.y * 0.25f);
    return tc::Color((a.r + (b.r - a.r) * mix) * shade,
                     (a.g + (b.g - a.g) * mix) * shade,
                     (a.b + (b.b - a.b) * mix) * shade,
                     a.a);
}

bool Cloth::finite(const tc::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace tcxCloth
