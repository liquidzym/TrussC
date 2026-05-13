#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

float envFloat(const char* name, float fallback, float minValue, float maxValue) {
    if (const char* value = std::getenv(name)) {
        return std::clamp(static_cast<float>(std::atof(value)), minValue, maxValue);
    }
    return fallback;
}

void drawThickText(const std::string& text, float x, float y, float scale, const tc::Color& color) {
    tc::setColor(color);
    for (int oy = -2; oy <= 2; ++oy) {
        for (int ox = -2; ox <= 2; ++ox) {
            if (std::abs(ox) + std::abs(oy) > 3) continue;
            tc::drawBitmapString(text, x + static_cast<float>(ox), y + static_cast<float>(oy), scale);
        }
    }
}

} // namespace

void tcApp::setup() {
    densityMix_ = envFloat("TCX_LIQUID_TEXT_DENSITY", densityMix_, 0.0f, 0.25f);
    temperatureMix_ = envFloat("TCX_LIQUID_TEXT_TEMPERATURE", temperatureMix_, 0.0f, 0.12f);
    if (const char* env = std::getenv("TCX_LIQUID_TEXT_SOURCE")) {
        showSource_ = std::atoi(env) != 0;
    }
    resizeSystems();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float time = tc::getElapsedTimef();

    renderTextSource(time);
    fluid_.applyDensityTexture(textSource_.getTexture(), densityMix_);
    fluid_.applyTemperatureTexture(textSource_.getTexture(), temperatureMix_);
    injectProceduralVelocity(time);

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
    tc::clear(0.01f, 0.012f, 0.016f);
    if (showCombined_) {
        fluid_.drawCombined(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    } else {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }

    if (showSource_ && textSource_.isAllocated()) {
        const float previewW = 240.0f;
        const float previewH = previewW * static_cast<float>(textSource_.getHeight()) /
                               std::max(1.0f, static_cast<float>(textSource_.getWidth()));
        textSource_.draw(tc::getWindowWidth() - previewW - 18.0f, 18.0f, previewW, previewH);
    }

    tc::setColor(1.0f);
    tc::drawBitmapString("fluid-liquid-text | drag inject | c combined | s source | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("PixelFlow Fluid_LiquidText parity target",
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
    tc::drawBitmapString("density " + tc::toString(densityMix_, 3) +
                             " temp " + tc::toString(temperatureMix_, 3) +
                             " sim " + tc::toString(fluid_.simWidth()) + "x" +
                             tc::toString(fluid_.simHeight()),
                         18, 28 + tcx::flow::example::kHudLine * 2.0f,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'c')) showCombined_ = !showCombined_;
    if (keyIs(key, 's')) showSource_ = !showSource_;
    if (keyIs(key, 'r')) fluid_.reset();
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    (void)button;
    previousMouse_ = pos;
    wasMousePressed_ = true;
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
    settings.enableTemperature = true;
    settings.enableBuoyancy = true;
    settings.enableVorticity = true;
    settings.buoyancy = 0.42f;
    settings.vorticity = 0.72f;
    settings.velocityDissipation = 0.992f;
    settings.densityDissipation = 0.992f;
    settings.temperatureDissipation = 0.992f;
    settings.viscosity = 0.006f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);

    const int w = std::max(1, tc::getWindowWidth());
    const int h = std::max(1, tc::getWindowHeight());
    if (!textSource_.isAllocated() || textSource_.getWidth() != w || textSource_.getHeight() != h) {
        textSource_.allocate(w, h, 1, tc::TextureFormat::RGBA8);
        textSource_.getTexture().setFilter(tc::TextureFilter::Linear);
    }
}

void tcApp::renderTextSource(float time) {
    if (!textSource_.isAllocated()) return;

    const float w = static_cast<float>(textSource_.getWidth());
    const float h = static_cast<float>(textSource_.getHeight());
    const float scale = std::clamp(w / 170.0f, 3.5f, 8.0f);
    const float centerX = w * 0.14f + std::sin(time * 0.18f) * w * 0.012f;
    const float startY = h * 0.24f;
    const float line = 18.0f * scale;

    textSource_.begin(0.0f, 0.0f, 0.0f, 0.0f);
    drawThickText("Processing", centerX, startY, scale, tc::Color::fromBytes(230, 130, 0, 255));
    drawThickText("Fluid", centerX + w * 0.20f, startY + line, scale, tc::Color::fromBytes(0, 130, 230, 255));
    drawThickText("Simulation", centerX + w * 0.04f, startY + line * 2.0f, scale, tc::Color::fromBytes(0, 130, 230, 255));
    textSource_.end();
}

void tcApp::injectProceduralVelocity(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float radius = std::min(w, h) * 0.12f;

    const tc::Vec2 p0(w * (0.46f + std::sin(time * 0.72f) * 0.22f),
                      h * (0.45f + std::cos(time * 0.61f) * 0.18f));
    const tc::Vec2 p1(w * (0.54f + std::sin(time * 0.48f + 2.4f) * 0.24f),
                      h * (0.57f + std::cos(time * 0.77f + 1.3f) * 0.20f));
    fluid_.addVelocity(p0, radius, tc::Vec2(std::cos(time * 0.72f) * 72.0f,
                                            -std::sin(time * 0.61f) * 56.0f));
    fluid_.addVelocity(p1, radius * 0.82f, tc::Vec2(-std::cos(time * 0.48f + 2.4f) * 64.0f,
                                                    std::sin(time * 0.77f + 1.3f) * 62.0f));
}
