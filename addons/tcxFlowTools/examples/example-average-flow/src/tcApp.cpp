#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <fstream>

void tcApp::setup() {
    regions_[0].color = tc::Color(0.95f, 0.25f, 0.36f, 0.92f);
    regions_[1].color = tc::Color(0.18f, 0.78f, 0.96f, 0.92f);
    regions_[2].color = tc::Color(0.44f, 0.90f, 0.38f, 0.92f);
    regions_[3].color = tc::Color(1.00f, 0.78f, 0.18f, 0.92f);
    resizeSystems();
    loadAverageSettings();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = std::min(1.0f / 30.0f, static_cast<float>(tc::getDeltaTime()));
    const float time = tc::getElapsedTimef();
    if (!paused_) {
        injectProceduralFlow(time, dt);
        handleMouseFlow();
        fluid_.update(dt);
    }

    readbackOk_ = fluid_.refreshVelocityReadback();
    if (readbackOk_) {
        for (auto& region : regions_) {
            region.flow.update(fluid_, 12, 18);
        }
    }
}

void tcApp::draw() {
    tc::clear(0.0f, 0.0f, 0.0f);
    drawView();
    if (showVectors_) {
        fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }

    if (showAverage_) {
        for (int i = 0; i < static_cast<int>(regions_.size()); ++i) {
            drawRegion(regions_[static_cast<std::size_t>(i)], i);
        }
    }

    tc::setColor(1.0f);
    tc::drawBitmapString("average-flow | drag velocity | a average | v vectors | p pause | s save l load | 1 density 2 velocity 3 combined | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("ofxFlowTools AverageFlowWatcher parity | ROI average | magnitude event | persistent settings | view " + viewName() +
                             " | " + std::string(readbackOk_ ? "readback on" : "readback unavailable") +
                             " | " + std::string(paused_ ? "paused" : "running"),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit == 1) viewMode_ = ViewMode::Density;
    if (digit == 2) viewMode_ = ViewMode::Velocity;
    if (digit == 3) viewMode_ = ViewMode::Combined;
    if (keyIs(key, 'a')) showAverage_ = !showAverage_;
    if (keyIs(key, 'v')) showVectors_ = !showVectors_;
    if (keyIs(key, 'p')) paused_ = !paused_;
    if (keyIs(key, 's')) saveAverageSettings();
    if (keyIs(key, 'l')) loadAverageSettings();
    if (keyIs(key, 'r')) {
        fluid_.reset();
        for (auto& region : regions_) {
            region.flow.reset();
        }
        paused_ = false;
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = 0.333f;
    fluidSettings.outputResolutionScale = 1.0f;
    fluidSettings.timestep = 0.125f;
    fluidSettings.solverIterations = 28;
    fluidSettings.enableVorticity = true;
    fluidSettings.vorticity = 0.45f;
    fluidSettings.velocityDissipation = 0.992f;
    fluidSettings.densityDissipation = 0.996f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), fluidSettings);

    tcx::flow::OpticalFlowSettings flowSettings;
    flowSettings.strength = 1.4f;
    flowSettings.temporalSmoothing = 0.72f;
    opticalFlow_.setup(128, 72, flowSettings);
    configureRegions();
}

void tcApp::configureRegions() {
    for (int i = 0; i < static_cast<int>(regions_.size()); ++i) {
        const float x = (static_cast<float>(i) + 1.0f) / (static_cast<float>(regions_.size()) + 1.0f);
        regions_[static_cast<std::size_t>(i)].flow.setRoi(x - 0.10f, 0.18f, 0.20f, 0.62f);
        regions_[static_cast<std::size_t>(i)].flow.setNormalization(0.40f);
        regions_[static_cast<std::size_t>(i)].flow.setEventThreshold(0.16f);
        regions_[static_cast<std::size_t>(i)].flow.setEventBase(0.55f);
        regions_[static_cast<std::size_t>(i)].flow.setHistoryCapacity(96);
        regions_[static_cast<std::size_t>(i)].flow.clearHistory();
    }
}

void tcApp::injectProceduralFlow(float time, float dt) {
    opticalFlow_.updateProcedural(time, dt);
    fluid_.applyVelocityField(opticalFlow_.cpuFlow(), opticalFlow_.width(), opticalFlow_.height(), 1.8f);

    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    for (int i = 0; i < 3; ++i) {
        const float phase = time * (0.55f + 0.18f * i) + static_cast<float>(i) * 2.1f;
        const tc::Vec2 p(w * (0.50f + std::sin(phase) * (0.30f - 0.04f * i)),
                         h * (0.50f + std::cos(phase * 0.83f) * (0.22f + 0.03f * i)));
        const tc::Vec2 v(std::cos(phase) * 92.0f, -std::sin(phase * 0.83f) * 76.0f);
        fluid_.addVelocity(p, 46.0f, v);
        fluid_.addDensity(p, 34.0f, tc::Color(0.08f + 0.08f * i, 0.35f, 0.88f - 0.12f * i, 0.50f));
    }
}

