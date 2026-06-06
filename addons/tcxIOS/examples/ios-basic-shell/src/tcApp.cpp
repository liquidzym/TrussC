#include "tcApp.h"

#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);
    tc::redraw();
}

void tcApp::update() {
    ios::update();
}

void tcApp::draw() {
    tc::clear(0.08f);
    tc::setColor(1.0f);

    const ios::ScreenInfo screen = ios::app().mainScreen();
    const ios::SafeAreaInsets safeArea = ios::app().safeAreaInsets();
    const ios::DeviceInfo device = ios::app().deviceInfo();

    std::ostringstream text;
    text << "tcxIOS basic shell\n\n"
         << "App state: " << ios::toString(ios::app().state()) << "\n"
         << "Orientation: " << ios::toString(ios::app().orientation()) << "\n"
         << "Screen: " << screen.pixelWidth << " x " << screen.pixelHeight
         << " @ " << screen.scale << "x, max FPS " << screen.maximumFramesPerSecond << "\n"
         << "Safe area: top " << safeArea.top << ", left " << safeArea.left
         << ", bottom " << safeArea.bottom << ", right " << safeArea.right << "\n"
         << "Device: " << device.model << " / " << device.systemName
         << " " << device.systemVersion << "\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'A') {
        status_ = "Alert requested.";
        ios::nativeUI().showAlert({"tcxIOS", "Native alert request from TrussC.", {"OK"}, -1},
                                  [this](ios::Result<ios::AlertResult> result) {
            if (result.ok) {
                status_ = "Alert closed with button " + std::to_string(result.value.buttonIndex) + ".";
            } else {
                status_ = "Alert failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            }
            tc::redraw();
        });
        tc::redraw();
    } else if (key == 'U') {
        status_ = "Open URL requested.";
        ios::nativeUI().openURL("https://trussc.org", [this](ios::Result<void> result) {
            status_ = result.ok
                ? "URL open request accepted."
                : "URL open failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
        tc::redraw();
    }
}
