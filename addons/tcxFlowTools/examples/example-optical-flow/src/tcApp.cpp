#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <cmath>

void tcApp::setup() {
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float t = tc::getElapsedTimef();
    opticalFlow_.updateProcedural(t, dt);
    fluid_.applyVelocityField(opticalFlow_.cpuFlow(), opticalFlow_.width(), opticalFlow_.height(), flowToFluidScale_);

    const float x = tc::getWindowWidth() * (0.5f + std::sin(t * 0.9f) * 0.25f);
    const float y = tc::getWindowHeight() * (0.5f + std::cos(t * 1.2f) * 0.25f);
    fluid_.addDensity(tc::Vec2(x, y), 42.0f, tc::Color(0.15f, 0.65f, 1.0f, 1.0f));
    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.035f, 0.04f, 0.05f);
    fluid_.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    if (showFlow_) {
        opticalFlow_.drawFlow(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    tc::setColor(1.0f);
    tc::drawBitmapString("optical-flow | f overlay | +/- strength", 18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("flow scale " + tc::toString(flowToFluidScale_, 2),
                         18, 28 + tcx::flow::example::kHudLine, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'f')) showFlow_ = !showFlow_;
    if (key == '+' || key == '=') flowToFluidScale_ += 0.5f;
    if (key == '-') flowToFluidScale_ = std::max(0.0f, flowToFluidScale_ - 0.5f);
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = 0.25f;
    fluidSettings.solverIterations = 16;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), fluidSettings);

    tcx::flow::OpticalFlowSettings flowSettings;
    flowSettings.strength = 1.0f;
    flowSettings.temporalSmoothing = 0.65f;
    opticalFlow_.setup(96, 54, flowSettings);
}
