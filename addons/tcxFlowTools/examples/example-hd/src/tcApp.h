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
    void resizeFluid();

    tcx::flow::Fluid2D fluid_;
    float scale_ = 0.25f;
    float outputScale_ = 1.0f;
};
