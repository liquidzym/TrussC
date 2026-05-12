#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>

void tcApp::setup() {
    resizeSystems();
    previousMouse_ = tc::Vec2(tc::getMouseX(), tc::getMouseY());
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float time = tc::getElapsedTimef();
    injectSources(time);

    const tc::Vec2 mouse = tc::getMousePos();
    if (tc::isMousePressed()) {
        if (!wasMousePressed_) {
            previousMouse_ = mouse;
        }
        mouseFlow_.addDrag(fluid_, mouse, previousMouse_, tc::getMouseButton());
        previousMouse_ = mouse;
        wasMousePressed_ = true;
    } else {
        previousMouse_ = mouse;
        wasMousePressed_ = false;
    }

    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.012f, 0.016f, 0.02f);
    if (showDensity_) {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    if (showLic_) {
        fluid_.drawLic(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }

    tc::setColor(1.0f);
    tc::drawBitmapString("lic-streamlines | drag inject | l LIC | d density | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("sim " + tc::toString(fluid_.simWidth()) + "x" +
                             tc::toString(fluid_.simHeight()),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'l')) showLic_ = !showLic_;
    if (keyIs(key, 'd')) showDensity_ = !showDensity_;
    if (keyIs(key, 'r')) fluid_.reset();
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    (void)button;
    previousMouse_ = pos;
    wasMousePressed_ = true;
}

void tcApp::mouseDragged(tc::Vec2 pos, int button) {
    (void)pos;
    (void)button;
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.5f;
    settings.timestep = 0.125f;
    settings.solverIterations = 36;
    settings.enableVorticity = true;
    settings.vorticity = 0.65f;
    settings.velocityDissipation = 0.997f;
    settings.densityDissipation = 0.996f;
    settings.viscosity = 0.008f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
}

void tcApp::injectSources(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float radius = std::min(w, h) * 0.065f;

    const tc::Vec2 p0(w * (0.50f + std::sin(time * 0.72f) * 0.27f),
                      h * (0.50f + std::cos(time * 0.56f) * 0.22f));
    const tc::Vec2 v0(std::cos(time * 0.72f) * 46.0f,
                      -std::sin(time * 0.56f) * 38.0f);
    fluid_.addVelocity(p0, radius, v0);
    fluid_.addDensity(p0, radius * 0.7f, tc::Color(0.06f, 0.30f, 0.75f, 0.75f));

    const tc::Vec2 p1(w * (0.50f + std::sin(time * 1.03f + 2.0f) * 0.23f),
                      h * (0.50f + std::cos(time * 0.92f + 1.2f) * 0.25f));
    const tc::Vec2 v1(std::cos(time * 1.03f + 2.0f) * -42.0f,
                      -std::sin(time * 0.92f + 1.2f) * 44.0f);
    fluid_.addVelocity(p1, radius * 0.85f, v1);
    fluid_.addDensity(p1, radius * 0.55f, tc::Color(0.05f, 0.65f, 0.95f, 0.65f));
}
