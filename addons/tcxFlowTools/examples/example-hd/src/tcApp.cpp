#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>

void tcApp::setup() {
    if (const char* env = std::getenv("TCX_HD_SCALE")) {
        scale_ = std::clamp(static_cast<float>(std::atof(env)), 0.05f, 1.0f);
    }
    if (const char* env = std::getenv("TCX_HD_OUTPUT_SCALE")) {
        outputScale_ = std::clamp(static_cast<float>(std::atof(env)), 0.05f, 1.0f);
    }
    resizeFluid();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float t = tc::getElapsedTimef();
    for (int i = 0; i < 3; ++i) {
        const float phase = t + i * 2.09439f;
        const tc::Vec2 p(tc::getWindowWidth() * (0.5f + std::sin(phase) * 0.32f),
                         tc::getWindowHeight() * (0.5f + std::cos(phase * 0.8f) * 0.28f));
        fluid_.addVelocity(p, 64.0f, tc::Vec2(std::cos(phase) * 70.0f, std::sin(phase) * 70.0f));
        fluid_.addDensity(p, 48.0f, tc::Color(0.2f + i * 0.25f, 0.75f - i * 0.18f, 1.0f, 1.0f));
    }
    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.035f, 0.04f, 0.045f);
    fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    tc::setColor(1.0f);
    tc::drawBitmapString("hd fluid | 1 1x | 2 0.5x | 3 0.25x | o output", 18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("display " + tc::toString(tc::getWindowWidth()) + "x" + tc::toString(tc::getWindowHeight()),
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
    tc::drawBitmapString("sim " + tc::toString(fluid_.simWidth()) + "x" + tc::toString(fluid_.simHeight()) +
                             " scale " + tc::toString(scale_, 2),
                         18, 28 + tcx::flow::example::kHudLine * 2.0f, tcx::flow::example::kHudScale);
    tc::drawBitmapString("output " + tc::toString(fluid_.outputWidth()) + "x" + tc::toString(fluid_.outputHeight()) +
                             " scale " + tc::toString(outputScale_, 2),
                         18, 28 + tcx::flow::example::kHudLine * 3.0f, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit == 1) { scale_ = 1.0f; resizeFluid(); }
    if (digit == 2) { scale_ = 0.5f; resizeFluid(); }
    if (digit == 3) { scale_ = 0.25f; resizeFluid(); }
    if (keyIs(key, 'o')) {
        outputScale_ = outputScale_ >= 1.0f ? 0.5f : 1.0f;
        resizeFluid();
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeFluid();
}

void tcApp::resizeFluid() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = scale_;
    settings.outputResolutionScale = outputScale_;
    settings.solverIterations = scale_ >= 1.0f ? 10 : 18;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
}
