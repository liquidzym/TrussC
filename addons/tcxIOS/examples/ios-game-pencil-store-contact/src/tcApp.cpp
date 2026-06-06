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
    controllerCount_ = static_cast<int>(ios::gameController().connectedControllerNames().size());
    if (ios::gameController().latest(gamepad_)) tc::redraw();
}

void tcApp::draw() {
    tc::clear(0.08f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS game / pencil / store / contacts\n\n"
         << "Controllers: " << controllerCount_ << "\n"
         << "Gamepad: " << gamepad_.name << "\n"
         << "A: " << (gamepad_.buttonA.pressed ? "down" : "up")
         << ", LX: " << gamepad_.leftThumbstickX
         << ", LY: " << gamepad_.leftThumbstickY << "\n"
         << "Can make payments: " << (ios::storeKit().canMakePayments() ? "yes" : "no") << "\n\n"
         << "P: present PencilKit canvas\n"
         << "X: capture PencilKit drawing\n"
         << "G: request StoreKit product metadata\n"
         << "B: purchase cached product\n"
         << "C: pick contact\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'P') {
        ios::pencilCanvas().present({}, [this](ios::Result<void> result) {
            status_ = result.ok
                ? "Pencil canvas presented."
                : "Pencil canvas failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    } else if (key == 'X') {
        auto result = ios::pencilCanvas().capture();
        status_ = result.ok
            ? "Pencil data bytes: " + std::to_string(result.value.data.size()) + ", png bytes: " + std::to_string(result.value.png.size())
            : "Pencil capture failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    } else if (key == 'G') {
        ios::storeKit().requestProducts({"com.trussc.tcxios.example.product"},
                                        [this](ios::Result<std::vector<ios::StoreProduct>> result) {
            status_ = result.ok
                ? "Store products returned: " + std::to_string(result.value.size())
                : "Store products failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    } else if (key == 'B') {
        ios::storeKit().purchase("com.trussc.tcxios.example.product",
                                 [this](ios::Result<ios::StorePurchaseResult> result) {
            status_ = result.ok
                ? "Purchase completed."
                : "Purchase failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    } else if (key == 'C') {
        ios::contactsUI().pickContact([this](ios::Result<ios::PickedContact> result) {
            status_ = result.ok
                ? "Contact: " + result.value.givenName + " " + result.value.familyName
                : "Contact failed: " + ios::toString(result.error.code) + " - " + result.error.message;
            tc::redraw();
        });
    }
}
