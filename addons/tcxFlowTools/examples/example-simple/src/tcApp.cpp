#include "tcApp.h"

#include "../../common/ExampleControls.h"

void tcApp::setup() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.5f;
    settings.timestep = 0.125f;
    settings.solverIterations = 40;
    settings.enableVorticity = true;
    settings.vorticity = 1.0f;
    settings.velocityDissipation = 0.995f;
    settings.densityDissipation = 0.998f;
    settings.viscosity = 0.04f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
    previousMouse_ = tc::Vec2(tc::getMouseX(), tc::getMouseY());
}

void tcApp::update() {
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
    fluid_.update(static_cast<float>(tc::getDeltaTime()));
}

void tcApp::draw() {
    tc::clear(0.04f, 0.045f, 0.055f);
    switch (mode_) {
        case tcx::flow::FlowVisualizer::Mode::Density:
            fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::Velocity:
            fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::VelocityField:
        case tcx::flow::FlowVisualizer::Mode::VelocityDots:
            fluid_.drawVelocity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::Pressure:
        case tcx::flow::FlowVisualizer::Mode::PressureField:
            fluid_.drawPressure(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
        case tcx::flow::FlowVisualizer::Mode::Temperature:
        case tcx::flow::FlowVisualizer::Mode::TemperatureField:
            fluid_.drawTemperature(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
            break;
    }

    tc::setColor(1.0f);
    tc::drawBitmapString("simple | drag inject | d/v/p/t view | r reset", 18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("sim " + tc::toString(fluid_.simWidth()) + "x" + tc::toString(fluid_.simHeight()),
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'r')) fluid_.reset();
    if (keyIs(key, 'd')) mode_ = tcx::flow::FlowVisualizer::Mode::Density;
    if (keyIs(key, 'v')) mode_ = tcx::flow::FlowVisualizer::Mode::Velocity;
    if (keyIs(key, 'p')) mode_ = tcx::flow::FlowVisualizer::Mode::Pressure;
    if (keyIs(key, 't')) mode_ = tcx::flow::FlowVisualizer::Mode::Temperature;
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
    fluid_.resize(width, height);
}
