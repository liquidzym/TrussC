#include "tcApp.h"

#include <algorithm>
#include <cmath>

namespace {

float viewWidth() {
    return std::max(640.0f, static_cast<float>(tc::getWindowWidth()));
}

float viewHeight() {
    return std::max(480.0f, static_cast<float>(tc::getWindowHeight()));
}

} // namespace

void tcApp::setup() {
    tc::setWindowTitle("tcxCloth clothCollision");
    tc::setDefaultScreenFov(45.0f);
    tc::setIndependentFps(60.0f, 0.0f);
    sphere_.radius = 76.0f;
    rebuild();
}

void tcApp::update() {
    if (!paused_) {
        updateCollider();
        cloth_.update(static_cast<float>(tc::getDeltaTime()));
    }
    tc::redraw();
}

void tcApp::draw() {
    tc::clear(0.030f, 0.038f, 0.044f);

    tc::setColor(1.0f, 0.48f, 0.18f, 0.34f);
    tc::drawSphere(sphere_.center, sphere_.radius, 24);
    cloth_.draw();
    if (showWire_) {
        tc::setColor(0.88f, 0.94f, 1.0f, 0.48f);
        cloth_.drawWire();
    }
    tc::setColor(1.0f, 0.74f, 0.36f, 0.92f);
    tc::drawSphere(sphere_.center, 4.0f, 10);

    tc::setColor(0.86f, 0.90f, 0.94f, 0.90f);
    tc::drawBitmapString("tcxCloth clothCollision | drag sphere | W wire | Space pause | R reset", 18, 28);
    tc::drawBitmapString("backend: " + cloth_.backendReason(), 18, 48);
}

void tcApp::keyPressed(int key) {
    if (key == 'W' || key == 'w') showWire_ = !showWire_;
    if (key == tc::KEY_SPACE) paused_ = !paused_;
    if (key == 'R' || key == 'r') rebuild();
}

void tcApp::mouseDragged(tc::Vec2 pos, int button) {
    (void)button;
    mouseControl_ = true;
    sphere_.center.x = pos.x;
    sphere_.center.y = pos.y;
    cloth_.setSphereColliders(std::span<const tcxCloth::SphereCollider>(&sphere_, 1));
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    rebuild();
}

void tcApp::rebuild() {
    tcxCloth::ClothSettings settings;
    settings.columns = 58;
    settings.rows = 48;
    const float w = viewWidth();
    const float h = viewHeight();
    settings.width = std::min(w * 0.60f, 560.0f);
    settings.height = std::min(h * 0.46f, 320.0f);
    settings.origin = tc::Vec3((w - settings.width) * 0.5f, 140.0f, 0.0f);
    settings.constraintIterations = 18;
    settings.damping = 0.014f;
    settings.structuralStiffness = 0.96f;
    settings.shearStiffness = 0.86f;
    settings.bendStiffness = 0.42f;
    settings.backend = tcxCloth::ClothSettings::SolverBackend::Auto;

    cloth_.setup(settings);
    cloth_.pinParticle(1, 0, true);
    cloth_.pinParticle(settings.columns / 2, 0, true);
    cloth_.pinParticle(settings.columns - 2, 0, true);
    cloth_.setGravity(tc::Vec3(0.0f, 360.0f, 0.0f));
    cloth_.setWind(tc::Vec3(0.0f, 0.0f, 1.0f), 0.6f);
    mouseControl_ = false;
    updateCollider();
    const int warmupFrames = 48;
    for (int i = 0; i < warmupFrames; ++i) {
        updateCollider();
        cloth_.update(1.0f / 60.0f);
    }
}

void tcApp::updateCollider() {
    if (!mouseControl_) {
        const float t = tc::getElapsedTimef();
        sphere_.center = tc::Vec3(viewWidth() * (0.52f + 0.10f * std::sin(t * 0.7f)),
                                  140.0f + std::min(viewHeight() * 0.46f, 320.0f) * (0.58f + 0.08f * std::sin(t * 1.1f)),
                                  -42.0f + 88.0f * (0.5f + 0.5f * std::sin(t * 0.9f)));
    }
    cloth_.setSphereColliders(std::span<const tcxCloth::SphereCollider>(&sphere_, 1));
}
