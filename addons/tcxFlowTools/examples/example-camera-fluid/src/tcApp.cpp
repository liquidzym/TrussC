#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <cmath>
#include <string>

void tcApp::setup() {
    grabber_.setDeviceID(0);
    grabber_.setDesiredFrameRate(60);
    cameraStarted_ = grabber_.setup(1280, 720);
    resizeSystems();
}

void tcApp::update() {
    const float dt = static_cast<float>(tc::getDeltaTime());
    const float t = tc::getElapsedTimef();
    grabber_.update();
    if (!cameraStarted_ && grabber_.isInitialized()) {
        cameraStarted_ = true;
    }

    if (grabber_.isInitialized() && grabber_.getTexture().isAllocated()) {
        updateCameraInput(dt);
    } else {
        updateFallbackInput(dt, t);
    }

    fluid_.update(dt);
}

void tcApp::draw() {
    tc::clear(0.035f, 0.035f, 0.045f);
    const float winW = static_cast<float>(tc::getWindowWidth());
    const float winH = static_cast<float>(tc::getWindowHeight());
    if (showCamera_ && grabber_.isInitialized()) {
        tc::setColor(1.0f, 1.0f, 1.0f, 0.42f);
        grabber_.draw(0, 0, winW, winH);
    }
    fluid_.drawDensity(0, 0, winW, winH);
    if (showFlow_) {
        opticalFlow_.drawFlow(0, 0, winW, winH);
    }
    tc::setColor(1.0f);
    const char* input = grabber_.isInitialized() ? "camera texture -> GPU optical flow" :
                        (grabber_.isPendingPermission() ? "waiting for camera permission" : "procedural fallback");
    const char* gpu = (fluid_.lastUpdateUsedGpu() && opticalFlow_.lastUpdateUsedGpu()) ? "GPU" : "fallback";
    const std::string label = std::string("tcxFlowTools camera-fluid | ") + input + " | " + gpu +
                              " | f flow | c camera | r reset";
    tc::drawBitmapString(label, 18, 28, tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'f')) showFlow_ = !showFlow_;
    if (keyIs(key, 'c')) showCamera_ = !showCamera_;
    if (keyIs(key, 'r')) fluid_.reset();
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings fluidSettings;
    fluidSettings.resolutionScale = 0.5f;
    fluidSettings.timestep = 0.145f;
    fluidSettings.solverIterations = 32;
    fluidSettings.velocityDissipation = 0.994f;
    fluidSettings.densityDissipation = 0.995f;
    fluidSettings.enableTemperature = true;
    fluidSettings.enableBuoyancy = true;
    fluidSettings.vorticity = 0.58f;
    fluidSettings.viscosity = 0.006f;
    fluidSettings.buoyancy = 0.16f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), fluidSettings);

    tcx::flow::OpticalFlowSettings flowSettings;
    flowSettings.strength = 1.65f;
    flowSettings.offset = 2.0f;
    flowSettings.lambda = 0.035f;
    flowSettings.threshold = 0.035f;
    flowSettings.decay = 0.92f;
    flowSettings.blurRadius = 2.2f;
    flowSettings.temporalSmoothing = 0.66f;
    opticalFlow_.setup(320, 180, flowSettings);
    fluid_.clearObstacles();
}

void tcApp::updateCameraInput(float dt) {
    opticalFlow_.update(grabber_.getTexture(), dt);
    if (const tc::Texture* flowTexture = opticalFlow_.getFlowTexture()) {
        fluid_.applyVelocityTexture(*flowTexture, 2.25f);
        fluid_.applyVelocityDensityTexture(*flowTexture, 0.76f, tc::Color(0.03f, 0.32f, 1.0f, 1.0f));
        fluid_.applyVelocityTemperatureTexture(*flowTexture, 0.18f);
    } else {
        fluid_.applyVelocityField(opticalFlow_.cpuFlow(), opticalFlow_.width(), opticalFlow_.height(), 1.6f);
    }
}

void tcApp::updateFallbackInput(float dt, float time) {
    opticalFlow_.updateProcedural(time, dt);
    fluid_.applyVelocityField(opticalFlow_.cpuFlow(), opticalFlow_.width(), opticalFlow_.height(), 1.6f);
    const tc::Vec2 p(tc::getWindowWidth() * (0.5f + std::sin(time * 1.4f) * 0.35f),
                     tc::getWindowHeight() * (0.5f + std::cos(time * 1.1f) * 0.28f));
    fluid_.addDensity(p, 54.0f, tc::Color(1.0f, 0.35f, 0.15f, 0.95f));
    fluid_.addTemperature(p, 54.0f, 1.0f);
}
