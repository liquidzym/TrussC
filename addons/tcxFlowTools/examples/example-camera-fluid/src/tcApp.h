#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void windowResized(int width, int height) override;

private:
    void resizeSystems();
    void updateCameraInput(float dt);
    void updateFallbackInput(float dt, float time);

    tc::VideoGrabber grabber_;
    tcx::flow::Fluid2D fluid_;
    tcx::flow::OpticalFlow opticalFlow_;
    bool showFlow_ = false;
    bool showCamera_ = true;
    bool cameraStarted_ = false;
};
