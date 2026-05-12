#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

void tcApp::setup() {
    if (const char* env = std::getenv("TCX_PARTICLE_VARIANT")) {
        const std::string value(env);
        if (value == "2" || value == "attractor") {
            variant_ = tcx::flow::ParticleFlowVariant::Attractor;
        } else if (value == "3" || value == "impulse") {
            variant_ = tcx::flow::ParticleFlowVariant::Impulse;
        } else {
            variant_ = tcx::flow::ParticleFlowVariant::Flow;
        }
    }
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float time = tc::getElapsedTimef();
    injectFluid(time);
    fluid_.update(dt);
    particles_.settings().variantCenter = tc::Vec2(0.5f + std::sin(time * 0.65f) * 0.24f,
                                                  0.5f + std::cos(time * 0.52f) * 0.22f);
    particles_.update(fluid_, dt);
}

void tcApp::draw() {
    tc::clear(0.014f, 0.016f, 0.022f);
    if (showFluid_) {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    particles_.draw(0, 0, tc::getWindowWidth(), tc::getWindowHeight());

    tc::setColor(1.0f);
    const std::string gpu = particles_.lastUpdateUsedGpu() ? "GPU" : "CPU fallback";
    tc::drawBitmapString("particle-variants | 1 flow 2 attractor 3 impulse | f fluid | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString(modeName() + " | " + gpu + " | count " +
                             tc::toString(particles_.particleCount()),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit == 1) {
        variant_ = tcx::flow::ParticleFlowVariant::Flow;
        configureParticles();
    } else if (digit == 2) {
        variant_ = tcx::flow::ParticleFlowVariant::Attractor;
        configureParticles();
    } else if (digit == 3) {
        variant_ = tcx::flow::ParticleFlowVariant::Impulse;
        configureParticles();
    }
    if (keyIs(key, 'f')) showFluid_ = !showFluid_;
    if (keyIs(key, 'r')) {
        fluid_.reset();
        particles_.reset();
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = 0.35f;
    fluidSettings.timestep = 0.125f;
    fluidSettings.solverIterations = 28;
    fluidSettings.enableVorticity = true;
    fluidSettings.vorticity = 0.55f;
    fluidSettings.velocityDissipation = 0.998f;
    fluidSettings.densityDissipation = 0.996f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), fluidSettings);
    configureParticles();
}

void tcApp::configureParticles() {
    tcx::flow::ParticleFlowSettings settings;
    settings.particleCount = 18000;
    settings.lifetime = 6.0f;
    settings.velocityScale = 10.0f;
    settings.spawnRadius = 180.0f;
    settings.particleSize = 1.6f;
    settings.variant = variant_;
    if (variant_ == tcx::flow::ParticleFlowVariant::Attractor) {
        settings.variantStrength = 0.42f;
        settings.particleColor = tc::Color(1.0f, 0.78f, 0.28f, 0.72f);
    } else if (variant_ == tcx::flow::ParticleFlowVariant::Impulse) {
        settings.variantStrength = 0.34f;
        settings.particleColor = tc::Color(0.35f, 0.90f, 1.0f, 0.70f);
    } else {
        settings.variantStrength = 0.0f;
        settings.particleColor = tc::Color(0.95f, 0.95f, 1.0f, 0.62f);
    }
    particles_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
}

void tcApp::injectFluid(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const tc::Vec2 center(w * 0.5f, h * 0.5f);
    const tc::Vec2 p0(center.x + std::sin(time * 0.82f) * w * 0.28f,
                      center.y + std::cos(time * 0.71f) * h * 0.22f);
    const tc::Vec2 p1(center.x + std::sin(time * 1.17f + 2.4f) * w * 0.18f,
                      center.y + std::cos(time * 0.96f + 1.1f) * h * 0.27f);
    fluid_.addVelocity(p0, 78.0f, tc::Vec2(std::cos(time * 0.82f) * 95.0f,
                                           -std::sin(time * 0.71f) * 80.0f));
    fluid_.addVelocity(p1, 62.0f, tc::Vec2(std::cos(time * 1.17f + 2.4f) * -70.0f,
                                           -std::sin(time * 0.96f + 1.1f) * 90.0f));
    fluid_.addDensity(p0, 34.0f, tc::Color(0.05f, 0.28f, 0.78f, 0.50f));
    fluid_.addDensity(p1, 30.0f, tc::Color(0.08f, 0.58f, 0.95f, 0.45f));
}

std::string tcApp::modeName() const {
    if (variant_ == tcx::flow::ParticleFlowVariant::Attractor) return "attractor";
    if (variant_ == tcx::flow::ParticleFlowVariant::Impulse) return "impulse";
    return "flow";
}
