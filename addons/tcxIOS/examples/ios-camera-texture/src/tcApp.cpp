#include "tcApp.h"

#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);
    startCamera();
    tc::redraw();
}

void tcApp::update() {
    ios::update();
    if (ios::camera().latestFrame(latestFrame_)) {
        ++frameCount_;
        textureReady_ = ios::camera().uploadLatestFrameToTexture(cameraTexture_);
        tc::redraw();
    }
}

void tcApp::draw() {
    tc::clear(0.06f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS camera texture\n\n"
         << "Camera running: " << (ios::camera().isRunning() ? "yes" : "no") << "\n"
         << "Frames seen: " << frameCount_ << "\n"
         << "Latest frame: " << latestFrame_.width << " x " << latestFrame_.height
         << ", bytes " << latestFrame_.data.size() << "\n\n"
         << "C: start/stop camera\n"
         << "H: haptic impact\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);

    const float previewX = 24.0f;
    const float previewY = 250.0f;
    const float previewW = 320.0f;
    const float previewH = 180.0f;
    if (textureReady_) {
        tc::setColor(1.0f);
        cameraTexture_.draw(previewX, previewY, previewW, previewH);
    } else {
        tc::setColor(0.2f, 0.4f, 0.9f, 0.35f);
        tc::drawRect(previewX, previewY, previewW, previewH);
        tc::setColor(1.0f);
        tc::drawBitmapString("Waiting for camera frame", previewX + 18.0f, previewY + 82.0f);
    }
}

void tcApp::keyPressed(int key) {
    if (key == 'C') {
        if (ios::camera().isRunning()) {
            ios::camera().stop();
            status_ = "Camera stopped.";
        } else {
            startCamera();
        }
        tc::redraw();
    } else if (key == 'H') {
        bool ok = ios::haptics().impact(ios::HapticImpactStyle::Medium);
        status_ = ok ? "Haptic impact requested." : "Haptic impact unavailable.";
        tc::redraw();
    }
}

void tcApp::startCamera() {
    status_ = "Requesting camera permission.";
    ios::permissions().request(ios::Permission::Camera, [this](ios::Result<ios::PermissionState> result) {
        if (!result.ok) {
            status_ = "Camera permission failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
            return;
        }
        if (result.value != ios::PermissionState::Authorized) {
            status_ = "Camera permission is " + ios::toString(result.value) + ".";
            tc::redraw();
            return;
        }
        startCameraAfterPermission();
    });
}

void tcApp::startCameraAfterPermission() {
    ios::CameraConfig config;
    config.width = 1280;
    config.height = 720;
    config.framesPerSecond = 30;
    ios::camera().start(config, [this](ios::Result<void> result) {
        status_ = result.ok
            ? "Camera started."
            : "Camera start failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    });
}
