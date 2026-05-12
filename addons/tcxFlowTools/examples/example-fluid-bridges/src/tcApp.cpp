#include "tcApp.h"

#include "../../common/ExampleControls.h"

void tcApp::setup() {
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    if (mode_ == 1) {
        velocityBridge_.update(dt);
        velocityBridge_.applyTo(fluid_);
    } else if (mode_ == 2) {
        densityBridge_.update(dt);
        densityBridge_.applyTo(fluid_);
    } else if (mode_ == 3) {
        temperatureBridge_.update(dt);
        temperatureBridge_.applyTo(fluid_);
    } else {
        combinedBridge_.update(dt);
        combinedBridge_.applyTo(fluid_);
    }
    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.04f, 0.04f, 0.05f);
    fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    tc::setColor(1.0f);
    tc::drawBitmapString("bridges | 1 velocity 2 density 3 temp 4 combined | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("mode " + tc::toString(mode_),
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit >= 1 && digit <= 4) mode_ = digit;
    if (keyIs(key, 'r')) fluid_.reset();
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.25f;
    settings.enableTemperature = true;
    settings.enableBuoyancy = true;
    settings.buoyancy = 0.2f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
    velocityBridge_.setup(tc::getWindowWidth(), tc::getWindowHeight());
    densityBridge_.setup(tc::getWindowWidth(), tc::getWindowHeight());
    temperatureBridge_.setup(tc::getWindowWidth(), tc::getWindowHeight());
    combinedBridge_.setup(tc::getWindowWidth(), tc::getWindowHeight());
}
