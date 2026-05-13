#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mousePressed(tc::Vec2 pos, int button) override;
    void windowResized(int width, int height) override;

private:
    void resizeSystems();
    void renderImageSource();
    void injectPaintingFlow(float time);

    tcx::flow::Fluid2D fluid_;
    tc::Image sourceImage_;
    tc::Fbo imageSource_;
    tc::Vec2 previousMouse_;
    bool wasMousePressed_ = false;
    bool showSource_ = false;
    bool showCombined_ = false;
    bool sourceReady_ = false;
    int updateFrame_ = 0;
    float continuousMix_ = 0.46f;
};
