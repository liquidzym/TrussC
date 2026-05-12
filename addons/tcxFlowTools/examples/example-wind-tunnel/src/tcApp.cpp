#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
float smooth01(float edge0, float edge1, float x) {
    const float denom = std::max(0.000001f, edge1 - edge0);
    const float t = std::clamp((x - edge0) / denom, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

void tcApp::setup() {
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float t = tc::getElapsedTimef();
    if (mouseObstacle_) {
        rebuildObstacles();
    }
    injectWind(t);
    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.018f, 0.022f, 0.03f);
    switch (mode_) {
        case tcx::flow::FlowVisualizer::Mode::Density:
            fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::Pressure:
            fluid_.drawPressure(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::Temperature:
            fluid_.drawTemperature(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::Velocity:
            fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
    }
    if (showVelocity_) {
        fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    drawObstacles();

    tc::setColor(1.0f);
    const std::string gpu = fluid_.lastUpdateUsedGpu() ? "GPU" : "CPU fallback";
    tc::drawBitmapString("wind-tunnel | " + gpu + " | d/v/p/t view | o obstacle | +/- wind | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("wind " + tc::toString(windStrength_, 2) + " | sim " +
                             tc::toString(fluid_.simWidth()) + "x" + tc::toString(fluid_.simHeight()) +
                             " | texture inlet",
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'd')) mode_ = tcx::flow::FlowVisualizer::Mode::Density;
    if (keyIs(key, 'v')) showVelocity_ = !showVelocity_;
    if (keyIs(key, 'p')) mode_ = tcx::flow::FlowVisualizer::Mode::Pressure;
    if (keyIs(key, 't')) mode_ = tcx::flow::FlowVisualizer::Mode::Temperature;
    if (keyIs(key, 'o')) {
        mouseObstacle_ = !mouseObstacle_;
        rebuildObstacles();
    }
    if (key == '+' || key == '=') windStrength_ = std::min(2.5f, windStrength_ + 0.15f);
    if (key == '-') windStrength_ = std::max(0.2f, windStrength_ - 0.15f);
    if (keyIs(key, 'r')) {
        fluid_.reset();
        rebuildObstacles();
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.5f;
    settings.timestep = 0.13f;
    settings.solverIterations = 36;
    settings.velocityDissipation = 0.999f;
    settings.densityDissipation = 0.997f;
    settings.enableVorticity = true;
    settings.vorticity = 0.42f;
    settings.viscosity = 0.006f;
    settings.enableObstacles = true;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
    rebuildObstacles();
}

void tcApp::rebuildObstacles() {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    fluid_.clearObstacles();
    fluid_.addObstacle(tc::Vec2(w * 0.46f, h * 0.50f), std::min(w, h) * 0.105f);
    fluid_.addObstacle(tc::Vec2(w * 0.62f, h * 0.30f), std::min(w, h) * 0.052f);
    fluid_.addObstacle(tc::Vec2(w * 0.64f, h * 0.70f), std::min(w, h) * 0.058f);
    if (mouseObstacle_) {
        fluid_.addObstacle(tc::getMousePos(), std::min(w, h) * 0.07f);
    }
}

void tcApp::injectWind(float time) {
    updateWindField(time);
    updateDensitySource(time);
    fluid_.applyVelocityField(windField_, windFieldWidth_, windFieldHeight_, 1.0f);
    fluid_.applyDensityTexture(densitySourceTexture_, 0.78f);
}

void tcApp::updateWindField(float time) {
    windField_.assign(static_cast<std::size_t>(windFieldWidth_ * windFieldHeight_), tc::Vec2(0, 0));
    for (int y = 0; y < windFieldHeight_; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(windFieldHeight_);
        const float edgeFade = smooth01(0.33f, 0.40f, v) * (1.0f - smooth01(0.60f, 0.67f, v));
        for (int x = 0; x < windFieldWidth_; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(windFieldWidth_);
            const float inletRamp = 1.0f - smooth01(0.88f, 1.0f, u);
            const float wave = std::sin(time * 0.55f + v * 8.0f + u * 4.0f) * 0.06f;
            const float vx = (46.0f + 2.5f * wave) * windStrength_ * inletRamp * edgeFade;
            const float vy = wave * 11.0f * windStrength_ * inletRamp * edgeFade;
            windField_[static_cast<std::size_t>(y * windFieldWidth_ + x)] = tc::Vec2(vx, vy);
        }
    }
}

void tcApp::updateDensitySource(float time) {
    densitySourcePixels_.assign(static_cast<std::size_t>(sourceWidth_ * sourceHeight_) * 4, 0.0f);
    for (int y = 0; y < sourceHeight_; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(sourceHeight_);
        const float band = smooth01(0.34f, 0.41f, v) * (1.0f - smooth01(0.59f, 0.66f, v));
        const float center = 1.0f - std::abs(v - 0.5f) * 2.0f;
        for (int x = 0; x < sourceWidth_; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(sourceWidth_);
            const float inlet = smooth01(0.0f, 0.006f, u) * (1.0f - smooth01(0.032f, 0.052f, u));
            const float wakeTexture = 0.92f
                + 0.035f * std::sin(v * 23.0f + time * 0.43f)
                + 0.025f * std::sin(u * 61.0f + v * 17.0f - time * 0.36f);
            const float value = std::clamp(inlet * band * wakeTexture * 0.94f, 0.0f, 1.0f);
            const std::size_t base = static_cast<std::size_t>(y * sourceWidth_ + x) * 4;
            densitySourcePixels_[base + 0] = value * 0.016f;
            densitySourcePixels_[base + 1] = value * (0.20f + 0.055f * center);
            densitySourcePixels_[base + 2] = value * (0.72f + 0.16f * center);
            densitySourcePixels_[base + 3] = value;
        }
    }

    if (!densitySourceTexture_.isAllocated() ||
        densitySourceTexture_.getWidth() != sourceWidth_ ||
        densitySourceTexture_.getHeight() != sourceHeight_) {
        densitySourceTexture_.allocate(sourceWidth_, sourceHeight_, tc::TextureFormat::RGBA32F, tc::TextureUsage::Dynamic);
        densitySourceTexture_.setFilter(tc::TextureFilter::Linear);
    }
    densitySourceTexture_.loadData(densitySourcePixels_.data(), sourceWidth_, sourceHeight_, 4);
}

void tcApp::drawObstacles() const {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float s = std::min(w, h);
    tc::setColor(0.015f, 0.018f, 0.024f, 0.94f);
    tc::drawCircle(w * 0.46f, h * 0.50f, s * 0.105f);
    tc::drawCircle(w * 0.62f, h * 0.30f, s * 0.052f);
    tc::drawCircle(w * 0.64f, h * 0.70f, s * 0.058f);
    tc::setColor(0.35f, 0.55f, 0.70f, 0.45f);
    tc::drawCircle(w * 0.46f, h * 0.50f, s * 0.105f + 1.5f);
    tc::drawCircle(w * 0.62f, h * 0.30f, s * 0.052f + 1.5f);
    tc::drawCircle(w * 0.64f, h * 0.70f, s * 0.058f + 1.5f);
    if (mouseObstacle_) {
        tc::setColor(0.06f, 0.09f, 0.12f, 0.82f);
        tc::drawCircle(tc::getMouseX(), tc::getMouseY(), s * 0.07f);
    }
}
