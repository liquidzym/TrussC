#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {

float envFloat(const char* name, float fallback, float minValue, float maxValue) {
    if (const char* value = std::getenv(name)) {
        return std::clamp(static_cast<float>(std::atof(value)), minValue, maxValue);
    }
    return fallback;
}

int envInt(const char* name, int fallback, int minValue, int maxValue) {
    if (const char* value = std::getenv(name)) {
        return std::clamp(std::atoi(value), minValue, maxValue);
    }
    return fallback;
}

float hash01(int n) {
    const float x = std::sin(static_cast<float>(n) * 12.9898f + 78.233f) * 43758.5453f;
    return x - std::floor(x);
}

float trailLengthForPreset(int preset) {
    const float values[] = {8.0f, 15.0f, 32.0f};
    return values[std::clamp(preset, 0, 2)];
}

tc::Vec2 rotate90(const tc::Vec2& v) {
    return tc::Vec2(-v.y, v.x);
}

} // namespace

void tcApp::setup() {
    particleCount_ = envInt("TCX_PHYSARUM_PARTICLES", particleCount_, 1000, 160000);
    trailPreset_ = static_cast<int>(envFloat("TCX_PHYSARUM_TRAIL_PRESET", static_cast<float>(trailPreset_), 0.0f, 2.0f));
    flowRangeScale_ = envFloat("TCX_PHYSARUM_FLOW_RANGE", flowRangeScale_, 0.25f, 1.20f);
    flowStrengthScale_ = envFloat("TCX_PHYSARUM_FLOW_STRENGTH", flowStrengthScale_, 0.35f, 1.35f);
    resizeSystems();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = std::min(1.0f / 30.0f, static_cast<float>(tc::getDeltaTime()));
    const float time = tc::getElapsedTimef();
    const float measuredFps = tc::getFps();
    if (std::isfinite(measuredFps) && measuredFps > 0.0f) {
        fps_ = fps_ <= 0.0f ? measuredFps : fps_ * 0.88f + measuredFps * 0.12f;
        frameMs_ = fps_ > 0.0f ? 1000.0f / fps_ : 0.0f;
    }
    if (!paused_) {
        if (autoFlow_) {
            injectAutonomousFlow(time);
        }
        handlePointerInput();
        fluid_.update(dt);
        gpuTrail_.settings().inkColor.a = showParticles_ ? 1.0f : 0.0f;
        if (useGpuTrail_) {
            gpuTrail_.update(fluid_, dt);
        }
        if (!gpuTrailActive()) {
            fluid_.refreshVelocityReadback();
            updateStrokeParticles(dt);
        }
    } else {
        previousMouse_ = tc::getMousePos();
    }
}

