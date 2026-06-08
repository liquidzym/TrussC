#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>

void tcApp::setup() {
    resizeSystems();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = std::min(1.0f / 30.0f, static_cast<float>(tc::getDeltaTime()));
    const float time = tc::getElapsedTimef();
    if (!paused_) {
        injectReferenceSource(time);
        handleMouseInput();
        fluid_.update(dt);
        particles_.update(fluid_, dt);
    }
}

void tcApp::draw() {
    tc::clear(0.0f, 0.0f, 0.0f);
    if (showFluid_) {
        drawFluidView();
    }
    particles_.draw(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    drawObstacles();

    const std::string gpu = particles_.lastUpdateUsedGpu() ? "GPU texture particles" : "CPU fallback particles";
    tc::setColor(1.0f);
    tc::drawBitmapString("fluid-custom-particles | left velocity+particles | middle heat+particles | right particles",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("PixelFlow Fluid_CustomParticles parity | " + gpu +
                             " | count " + tc::toString(particles_.particleCount()) +
                             " | view " + viewName(),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
    tc::drawBitmapString("p pause | q fluid | w velocity | 1 density 2 temp 3 pressure 4 velocity 5 combined | +/- grid | r reset | " +
                             std::string(paused_ ? "paused" : "running"),
                         18, 28 + tcx::flow::example::kHudLine * 2.0f,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::digitKey;
    using tcx::flow::example::keyIs;
    const int digit = digitKey(key);
    if (digit == 1) fluidView_ = FluidView::Density;
    if (digit == 2) fluidView_ = FluidView::Temperature;
    if (digit == 3) fluidView_ = FluidView::Pressure;
    if (digit == 4) fluidView_ = FluidView::Velocity;
    if (digit == 5) fluidView_ = FluidView::Combined;
    if (keyIs(key, 'p')) paused_ = !paused_;
    if (keyIs(key, 'q')) showFluid_ = !showFluid_;
    if (keyIs(key, 'w')) {
        showFluid_ = true;
        fluidView_ = FluidView::Velocity;
    }
    if (key == static_cast<int>('+') || key == static_cast<int>('=')) {
        cycleGridScale(1);
    }
    if (key == static_cast<int>('-') || key == static_cast<int>('_')) {
        cycleGridScale(-1);
    }
    if (keyIs(key, 'r')) {
        fluid_.reset();
        particles_.reset();
        configureObstacles();
        paused_ = false;
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
    const float scales[] = {0.50f, 0.333f, 0.25f};
    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = scales[std::clamp(gridPreset_, 0, 2)];
    fluidSettings.outputResolutionScale = 1.0f;
    fluidSettings.timestep = 0.125f;
    fluidSettings.solverIterations = 34;
    fluidSettings.enableVorticity = true;
    fluidSettings.vorticity = 0.18f;
    fluidSettings.enableTemperature = true;
    fluidSettings.enableBuoyancy = true;
    fluidSettings.buoyancy = 0.24f;
    fluidSettings.velocityDissipation = 0.990f;
    fluidSettings.densityDissipation = 0.999f;
    fluidSettings.temperatureDissipation = 0.80f;
    fluidSettings.viscosity = 0.004f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), fluidSettings);
    configureObstacles();

    tcx::flow::ParticleFlowSettings particleSettings;
    particleSettings.particleCount = 65536;
    particleSettings.lifetime = 8.0f;
    particleSettings.lifespanSpread = 0.30f;
    particleSettings.velocityScale = 18.0f;
    particleSettings.damping = 0.992f;
    particleSettings.spawnRadius = 120.0f;
    particleSettings.particleSize = 1.35f;
    particleSettings.mass = 0.92f;
    particleSettings.massSpread = 0.55f;
    particleSettings.sizeSpread = 0.32f;
    particleSettings.particleColor = tc::Color(1.0f, 0.64f, 0.16f, 0.62f);
    particleSettings.variant = tcx::flow::ParticleFlowVariant::Flow;
    particles_.setup(tc::getWindowWidth(), tc::getWindowHeight(), particleSettings);
}

void tcApp::configureObstacles() {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float s = std::min(w, h);
    obstacles_.clear();
    obstacles_.push_back({tc::Vec2(w * 0.50f, h * 0.27f), s * 0.115f});
    obstacles_.push_back({tc::Vec2(w * 0.50f, h * 0.27f), s * 0.080f});

    fluid_.clearObstacles();
    for (const auto& obstacle : obstacles_) {
        fluid_.addObstacle(obstacle.position, obstacle.radius);
    }
}

void tcApp::injectReferenceSource(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const tc::Vec2 source(w * 0.50f + std::sin(time * 0.36f) * w * 0.08f,
                          h * 0.08f + std::cos(time * 0.47f) * h * 0.020f);
    fluid_.addDensity(source, 42.0f, tc::Color(0.20f, 0.30f, 0.50f, 0.60f));
    fluid_.addTemperature(source, 40.0f, 1.0f);
    fluid_.addVelocity(source, 46.0f, tc::Vec2(34.0f + std::sin(time * 0.9f) * 24.0f, 72.0f));
    particles_.spawn(source, 42.0f, 120);
}

void tcApp::handleMouseInput() {
    const tc::Vec2 mouse = tc::getMousePos();
    if (!tc::isMousePressed()) {
        previousMouse_ = mouse;
        activeMouseButton_ = -1;
        wasMousePressed_ = false;
        return;
    }
    // PixelFlow adds particles per source; spawn requests stay independent in the GPU path.
    if (activeMouseButton_ < 0) {
        activeMouseButton_ = tc::getMouseButton();
    }
    if (!wasMousePressed_) {
        previousMouse_ = mouse;
    }

    const tc::Vec2 delta = mouse - previousMouse_;
    if (activeMouseButton_ == 0) {
        const float distance = delta.length();
        if (distance > 0.0f) {
            fluid_.addDensity(mouse, 16.0f, tc::Color(0.25f, 0.0f, 0.10f, 0.72f));
            fluid_.addVelocity(mouse, 24.0f, delta * 15.0f);
        }
        particles_.spawn(mouse, 32.0f, 320);
    } else if (activeMouseButton_ == 2) {
        fluid_.addDensity(mouse, 18.0f, tc::Color(0.25f, 0.0f, 0.10f, 0.62f));
        fluid_.addTemperature(mouse, 24.0f, 2.0f);
        particles_.spawn(mouse, 20.0f, 120);
    } else {
        particles_.spawn(mouse, 58.0f, 320);
    }

    previousMouse_ = mouse;
    wasMousePressed_ = true;
}

void tcApp::drawFluidView() const {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    switch (fluidView_) {
        case FluidView::Density:
            fluid_.drawDensity(0, 0, w, h);
            break;
        case FluidView::Temperature:
            fluid_.drawTemperature(0, 0, w, h);
            break;
        case FluidView::Pressure:
            fluid_.drawPressure(0, 0, w, h);
            break;
        case FluidView::Velocity:
            fluid_.drawVelocity(0, 0, w, h);
            break;
        case FluidView::Combined:
            fluid_.drawCombined(0, 0, w, h);
            break;
    }
    tc::setColor(0.0f, 0.0f, 0.0f, 0.22f);
    tc::drawRect(0, 0, w, h);
}

void tcApp::drawObstacles() const {
    tc::setColor(0.78f, 0.80f, 0.78f, 0.42f);
    for (const auto& obstacle : obstacles_) {
        tc::drawCircle(obstacle.position.x, obstacle.position.y, obstacle.radius);
    }
}

void tcApp::cycleGridScale(int direction) {
    gridPreset_ = std::clamp(gridPreset_ - direction, 0, 2);
    resizeSystems();
}

std::string tcApp::viewName() const {
    switch (fluidView_) {
        case FluidView::Density: return "density";
        case FluidView::Temperature: return "temperature";
        case FluidView::Pressure: return "pressure";
        case FluidView::Velocity: return "velocity";
        case FluidView::Combined: return "combined";
    }
    return "unknown";
}