void tcApp::handleMouseFlow() {
    const tc::Vec2 mouse = tc::getMousePos();
    if (tc::isMousePressed()) {
        const tc::Vec2 delta = mouse - previousMouse_;
        fluid_.addVelocity(mouse, 38.0f, delta * 16.0f);
        fluid_.addDensity(mouse, 28.0f, tc::Color(1.0f, 0.30f, 0.10f, 0.72f));
    }
    previousMouse_ = mouse;
}

void tcApp::drawView() const {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    switch (viewMode_) {
        case ViewMode::Density:
            fluid_.drawDensity(0, 0, w, h);
            break;
        case ViewMode::Velocity:
            fluid_.drawVelocity(0, 0, w, h);
            break;
        case ViewMode::Combined:
            fluid_.drawCombined(0, 0, w, h);
            break;
    }
    tc::setColor(0.0f, 0.0f, 0.0f, 0.18f);
    tc::drawRect(0, 0, w, h);
}

void tcApp::drawRegion(const RegionState& region, int index) const {
    const auto& roi = region.flow.roi();
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float x = roi.x * w;
    const float y = roi.y * h;
    const float rw = roi.width * w;
    const float rh = roi.height * h;
    drawRegionBorder(roi, region.color);

    const tc::Vec2 center(x + rw * 0.5f, y + rh * 0.5f);
    const tc::Vec2 arrow = region.flow.velocity() * std::min(rw, rh) * 0.46f;
    tc::setColor(region.color);
    tc::drawLine(center.x, center.y, center.x + arrow.x, center.y + arrow.y);
    tc::drawCircle(center.x + arrow.x, center.y + arrow.y, 5.0f + region.flow.magnitude() * 18.0f);

    const float graphH = std::min(72.0f, rh * 0.22f);
    const float gy = y + rh - graphH - 8.0f;
    const auto& history = region.flow.history();
    if (history.size() > 1) {
        tc::setColor(region.color.r, region.color.g, region.color.b, 0.78f);
        const float historyCapacity = static_cast<float>(std::max<std::size_t>(1, region.flow.historyCapacity()));
        for (std::size_t i = 1; i < history.size(); ++i) {
            const float x0 = x + static_cast<float>(i - 1) * rw / historyCapacity;
            const float x1 = x + static_cast<float>(i) * rw / historyCapacity;
            const float y0 = gy + graphH * (1.0f - history[i - 1]);
            const float y1 = gy + graphH * (1.0f - history[i]);
            tc::drawLine(x0, y0, x1, y1);
        }
    }

    tc::setColor(region.flow.magnitudeEvent() ? tc::Color(1.0f, 0.18f, 0.10f, 0.90f) : region.color);
    const std::string events = std::string(region.flow.magnitudeEvent() ? "mag*" : "mag ") +
        " vx " + std::to_string(region.flow.velocityEventX()) +
        " vy " + std::to_string(region.flow.velocityEventY());
    tc::drawBitmapString("ROI average " + std::to_string(index + 1) + " | magnitude event " + events,
                         x + 8.0f, y + 22.0f, 0.82f);
}

void tcApp::drawRegionBorder(const tcx::flow::AverageFlow::Region& roi, const tc::Color& color) const {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float x = roi.x * w;
    const float y = roi.y * h;
    const float rw = roi.width * w;
    const float rh = roi.height * h;
    tc::setColor(0.0f, 0.0f, 0.0f, 0.26f);
    tc::drawRect(x, y, rw, rh);
    tc::setColor(color);
    tc::drawLine(x, y, x + rw, y);
    tc::drawLine(x + rw, y, x + rw, y + rh);
    tc::drawLine(x + rw, y + rh, x, y + rh);
    tc::drawLine(x, y + rh, x, y);
}

std::string tcApp::settingsPath() const {
    return "example-average-flow-settings.txt";
}

bool tcApp::saveAverageSettings() const {
    std::ofstream file(settingsPath());
    if (!file.is_open()) return false;
    for (const auto& region : regions_) {
        file << region.flow.serializeSettings() << '\n';
    }
    return file.good();
}

bool tcApp::loadAverageSettings() {
    std::ifstream file(settingsPath());
    if (!file.is_open()) return false;
    bool loaded = false;
    std::string line;
    for (auto& region : regions_) {
        if (!std::getline(file, line)) break;
        loaded = region.flow.applySettingsString(line) || loaded;
    }
    return loaded;
}

std::string tcApp::viewName() const {
    switch (viewMode_) {
        case ViewMode::Density: return "density";
        case ViewMode::Velocity: return "velocity";
        case ViewMode::Combined: return "combined";
    }
    return "unknown";
}
