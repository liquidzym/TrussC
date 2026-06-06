#pragma once

#include "TrussC.h"
#include "tcxIOS.h"

#include <filesystem>
#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void registerAndScheduleRefresh();
    void startDownload();

    std::string status_ = "B: register/schedule BG task, D: start background download.";
    std::filesystem::path downloadPath_;
    double downloadFraction_ = 0.0;
};
