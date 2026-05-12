#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

void tcApp::setup() {
    if (const char* env = std::getenv("TCX_SPLIT_MODE")) {
        mode_ = std::clamp(static_cast<int>(std::atoi(env)), 0, 2);
    }
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float time = tc::getElapsedTimef();
    injectFluid(time);
    fluid_.update(dt);
    splitVelocity_.update(fluid_, 28, 18);
    splitVelocity_.updateTexture(fluid_, fluid_.outputWidth(), fluid_.outputHeight(), 0.075f, mode_);
}

void tcApp::draw() {
    tc::clear(0.012f, 0.014f, 0.018f);
    if (showDensity_) {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    splitVelocity_.draw(0, 0, tc::getWindowWidth(), tc::getWindowHeight());

    const auto& result = splitVelocity_.result();
    tc::setColor(1.0f);
    const std::string gpu = splitVelocity_.lastUpdateUsedGpu() ? "GPU" : "CPU metrics only";
    tc::drawBitmapString("split-velocity | 1 combined 2 positive 3 negative | d density | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString(modeName() + " | " + gpu + " | pos " +
                             tc::toString(result.positive.x, 2) + "," + tc::toString(result.positive.y, 2) +
                             " neg " + tc::toString(result.negative.x, 2) + "," + tc::toString(result.negative.y, 2),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit >= 1 && digit <= 3) mode_ = digit - 1;
    if (keyIs(key, 'd')) showDensity_ = !showDensity_;
    if (keyIs(key, 'r')) fluid_.reset();
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.5f;
    settings.outputResolutionScale = 1.0f;
    settings.timestep = 0.125f;
    settings.solverIterations = 30;
    settings.enableVorticity = true;
    settings.vorticity = 0.55f;
    settings.velocityDissipation = 0.998f;
    settings.densityDissipation = 0.996f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
}

void tcApp::injectFluid(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const tc::Vec2 center(w * 0.5f, h * 0.5f);
    const float radius = std::min(w, h) * 0.07f;

    const tc::Vec2 p0(center.x + std::sin(time * 0.83f) * w * 0.30f,
                      center.y + std::cos(time * 0.71f) * h * 0.22f);
    const tc::Vec2 p1(center.x + std::sin(time * 1.21f + 2.1f) * w * 0.24f,
                      center.y + std::cos(time * 0.94f + 1.4f) * h * 0.30f);
    fluid_.addVelocity(p0, radius, tc::Vec2(std::cos(time * 0.83f) * 95.0f,
                                           -std::sin(time * 0.71f) * 80.0f));
    fluid_.addVelocity(p1, radius * 0.85f, tc::Vec2(std::cos(time * 1.21f + 2.1f) * -85.0f,
                                                   -std::sin(time * 0.94f + 1.4f) * 92.0f));
    fluid_.addDensity(p0, radius * 0.72f, tc::Color(0.05f, 0.35f, 0.82f, 0.70f));
    fluid_.addDensity(p1, radius * 0.62f, tc::Color(0.12f, 0.72f, 0.95f, 0.62f));
}

std::string tcApp::modeName() const {
    if (mode_ == 1) return "positive";
    if (mode_ == 2) return "negative";
    return "combined";
}
