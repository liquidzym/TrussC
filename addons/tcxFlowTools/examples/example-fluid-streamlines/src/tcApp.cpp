#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>

namespace {

float hash01(int value) {
    unsigned int x = static_cast<unsigned int>(value) * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return static_cast<float>(x & 0xffffu) / 65535.0f;
}

float clampf(float value, float lo, float hi) {
    return std::max(lo, std::min(value, hi));
}

tc::Color mixColor(const tc::Color& a, const tc::Color& b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    return a * (1.0f - t) + b * t;
}

tc::Vec2 safeDirection(const tc::Vec2& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) return tc::Vec2(0, 0);
    return v * (1.0f / len);
}

bool insideWindow(const tc::Vec2& p) {
    return p.x >= 0.0f && p.y >= 0.0f &&
           p.x <= static_cast<float>(tc::getWindowWidth()) &&
           p.y <= static_cast<float>(tc::getWindowHeight());
}

} // namespace

void tcApp::setup() {
    resizeSystems();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = std::min(1.0f / 30.0f, static_cast<float>(tc::getDeltaTime()));
    const float time = tc::getElapsedTimef();

    if (!paused_) {
        injectReferenceSources(time);
        handleMouseInput();
        fluid_.update(dt);
    }
    velocityReadbackReady_ = showStreamlines_ ? fluid_.refreshVelocityReadback() : false;
}

void tcApp::draw() {
    tc::clear(0.0f, 0.0f, 0.0f);
    drawBackground();
    drawObstacles();
    drawVelocityVectors();
    drawStreamlines();
    drawStreamParticles();

    tc::setColor(1.0f);
    tc::drawBitmapString("fluid-streamlines | drag velocity | right heat | s lines | m mode | v vectors | a particles | p pause | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("PixelFlow Fluid_StreamLines / FlowField_LIC_StreamLines parity | " +
                             std::string(backgroundModeName()) + " | seeds " +
                             tc::toString(static_cast<int>(streamSeeds_.size())) + " | " +
                             "line length " + tc::toString(lineLength_, 0) + " | " +
                             (velocityReadbackReady_ ? "readback on" : "readback off"),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
    tc::drawBitmapString("1-3 density | +/- line length | " + std::string(paused_ ? "paused" : "running"),
                         18, 28 + tcx::flow::example::kHudLine * 2.0f,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    if (keyIs(key, 's')) showStreamlines_ = !showStreamlines_;
    if (keyIs(key, 'm')) cycleBackgroundMode();
    if (keyIs(key, 'v')) showVelocityVectors_ = !showVelocityVectors_;
    if (keyIs(key, 'a')) showStreamParticles_ = !showStreamParticles_;
    if (keyIs(key, 'p')) paused_ = !paused_;
    if (key == static_cast<int>('+') || key == static_cast<int>('=')) {
        lineLength_ = std::min(260.0f, lineLength_ + 18.0f);
    }
    if (key == static_cast<int>('-') || key == static_cast<int>('_')) {
        lineLength_ = std::max(40.0f, lineLength_ - 18.0f);
    }
    if (keyIs(key, 'r')) {
        fluid_.reset();
        configureObstacles();
        paused_ = false;
    }
    const int digit = digitKey(key);
    if (digit >= 1 && digit <= 3) {
        setStreamPreset(digit - 1);
    }
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    previousMouse_ = pos;
    activeMouseButton_ = button;
    wasMousePressed_ = true;
}

void tcApp::mouseReleased(tc::Vec2 pos, int button) {
    (void)pos;
    (void)button;
    activeMouseButton_ = -1;
    wasMousePressed_ = false;
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
    settings.outputResolutionScale = 1.0f;
    settings.timestep = 0.125f;
    settings.solverIterations = 40;
    settings.enableVorticity = true;
    settings.vorticity = 0.20f;
    settings.enableTemperature = true;
    settings.enableBuoyancy = true;
    settings.buoyancy = 0.30f;
    settings.densityWeight = 0.020f;
    settings.velocityDissipation = 0.990f;
    settings.densityDissipation = 0.999f;
    settings.temperatureDissipation = 0.50f;
    settings.viscosity = 0.004f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
    configureObstacles();
    resetStreamSeeds();
}

void tcApp::configureObstacles() {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float s = std::min(w, h);
    obstacles_.clear();
    obstacles_.push_back({tc::Vec2(w * 0.27f, h * 0.32f), s * 0.045f});
    obstacles_.push_back({tc::Vec2(w * 0.54f, h * 0.62f), s * 0.052f});
    obstacles_.push_back({tc::Vec2(w * 0.71f, h * 0.38f), s * 0.040f});
    obstacles_.push_back({tc::Vec2(w * 0.82f, h * 0.72f), s * 0.060f});

    fluid_.clearObstacles();
    for (const auto& obstacle : obstacles_) {
        fluid_.addObstacle(obstacle.position, obstacle.radius);
    }
}

void tcApp::resetStreamSeeds() {
    streamSeeds_.clear();
    const int spacing = streamSpacing();
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float pad = static_cast<float>(spacing) * 0.75f;
    for (float y = pad; y <= h - pad; y += static_cast<float>(spacing)) {
        for (float x = pad; x <= w - pad; x += static_cast<float>(spacing)) {
            streamSeeds_.push_back(tc::Vec2(x, y));
        }
    }
}

void tcApp::injectReferenceSources(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());

    const tc::Vec2 topRight(w - 82.0f, 34.0f);
    fluid_.addDensity(topRight, 56.0f, tc::Color(1.0f, 0.95f, 0.82f, 0.32f));
    fluid_.addTemperature(topRight, 60.0f, 4.0f);
    fluid_.addVelocity(topRight, 64.0f, tc::Vec2(-58.0f, 88.0f));

    const tc::Vec2 center(w * 0.50f + std::sin(time * 0.45f) * w * 0.04f,
                          h * 0.50f + std::cos(time * 0.37f) * h * 0.04f);
    fluid_.addDensity(center, 22.0f, tc::Color(0.82f, 0.88f, 1.0f, 0.28f));
    fluid_.addTemperature(center, 24.0f, -3.0f);
    fluid_.addVelocity(center, 48.0f, tc::Vec2(std::sin(time * 1.10f) * 64.0f, -74.0f));

    const tc::Vec2 lower(w * (0.28f + std::sin(time * 0.63f) * 0.12f),
                         h * (0.78f + std::cos(time * 0.55f) * 0.05f));
    fluid_.addDensity(lower, 36.0f, tc::Color(0.10f, 0.45f, 1.0f, 0.20f));
    fluid_.addVelocity(lower, 58.0f, tc::Vec2(120.0f, std::sin(time * 0.9f) * 32.0f));
}

void tcApp::handleMouseInput() {
    const tc::Vec2 mouse = tc::getMousePos();
    if (!tc::isMousePressed()) {
        previousMouse_ = mouse;
        activeMouseButton_ = -1;
        wasMousePressed_ = false;
        return;
    }
    if (activeMouseButton_ < 0) {
        activeMouseButton_ = tc::getMouseButton();
    }
    if (!wasMousePressed_) {
        previousMouse_ = mouse;
    }

    const tc::Vec2 delta = mouse - previousMouse_;
    const float distance = delta.length();
    if (distance > 0.0f && activeMouseButton_ == 0) {
        const int steps = std::max(1, std::min(72, static_cast<int>(std::ceil(distance / 4.0f))));
        const tc::Vec2 segmentVelocity = delta * (15.0f / static_cast<float>(steps));
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            fluid_.addVelocity(previousMouse_ + delta * t, 18.0f, segmentVelocity);
        }
        fluid_.addDensity(mouse, 9.0f, tc::Color(0.95f, 0.98f, 1.0f, 0.35f));
    } else if (activeMouseButton_ == 1) {
        fluid_.addDensity(mouse, 28.0f, tc::Color(0.0f, 0.42f, 1.0f, 0.42f));
        fluid_.addTemperature(mouse, 22.0f, 1.6f);
    }

    previousMouse_ = mouse;
    wasMousePressed_ = true;
}

