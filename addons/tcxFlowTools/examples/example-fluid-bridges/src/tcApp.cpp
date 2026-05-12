#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>

void tcApp::setup() {
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    updateInputTexture(tc::getElapsedTimef());
    if (mode_ == 1) {
        velocityBridge_.update(inputTexture_.getTexture(), dt);
        velocityBridge_.applyTo(fluid_);
    } else if (mode_ == 2) {
        densityBridge_.update(inputTexture_.getTexture(), dt);
        densityBridge_.applyTo(fluid_);
    } else if (mode_ == 3) {
        temperatureBridge_.update(inputTexture_.getTexture(), dt);
        temperatureBridge_.applyTo(fluid_);
    } else {
        combinedBridge_.update(inputTexture_.getTexture(), dt);
        combinedBridge_.applyTo(fluid_);
    }
    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.04f, 0.04f, 0.05f);
    if (mode_ == 1) {
        fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    } else if (mode_ == 2) {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    } else if (mode_ == 3) {
        fluid_.drawTemperature(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    } else {
        fluid_.drawCombined(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
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
    if (digit >= 1 && digit <= 4 && digit != mode_) {
        mode_ = digit;
        fluid_.reset();
    }
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
    velocityBridge_.settings().threshold = 0.10f;
    velocityBridge_.settings().velocityScale = 36.0f;
    densityBridge_.settings().threshold = 0.10f;
    densityBridge_.settings().densityScale = 1.4f;
    temperatureBridge_.settings().threshold = 0.10f;
    temperatureBridge_.settings().temperatureScale = 1.2f;
    combinedBridge_.settings().threshold = 0.10f;
    combinedBridge_.settings().velocityScale = 36.0f;
    combinedBridge_.settings().densityScale = 1.4f;
    combinedBridge_.settings().temperatureScale = 1.2f;
    if (!inputTexture_.isAllocated() ||
        inputTexture_.getWidth() != tc::getWindowWidth() ||
        inputTexture_.getHeight() != tc::getWindowHeight()) {
        inputTexture_.allocate(tc::getWindowWidth(), tc::getWindowHeight(), 1, tc::TextureFormat::RGBA8);
        inputTexture_.getTexture().setFilter(tc::TextureFilter::Linear);
    }
}

void tcApp::updateInputTexture(float time) {
    if (!inputTexture_.isAllocated()) return;

    inputTexture_.begin(0.0f, 0.0f, 0.0f, 1.0f);
    const float w = static_cast<float>(inputTexture_.getWidth());
    const float h = static_cast<float>(inputTexture_.getHeight());
    tc::setColor(0.05f, 0.06f, 0.08f, 1.0f);
    tc::drawRect(0, 0, w, h);
    tc::setColor(0.15f, 0.70f, 1.0f, 1.0f);
    tc::drawCircle(w * (0.5f + std::sin(time * 0.9f) * 0.28f),
                   h * (0.5f + std::cos(time * 1.1f) * 0.22f),
                   std::min(w, h) * 0.12f);
    tc::setColor(1.0f, 0.35f, 0.12f, 1.0f);
    tc::drawCircle(w * (0.5f + std::sin(time * 1.3f + 2.1f) * 0.22f),
                   h * (0.5f + std::cos(time * 0.8f + 1.4f) * 0.26f),
                   std::min(w, h) * 0.08f);
    inputTexture_.end();
}
