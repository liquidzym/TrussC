#include "tcApp.h"

#include <filesystem>
#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);
    tc::redraw();
}

void tcApp::update() {
    ios::update();
    if (ios::arKit().latestFrame(arFrame_)) tc::redraw();
}

void tcApp::draw() {
    tc::clear(0.04f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS AR / Vision / CoreML\n\n"
         << "AR world tracking supported: " << (ios::arKit().isWorldTrackingSupported() ? "yes" : "no") << "\n"
         << "Latest AR frame: " << arFrame_.cameraImageWidth << " x " << arFrame_.cameraImageHeight << "\n\n"
         << "A: start AR world tracking\n"
         << "V: run Vision rectangles on /tmp/tcxios-vision.png\n"
         << "S: make foreground mask for /tmp/tcxios-vision.png\n"
         << "M: inspect /tmp/tcxios-model.mlmodelc\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'A') {
        ios::arKit().start({}, [this](ios::Result<void> result) {
            status_ = result.ok
                ? "AR session started."
                : "AR failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    } else if (key == 'V') {
        ios::vision().detectRectangles(std::filesystem::temp_directory_path() / "tcxios-vision.png",
                                       [this](ios::Result<std::vector<ios::VisionRectangle>> result) {
            status_ = result.ok
                ? "Vision rectangles: " + std::to_string(result.value.size())
                : "Vision failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    } else if (key == 'S') {
        ios::VisionMaskRequest request;
        request.imagePath = std::filesystem::temp_directory_path() / "tcxios-vision.png";
        request.kind = ios::VisionMaskKind::ForegroundInstances;
        request.outputWidth = 320;
        request.outputHeight = 320;
        ios::vision().makeMask(request, [this](ios::Result<ios::VisionMaskResult> result) {
            if (result.ok) {
                status_ = "Vision mask: " + std::to_string(result.value.width) + " x " +
                          std::to_string(result.value.height) + ", " +
                          std::to_string(result.value.alpha.size()) + " alpha bytes";
            } else {
                status_ = "Vision mask failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            }
            tc::redraw();
        });
    } else if (key == 'M') {
        auto result = ios::coreML().inspectModel(std::filesystem::temp_directory_path() / "tcxios-model.mlmodelc");
        status_ = result.ok
            ? "CoreML model is loadable."
            : "CoreML failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    }
}
