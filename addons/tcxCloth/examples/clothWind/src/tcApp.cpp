#include "tcApp.h"

#include <algorithm>
#include <cmath>

namespace {

float viewWidth() {
    return std::max(760.0f, static_cast<float>(tc::getWindowWidth()));
}

float viewHeight() {
    return std::max(520.0f, static_cast<float>(tc::getWindowHeight()));
}

} // namespace

void tcApp::setup() {
    tc::setWindowTitle("tcxCloth clothWind");
    tc::setDefaultScreenFov(45.0f);
    tc::setIndependentFps(60.0f, 0.0f);
    exitRequestedListener_ = tc::events().exitRequested.listen([this](tc::ExitRequestEventArgs&) {
        shuttingDown_ = true;
        paused_ = true;
        cloth_.release();
    });
    rebuild();
}

void tcApp::update() {
    if (!paused_ && !shuttingDown_) {
        const float t = tc::getElapsedTimef();
        cloth_.setWind(tc::Vec3(0.42f * std::sin(t * 0.7f), 0.04f, 1.0f), 8.0f + 2.2f * std::sin(t * 1.1f));
        cloth_.update(static_cast<float>(tc::getDeltaTime()));
    }
    tc::redraw();
}

void tcApp::draw() {
    tc::clear(0.026f, 0.034f, 0.040f);
    if (!shuttingDown_) {
        drawClothScene();
    }

    tc::setColor(0.86f, 0.90f, 0.94f, 0.90f);
    tc::drawBitmapString("tcxCloth clothWind | N normals | W wire | Space pause | R reset", 18, 28);
    tc::drawBitmapString("backend: " + cloth_.backendReason(), 18, 48);
}

void tcApp::drawClothScene() {
    const float centerX = viewWidth() * 0.5f;
    const float centerY = 138.0f + std::min(viewHeight() * 0.48f, 340.0f) * 0.5f;
    tc::pushMatrix();
    tc::translate(centerX, centerY, 0.0f);
    tc::rotateX(-0.08f);
    tc::rotateY(-0.42f);
    tc::translate(-centerX, -centerY, 0.0f);
    cloth_.draw();
    if (showWire_) {
        tc::setColor(0.88f, 0.94f, 1.0f, 0.34f);
        cloth_.drawWire();
    }
    if (showNormals_) {
        drawNormalDebug();
    }
    tc::popMatrix();
}

void tcApp::keyPressed(int key) {
    if (key == 'N' || key == 'n') showNormals_ = !showNormals_;
    if (key == 'W' || key == 'w') showWire_ = !showWire_;
    if (key == tc::KEY_SPACE) paused_ = !paused_;
    if (key == 'R' || key == 'r') rebuild();
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    if (!shuttingDown_) {
        rebuild();
    }
}

void tcApp::exit() {
    shuttingDown_ = true;
    paused_ = true;
    cloth_.release();
}

void tcApp::rebuild() {
    if (shuttingDown_) return;

    tcxCloth::ClothSettings settings;
    settings.columns = 96;
    settings.rows = 64;
    const float w = viewWidth();
    const float h = viewHeight();
    settings.width = std::min(w * 0.70f, 760.0f);
    settings.height = std::min(h * 0.48f, 340.0f);
    settings.origin = tc::Vec3((w - settings.width) * 0.5f, 138.0f, 0.0f);
    settings.constraintIterations = 12;
    settings.damping = 0.74f;
    settings.structuralStiffness = 0.86f;
    settings.shearStiffness = 0.54f;
    settings.bendStiffness = 0.16f;
    settings.backend = tcxCloth::ClothSettings::SolverBackend::Auto;

    cloth_.setup(settings);
    cloth_.pinTopEdge(10);
    cloth_.setGravity(tc::Vec3(0.0f, 420.0f, 0.0f));
    cloth_.setWind(tc::Vec3(0.0f, 0.04f, 1.0f), 8.0f);
    const int warmupFrames = 20;
    for (int i = 0; i < warmupFrames; ++i) {
        cloth_.update(1.0f / 60.0f);
    }
}

void tcApp::drawNormalDebug() {
    const auto particles = cloth_.particles();
    const int cols = cloth_.columns();
    const int rows = cloth_.rows();
    tc::setColor(0.98f, 0.80f, 0.38f, 0.42f);
    for (int y = 0; y < rows; y += 8) {
        for (int x = 0; x < cols; x += 8) {
            const auto& p = particles[static_cast<std::size_t>(y * cols + x)];
            tc::drawLine(p.position, p.position + p.normal * 24.0f);
        }
    }
}