void tcApp::setStreamPreset(int preset) {
    streamPreset_ = std::clamp(preset, 0, 2);
    resetStreamSeeds();
}

void tcApp::cycleBackgroundMode() {
    const int next = (static_cast<int>(backgroundMode_) + 1) % 6;
    backgroundMode_ = static_cast<BackgroundMode>(next);
}

int tcApp::streamSpacing() const {
    if (streamPreset_ == 0) return 12;
    if (streamPreset_ == 2) return 26;
    return 18;
}

int tcApp::streamSamples() const {
    if (streamPreset_ == 0) return 30;
    if (streamPreset_ == 2) return 18;
    return 24;
}

float tcApp::streamStepScale() const {
    if (streamPreset_ == 0) return 0.052f;
    if (streamPreset_ == 2) return 0.062f;
    return 0.056f;
}

const char* tcApp::backgroundModeName() const {
    switch (backgroundMode_) {
        case BackgroundMode::Combined: return "combined";
        case BackgroundMode::Density: return "density";
        case BackgroundMode::Temperature: return "temperature";
        case BackgroundMode::Velocity: return "velocity";
        case BackgroundMode::Lic: return "LIC";
        case BackgroundMode::None: return "black";
    }
    return "unknown";
}

void tcApp::drawBackground() const {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    switch (backgroundMode_) {
        case BackgroundMode::Combined:
            fluid_.drawCombined(0, 0, w, h);
            break;
        case BackgroundMode::Density:
            fluid_.drawDensity(0, 0, w, h);
            break;
        case BackgroundMode::Temperature:
            fluid_.drawTemperature(0, 0, w, h);
            break;
        case BackgroundMode::Velocity:
            fluid_.drawVelocity(0, 0, w, h);
            break;
        case BackgroundMode::Lic:
            fluid_.drawLic(0, 0, w, h);
            break;
        case BackgroundMode::None:
            break;
    }
    if (showStreamlines_ && backgroundMode_ != BackgroundMode::None) {
        const float dim = backgroundMode_ == BackgroundMode::Lic ? 0.18f : 0.34f;
        tc::setColor(0.0f, 0.0f, 0.0f, dim);
        tc::drawRect(0, 0, w, h);
    }
}

