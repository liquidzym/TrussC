#include "tcApp.h"

#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);

    ios::networkStatus().start([this](const ios::NetworkPath& path) {
        networkPath_ = path;
        tc::redraw();
    });

    tc::redraw();
}

void tcApp::update() {
    ios::update();
}

void tcApp::draw() {
    tc::clear(0.07f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS location / notification\n\n"
         << "Network: " << ios::toString(networkPath_.status)
         << ", expensive " << (networkPath_.expensive ? "yes" : "no")
         << ", constrained " << (networkPath_.constrained ? "yes" : "no") << "\n"
         << "Location running: " << (ios::location().isRunning() ? "yes" : "no") << "\n"
         << "Location updates: " << locationUpdates_ << "\n"
         << "Latest: " << latestLocation_.coordinate.latitude
         << ", " << latestLocation_.coordinate.longitude << "\n\n"
         << "L: request/start location\n"
         << "N: schedule local notification\n"
         << "W: open Safari\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'L') {
        startLocation();
    } else if (key == 'N') {
        scheduleNotification();
    } else if (key == 'W') {
        status_ = "Opening Safari.";
        ios::web().openSafari({"https://trussc.org", false, true}, [this](ios::Result<void> result) {
            status_ = result.ok
                ? "Safari dismissed."
                : "Safari failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
        tc::redraw();
    }
}

void tcApp::startLocation() {
    status_ = "Requesting when-in-use location permission.";
    ios::location().requestWhenInUse([this](ios::Result<ios::PermissionState> result) {
        if (!result.ok) {
            status_ = "Location permission failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
            return;
        }
        if (result.value != ios::PermissionState::Authorized) {
            status_ = "Location permission is " + ios::toString(result.value) + ".";
            tc::redraw();
            return;
        }

        ios::location().start({ios::LocationAccuracy::HundredMeters, 10.0},
                              [this](ios::Result<ios::LocationSample> sample) {
            if (sample.ok) {
                latestLocation_ = sample.value;
                ++locationUpdates_;
                status_ = "Location updated.";
            } else {
                status_ = "Location update failed: " + ios::toString(sample.error.code) + " - " + sample.error.message;
            }
            tc::redraw();
        });
        status_ = "Location updates started.";
        tc::redraw();
    });
    tc::redraw();
}

void tcApp::scheduleNotification() {
    status_ = "Requesting notification permission.";
    ios::permissions().request(ios::Permission::Notifications, [this](ios::Result<ios::PermissionState> result) {
        if (!result.ok || result.value != ios::PermissionState::Authorized) {
            status_ = result.ok
                ? "Notification permission is " + ios::toString(result.value) + "."
                : "Notification permission failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
            return;
        }

        ios::notifications().schedule({"tcxios-location-example", "tcxIOS", "Local notification from TrussC.", 5.0, false},
                                      [this](ios::Result<std::string> scheduled) {
            status_ = scheduled.ok
                ? "Notification scheduled: " + scheduled.value
                : "Notification failed: " + ios::toString(scheduled.error.code) + " - " + scheduled.error.message;
            tc::redraw();
        });
    });
    tc::redraw();
}
