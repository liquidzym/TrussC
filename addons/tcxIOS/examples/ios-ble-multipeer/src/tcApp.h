#pragma once

#include "TrussC.h"
#include "tcxIOS.h"

#include <string>
#include <vector>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void startBLEScan();
    void connectFirstPeripheral();
    void startMultipeer();
    void sendMultipeerMessage();

    std::string status_ = "S: BLE scan, C: connect first, M: multipeer start, D: send data.";
    std::vector<tcx::ios::BLEPeripheralInfo> peripherals_;
    std::vector<tcx::ios::MultipeerPeer> peers_;
    int messagesReceived_ = 0;
};
