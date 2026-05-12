#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void windowResized(int width, int height) override;

private:
    void resizeSystems();
    void injectFluid(float time);
    std::string modeName() const;

    tcx::flow::Fluid2D fluid_;
    tcx::flow::SplitVelocity splitVelocity_;
    int mode_ = 0;
    bool showDensity_ = true;
};
