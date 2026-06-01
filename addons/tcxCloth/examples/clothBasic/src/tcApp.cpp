#include "tcApp.h"

#include <algorithm>
#include <string>

namespace {

float viewWidth() {
    return std::max(640.0f, static_cast<float>(tc::getWindowWidth()));
}

float viewHeight() {
    return std::max(480.0f, static_cast<float>(tc::getWindowHeight()));
}

} // namespace

void tcApp::setup() {
    tc::setWindowTitle("tcxCloth clothBasic");
    tc::setDefaultScreenFov(45.0f);
    tc::setIndependentFps(60.0f, 0.0f);
    rebuild();
}

void tcApp::update() {
    if (!paused_) {
        cloth_.update(static_cast<float>(tc::getDeltaTime()));
    }
    tc::redraw();
}

void tcApp::draw() {
    tc::clear(0.035f, 0.042f, 0.050f);
    cloth_.draw();
    if (showWire_) {
        tc::setColor(0.92f, 0.97f, 1.0f, 0.42f);
        cloth_.drawWire();
    }

    tc::setColor(0.86f, 0.90f, 0.94f, 0.90f);
    tc::drawBitmapString("tcxCloth clothBasic | W wire | Space pause | R reset | [ ] iterations", 18, 28);
    tc::drawBitmapString("iterations: " + std::to_string(iterations_) + " | backend: " + cloth_.backendReason(), 18, 48);
}

void tcApp::keyPressed(int key) {
    if (key == 'W' || key == 'w') showWire_ = !showWire_;
    if (key == tc::KEY_SPACE) paused_ = !paused_;
    if (key == 'R' || key == 'r') rebuild();
    if (key == '[' || key == ']') {
        iterations_ += key == '[' ? -1 : 1;
        iterations_ = std::clamp(iterations_, 1, 24);
        rebuild();
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    rebuild();
}

void tcApp::rebuild() {
    tcxCloth::ClothSettings settings;
    settings.columns = 64;
    settings.rows = 64;
    const float w = viewWidth();
    const float h = viewHeight();
    settings.width = std::min(w * 0.64f, 620.0f);
    settings.height = std::min(h * 0.48f, 330.0f);
    settings.origin = tc::Vec3((w - settings.width) * 0.5f, 138.0f, 0.0f);
    settings.constraintIterations = iterations_;
    settings.damping = 0.66f;
    settings.structuralStiffness = 0.88f;
    settings.shearStiffness = 0.58f;
    settings.bendStiffness = 0.18f;
    settings.backend = tcxCloth::ClothSettings::SolverBackend::Auto;

    cloth_.setup(settings);
    cloth_.pinParticle(0, 0, true);
    cloth_.pinParticle(settings.columns - 1, 0, true);
    cloth_.setGravity(tc::Vec3(0.0f, 560.0f, 0.0f));
    cloth_.setWind(tc::Vec3(0.0f, 0.0f, 1.0f), 1.8f);
    const int warmupFrames = 120;
    for (int i = 0; i < warmupFrames; ++i) {
        cloth_.update(1.0f / 60.0f);
    }
}
