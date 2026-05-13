#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace {

float envFloat(const char* name, float fallback, float minValue, float maxValue) {
    if (const char* value = std::getenv(name)) {
        return std::clamp(static_cast<float>(std::atof(value)), minValue, maxValue);
    }
    return fallback;
}

std::filesystem::path referenceImagePath() {
    const std::filesystem::path here(__FILE__);
    return here.parent_path() / ".." / ".." / ".." / "_fcache" /
           "PixelFlow" / "examples" / "data" / "mc_escher.jpg";
}

} // namespace

void tcApp::setup() {
    // PixelFlow's 0.01 density texture mix is additive; this GPU pass uses a max
    // texture floor, so the parity default must be high enough to keep the source readable.
    continuousMix_ = envFloat("TCX_LIQUID_PAINTING_MIX", continuousMix_, 0.0f, 1.0f);
    if (const char* env = std::getenv("TCX_LIQUID_PAINTING_SOURCE")) {
        showSource_ = std::atoi(env) != 0;
    }
    sourceReady_ = sourceImage_.load(referenceImagePath());
    resizeSystems();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float time = tc::getElapsedTimef();

    renderImageSource();
    if (imageSource_.isAllocated()) {
        const float mix = updateFrame_ < 8 ? 0.95f : continuousMix_;
        fluid_.applyDensityTexture(imageSource_.getTexture(), mix);
    }
    injectPaintingFlow(time);

    const tc::Vec2 mouse = tc::getMousePos();
    if (tc::isMousePressed()) {
        if (!wasMousePressed_) {
            previousMouse_ = mouse;
        }
        const tc::Vec2 delta = mouse - previousMouse_;
        const float distance = delta.length();
        if (distance > 0.0f) {
            fluid_.addVelocity(mouse, 48.0f, delta * 36.0f);
        }
        if (tc::getMouseButton() == 1) {
            fluid_.addDensity(mouse, 42.0f, tc::Color(1.0f, 0.0f, 0.40f, 0.35f));
        } else if (tc::getMouseButton() == 2) {
            fluid_.addTemperature(mouse, 24.0f, 6.0f);
        }
        previousMouse_ = mouse;
        wasMousePressed_ = true;
    } else {
        previousMouse_ = mouse;
        wasMousePressed_ = false;
    }

    fluid_.update(dt);
    ++updateFrame_;
}

void tcApp::draw() {
    tc::clear(0.0f, 0.0f, 0.0f);
    if (showCombined_) {
        fluid_.drawCombined(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    } else {
        fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }

    if (showSource_ && imageSource_.isAllocated()) {
        const float previewW = 240.0f;
        const float previewH = previewW * static_cast<float>(imageSource_.getHeight()) /
                               std::max(1.0f, static_cast<float>(imageSource_.getWidth()));
        imageSource_.draw(18.0f, tc::getWindowHeight() - previewH - 18.0f, previewW, previewH);
    }

    tc::setColor(1.0f);
    tc::drawBitmapString("fluid-liquid-painting | drag inject | c combined | s source | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("PixelFlow Fluid_LiquidPainting parity target | mix " +
                             tc::toString(continuousMix_, 3),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'c')) showCombined_ = !showCombined_;
    if (keyIs(key, 's')) showSource_ = !showSource_;
    if (keyIs(key, 'r')) {
        fluid_.reset();
        updateFrame_ = 0;
    }
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
    settings.solverIterations = 38;
    settings.enableVorticity = true;
    settings.vorticity = 0.52f;
    settings.enableTemperature = true;
    settings.velocityDissipation = 0.986f;
    settings.densityDissipation = 0.9985f;
    settings.temperatureDissipation = 0.70f;
    settings.viscosity = 0.004f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);

    const int w = std::max(1, tc::getWindowWidth());
    const int h = std::max(1, tc::getWindowHeight());
    if (!imageSource_.isAllocated() || imageSource_.getWidth() != w || imageSource_.getHeight() != h) {
        imageSource_.allocate(w, h, 1, tc::TextureFormat::RGBA8);
        imageSource_.getTexture().setFilter(tc::TextureFilter::Linear);
        updateFrame_ = 0;
    }
}

void tcApp::renderImageSource() {
    if (!imageSource_.isAllocated()) return;

    const float w = static_cast<float>(imageSource_.getWidth());
    const float h = static_cast<float>(imageSource_.getHeight());
    imageSource_.begin(0.0f, 0.0f, 0.0f, 0.0f);
    if (sourceReady_) {
        const float drawH = h * 0.89f;
        const float drawW = drawH * static_cast<float>(sourceImage_.getWidth()) /
                            std::max(1.0f, static_cast<float>(sourceImage_.getHeight()));
        sourceImage_.draw(w * 0.24f - drawW * 0.5f, h * 0.50f - drawH * 0.5f, drawW, drawH);
    } else {
        tc::setColor(0.88f, 0.58f, 0.30f, 1.0f);
        tc::drawRect(w * 0.10f, h * 0.08f, w * 0.36f, h * 0.84f);
        tc::setColor(0.15f, 0.12f, 0.09f, 1.0f);
        for (int i = 0; i < 14; ++i) {
            const float y = h * (0.10f + i * 0.06f);
            tc::drawTriangle(w * 0.11f, y, w * 0.43f, y + h * 0.02f, w * 0.20f, y + h * 0.06f);
        }
    }
    imageSource_.end();
}

void tcApp::injectPaintingFlow(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float edgeX = w * 0.42f;
    const float radius = h * 0.095f;

    for (int i = 0; i < 4; ++i) {
        const float phase = time * (0.58f + i * 0.11f) + i * 1.7f;
        const tc::Vec2 p(edgeX + std::sin(phase * 0.7f) * w * 0.020f,
                         h * (0.18f + i * 0.20f) + std::cos(phase) * h * 0.065f);
        const tc::Vec2 v(w * (0.045f + i * 0.006f),
                         std::sin(phase * 1.2f) * h * 0.030f);
        fluid_.addVelocity(p, radius, v);
        fluid_.addTemperature(p, radius * 0.55f, std::sin(phase) * 0.35f);
    }
}
