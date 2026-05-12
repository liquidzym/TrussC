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
    void mouseDragged(tc::Vec2 pos, int button) override;
    void windowResized(int width, int height) override;

private:
    tcx::flow::Fluid2D fluid_;
    tcx::flow::MouseFlow mouseFlow_;
    tcx::flow::FlowVisualizer::Mode mode_ = tcx::flow::FlowVisualizer::Mode::Density;
    tc::Vec2 previousMouse_;
    bool wasMousePressed_ = false;
};