void tcApp::draw() {
    drawFluidView();

    if (!showHud_) return;
    tc::setColor(0.0f, 0.0f, 0.0f, 0.32f);
    tc::drawRect(0, 0, tc::getWindowWidth(), 148.0f);
    tc::setColor(1.0f);
    tc::drawBitmapString("fluid-physarum-trails | move/drag inject velocity | 1 fluid 2 pressure 3 velocity",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("OpenProcessing GPU-IO Physarum-inspired | " +
                             std::string("fluid-driven stroke particles") +
                             " | particles " + tc::toString(gpuTrailActive() ? gpuTrail_.particleCount()
                                                                              : static_cast<int>(strokeParticles_.size())) +
                             " | view " + viewName(),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
    tc::drawBitmapString("a demo-flow " + std::string(autoFlow_ ? "on" : "off") +
                             " | g gpu | m move-inject | o particles | +/- trail " +
                             tc::toString(trailLengthForPreset(trailPreset_), 0) +
                             " | [] range " + tc::toString(flowRangeScale_, 2) +
                             " | p pause | h hud | r reset | " +
                             std::string(paused_ ? "paused" : "running"),
                         18, 28 + tcx::flow::example::kHudLine * 2.0f,
                         tcx::flow::example::kHudScale);
    tc::drawBitmapString("fps " + tc::toString(fps_, 1) +
                             " | " + tc::toString(frameMs_, 1) + " ms" +
                             " | path " + std::string(gpuTrailActive() ? "gpu-pingpong + gpu-trail"
                                                                       : "cpu-readback + batched mesh") +
                             " | batches " + tc::toString(gpuTrailActive() ? 1 : lastLineBatches_ + lastPointBatches_) +
                             " | vertices " + tc::toString(gpuTrailActive() ? gpuTrail_.lastDepositVertices()
                                                                            : lastTrailVertices_),
                         18, 28 + tcx::flow::example::kHudLine * 3.0f,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit == 1) viewMode_ = ViewMode::Fluid;
    if (digit == 2) viewMode_ = ViewMode::Pressure;
    if (digit == 3) viewMode_ = ViewMode::Velocity;
    if (keyIs(key, 'a')) autoFlow_ = !autoFlow_;
    if (keyIs(key, 'g')) {
        useGpuTrail_ = !useGpuTrail_;
        resetSystems();
    }
    if (keyIs(key, 'm')) injectOnMove_ = !injectOnMove_;
    if (keyIs(key, 'o')) showParticles_ = !showParticles_;
    if (keyIs(key, 'p')) paused_ = !paused_;
    if (keyIs(key, 'h')) showHud_ = !showHud_;
    if (key == static_cast<int>('+') || key == static_cast<int>('=')) {
        trailPreset_ = std::clamp(trailPreset_ + 1, 0, 2);
        applyTrailPreset();
    }
    if (key == static_cast<int>('-') || key == static_cast<int>('_')) {
        trailPreset_ = std::clamp(trailPreset_ - 1, 0, 2);
        applyTrailPreset();
    }
    if (key == static_cast<int>('[') || key == static_cast<int>('{')) {
        flowRangeScale_ = std::max(0.25f, flowRangeScale_ - 0.06f);
    }
    if (key == static_cast<int>(']') || key == static_cast<int>('}')) {
        flowRangeScale_ = std::min(1.20f, flowRangeScale_ + 0.06f);
    }
    if (keyIs(key, 'r')) {
        resetSystems();
        paused_ = false;
    }
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    previousMouse_ = pos;
    activeMouseButton_ = button;
}

void tcApp::mouseReleased(tc::Vec2 pos, int button) {
    (void)pos;
    (void)button;
    activeMouseButton_ = -1;
}

void tcApp::mouseDragged(tc::Vec2 pos, int button) {
    (void)pos;
    activeMouseButton_ = button;
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    const int w = std::max(1, tc::getWindowWidth());
    const int h = std::max(1, tc::getWindowHeight());

    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = 0.125f;
    fluidSettings.outputResolutionScale = 1.0f;
    fluidSettings.timestep = 1.0f;
    fluidSettings.solverIterations = 3;
    fluidSettings.enableVorticity = false;
    fluidSettings.vorticity = 0.0f;
    fluidSettings.velocityDissipation = 1.0f;
    fluidSettings.densityDissipation = 0.995f;
    fluidSettings.viscosity = 0.0f;
    fluidSettings.velocitySplatMode = tcx::flow::FluidVelocitySplatMode::AdditiveClamp;
    fluidSettings.velocitySplatClamp = 30.0f;
    fluidSettings.persistentPressure = true;
    fluidSettings.wrapEdges = true;
    fluid_.setup(w, h, fluidSettings);

    tcx::flow::PhysarumTrailFlowSettings gpuTrailSettings;
    gpuTrailSettings.particleCount = particleCount_;
    gpuTrailSettings.trailLength = trailLengthForPreset(trailPreset_);
    gpuTrailSettings.trailFadeScale = 0.78f;
    gpuTrailSettings.lifetime = 1000.0f / 60.0f;
    gpuTrailSettings.velocityScale = 1.0f;
    gpuTrailSettings.renderSubsteps = 3;
    gpuTrailSettings.maxParticleStep = 10.0f;
    gpuTrailSettings.strokeThickness = 1.15f;
    gpuTrailSettings.inkStrength = 1.24f;
    gpuTrailSettings.dashLength = 0.72f;
    gpuTrailSettings.dashVelocityScale = 0.26f;
    gpuTrailSettings.inkColor = tc::Color(0.010f, 0.014f, 0.095f, showParticles_ ? 1.0f : 0.0f);
    gpuTrailSettings.backgroundColor = tc::Color(0.985f, 0.940f, 0.870f, 1.0f);
    gpuTrailSettings.useGpuParticles = useGpuTrail_;
    gpuTrail_.setup(w, h, gpuTrailSettings);

    if (!trailFbo_.isAllocated() || trailFbo_.getWidth() != w || trailFbo_.getHeight() != h) {
        trailFbo_.allocate(w, h, 1, tc::TextureFormat::RGBA8);
        trailNeedsClear_ = true;
    }
    clearTrailBuffer();

    const int requested = std::min(particleCount_, std::max(4096, static_cast<int>(w * h * 0.12f)));
    strokeParticles_.resize(static_cast<std::size_t>(requested));
    resetStrokeParticles();

    previousMouse_ = tc::getMousePos();
}

void tcApp::resetSystems() {
    resizeSystems();
    fluid_.reset();
    resetStrokeParticles();
    clearTrailBuffer();
}

void tcApp::resetStrokeParticles() {
    for (std::size_t i = 0; i < strokeParticles_.size(); ++i) {
        respawnStrokeParticle(strokeParticles_[i], static_cast<int>(i));
        strokeParticles_[i].age = hash01(static_cast<int>(i) * 131 + 19) * strokeParticles_[i].lifetime;
    }
}

void tcApp::injectAutonomousFlow(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float s = std::min(w, h);
    auto injectVortex = [&](const tc::Vec2& baseCenter, float orbit, float strength,
                            float spin, float phase, int samples) {
        for (int i = 0; i < samples; ++i) {
            const float a = phase + static_cast<float>(i) * 6.2831853f / static_cast<float>(samples);
            const tc::Vec2 radial(std::cos(a), std::sin(a));
            const tc::Vec2 p = baseCenter + radial * orbit;
            const tc::Vec2 tangent(-radial.y, radial.x);
            const tc::Vec2 inward = (baseCenter - p) * 0.055f;
            fluid_.addVelocity(p,
                               orbit * 0.42f * flowRangeScale_,
                               (tangent * (strength * spin) + inward) * flowStrengthScale_);
        }
    };

    const tc::Vec2 c0(w * 0.38f + std::sin(time * 0.13f) * w * 0.035f,
                      h * 0.40f + std::cos(time * 0.17f) * h * 0.030f);
    const tc::Vec2 c1(w * 0.55f + std::cos(time * 0.11f) * w * 0.040f,
                      h * 0.53f + std::sin(time * 0.19f) * h * 0.035f);
    const tc::Vec2 c2(w * 0.47f + std::sin(time * 0.09f + 1.7f) * w * 0.050f,
                      h * 0.70f + std::cos(time * 0.14f) * h * 0.025f);
    const tc::Vec2 c3(w * 0.68f + std::cos(time * 0.10f + 0.6f) * w * 0.030f,
                      h * 0.42f + std::sin(time * 0.16f + 1.1f) * h * 0.030f);

    injectVortex(c0, s * 0.090f, 300.0f, 1.0f, time * 0.20f, 18);
    injectVortex(c1, s * 0.120f, 255.0f, -1.0f, time * -0.16f + 0.8f, 20);
    injectVortex(c2, s * 0.082f, 285.0f, 1.0f, time * 0.24f + 1.5f, 16);
    injectVortex(c3, s * 0.070f, 230.0f, -1.0f, time * -0.19f + 2.2f, 14);

    const tc::Vec2 inlet(w * 0.16f, h * (0.28f + std::sin(time * 0.22f) * 0.13f));
    fluid_.addVelocity(inlet,
                       s * 0.18f * flowRangeScale_,
                       tc::Vec2(260.0f, 46.0f * std::sin(time * 0.65f)) * flowStrengthScale_);
    const tc::Vec2 shear(w * 0.78f, h * (0.34f + std::cos(time * 0.18f) * 0.16f));
    fluid_.addVelocity(shear,
                       s * 0.16f * flowRangeScale_,
                       tc::Vec2(-210.0f, 58.0f * std::cos(time * 0.50f)) * flowStrengthScale_);
}

void tcApp::handlePointerInput() {
    const tc::Vec2 mouse = tc::getMousePos();
    const bool pressed = tc::isMousePressed();
    if (pressed && activeMouseButton_ < 0) {
        activeMouseButton_ = tc::getMouseButton();
    }

    const tc::Vec2 delta = mouse - previousMouse_;
    const float distance = delta.length();
    const bool shouldInject = distance > 0.25f && (pressed || injectOnMove_);
    if (shouldInject) {
        injectPointerSegment(mouse, previousMouse_, pressed ? activeMouseButton_ : -1);
    }

    previousMouse_ = mouse;
    if (!pressed) {
        activeMouseButton_ = -1;
    }
}

void tcApp::injectPointerSegment(const tc::Vec2& current, const tc::Vec2& previous, int button) {
    const tc::Vec2 delta = current - previous;
    const float distance = std::max(1.0f, delta.length());
    const int steps = std::clamp(static_cast<int>(std::ceil(distance / 18.0f)), 1, 24);
    const float baseRadius = (button == 2 ? 36.0f : 30.0f) * flowRangeScale_;
    tc::Vec2 velocity(delta.x, delta.y);
    velocity = velocity * (2.0f * flowStrengthScale_);
    const float speed = velocity.length();
    const float maxSpeed = 30.0f;
    if (speed > maxSpeed) {
        velocity = velocity * (maxSpeed / speed);
    }
    for (int i = 0; i < steps; ++i) {
        const float t = steps <= 1 ? 1.0f : static_cast<float>(i) / static_cast<float>(steps - 1);
        const tc::Vec2 p = previous + delta * t;
        fluid_.addVelocity(p, baseRadius, velocity);
    }
}

void tcApp::updateStrokeParticles(float dt) {
    if (strokeParticles_.empty()) return;
    const float w = static_cast<float>(std::max(1, tc::getWindowWidth()));
    const float h = static_cast<float>(std::max(1, tc::getWindowHeight()));
    const int substeps = 3;
    const float invSubsteps = 1.0f / static_cast<float>(substeps);
    const float frameScale = std::clamp(dt * 60.0f, 0.25f, 2.0f);

    for (std::size_t i = 0; i < strokeParticles_.size(); ++i) {
        auto& particle = strokeParticles_[i];
        particle.previous = particle.position;
        particle.age += dt;
        if (particle.age >= particle.lifetime) {
            respawnStrokeParticle(particle, static_cast<int>(i));
            continue;
        }

        for (int stepIndex = 0; stepIndex < substeps; ++stepIndex) {
            tc::Vec2 velocity = fluid_.sampleVelocityAtPosition(particle.position);
            const float speed = velocity.length();
            if (speed > 0.0001f) {
                tc::Vec2 dir = velocity / speed;
                const float pxStep = std::clamp(speed * 0.038f * frameScale * invSubsteps, 0.0f, 7.0f);
                particle.position += dir * pxStep;
            }

            bool wrapped = false;
            if (particle.position.x < 0.0f) {
                particle.position.x += w;
                wrapped = true;
            } else if (particle.position.x >= w) {
                particle.position.x -= w;
                wrapped = true;
            }
            if (particle.position.y < 0.0f) {
                particle.position.y += h;
                wrapped = true;
            } else if (particle.position.y >= h) {
                particle.position.y -= h;
                wrapped = true;
            }
            if (wrapped) {
                particle.previous = particle.position;
            }
        }
    }
}

void tcApp::drawStrokeParticles() const {
    constexpr int kMaxBatchVertices = 32000;
    lastLineBatches_ = 0;
    lastPointBatches_ = 0;
    lastTrailVertices_ = 0;

    tc::Mesh lineMesh;
    lineMesh.setMode(tc::PrimitiveMode::Lines);
    lineMesh.getVertices().reserve(kMaxBatchVertices);
    lineMesh.getColors().reserve(kMaxBatchVertices);

    tc::Mesh pointMesh;
    pointMesh.setMode(tc::PrimitiveMode::Points);
    pointMesh.getVertices().reserve(kMaxBatchVertices);
    pointMesh.getColors().reserve(kMaxBatchVertices);

    auto flushLines = [&]() {
        if (lineMesh.getNumVertices() == 0) return;
        lastTrailVertices_ += lineMesh.getNumVertices();
        lineMesh.draw();
        lineMesh.clear();
        ++lastLineBatches_;
    };
    auto flushPoints = [&]() {
        if (pointMesh.getNumVertices() == 0) return;
        lastTrailVertices_ += pointMesh.getNumVertices();
        pointMesh.draw();
        pointMesh.clear();
        ++lastPointBatches_;
    };

    for (const auto& particle : strokeParticles_) {
        const tc::Vec2 delta = particle.position - particle.previous;
        const float len = delta.length();
        if (len > 18.0f) continue;

        const float ageFraction = std::clamp(particle.age / std::max(0.0001f, particle.lifetime), 0.0f, 1.0f);
        const float fadeIn = std::clamp(ageFraction * 10.0f, 0.0f, 1.0f);
        const float speedAlpha = std::clamp(len / 5.0f, 0.45f, 1.0f);
        const float alpha = std::clamp((0.060f + 0.155f * speedAlpha) * fadeIn * particle.shade,
                                       0.0f, 0.28f);

        tc::Vec2 dir;
        if (len < 0.45f) {
            const float angle = particle.seed * 6.2831853f;
            dir = tc::Vec2(std::cos(angle), std::sin(angle));
        } else {
            dir = delta / len;
        }

        const float dash = std::clamp(0.72f + len * 0.26f, 0.65f, 2.8f);
        const float sampleCount = 3.0f;
        for (int sample = 0; sample < 3; ++sample) {
            const float t = sampleCount <= 1.0f ? 1.0f : static_cast<float>(sample) / (sampleCount - 1.0f);
            const tc::Vec2 p = particle.previous + delta * t;
            const float sampleAlpha = alpha * (sample == 1 ? 1.0f : 0.82f);
            if (lineMesh.getNumVertices() + 2 > kMaxBatchVertices) {
                flushLines();
            }
            lineMesh.addColor(0.010f, 0.014f, 0.095f, sampleAlpha);
            lineMesh.addVertex(p.x - dir.x * dash * 0.5f, p.y - dir.y * dash * 0.5f);
            lineMesh.addColor(0.010f, 0.014f, 0.095f, sampleAlpha);
            lineMesh.addVertex(p.x + dir.x * dash * 0.5f, p.y + dir.y * dash * 0.5f);

            if (len < 1.2f) {
                if (pointMesh.getNumVertices() + 1 > kMaxBatchVertices) {
                    flushPoints();
                }
                pointMesh.addColor(0.010f, 0.014f, 0.095f, sampleAlpha * 0.82f);
                pointMesh.addVertex(p.x, p.y);
            }
        }
    }

    flushLines();
    flushPoints();
}

void tcApp::respawnStrokeParticle(StrokeParticle& particle, int index) {
    const float w = static_cast<float>(std::max(1, tc::getWindowWidth()));
    const float h = static_cast<float>(std::max(1, tc::getWindowHeight()));
    const float u = hash01(index * 17 + 3);
    const float v = hash01(index * 29 + 11);
    particle.position = tc::Vec2(u * w, v * h);
    particle.previous = particle.position;
    particle.seed = hash01(index * 43 + 7);
    particle.shade = 0.58f + hash01(index * 59 + 23) * 0.72f;
    particle.lifetime = 14.0f + hash01(index * 71 + 31) * 8.0f;
    particle.age = 0.0f;
}

void tcApp::drawFluidView() {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    switch (viewMode_) {
        case ViewMode::Fluid:
            tc::clear(0.985f, 0.940f, 0.870f);
            if (gpuTrailActive()) {
                gpuTrail_.draw(0, 0, w, h);
            } else {
                updateTrailBuffer();
                if (trailFbo_.isAllocated()) {
                    tc::setColor(1.0f);
                    trailFbo_.draw(0, 0, w, h);
                }
            }
            break;
        case ViewMode::Pressure:
            tc::clear(0.0f, 0.0f, 0.0f);
            fluid_.drawPressure(0, 0, w, h);
            break;
        case ViewMode::Velocity:
            tc::clear(0.985f, 0.94f, 0.86f);
            visualizer_.drawVelocityField(fluid_, 0, 0, w, h, 96, 54, 0.075f);
            break;
    }
}

void tcApp::updateTrailBuffer() {
    if (!trailFbo_.isAllocated()) return;
    if (trailNeedsClear_) {
        clearTrailBuffer();
    }
    if (paused_) return;

    const float fadeAlpha = std::clamp(0.78f / std::max(1.0f, trailLengthForPreset(trailPreset_)), 0.008f, 0.08f);
    trailFbo_.begin();
    tc::setColor(0.985f, 0.940f, 0.870f, fadeAlpha);
    tc::drawRect(0, 0, trailFbo_.getWidth(), trailFbo_.getHeight());
    if (showParticles_) {
        drawStrokeParticles();
    }
    trailFbo_.end();
}

void tcApp::clearTrailBuffer() {
    if (!trailFbo_.isAllocated()) return;
    trailFbo_.begin(0.985f, 0.940f, 0.870f, 1.0f);
    trailFbo_.end();
    trailNeedsClear_ = false;
}

void tcApp::applyTrailPreset() {
    const float trailLength = trailLengthForPreset(trailPreset_);
    gpuTrail_.settings().trailLength = trailLength;
}

bool tcApp::gpuTrailActive() const {
    return useGpuTrail_ && gpuTrail_.isReady();
}

std::string tcApp::viewName() const {
    switch (viewMode_) {
        case ViewMode::Fluid: return "fluid";
        case ViewMode::Pressure: return "pressure";
        case ViewMode::Velocity: return "velocity";
    }
    return "unknown";
}
