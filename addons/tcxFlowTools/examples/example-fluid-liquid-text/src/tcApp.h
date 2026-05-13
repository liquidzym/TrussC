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
    void renderTextSource(float time);
    void injectProceduralVelocity(float time);

    tcx::flow::Fluid2D fluid_;
    tcx::flow::MouseFlow mouseFlow_;
    tc::Fbo textSource_;
    tc::Vec2 previousMouse_;
    bool wasMousePressed_ = false;
    bool showSource_ = false;
    bool showCombined_ = true;
    float densityMix_ = 0.145f;
    float temperatureMix_ = 0.038f;
};
