#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

tcx::flow::BridgeMaskSource maskSourceForIndex(int index) {
    switch (index % 7) {
        case 1: return tcx::flow::BridgeMaskSource::Alpha;
        case 2: return tcx::flow::BridgeMaskSource::Red;
        case 3: return tcx::flow::BridgeMaskSource::Green;
        case 4: return tcx::flow::BridgeMaskSource::Blue;
        case 5: return tcx::flow::BridgeMaskSource::MaxRgb;
        case 6: return tcx::flow::BridgeMaskSource::Saturation;
        case 0:
        default: return tcx::flow::BridgeMaskSource::Luminance;
    }
}

std::string maskSourceName(tcx::flow::BridgeMaskSource source) {
    switch (source) {
        case tcx::flow::BridgeMaskSource::Alpha: return "alpha";
        case tcx::flow::BridgeMaskSource::Red: return "red";
        case tcx::flow::BridgeMaskSource::Green: return "green";
        case tcx::flow::BridgeMaskSource::Blue: return "blue";
        case tcx::flow::BridgeMaskSource::MaxRgb: return "max-rgb";
        case tcx::flow::BridgeMaskSource::Saturation: return "saturation";
        case tcx::flow::BridgeMaskSource::Luminance:
        default: return "luminance";
    }
}

} // namespace

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
    tc::drawBitmapString("bridges | 1 velocity 2 density 3 temp 4 combined | i invert | a alpha-mask | m mask-source | x mirror x | y mirror y | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("mode " + tc::toString(mode_) +
                             " | invert " + std::string(invert_ ? "on" : "off") +
                             " | alpha " + std::string(useAlphaAsMask_ ? "on" : "off") +
                             " | mx " + std::string(mirrorX_ ? "on" : "off") +
                             " | my " + std::string(mirrorY_ ? "on" : "off") +
                             " | mask-source " + maskSourceName(maskSourceForIndex(maskSourceIndex_)) +
                             " | softness " + tc::toString(maskSoftness_, 2) +
                             " | gamma " + tc::toString(maskGamma_, 2),
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
    tc::drawBitmapString("[ ] softness | , . gamma",
                         18, 28 + tcx::flow::example::kHudLine * 2.0f, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit >= 1 && digit <= 4 && digit != mode_) {
        mode_ = digit;
        fluid_.reset();
    }
    if (keyIs(key, 'i')) {
        invert_ = !invert_;
        applyBridgeSettings();
        fluid_.reset();
    }
    if (keyIs(key, 'a')) {
        useAlphaAsMask_ = !useAlphaAsMask_;
        applyBridgeSettings();
        fluid_.reset();
    }
    if (keyIs(key, 'x')) {
        mirrorX_ = !mirrorX_;
        applyBridgeSettings();
        fluid_.reset();
    }
    if (keyIs(key, 'y')) {
        mirrorY_ = !mirrorY_;
        applyBridgeSettings();
        fluid_.reset();
    }
    if (keyIs(key, 'm')) {
        maskSourceIndex_ = (maskSourceIndex_ + 1) % 7;
        useAlphaAsMask_ = maskSourceForIndex(maskSourceIndex_) == tcx::flow::BridgeMaskSource::Alpha;
        applyBridgeSettings();
        fluid_.reset();
    }
    if (key == static_cast<int>(']') || key == static_cast<int>('}')) {
        maskSoftness_ = std::min(0.50f, maskSoftness_ + 0.02f);
        applyBridgeSettings();
        fluid_.reset();
    }
    if (key == static_cast<int>('[') || key == static_cast<int>('{')) {
        maskSoftness_ = std::max(0.0f, maskSoftness_ - 0.02f);
        applyBridgeSettings();
        fluid_.reset();
    }
    if (key == static_cast<int>('.') || key == static_cast<int>('>')) {
        maskGamma_ = std::min(4.0f, maskGamma_ + 0.10f);
        applyBridgeSettings();
        fluid_.reset();
    }
    if (key == static_cast<int>(',') || key == static_cast<int>('<')) {
        maskGamma_ = std::max(0.20f, maskGamma_ - 0.10f);
        applyBridgeSettings();
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
    applyBridgeSettings();
    if (!inputTexture_.isAllocated() ||
        inputTexture_.getWidth() != tc::getWindowWidth() ||
        inputTexture_.getHeight() != tc::getWindowHeight()) {
        inputTexture_.allocate(tc::getWindowWidth(), tc::getWindowHeight(), 1, tc::TextureFormat::RGBA8);
        inputTexture_.getTexture().setFilter(tc::TextureFilter::Linear);
    }
}

void tcApp::applyBridgeSettings() {
    auto apply = [this](tcx::flow::BridgeFlow& bridge) {
        bridge.settings().invert = invert_;
        bridge.settings().useAlphaAsMask = useAlphaAsMask_;
        bridge.settings().mirrorX = mirrorX_;
        bridge.settings().mirrorY = mirrorY_;
        bridge.settings().maskSource = maskSourceForIndex(maskSourceIndex_);
        bridge.settings().maskSoftness = maskSoftness_;
        bridge.settings().maskGamma = maskGamma_;
    };
    apply(velocityBridge_);
    apply(densityBridge_);
    apply(temperatureBridge_);
    apply(combinedBridge_);
}

void tcApp::updateInputTexture(float time) {
    if (!inputTexture_.isAllocated()) return;

    inputTexture_.begin(0.0f, 0.0f, 0.0f, 0.0f);
    const float w = static_cast<float>(inputTexture_.getWidth());
    const float h = static_cast<float>(inputTexture_.getHeight());
    tc::setColor(0.05f, 0.06f, 0.08f, 0.18f);
    tc::drawRect(0, 0, w, h);
    tc::setColor(0.15f, 0.70f, 1.0f, 0.95f);
    tc::drawCircle(w * (0.5f + std::sin(time * 0.9f) * 0.28f),
                   h * (0.5f + std::cos(time * 1.1f) * 0.22f),
                   std::min(w, h) * 0.12f);
    tc::setColor(1.0f, 0.35f, 0.12f, 0.82f);
    tc::drawCircle(w * (0.5f + std::sin(time * 1.3f + 2.1f) * 0.22f),
                   h * (0.5f + std::cos(time * 0.8f + 1.4f) * 0.26f),
                   std::min(w, h) * 0.08f);
    inputTexture_.end();
}
