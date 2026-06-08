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
    void applyBridgeSettings();
    void updateInputTexture(float time);

    tcx::flow::Fluid2D fluid_;
    tcx::flow::VelocityBridge velocityBridge_;
    tcx::flow::DensityBridge densityBridge_;
    tcx::flow::TemperatureBridge temperatureBridge_;
    tcx::flow::CombinedBridge combinedBridge_;
    tc::Fbo inputTexture_;
    int mode_ = 4;
    bool invert_ = false;
    bool useAlphaAsMask_ = false;
    bool mirrorX_ = false;
    bool mirrorY_ = false;
    int maskSourceIndex_ = 0;
    float maskSoftness_ = 0.0f;
    float maskGamma_ = 1.0f;
};
