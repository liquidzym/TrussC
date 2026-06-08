#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <cmath>
#include <string>

void tcApp::setup() {
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float t = tc::getElapsedTimef();
    const tc::Vec2 center(tc::getWindowWidth() * 0.5f, tc::getWindowHeight() * 0.5f);
    const tc::Vec2 orbit(center.x + std::sin(t) * 180.0f, center.y + std::cos(t * 1.2f) * 120.0f);
    fluid_.addVelocity(orbit, 90.0f, tc::Vec2(std::cos(t) * 90.0f, -std::sin(t) * 90.0f));
    fluid_.addDensity(orbit, 42.0f, tc::Color(0.1f, 0.5f, 1.0f, 0.65f));
    fluid_.update(dt);
    particles_.update(fluid_, dt);
}

void tcApp::draw() {
    tc::clear(0.02f, 0.025f, 0.03f);
    if (showFluid_) {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    particles_.draw(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    tc::setColor(1.0f);
    const std::string mode = particles_.lastUpdateUsedGpu() ? "GPU particles" : "CPU fallback particles";
    tc::drawBitmapString("particles | " + mode + " | f fluid | b birth-from-velocity | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("count " + tc::toString(particles_.particleCount()) +
                             " | age/lifespan/mass/size spread on | birth " +
                             std::string(particles_.settings().birthFromVelocity ? "velocity" : "center"),
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'f')) showFluid_ = !showFluid_;
    if (keyIs(key, 'b')) {
        particles_.settings().birthFromVelocity = !particles_.settings().birthFromVelocity;
        particles_.reset();
    }
    if (keyIs(key, 'r')) particles_.reset();
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = 0.25f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), fluidSettings);

    tcx::flow::ParticleFlowSettings particleSettings;
    particleSettings.particleCount = 12000;
    particleSettings.lifetime = 5.5f;
    particleSettings.lifespanSpread = 0.28f;
    particleSettings.velocityScale = 8.0f;
    particleSettings.spawnRadius = 160.0f;
    particleSettings.mass = 1.0f;
    particleSettings.massSpread = 0.45f;
    particleSettings.sizeSpread = 0.35f;
    particleSettings.birthFromVelocity = true;
    particleSettings.birthVelocityScale = 1.25f;
    particleSettings.birthVelocityJitter = 0.12f;
    particleSettings.ageFadePower = 1.15f;
    particles_.setup(tc::getWindowWidth(), tc::getWindowHeight(), particleSettings);
}