void tcApp::drawObstacles() const {
    tc::setColor(0.82f, 0.84f, 0.80f, 0.50f);
    for (const auto& obstacle : obstacles_) {
        tc::drawCircle(obstacle.position.x, obstacle.position.y, obstacle.radius);
    }
}

void tcApp::drawVelocityVectors() const {
    if (!showVelocityVectors_ || !velocityReadbackReady_) return;
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const int step = std::max(24, streamSpacing() * 3);
    tc::setColor(0.72f, 0.92f, 1.0f, 0.42f);
    for (float y = static_cast<float>(step) * 0.5f; y < h; y += static_cast<float>(step)) {
        for (float x = static_cast<float>(step) * 0.5f; x < w; x += static_cast<float>(step)) {
            const tc::Vec2 p(x, y);
            const tc::Vec2 velocity = fluid_.sampleVelocityAtPosition(p);
            const float speed = velocity.length();
            if (speed < 0.45f) continue;
            const tc::Vec2 dir = safeDirection(velocity);
            const float len = clampf(speed * 0.12f, 4.0f, 24.0f);
            tc::drawLine(p.x, p.y, p.x + dir.x * len, p.y + dir.y * len);
        }
    }
}

void tcApp::drawStreamParticles() const {
    if (!showStreamParticles_ || !velocityReadbackReady_) return;
    for (int i = 0; i < static_cast<int>(streamSeeds_.size()); i += 2) {
        const auto& seed = streamSeeds_[i];
        const float speed = fluid_.sampleVelocityAtPosition(seed).length();
        if (speed < 0.35f) continue;
        const float speedMix = clampf(speed / 90.0f, 0.0f, 1.0f);
        tc::setColor(1.0f, 0.40f + speedMix * 0.35f, 0.08f, 0.18f + speedMix * 0.42f);
        tc::drawCircle(seed.x, seed.y, 1.1f + speedMix * 2.2f);
    }
}

void tcApp::drawStreamlines() const {
    if (!showStreamlines_ || !velocityReadbackReady_) return;
    for (int i = 0; i < static_cast<int>(streamSeeds_.size()); ++i) {
        drawStreamline(streamSeeds_[i], 1, i);
        drawStreamline(streamSeeds_[i], -1, i);
    }
}

void tcApp::drawStreamline(const tc::Vec2& seed, int direction, int seedIndex) const {
    tc::Vec2 p = seed;
    const int samples = streamSamples();
    for (int i = 0; i < samples; ++i) {
        const tc::Vec2 velocity = fluid_.sampleVelocityAtPosition(p);
        const float speed = velocity.length();
        if (speed < 0.22f) return;

        const tc::Vec2 dir = safeDirection(velocity) * static_cast<float>(direction);
        const float lineStep = lineLength_ / std::max(1.0f, static_cast<float>(samples));
        const float speedMix = clampf(speed / 90.0f, 0.0f, 1.0f);
        const float step = clampf(lineStep * (0.34f + speedMix * 0.92f) +
                                      speed * streamStepScale() * 0.28f,
                                  1.0f, 16.0f);
        const tc::Vec2 next = p + dir * step;
        if (!insideWindow(next)) return;

        const float t = static_cast<float>(i) / std::max(1.0f, static_cast<float>(samples - 1));
        tc::setColor(streamlineColor(t, speed, seedIndex, direction));
        tc::drawLine(p.x, p.y, next.x, next.y);
        p = next;
    }
}

tc::Color tcApp::streamlineColor(float t, float speed, int seedIndex, int direction) const {
    const tc::Color blue(0.10f, 0.58f, 1.0f, 1.0f);
    const tc::Color magenta(1.0f, 0.12f, 0.92f, 1.0f);
    const tc::Color amber(1.0f, 0.70f, 0.18f, 1.0f);
    const float seedMix = hash01(seedIndex * 17 + (direction > 0 ? 3 : 11));
    const float speedMix = clampf(speed / 90.0f, 0.0f, 1.0f);
    tc::Color color = mixColor(blue, magenta, clampf(t * 0.72f + seedMix * 0.28f, 0.0f, 1.0f));
    color = mixColor(color, amber, speedMix * 0.35f);
    color.a = (0.36f + speedMix * 0.58f) * (1.0f - t * 0.44f);
    return color;
}
