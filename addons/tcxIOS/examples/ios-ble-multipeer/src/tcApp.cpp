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
    tc::clear(0.06f);
    tc::setColor(1.0f);

    std::ostringstream text;
    text << "tcxIOS BLE / Multipeer\n\n"
         << "Bluetooth: " << ios::toString(ios::bluetoothLE().state()) << "\n"
         << "Discovered peripherals: " << peripherals_.size() << "\n"
         << "Multipeer peers: " << peers_.size() << "\n"
         << "Messages received: " << messagesReceived_ << "\n\n"
         << "S: start BLE scan\n"
         << "C: connect first discovered peripheral\n"
         << "M: start Multipeer\n"
         << "D: send Multipeer message\n\n"
         << status_;

    tc::drawBitmapString(text.str(), 24.0f, 32.0f);
}

void tcApp::keyPressed(int key) {
    if (key == 'S') {
        startBLEScan();
    } else if (key == 'C') {
        connectFirstPeripheral();
    } else if (key == 'M') {
        startMultipeer();
    } else if (key == 'D') {
        sendMultipeerMessage();
    }
}

void tcApp::startBLEScan() {
    status_ = "BLE scan started.";
    ios::bluetoothLE().startScan({}, [this](const ios::BLEPeripheralInfo& peripheral) {
        peripherals_.push_back(peripheral);
        status_ = "BLE peripheral: " + peripheral.name + " / " + peripheral.identifier;
        tc::redraw();
    });
    tc::redraw();
}

void tcApp::connectFirstPeripheral() {
    if (peripherals_.empty()) {
        status_ = "No BLE peripheral discovered yet.";
        tc::redraw();
        return;
    }
    status_ = "Connecting BLE peripheral.";
    ios::bluetoothLE().connect(peripherals_.front().identifier, [this](ios::Result<void> result) {
        status_ = result.ok
            ? "BLE connected; services are discovering."
            : "BLE connect failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    });
    tc::redraw();
}

void tcApp::startMultipeer() {
    ios::multipeer().setPeerHandler([this](const std::vector<ios::MultipeerPeer>& peers) {
        peers_ = peers;
        tc::redraw();
    });
    ios::multipeer().setMessageHandler([this](const ios::MultipeerMessage& message) {
        ++messagesReceived_;
        status_ = "Message from " + message.peer.displayName + ".";
        tc::redraw();
    });
    ios::multipeer().start({"tcxios", "", true, true}, [this](ios::Result<void> result) {
        status_ = result.ok
            ? "Multipeer started."
            : "Multipeer failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    });
    tc::redraw();
}

void tcApp::sendMultipeerMessage() {
    const std::string text = "hello from tcxIOS";
    ios::multipeer().send(std::vector<std::uint8_t>(text.begin(), text.end()),
                          [this](ios::Result<void> result) {
        status_ = result.ok
            ? "Multipeer message sent."
            : "Multipeer send failed: " + ios::toString(result.error.code) + " - " + result.error.message;
        tc::redraw();
    });
    tc::redraw();
}
