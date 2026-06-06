#pragma once

#include "TrussC.h"
#include "tcxIOS.h"

#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void startLocation();
    void scheduleNotification();

    std::string status_ = "L: location, N: local notification, W: Safari.";
    tcx::ios::LocationSample latestLocation_;
    tcx::ios::NetworkPath networkPath_;
    int locationUpdates_ = 0;
};
