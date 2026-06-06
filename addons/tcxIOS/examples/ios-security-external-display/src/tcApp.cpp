#include "tcApp.h"

#include <sstream>

namespace ios = tcx::ios;

void tcApp::setup() {
    tc::setTouchAsMouse(true);
    tc::setIndependentFps(tc::VSYNC, tc::EVENT_DRIVEN);
    externalScreenCount_ = static_cast<int>(ios::externalDisplay().screens().size());
    ios::externalDisplay().setChangeHandler([this](const std::vector<ios::ExternalScreenInfo>& screens) {
        externalScreenCount_ = static_cast<int>(screens.size());
        status_ = "External display list changed.";
        tc::redraw();
    });
    tc::redraw();
}

void tcApp::update() {
    ios::update();
}

void tcApp::draw() {
    tc::clear(0.05f);
    tc::setColor(1.0f);

    const ios::AuthenticationAvailability auth =
        ios::localAuthentication().availability(ios::AuthenticationPolicy::DeviceOwnerAuthentication);

    std::ostringstream text;
    text << "tcxIOS security / external display\n\n"
         << "Auth available: " << (auth.available ? "yes" : "no")
         << " (" << auth.biometryType << ")\n"
         << "Screens: " << externalScreenCount_ << "\n\n"
         << "K: set and read Keychain string\n"
         << "R: remove Keychain item\n"
         << "A: LocalAuthentication prompt\n"
         << "E: show external display window\n"
         << "X: dismiss external displays\n"
         << "W: open Safari\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'K') {
        setAndReadKeychain();
    } else if (key == 'R') {
        removeKeychain();
    } else if (key == 'A') {
        authenticate();
    } else if (key == 'E') {
        showExternalDisplay();
    } else if (key == 'X') {
        ios::externalDisplay().dismissAll();
        status_ = "External displays dismissed.";
        tc::redraw();
    } else if (key == 'W') {
        ios::web().openSafari({"https://trussc.org"}, [this](ios::Result<void> result) {
            status_ = result.ok
                ? "Safari dismissed."
                : "Safari failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    }
}

void tcApp::setAndReadKeychain() {
    auto setResult = ios::keychain().setString("tcxIOS.example", "token", "trussc");
    if (!setResult.ok) {
        status_ = "Keychain set failed: " + ios::toString(setResult.error.code) + " - " + setResult.error.message;
        tc::redraw();
        return;
    }

    auto getResult = ios::keychain().getString("tcxIOS.example", "token");
    status_ = getResult.ok
        ? "Keychain value: " + getResult.value
        : "Keychain get failed: " + ios::toString(getResult.error.code) + " - " + getResult.error.message;
    tc::redraw();
}

void tcApp::removeKeychain() {
    auto result = ios::keychain().remove("tcxIOS.example", "token");
    status_ = result.ok
        ? "Keychain item removed."
        : "Keychain remove failed: " + ios::toString(result.error.code) + " - " + result.error.message;
    tc::redraw();
}

void tcApp::authenticate() {
    ios::localAuthentication().evaluate({"Authenticate tcxIOS example",
                                         ios::AuthenticationPolicy::DeviceOwnerAuthentication},
                                        [this](ios::Result<ios::AuthenticationResult> result) {
        status_ = result.ok
            ? "Authentication succeeded with " + result.value.biometryType + "."
            : "Authentication failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    });
    status_ = "Authentication requested.";
    tc::redraw();
}

void tcApp::showExternalDisplay() {
    ios::externalDisplay().show({"", "tcxIOS external display"},
                                [this](ios::Result<ios::ExternalDisplayPresentation> result) {
        status_ = result.ok
            ? "External display shown on " + result.value.screenIdentifier + "."
            : "External display failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    });
    tc::redraw();
}
