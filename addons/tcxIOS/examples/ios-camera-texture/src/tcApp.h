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
    void startCamera();
    void startCameraAfterPermission();

    std::string status_ = "Press C to start camera.";
    tcx::ios::CameraFrame latestFrame_;
    tc::Texture cameraTexture_;
    bool textureReady_ = false;
    int frameCount_ = 0;
};
